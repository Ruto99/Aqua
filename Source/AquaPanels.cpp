// パネル: プロジェクト（UI の一覧）・アセット・スタイル・変数

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
#include <map>

namespace AquaTool {

using namespace Ruto::EditorUI;

namespace {

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool Matches(const std::string& text, const std::string& query) {
    return query.empty() || ToLower(text).find(ToLower(query)) != std::string::npos;
}

std::string ExtOf(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    return dot == std::string::npos ? std::string() : ToLower(path.substr(dot));
}

const char* AssetIcon(const std::string& ext) {
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") return ICON_FA_IMAGE;
    if (ext == ".ttf" || ext == ".otf" || ext == ".ttc" || ext == ".bdf" || ext == ".fnt" || ext == ".rfont") return ICON_FA_FONT;
    if (ext == ".wav" || ext == ".ogg") return ICON_FA_VOLUME_HIGH;
    if (ext == ".aqua") return ICON_FA_DROPLET;
    if (ext == ".uishader") return ICON_FA_FILL_DRIP;
    if (ext == ".rui") return ICON_FA_OBJECT_GROUP;
    if (ext == ".sobt") return ICON_FA_COMMENT_DOTS; // Shion のテキスト
    return ICON_FA_FILE;
}

} // namespace

// ---------------------------------------------------------------------------
// プロジェクト（UI の一覧）
// ---------------------------------------------------------------------------

void App::DrawProjectPanel() {
    m_Project.Refresh();
    // ワークスペースの名前と種類
    ImGui::TextColored(Theme::kAccent, "%s", m_Project.IsLibrary() ? ICON_FA_BOOK : ICON_FA_GAMEPAD);
    ImGui::SameLine();
    ImGui::TextUnformatted(m_Project.GetName().c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_Project.IsLibrary() ? "ライブラリ" : "ゲームのプロジェクト");
    if (ImGui::Button(ICON_FA_DESKTOP "  Body")) {
        m_NewKind = LayoutKind::Body;
        m_OpenNewPopup = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("新しい Body（画面のいちばん親のレイアウト）");
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button(ICON_FA_PUZZLE_PIECE "  パーツ")) {
        m_NewKind = LayoutKind::Part;
        m_OpenNewPopup = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("新しいパーツ（ほかのレイアウトに部品として入れるレイアウト）");
    ImGui::SameLine();
    SearchField("##project-search", m_ProjectSearch, "検索");
    ImGui::Separator();

    std::error_code ec;
    const fs::path srcDir = m_Project.GetSourceDir();
    int deleteIndex = -1;
    const auto& sources = m_Project.GetSources();
    // Body とパーツに分けて並べる（中はフォルダの見出しつき）
    for (LayoutKind section : {LayoutKind::Body, LayoutKind::Part}) {
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::kAccent);
        ImGui::SeparatorText(std::format("{}  {}", LayoutKindIcon(section), section == LayoutKind::Part ? "パーツ（部品として入れる）" : "Body（画面）").c_str());
        ImGui::PopStyleColor();
        std::string lastFolder = "\x01";
        int shown = 0;
        for (size_t i = 0; i < sources.size(); ++i) {
            const Project::SourceInfo& s = sources[i];
            if (s.kind != section) continue;
            const std::string rel = FileSystem::ToGenericUtf8(fs::relative(s.path, srcDir, ec));
            if (!Matches(rel, m_ProjectSearch) && !Matches(s.output, m_ProjectSearch)) continue;
            ++shown;
            // フォルダの見出し（ライブラリの Body / Parts は見出しで分かるので省く）
            const size_t slash = rel.find_last_of('/');
            std::string folder = slash == std::string::npos ? std::string() : rel.substr(0, slash);
            for (const char* top : {"Body", "Parts"}) {
                if (!m_Project.IsLibrary()) break;
                const std::string t = top;
                if (folder == t) folder.clear();
                else if (folder.starts_with(t + "/")) folder = folder.substr(t.size() + 1);
            }
            if (folder != lastFolder) {
                lastFolder = folder;
                if (!folder.empty()) ImGui::TextDisabled(ICON_FA_FOLDER "  %s", folder.c_str());
            }
            bool isOpen = false, isActive = false;
            for (size_t d = 0; d < m_Docs.size(); ++d) {
                if (fs::equivalent(m_Docs[d]->sourcePath, s.path, ec)) {
                    isOpen = true;
                    isActive = static_cast<int>(d) == m_Active;
                }
            }
            ImGui::PushID(static_cast<int>(i));
            if (!folder.empty()) ImGui::Indent();
            const std::string label = std::format("{}  {}", isOpen ? ICON_FA_PEN_TO_SQUARE : LayoutKindIcon(s.kind), FileSystem::ToUtf8(s.path.stem()));
            if (ImGui::Selectable(label.c_str(), isActive, ImGuiSelectableFlags_AllowDoubleClick)) OpenDocument(s.path);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::SetTooltip("%s\n→ %s%s%s", FileSystem::ToUtf8(s.path).c_str(), m_Project.IsLibrary() ? "" : "Assets/", s.output.c_str(),
                                  s.kind == LayoutKind::Part ? "\nキャンバス・階層へドラッグすると部品として入ります" : "");
            // パーツは、ほかのレイアウトに部品として入れられるように書き出した .aqua をドラッグできる
            if (s.kind == LayoutKind::Part && ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload(kAssetPayload, s.output.data(), s.output.size());
                ImGui::Text(ICON_FA_PUZZLE_PIECE "  %s（部品として入れる）", FileSystem::ToUtf8(s.path.stem()).c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginPopupContextItem("source-menu")) {
                if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  開く")) OpenDocument(s.path);
                if (ImGui::MenuItem(ICON_FA_FILE_EXPORT "  書き出す")) {
                    if (Document* d = OpenDocument(s.path)) {
                        d->Export();
                        Notify("書き出しました: " + d->output);
                    }
                }
                if (m_Project.IsLibrary() && ImGui::MenuItem(ICON_FA_PAPER_PLANE "  RutoEngine に送る", nullptr, false, m_Project.sendTarget.Valid())) {
                    std::string error;
                    if (Document* d = OpenDocument(s.path)) d->Export();
                    if (m_Project.SendLayout(s.output, &error)) Notify("送りました: Assets/" + m_Project.SendPathFor(s.output));
                    else Notify(error, true);
                }
                const LayoutKind other = s.kind == LayoutKind::Part ? LayoutKind::Body : LayoutKind::Part;
                if (ImGui::MenuItem(std::format("{}  {}にする", LayoutKindIcon(other), LayoutKindLabel(other)).c_str())) {
                    // 開いていれば文書で、開いていなければファイルを直接書き換える
                    bool done = false;
                    for (auto& d : m_Docs) {
                        if (fs::equivalent(d->sourcePath, s.path, ec)) {
                            d->SetKind(other);
                            done = true;
                        }
                    }
                    if (!done) {
                        if (auto doc = Document::Open(m_Project, s.path)) doc->SetKind(other);
                    }
                    m_Project.Refresh(true);
                    Notify(std::format("{}にしました", LayoutKindLabel(other)));
                }
                if (ImGui::MenuItem(ICON_FA_CLONE "  複製")) {
                    fs::path copy = s.path;
                    for (int n = 2; fs::exists(copy, ec); ++n) copy.replace_filename(FileSystem::FromUtf8(std::format("{}{}{}", FileSystem::ToUtf8(s.path.stem()), n, Ruto::Aqua::kSourceExtension)));
                    fs::copy_file(s.path, copy, ec);
                    // 書き出し先は新しい名前に合わせる
                    if (auto j = ReadSourceFile(copy)) {
                        (*j)["output"] = m_Project.DefaultOutputFor(copy);
                        WriteSourceFile(copy, *j);
                    }
                    m_Project.Refresh(true);
                    if (Document* d = OpenDocument(copy)) d->Export();
                }
                if (ImGui::MenuItem(ICON_FA_FOLDER "  エクスプローラーで表示")) Platform::OpenInFileBrowser(s.path.parent_path());
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_TRASH "  削除（.aqs と .aqua）")) deleteIndex = static_cast<int>(i);
                ImGui::EndPopup();
            }
            if (!folder.empty()) ImGui::Unindent();
            ImGui::PopID();
        }
        if (shown == 0) {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", section == LayoutKind::Part ? "パーツはまだありません。何度も使う見た目（カード・ボタン・ゲージなど）をパーツにすると、Body に入れて使い回せます"
                                                                  : "Body はまだありません。上の「Body」で作ってください");
            ImGui::PopTextWrapPos();
        }
        ImGui::Spacing();
    }
    if (deleteIndex >= 0) {
        ImGui::OpenPopup("UI を削除");
        m_DeleteSource = sources[static_cast<size_t>(deleteIndex)].path;
    }
    if (ImGui::BeginPopupModal("UI を削除", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const fs::path target = m_DeleteSource;
        std::string output;
        for (const auto& s : sources) {
            if (s.path == target) output = s.output;
        }
        ImGui::Text("%s を削除しますか？", FileSystem::ToUtf8(target.filename()).c_str());
        ImGui::TextDisabled("編集用ファイルと、書き出した %s%s を消します（元に戻せません）", m_Project.IsLibrary() ? "" : "Assets/", output.c_str());
        ImGui::TextDisabled("この UI を使っている UIDocument は表示されなくなります");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, Theme::kError);
        if (ImGui::Button("削除", {120.0f * m_Scale, 0.0f})) {
            for (size_t d = 0; d < m_Docs.size(); ++d) {
                if (fs::equivalent(m_Docs[d]->sourcePath, target, ec)) {
                    CloseDocument(d);
                    break;
                }
            }
            fs::remove(target, ec);
            if (!output.empty()) {
                fs::remove(Assets::GetPath(output), ec);
                std::string header = output;
                if (const size_t dot = header.find_last_of('.'); dot != std::string::npos) header = header.substr(0, dot);
                if (!m_Project.IsLibrary()) fs::remove(m_Project.GetLayout().GeneratedDir() / FileSystem::FromUtf8(header + ".h"), ec);
            }
            m_Project.Refresh(true);
            Log::Info("UI を削除しました: {}", FileSystem::ToUtf8(target.filename()));
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("キャンセル", {120.0f * m_Scale, 0.0f}) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // 取り込める以前の UI（.rui）
    std::vector<std::string> legacy;
    for (const std::string& f : m_Project.GetAssets(AssetType::Widget)) {
        if (ExtOf(f) != ".rui") continue;
        std::string aqua = f.substr(0, f.size() - 4) + ".aqua";
        if (m_Project.FindSourceFor(aqua).empty()) legacy.push_back(f);
    }
    if (!legacy.empty()) {
        ImGui::Spacing();
        ImGui::SeparatorText("以前の UI（.rui）");
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("RutoEditor の UI デザイナーで作った UI です。取り込むと Aqua で編集できます（同じ場所に .aqua を書き出します）");
        ImGui::PopTextWrapPos();
        for (const std::string& f : legacy) {
            ImGui::PushID(f.c_str());
            if (ImGui::SmallButton(ICON_FA_FILE_IMPORT " 取り込む")) OpenDocument(Assets::GetPath(f));
            ImGui::SameLine();
            ImGui::TextUnformatted(f.c_str());
            ImGui::PopID();
        }
    }
}

// ---------------------------------------------------------------------------
// アセット
// ---------------------------------------------------------------------------

void App::DrawAssetsPanel() {
    static const char* kFilters[] = {"すべて", ICON_FA_IMAGE " 画像", ICON_FA_FONT " フォント", ICON_FA_VOLUME_HIGH " 音", ICON_FA_DROPLET " UI", ICON_FA_FILL_DRIP " シェーダー"};
    ImGui::SetNextItemWidth(130.0f * m_Scale);
    ImGui::Combo("##asset-filter", &m_AssetFilter, kFilters, IM_ARRAYSIZE(kFilters));
    ImGui::SameLine();
    SearchField("##asset-search", m_AssetSearch, "検索", 220.0f * m_Scale);
    ImGui::SameLine();
    if (IconButton(ICON_FA_ROTATE, "一覧を更新")) m_Project.Refresh(true);
    ImGui::SameLine();
    if (IconButton(ICON_FA_FOLDER_OPEN, m_Project.IsLibrary() ? "ライブラリのフォルダを開く" : "Assets フォルダを開く")) Platform::OpenInFileBrowser(m_Project.GetAssetRoot());
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("キャンバス・階層・詳細の欄へドラッグして使います");

    const float cellW = std::round(96.0f * m_Scale);
    const float thumb = std::round(64.0f * m_Scale);
    ImGui::BeginChild("##asset-grid");
    const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / (cellW + ImGui::GetStyle().ItemSpacing.x)));
    int col = 0;
    for (const std::string& path : m_Project.GetAllAssets()) {
        const std::string ext = ExtOf(path);
        const bool image = ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga";
        const bool font = ext == ".ttf" || ext == ".otf" || ext == ".ttc" || ext == ".bdf" || ext == ".fnt" || ext == ".rfont";
        const bool audio = ext == ".wav" || ext == ".ogg";
        const bool ui = ext == ".aqua" || ext == ".rui";
        const bool shader = ext == ".uishader";
        if ((m_AssetFilter == 1 && !image) || (m_AssetFilter == 2 && !font) || (m_AssetFilter == 3 && !audio) || (m_AssetFilter == 4 && !ui) || (m_AssetFilter == 5 && !shader)) continue;
        if (!Matches(path, m_AssetSearch)) continue;
        if (col > 0) ImGui::SameLine();
        ImGui::PushID(path.c_str());
        ImGui::BeginGroup();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##cell", {cellW, thumb + ImGui::GetTextLineHeight() * 2.2f});
        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (hovered) dl->AddRectFilled(p, {p.x + cellW, p.y + thumb + ImGui::GetTextLineHeight() * 2.2f}, ToU32(Theme::kFrameHover), 4.0f);
        const ImVec2 tp{p.x + (cellW - thumb) * 0.5f, p.y + 2.0f};
        std::shared_ptr<Texture> tex = image ? Assets::GetTexture(path) : nullptr;
        if (tex && tex->GetWidth() > 0 && tex->GetHeight() > 0) {
            // 縦横比を保って枠に収める
            const float aspect = static_cast<float>(tex->GetWidth()) / static_cast<float>(tex->GetHeight());
            const float w = aspect >= 1.0f ? thumb : thumb * aspect, h = aspect >= 1.0f ? thumb / aspect : thumb;
            const ImVec2 a{tp.x + (thumb - w) * 0.5f, tp.y + (thumb - h) * 0.5f};
            dl->AddImage(ImTextureRef(TextureId(*tex)), a, {a.x + w, a.y + h}, {0, 1}, {1, 0});
        } else {
            dl->AddRectFilled(tp, {tp.x + thumb, tp.y + thumb}, ToU32(Theme::kFrame), 6.0f);
            const char* icon = AssetIcon(ext);
            ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.8f);
            const ImVec2 ts = ImGui::CalcTextSize(icon);
            dl->AddText({tp.x + (thumb - ts.x) * 0.5f, tp.y + (thumb - ts.y) * 0.5f}, ToU32(ui ? Theme::kAccent : shader ? ImVec4(0.85f, 0.6f, 1.0f, 1.0f) : Theme::kTextDim), icon);
            ImGui::PopFont();
        }
        // 名前（2 行まで、はみ出しは切る）
        const std::string name = FileSystem::ToUtf8(FileSystem::FromUtf8(path).filename());
        dl->PushClipRect({p.x, p.y}, {p.x + cellW, p.y + thumb + ImGui::GetTextLineHeight() * 2.2f}, true);
        const ImVec2 ns = ImGui::CalcTextSize(name.c_str(), nullptr, false, cellW - 4.0f);
        dl->AddText(nullptr, 0.0f, {p.x + std::max((cellW - ns.x) * 0.5f, 2.0f), tp.y + thumb + 3.0f}, ToU32(Theme::kText), name.c_str(), nullptr, cellW - 4.0f);
        dl->PopClipRect();
        if (hovered) ImGui::SetTooltip("%s%s", path.c_str(), tex ? std::format("\n{} × {}", tex->GetWidth(), tex->GetHeight()).c_str() : "");
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(kAssetPayload, path.data(), path.size());
            ImGui::Text("%s  %s", AssetIcon(ext), path.c_str());
            ImGui::EndDragDropSource();
        }
        // UI はダブルクリックで開く
        if (ui && hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) OpenDocument(Assets::GetPath(path));
        // シェーダーはダブルクリックで編集する（キャンバス・階層のウィジェットへドラッグすると掛かる）
        if (shader && hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) OpenShaderEditor(path);
        ImGui::EndGroup();
        ImGui::PopID();
        col = (col + 1) % columns;
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// スタイル
// ---------------------------------------------------------------------------

