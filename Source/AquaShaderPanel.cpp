// パネル: シェーダー（UI のシェーダー .uishader を書く）
//
//   左: プロジェクトのシェーダーの一覧・見本（グレースケール・ディゾルブ・シャインなど）から作る
//   右: エディター。書くと少し待ってコンパイルし、エラーを行ごとに出す（クリックでその行へ）。
//       コンパイルできたらすぐ保存して、キャンバスのウィジェットに反映する（書きながら反映）。
//   @param を足す・消す・並べ替えても、ウィジェットの値は名前で引き継ぐ。

#include "AquaApp.h"

#include <Ruto/EditorUI/EditorUI.h>
#include <Ruto/EditorUI/Widgets.h>

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <regex>

namespace AquaTool {

using namespace Ruto::EditorUI;

namespace {

constexpr double kCompileDelay = 0.35; // 書くのを止めてからコンパイルするまで

struct ErrorLine {
    int line = 0; // 0 なら行が分からない
    std::string message;
    bool warning = false;
};

// GLSL のエラー（"0:3(17): error: ..." / "0(3) : error ..." / "ERROR: 0:3: ..."）を行ごとに分ける
std::vector<ErrorLine> ParseErrors(const std::string& log) {
    std::vector<ErrorLine> out;
    static const std::regex kLine(R"((?:ERROR:\s*)?\d+[:(](\d+)\)?(?:\(\d+\))?\s*:?\s*(.*))");
    size_t start = 0;
    while (start < log.size()) {
        size_t end = log.find('\n', start);
        if (end == std::string::npos) end = log.size();
        std::string line = log.substr(start, end - start);
        start = end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\0')) line.pop_back();
        if (line.empty()) continue;
        std::smatch m;
        if (std::regex_match(line, m, kLine)) {
            const int n = std::atoi(m[1].str().c_str());
            out.push_back({n >= 10000 ? 0 : n, m[2].str()}); // 10000 行目より後は組み込みの部分
        } else {
            out.push_back({0, line});
        }
        out.back().warning = out.back().message.find("warning") != std::string::npos || out.back().message.find("WARNING") != std::string::npos;
    }
    // エラーを先に（警告は後ろ）
    std::stable_sort(out.begin(), out.end(), [](const ErrorLine& a, const ErrorLine& b) { return !a.warning && b.warning; });
    return out;
}

// テキストの line 行目（1 から）の始まりと終わり（バイト）
std::pair<int, int> LineRange(const std::string& text, int line) {
    int start = 0;
    for (int l = 1; l < line; ++l) {
        const size_t nl = text.find('\n', static_cast<size_t>(start));
        if (nl == std::string::npos) return {static_cast<int>(text.size()), static_cast<int>(text.size())};
        start = static_cast<int>(nl) + 1;
    }
    size_t end = text.find('\n', static_cast<size_t>(start));
    if (end == std::string::npos) end = text.size();
    return {start, static_cast<int>(end)};
}

struct EditorCallback {
    App* app = nullptr;
    int* jumpLine = nullptr;
    int* cursorLine = nullptr;
    const std::string* text = nullptr;
};

int ShaderEditorCallback(ImGuiInputTextCallbackData* data) {
    auto* cb = static_cast<EditorCallback*>(data->UserData);
    if (*cb->jumpLine > 0) {
        // エラーの行を選ぶ
        const std::string buf(data->Buf, static_cast<size_t>(data->BufTextLen));
        const auto [a, b] = LineRange(buf, *cb->jumpLine);
        data->SelectionStart = a;
        data->SelectionEnd = b;
        data->CursorPos = b;
        *cb->jumpLine = -1;
    }
    int line = 1;
    for (int i = 0; i < data->CursorPos && i < data->BufTextLen; ++i) {
        if (data->Buf[i] == '\n') ++line;
    }
    *cb->cursorLine = line;
    return 0;
}

const char* TypeName(UIShaderParam::Type t) {
    switch (t) {
    case UIShaderParam::Type::Float: return "数値";
    case UIShaderParam::Type::Vec2: return "2 つの数値";
    case UIShaderParam::Type::Color: return "色";
    case UIShaderParam::Type::Toggle: return "オン / オフ";
    }
    return "";
}

} // namespace

// ---------------------------------------------------------------------------
// 開く・保存・作る
// ---------------------------------------------------------------------------

void App::OpenShaderEditor(const std::string& assetPath) {
    m_FocusShaderPanel = true;
    if (assetPath.empty()) {
        m_OpenNewShader = true;
        return;
    }
    LoadShaderForEdit(assetPath);
}

void App::LoadShaderForEdit(const std::string& assetPath) {
    if (assetPath == m_ShaderPath && !m_ShaderPath.empty()) return;
    // 書きかけで保存できていないものがあれば、先に保存する
    if (!m_ShaderPath.empty() && m_ShaderText != m_ShaderSavedText) SaveShader();
    const auto text = FileSystem::ReadTextFile(Assets::GetPath(assetPath));
    if (!text) {
        Notify("シェーダーを読めません: " + assetPath, true);
        return;
    }
    m_ShaderPath = assetPath;
    m_ShaderText = *text;
    m_ShaderSavedText = *text;
    m_ShaderCheck = UIShader::FromSource(m_ShaderText, m_ShaderPath);
    m_ShaderSavedParams = m_ShaderCheck->GetParams();
    m_ShaderEditTime = -1.0;
}

bool App::SaveShader() {
    if (m_ShaderPath.empty()) return false;
    const fs::path file = Assets::GetPath(m_ShaderPath);
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    if (!FileSystem::WriteTextFile(file, m_ShaderText)) {
        Notify("シェーダーを保存できません: " + m_ShaderPath, true);
        return false;
    }
    m_ShaderSavedText = m_ShaderText;
    UIShader::Reload(m_ShaderPath);
    m_Project.Refresh(true);

    // @param が変わったら、このシェーダーを使っているウィジェットの値を名前で引き継ぐ（新しいものは既定値）
    const std::vector<UIShaderParam> newParams = UIShader::ParseParams(m_ShaderText);
    auto same = [](const std::vector<UIShaderParam>& a, const std::vector<UIShaderParam>& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (a[i].name != b[i].name || a[i].type != b[i].type || a[i].slot != b[i].slot) return false;
        }
        return true;
    };
    if (!same(newParams, m_ShaderSavedParams)) {
        int updated = 0;
        for (auto& d : m_Docs) {
            bool changed = false;
            d->asset->root->ForEach([&](Widget& w) {
                if (w.generated || w.shader != m_ShaderPath) return;
                Vec4 values[Widget::kShaderParamCount]{};
                for (const UIShaderParam& p : newParams) {
                    values[p.slot] = p.defaultValue;
                    for (const UIShaderParam& old : m_ShaderSavedParams) {
                        if (old.name == p.name && old.type == p.type && old.slot >= 0 && old.slot < Widget::kShaderParamCount) values[p.slot] = w.shaderParams[old.slot];
                    }
                }
                std::copy(std::begin(values), std::end(values), std::begin(w.shaderParams));
                changed = true;
                ++updated;
            });
            if (changed) d->Changed();
        }
        if (updated > 0) Log::Info("シェーダーの @param が変わったので、{} 個のウィジェットの値を引き継ぎました", updated);
        m_ShaderSavedParams = newParams;
    }
    return true;
}

std::string App::CreateShader(const std::string& name, const std::string& source) {
    std::string base = ToIdentifier(name.empty() ? std::string("NewShader") : name);
    const std::string folder = m_Project.settings.outputFolder + "/Shaders/";
    std::string path = folder + base + ".uishader";
    std::error_code ec;
    for (int n = 2; fs::exists(Assets::GetPath(path), ec); ++n) path = std::format("{}{}{}.uishader", folder, base, n);
    const fs::path file = Assets::GetPath(path);
    fs::create_directories(file.parent_path(), ec);
    if (!FileSystem::WriteTextFile(file, source)) {
        Notify("シェーダーを作れません: " + path, true);
        return {};
    }
    m_Project.Refresh(true);
    Log::Info("シェーダーを作りました: Assets/{}", path);
    m_ShaderPath.clear();
    LoadShaderForEdit(path);
    return path;
}

void App::ApplyShaderToSelection(const std::string& assetPath) {
    Document* doc = ActiveDocument();
    Widget* w = doc && doc->ui.hasSelection ? Resolve(doc->asset->root.get(), doc->ui.selection) : nullptr;
    if (!w) {
        Notify("シェーダーを掛けるウィジェットを選んでください", true);
        return;
    }
    UIShader::Reload(assetPath);
    w->shader = assetPath;
    w->ResetShaderParams();
    doc->Changed();
    Notify(std::format("{} にシェーダーを掛けました", w->name));
}