void App::DrawStylesPanel(Document& doc) {
    WidgetAsset& asset = *doc.asset;
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("スタイル: 同じスタイルのウィジェットの見た目（色・角丸・文字の大きさなど）をまとめて変えられます。"
                        "詳細パネルの " ICON_FA_PLUS " で今の見た目から作り、" ICON_FA_ARROWS_ROTATE " で反映します。");
    ImGui::PopTextWrapPos();
    ImGui::Separator();
    if (asset.styles.empty()) {
        ImGui::TextDisabled("スタイルはまだありません");
        return;
    }
    // 使っているウィジェットの数
    std::map<std::string, int> uses;
    asset.root->ForEach([&](Widget& w) {
        if (!w.generated && !w.style.empty()) ++uses[w.style];
    });
    int removeIndex = -1;
    for (int i = 0; i < static_cast<int>(asset.styles.size()); ++i) {
        WidgetStyle& st = asset.styles[static_cast<size_t>(i)];
        ImGui::PushID(i);
        const bool renaming = m_RenameStyleFrom == st.name && !m_RenameStyleBuffer.empty();
        if (renaming) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (!ImGui::IsAnyItemActive()) ImGui::SetKeyboardFocusHere();
            if (ImGui::InputText("##rename-style", &m_RenameStyleBuffer, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll) || ImGui::IsItemDeactivated()) {
                const std::string to = m_RenameStyleBuffer;
                if (!to.empty() && to != st.name && !asset.FindStyle(to)) {
                    asset.root->ForEach([&](Widget& w) {
                        if (w.style == st.name) w.style = to;
                    });
                    st.name = to;
                    doc.Changed();
                }
                m_RenameStyleFrom.clear();
                m_RenameStyleBuffer.clear();
            }
        } else {
            const bool open = ImGui::TreeNodeEx("##style", ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding, "%s  %s  ", ICON_FA_BRUSH, st.name.c_str());
            ImGui::SameLine();
            ImGui::TextColored(WidgetIconColor(st.type), "%s %s", WidgetIcon(st.type), TypeLabel(st.type));
            ImGui::SameLine();
            ImGui::TextDisabled("· %d 個", uses[st.name]);
            if (ImGui::BeginPopupContextItem("style-menu")) {
                if (ImGui::MenuItem(ICON_FA_ARROW_POINTER "  使っているウィジェットを選ぶ", nullptr, false, uses[st.name] > 0)) {
                    // 今選んでいるものの次に使っているものへ（何度か選ぶと順に回る）
                    std::vector<Widget*> users;
                    asset.root->ForEach([&](Widget& w) {
                        if (!w.generated && w.style == st.name) users.push_back(&w);
                    });
                    Widget* current = doc.ui.hasSelection ? Resolve(asset.root.get(), doc.ui.selection) : nullptr;
                    auto it = std::find(users.begin(), users.end(), current);
                    Widget* next = it == users.end() || it + 1 == users.end() ? users.front() : *(it + 1);
                    doc.ui.selection = PathOf(next);
                    doc.ui.hasSelection = true;
                }
                if (ImGui::MenuItem(ICON_FA_PEN "  名前を変更")) {
                    m_RenameStyleFrom = st.name;
                    m_RenameStyleBuffer = st.name;
                }
                if (ImGui::MenuItem(ICON_FA_ARROWS_ROTATE "  すべてのウィジェットに当て直す")) {
                    asset.root->ForEach([&](Widget& w) {
                        if (w.style == st.name) asset.ApplyStyle(w);
                    });
                    doc.Changed();
                }
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_TRASH "  削除（ウィジェットの見た目はそのまま）")) removeIndex = i;
                ImGui::EndPopup();
            }
            if (open) {
                // スタイルの値（中身を少しだけ見せる。編集はウィジェットで行う）
                for (const auto& [k, v] : st.properties.Items()) {
                    std::string value = v.IsString() ? v.AsString() : v.Dump(0);
                    if (value.size() > 40) value = value.substr(0, 40) + "...";
                    ImGui::TextDisabled("  %s: %s", PropertyLabelJa(k).c_str(), value.c_str());
                }
                ImGui::TreePop();
            }
        }
        ImGui::PopID();
    }
    if (removeIndex >= 0) {
        const std::string name = asset.styles[static_cast<size_t>(removeIndex)].name;
        asset.root->ForEach([&](Widget& w) {
            if (w.style == name) w.style.clear();
        });
        asset.styles.erase(asset.styles.begin() + removeIndex);
        doc.Changed();
    }
}