// ---------------------------------------------------------------------------
// パネル
// ---------------------------------------------------------------------------

void App::DrawShaderPanel() {
    const double now = Platform::GetTime();
    // 書くのを止めて少ししたらコンパイルする（できたら保存してキャンバスに反映）
    if (m_ShaderEditTime >= 0.0 && now - m_ShaderEditTime > kCompileDelay) {
        m_ShaderEditTime = -1.0;
        m_ShaderCheck = UIShader::FromSource(m_ShaderText, m_ShaderPath);
        if (m_ShaderLive && m_ShaderCheck->IsValid()) SaveShader();
    }

    // ---- 新しいシェーダー ----
    if (m_OpenNewShader) {
        ImGui::OpenPopup("新しいシェーダー");
        m_NewShaderName = m_NewShaderPreset >= 0 ? UIShader::GetPresets()[static_cast<size_t>(m_NewShaderPreset)].name : "NewShader";
        m_OpenNewShader = false;
    }
    if (ImGui::BeginPopupModal("新しいシェーダー", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextDisabled("名前（Assets/%s/Shaders/名前.uishader に作ります）", m_Project.settings.outputFolder.c_str());
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(360.0f * m_Scale);
        const bool enter = ImGui::InputText("##name", &m_NewShaderName, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::TextDisabled("もとにするもの");
        ImGui::BeginChild("##presets", {360.0f * m_Scale, 220.0f * m_Scale}, ImGuiChildFlags_Borders);
        if (ImGui::Selectable(ICON_FA_FILE "  ひな形（説明付き）", m_NewShaderPreset < 0)) m_NewShaderPreset = -1;
        const auto presets = UIShader::GetPresets();
        for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
            if (ImGui::Selectable(std::format(ICON_FA_FILL_DRIP "  {}##p{}", presets[static_cast<size_t>(i)].label, i).c_str(), m_NewShaderPreset == i)) {
                if (m_NewShaderName.empty() || m_NewShaderName == "NewShader" || (m_NewShaderPreset >= 0 && m_NewShaderName == presets[static_cast<size_t>(m_NewShaderPreset)].name))
                    m_NewShaderName = presets[static_cast<size_t>(i)].name;
                m_NewShaderPreset = i;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", presets[static_cast<size_t>(i)].description);
        }
        ImGui::EndChild();
        Document* doc = ActiveDocument();
        const bool hasSel = doc && doc->ui.hasSelection && !doc->ui.selection.empty();
        ImGui::BeginDisabled(!hasSel);
        ImGui::Checkbox("作ったら選択中のウィジェットに掛ける", &m_NewShaderApply);
        ImGui::EndDisabled();
        ImGui::Spacing();
        if (ImGui::Button("作成", {120.0f * m_Scale, 0.0f}) || enter) {
            const std::string source = m_NewShaderPreset >= 0 ? presets[static_cast<size_t>(m_NewShaderPreset)].source : UIShader::NewShaderSource();
            const std::string path = CreateShader(m_NewShaderName, source);
            if (!path.empty() && hasSel && m_NewShaderApply) ApplyShaderToSelection(path);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("キャンセル", {120.0f * m_Scale, 0.0f}) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (!ImGui::BeginTable("##shader", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImGui::GetContentRegionAvail())) return;
    ImGui::TableSetupColumn("一覧", ImGuiTableColumnFlags_WidthFixed, 220.0f * m_Scale);
    ImGui::TableSetupColumn("エディター", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();

    // ---- 一覧 ----
    ImGui::TableSetColumnIndex(0);
    ImGui::BeginChild("##shader-list");
    if (ImGui::Button(ICON_FA_PLUS "  新しいシェーダー", {-FLT_MIN, 0.0f})) {
        m_NewShaderPreset = -1;
        m_OpenNewShader = true;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kAccent);
    ImGui::SeparatorText("このプロジェクト");
    ImGui::PopStyleColor();
    const auto& shaders = m_Project.GetAssets(AssetType::UIShader);
    if (shaders.empty()) ImGui::TextDisabled("まだありません");
    for (const std::string& path : shaders) {
        ImGui::PushID(path.c_str());
        auto sh = UIShader::Get(path);
        const bool bad = sh && !sh->IsValid();
        const std::string name = FileSystem::ToUtf8(FileSystem::FromUtf8(path).stem());
        ImGui::PushStyleColor(ImGuiCol_Text, bad ? Theme::kError : Theme::kText);
        if (ImGui::Selectable(std::format("{}  {}", bad ? ICON_FA_TRIANGLE_EXCLAMATION : ICON_FA_FILL_DRIP, name).c_str(), path == m_ShaderPath)) LoadShaderForEdit(path);
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("Assets/%s%s\nキャンバス・階層のウィジェットへドラッグすると掛かります", path.c_str(), bad ? "\nエラーがあります" : "");
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(kAssetPayload, path.data(), path.size());
            ImGui::Text(ICON_FA_FILL_DRIP "  %s", name.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginPopupContextItem("shader-menu")) {
            if (ImGui::MenuItem(ICON_FA_ARROW_POINTER "  選択中のウィジェットに掛ける")) ApplyShaderToSelection(path);
            if (ImGui::MenuItem(ICON_FA_CLONE "  複製")) {
                if (const auto text = FileSystem::ReadTextFile(Assets::GetPath(path))) CreateShader(name, *text);
            }
            if (ImGui::MenuItem(ICON_FA_FOLDER "  エクスプローラーで表示")) Platform::OpenInFileBrowser(Assets::GetPath(path).parent_path());
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kAccent);
    ImGui::SeparatorText("見本から作る");
    ImGui::PopStyleColor();
    const auto presets = UIShader::GetPresets();
    for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
        const UIShader::Preset& p = presets[static_cast<size_t>(i)];
        ImGui::PushID(i);
        if (ImGui::Selectable(std::format(ICON_FA_PLUS "  {}", p.label).c_str())) {
            m_NewShaderPreset = i;
            m_OpenNewShader = true;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s\nクリックで、この見本からシェーダーを作る", p.description);
        ImGui::PopID();
    }
    ImGui::EndChild();

    // ---- エディター ----
    ImGui::TableSetColumnIndex(1);
    ImGui::BeginChild("##shader-editor");
    if (m_ShaderPath.empty()) {
        ImGui::Spacing();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled(ICON_FA_FILL_DRIP "  UI のシェーダーで、ウィジェットの色や形を GLSL で変えられます（白黒・ディゾルブ・光が流れる・ホログラムなど）。");
        ImGui::TextDisabled("左の一覧から選ぶか、見本から作ってください。作ったシェーダーはキャンバス・階層のウィジェットへドラッグするか、詳細パネルの「シェーダー」で選ぶと掛かります。");
        ImGui::TextDisabled("// @param で書いた値は、詳細パネルでウィジェットごとに変えられ、アニメーションでも動かせます。");
        ImGui::PopTextWrapPos();
        ImGui::EndChild();
        ImGui::EndTable();
        return;
    }
    const bool dirty = m_ShaderText != m_ShaderSavedText;
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored({0.85f, 0.6f, 1.0f, 1.0f}, ICON_FA_FILL_DRIP);
    ImGui::SameLine();
    ImGui::Text("%s%s", m_ShaderPath.c_str(), dirty ? "  ●" : "");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip(dirty ? "保存していない変更があります" : "保存済み");
    ImGui::SameLine();
    ImGui::BeginDisabled(!dirty);
    if (ImGui::Button(ICON_FA_FLOPPY_DISK " 保存")) SaveShader();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Checkbox("書きながら反映", &m_ShaderLive);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("コンパイルできたら、すぐ保存してキャンバス（と RutoEditor・実行中のゲーム）に反映する");
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ARROW_POINTER " 選択中のウィジェットに掛ける")) {
        if (dirty) SaveShader();
        ApplyShaderToSelection(m_ShaderPath);
    }
    ImGui::SameLine();
    if (ImGui::BeginCombo("##replace", "見本で置き換え", ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_WidthFitPreview)) {
        for (const UIShader::Preset& p : presets) {
            if (ImGui::Selectable(p.label)) {
                m_ShaderText = p.source;
                m_ShaderEditTime = now;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", p.description);
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (IconButton(ICON_FA_CIRCLE_QUESTION, "使える関数・値", m_ShaderHelp)) m_ShaderHelp = !m_ShaderHelp;

    if (m_ShaderHelp) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("vec4 effect(vec4 color) に書く。color はふだん描かれる色（テクスチャ × 色）。返した色が描かれる");
        ImGui::TextDisabled("uTime 秒 / LocalUV() ウィジェットの中の 0〜1（左上が 0）/ LocalPixel() / ScreenUV() / uRect 範囲（ピクセル）/ uScreenSize");
        ImGui::TextDisabled("SampleTexture(uv) 画像を別の所で読む（vUV が今の所）/ vColor / Luma(rgb) 明るさ / HueShift(rgb, 0〜1) / Hash(vec2) / Noise(vec2)");
        ImGui::TextDisabled("// @param 名前 float 最小 最大 既定値 表示名 ・ vec2 最小 最大 x y ・ color r g b a ・ toggle 0か1（最大 6 個）");
        ImGui::PopTextWrapPos();
    }

    // 下の欄（エラー・@param）の高さを残す
    const std::vector<ErrorLine> errors = m_ShaderCheck && !m_ShaderCheck->IsValid() ? ParseErrors(m_ShaderCheck->GetError()) : std::vector<ErrorLine>{};
    const int infoLines = std::clamp(static_cast<int>(errors.size()), 1, 3) + 1;
    const float infoH = ImGui::GetTextLineHeightWithSpacing() * static_cast<float>(infoLines) + ImGui::GetStyle().ItemSpacing.y * 2.0f;
    const float avail = ImGui::GetContentRegionAvail().y;
    EditorCallback cb{this, &m_ShaderJumpLine, &m_ShaderCursorLine, &m_ShaderText};
    if (ImGui::InputTextMultiline("##code", &m_ShaderText, {-FLT_MIN, std::max({avail - infoH, avail * 0.55f, 80.0f})},
                                  ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways, ShaderEditorCallback, &cb))
        m_ShaderEditTime = now;
    const bool editorFocused = ImGui::IsItemActive();
    if (editorFocused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) SaveShader();
    if (m_ShaderJumpLine > 0 && !editorFocused) ImGui::ActivateItemByID(ImGui::GetItemID()); // エラーの行へ: 入力欄を選んでから行を選ぶ

    // ---- 状態 ----
    ImGui::TextDisabled("行 %d", m_ShaderCursorLine);
    ImGui::SameLine();
    if (m_ShaderEditTime >= 0.0) {
        ImGui::TextDisabled(ICON_FA_HOURGLASS_HALF "  コンパイル待ち...");
    } else if (m_ShaderCheck && m_ShaderCheck->IsValid()) {
        const auto& params = m_ShaderCheck->GetParams();
        ImGui::TextColored(Theme::kSuccess, ICON_FA_CHECK "  コンパイルできました%s", dirty ? "（まだ保存していません）" : "");
        ImGui::SameLine();
        ImGui::TextDisabled("·  @param %d 個%s", static_cast<int>(params.size()), params.empty() ? "" : ":");
        for (const UIShaderParam& p : params) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s（%s・%s）", p.label.c_str(), p.name.c_str(), TypeName(p.type));
        }
    } else if (m_ShaderCheck) {
        const int errorCount = static_cast<int>(std::count_if(errors.begin(), errors.end(), [](const ErrorLine& e) { return !e.warning; }));
        ImGui::TextColored(Theme::kError, ICON_FA_TRIANGLE_EXCLAMATION "  %d 個のエラー（クリックでその行へ。直るまで保存・反映しません）", errorCount);
        ImGui::BeginChild("##errors", {0.0f, 0.0f});
        for (size_t i = 0; i < errors.size(); ++i) {
            const ErrorLine& e = errors[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::PushStyleColor(ImGuiCol_Text, e.warning ? Theme::kWarning : Theme::kError);
            const std::string label = e.line > 0 ? std::format("行 {}: {}", e.line, e.message) : e.message;
            if (ImGui::Selectable(label.c_str()) && e.line > 0) m_ShaderJumpLine = e.line;
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();
    ImGui::EndTable();
}

} // namespace AquaTool