// ---------------------------------------------------------------------------
// 変数（プレビュー用の値）
// ---------------------------------------------------------------------------

void App::DrawVariablesPanel(Document& doc) {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("テキストの {変数} や「変数と連動」に入る値を試せます（ゲームではスクリプトの SetVariable で入れます）。"
                        "値は編集用ファイルに保存され、ゲームには書き出されません。");
    ImGui::PopTextWrapPos();
    ImGui::Separator();
    std::vector<std::string> vars = doc.CollectVariables();
    for (const auto& [k, v] : doc.previewVariables) {
        if (std::find(vars.begin(), vars.end(), k) == vars.end()) vars.push_back(k);
    }
    if (vars.empty()) ImGui::TextDisabled("この UI はまだ変数を使っていません");
    bool changed = false;
    std::string removeVar;
    if (!vars.empty() && BeginPropertyTable("vars")) {
        const std::vector<std::string> used = doc.CollectVariables();
        for (const std::string& name : vars) {
            ImGui::PushID(name.c_str());
            const bool isUsed = std::find(used.begin(), used.end(), name) != used.end();
            PropertyLabel(name, isUsed ? "この UI で使っている変数" : "使われていない変数（右クリックで削除）");
            std::string value = doc.previewVariables.count(name) ? doc.previewVariables[name] : std::string();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputTextWithHint("##v", "値（数値か文字）", &value)) {
                doc.previewVariables[name] = value;
                changed = true;
            }
            if (ImGui::BeginPopupContextItem("var-menu")) {
                if (ImGui::MenuItem(ICON_FA_ERASER "  値を消す")) removeVar = name;
                ImGui::EndPopup();
            }
            // 0〜1 の値（ゲージ・スライダー）を試しやすいように
            {
                char* end = nullptr;
                const double num = std::strtod(value.c_str(), &end);
                if (!value.empty() && end && *end == '\0' && num >= 0.0 && num <= 1.0) {
                    float fv = static_cast<float>(num);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderFloat("##slider", &fv, 0.0f, 1.0f, "%.2f")) {
                        doc.previewVariables[name] = std::format("{:.3f}", fv);
                        changed = true;
                    }
                }
            }
            ImGui::PopID();
        }
        EndPropertyTable();
    }
    if (!removeVar.empty()) {
        doc.previewVariables.erase(removeVar);
        if (doc.view) doc.view->SetAsset(doc.asset); // 古い値を残さないように作り直す
        changed = true;
    }
    ImGui::Spacing();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90.0f * m_Scale);
    const bool enter = ImGui::InputTextWithHint("##new-var", "変数を追加（名前）", &m_NewVariable, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if ((ImGui::Button(ICON_FA_PLUS " 追加", {-FLT_MIN, 0.0f}) || enter) && !m_NewVariable.empty()) {
        doc.previewVariables[m_NewVariable] = "0";
        m_NewVariable.clear();
        changed = true;
    }
    // 変数の値は元に戻すの対象にしない（すぐ保存だけする）
    if (changed) doc.Save();
}

} // namespace AquaTool
