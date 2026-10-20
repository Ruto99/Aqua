// デザイナー: パレット・階層・キャンバス・詳細（スロット・プロパティ・スタイル）
//
//   キャンバス: 選択（Shift + クリックで一緒に選ぶ）・移動（一緒に選んだものもまとめて）・8 方向のリサイズ・アンカー表示・
//               吸い付き（親と兄弟の端・中央）・解像度の切り替え・ホイールで拡大・中ボタンで移動・
//               プレビュー（実際に操作して確かめる）
//   アセットのドラッグ: 画像 → Image を作る / 画像・ボタン・枠に入れる、フォント → テキストに、音 → ボタンのクリック音、
//                     UI（.aqua）→ 部品（ユーザーウィジェット）として入れる、シェーダー（.uishader）→ そのウィジェットに掛ける

#include "AquaApp.h"

#include <Ruto/Aqua/AquaFormat.h>
#include <Ruto/Core/Utf8.h>
#include <Ruto/Engine/ShionText.h>

#include <Ruto/EditorUI/EditorUI.h>
#include <Ruto/EditorUI/Widgets.h>

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <functional>
#include <set>

namespace AquaTool {

using namespace Ruto::EditorUI;

namespace {

std::string LowerExtOf(std::string_view path) {
    const size_t dot = path.find_last_of('.');
    if (dot == std::string_view::npos) return {};
    std::string e(path.substr(dot));
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return e;
}

bool IsImageExt(const std::string& e) { return e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".bmp" || e == ".tga"; }
bool IsFontExt(const std::string& e) { return e == ".ttf" || e == ".otf" || e == ".ttc" || e == ".bdf" || e == ".fnt" || e == ".rfont"; }
bool IsAudioExt(const std::string& e) { return e == ".wav" || e == ".ogg"; }
bool IsUIExt(const std::string& e) { return e == ".aqua" || e == ".rui"; }

// layout（.aqua）が、中の部品をたどって target を使っているか（部品の入れ子が輪にならないように）
bool LayoutUses(const std::string& layout, const std::string& target, int depth = 0) {
    if (layout == target) return true;
    if (depth > 16) return true; // 深すぎるのは輪とみなす
    const auto bin = Ruto::Aqua::ReadFile(Assets::GetPath(layout));
    if (!bin) return false;
    for (const std::string& d : bin->dependencies) {
        if (IsUIExt(LowerExtOf(d)) && LayoutUses(d, target, depth + 1)) return true;
    }
    return false;
}
bool IsShaderExt(const std::string& e) { return e == ".uishader"; }

// テキストの入力欄で選んでいた範囲（タグを入れるボタンが使う。バイトの位置）
//   ボタンを押した瞬間は、まだ使っている入力欄がそのクリックでカーソルを動かしてしまうので、
//   マウスを押していないフレームの範囲を覚えておく
struct TextSelection {
    ImGuiID field = 0;
    int start = 0, end = 0;
};
TextSelection s_TextSelection;

void RememberTextSelection(ImGuiID field) {
    if (!ImGui::IsItemActive() || ImGui::IsMouseDown(ImGuiMouseButton_Left)) return;
    if (ImGuiInputTextState* st = ImGui::GetInputTextState(field)) {
        const int a = st->GetSelectionStart(), b = st->GetSelectionEnd();
        s_TextSelection = {field, std::min(a, b), std::max(a, b)};
        if (a == b) s_TextSelection.start = s_TextSelection.end = st->GetCursorPos();
    }
}

// 選んだ所をタグで囲む（何も選んでいなければカーソルの所、入力欄を使っていなければ全体を囲む）
void InsertTag(std::string& text, ImGuiID field, const std::string& open, const std::string& close) {
    const int size = static_cast<int>(text.size());
    int a = 0, b = size;
    if (s_TextSelection.field == field) {
        a = std::clamp(s_TextSelection.start, 0, size);
        b = std::clamp(s_TextSelection.end, 0, size);
    }
    if (close.empty()) a = b; // <pause> などは囲まずに入れる
    if (!close.empty()) text.insert(static_cast<size_t>(b), "</" + close + ">");
    text.insert(static_cast<size_t>(a), "<" + open + ">");
    // 入れたタグの後ろを次の位置にする（続けて押しても入れ子にならないように）
    const int added = static_cast<int>(open.size()) + 2 + (close.empty() ? 0 : static_cast<int>(close.size()) + 3);
    s_TextSelection.start = s_TextSelection.end = b + added;
}

// 詳細パネルの入力欄
class DesignerVisitor final : public PropertyVisitor {
public:
    explicit DesignerVisitor(Project& project) : m_Project(project) {}
    bool changed = false;
    Widget* target = nullptr;                                // 今編集しているウィジェット
    std::function<void(const std::string&)> onEditShader;    // シェーダーの「編集」ボタン
    std::function<void(int)> onEditTextAnim;                 // 文字アニメーションの「編集」ボタン（0: 出し方 / 1: 動き）

    void Property(std::string_view n, bool& v) override { if (Row(n)) changed |= ImGui::Checkbox(Id(n), &v); }
    void Property(std::string_view n, int& v) override { if (Row(n)) changed |= ImGui::DragInt(Id(n), &v, 0.2f); }
    void Property(std::string_view n, float& v) override { if (Row(n)) changed |= ImGui::DragFloat(Id(n), &v, 0.5f, 0.0f, 0.0f, "%.2f"); }
    void Property(std::string_view n, Vec2& v) override { if (Row(n)) changed |= DragVec(Id(n), &v.x, 2, 0.5f, 0.0f, "%.1f"); }
    void Property(std::string_view n, Vec3& v) override { if (Row(n)) changed |= DragVec(Id(n), &v.x, 3, 0.5f, 0.0f, "%.1f"); }
    void Property(std::string_view n, Vec4& v) override {
        if (const int slot = ShaderParamSlot(n); slot >= 0) {
            ShaderParamRow(slot, v);
            return;
        }
        if (Row(n)) changed |= DragVec(Id(n), &v.x, 4, 0.5f, 0.0f, "%.0f");
    }
    void Property(std::string_view n, std::string& v) override {
        if (n == "overrides") return; // パーツの上書き（専用の欄で編集する）
        if (n == "customReveal" || n == "customEffect") {
            // 自作の文字アニメーション（JSON）は「文字アニメーション」のパネルで編集する
            if (!Row(n)) return;
            const TextCharAnimation a = TextCharAnimation::Parse(v);
            int keys = 0;
            for (const TimelineCurve& c : a.curves) keys += static_cast<int>(c.keys.size());
            const std::string label = keys > 0 ? std::format(ICON_FA_PEN_TO_SQUARE "  編集（キー {} 個・{:.2f} 秒）##{}", keys, a.GetDuration(), n)
                                               : std::format(ICON_FA_PLUS "  作る##{}", n);
            if (ImGui::Button(label.c_str(), {-FLT_MIN, 0.0f}) && onEditTextAnim) onEditTextAnim(n == "customReveal" ? 0 : 1);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("アニメーションのパネルの「文字アニメーション」で、1 文字の動きをキーフレームで作る\n%s", n == "customReveal" ? "出し方を「自作」にすると使われる" : "動きを「自作」にするか、テキストの中の <custom>…</custom> で使われる");
            return;
        }
        if (n == "messageLabel") {
            MessageLabelRow(n, v);
            return;
        }
        if (!Row(n)) return;
        const bool richText = n == "text" && target && std::string_view(target->GetTypeName()) == "UIText";
        if (n == "text") changed |= ImGui::InputTextMultiline(Id(n), &v, {-FLT_MIN, ImGui::GetTextLineHeight() * 2.6f});
        else if (n == "userData") changed |= ImGui::InputTextMultiline(Id(n), &v, {-FLT_MIN, ImGui::GetTextLineHeight() * 3.6f});
        else if (n == "frames") {
            changed |= ImGui::InputTextMultiline(Id(n), &v, {-FLT_MIN, ImGui::GetTextLineHeight() * 4.6f});
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("コマの画像（1 行に 1 つ）。「今のコマ」にコンスタントのキーを打つとパラパラ漫画に");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##add-frame", ICON_FA_PLUS " 画像をコマに足す", ImGuiComboFlags_HeightLarge)) {
                for (const std::string& item : m_Project.GetAssets(AssetType::Texture)) {
                    if (ImGui::Selectable(item.c_str())) {
                        if (!v.empty() && v.back() != '\n') v += '\n';
                        v += item;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
        }
        else changed |= ImGui::InputText(Id(n), &v);
        if (n == "userData" && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("1 行に 1 つ「キー=値」。スクリプトから読める:\n  w->GetUserData(\"キー\") / w->GetUserDataFloat(\"キー\")");
        const ImGuiID field = ImGui::GetItemID();
        if (richText) RememberTextSelection(field);
        if (n == "text" && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("{変数名} で変数の値を表示（{score:1} なら小数 1 桁）%s", richText ? "\n<wave>…</wave> などのタグで文字ごとに動かせる（下のボタンで選んだ所を囲む）\nShion のラベルを設定すると、その文字が代わりに表示される" : "");
        if (richText) {
            // タグを入れる（選んだ所を囲む / カーソルの所に入れる）
            struct TagButton {
                const char* label;
                const char* open;
                const char* close;
                const char* tip;
            };
            static const TagButton kTags[] = {
                {"波", "wave", "wave", "<wave> 波のように上下する"}, {"震え", "shake", "shake", "<shake> ぶるぶる震える"},
                {"虹", "rainbow", "rainbow", "<rainbow> 虹色に変わる"}, {"脈", "pulse", "pulse", "<pulse> 大きくなったり小さくなったり"},
                {"揺れ", "swing", "swing", "<swing> 左右に傾く"}, {"乱れ", "glitch", "glitch", "<glitch> ときどきずれて色が乱れる"},
                {"色", "color=#ffcc33", "color", "<color=#RRGGBB> 色を変える（#の後を書き換える）"}, {"薄く", "alpha=0.5", "alpha", "<alpha=0.5> 薄くする"},
                {"止める", "pause=0.5", "", "<pause=0.5> 1 文字ずつ出すとき、ここで 0.5 秒止まる"},
                {"速さ", "speed=0.5", "speed", "<speed=0.5> 1 文字ずつ出す速さを変える（0.5 でゆっくり）"},
                {"自作", "custom", "custom", "<custom> 文字アニメーションのパネルで作った「動き」"},
            };
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4.0f, 1.0f});
            const float rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
            for (size_t i = 0; i < std::size(kTags); ++i) {
                // 入るなら同じ行に並べる
                const float next = ImGui::CalcTextSize(kTags[i].label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
                if (i > 0 && ImGui::GetItemRectMax().x + 3.0f + next <= rowRight) ImGui::SameLine(0.0f, 3.0f);
                if (ImGui::SmallButton(kTags[i].label)) {
                    InsertTag(v, field, kTags[i].open, kTags[i].close);
                    changed = true;
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", kTags[i].tip);
            }
            ImGui::PopStyleVar();
        }
        if ((n == "bind" || n == "visibleBind") && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("スクリプトの SetVariable(\"名前\", 値) と連動します%s", n == "visibleBind" ? "（!名前 で逆）" : "");
    }
    void Property(std::string_view, EntityRef&) override {}
    void Range(std::string_view n, float& v, float mn, float mx) override { if (Row(n)) changed |= ImGui::SliderFloat(Id(n), &v, mn, mx, "%.2f"); }
    void Color(std::string_view n, Vec3& v) override { if (Row(n)) changed |= ColorField(Id(n), &v.x, false); }
    void Color(std::string_view n, Vec4& v) override { if (Row(n)) changed |= ColorField(Id(n), &v.x, true); }
    void Enum(std::string_view n, int& v, std::span<const char* const> options) override {
        if (!Row(n)) return;
        const std::string preview = v >= 0 && v < static_cast<int>(options.size()) ? EnumOptionLabelJa(n, options[static_cast<size_t>(v)]) : "?";
        if (ImGui::BeginCombo(Id(n), preview.c_str())) {
            for (size_t i = 0; i < options.size(); ++i) {
                if (ImGui::Selectable(EnumOptionLabelJa(n, options[i]).c_str(), static_cast<int>(i) == v)) {
                    v = static_cast<int>(i);
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
    }
    void Asset(std::string_view n, std::string& path, AssetType type) override {
        if (!Row(n)) return;
        const auto& list = m_Project.GetAssets(type);
        if (type == AssetType::Texture && !path.empty()) {
            Thumbnail(Assets::GetTexture(path), {ImGui::GetFrameHeight(), ImGui::GetFrameHeight()});
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::SetNextItemWidth(-FLT_MIN);
        }
        const char* empty = type == AssetType::Font       ? "(システムのフォント)"
                            : type == AssetType::UIShader ? "(なし: ふつうに描く)"
                            : type == AssetType::Message  ? "(使わない: テキストを表示)"
                                                          : "(なし)";
        const std::string before = path;
        if (type == AssetType::UIShader) ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 4.0f);
        if (ImGui::BeginCombo(Id(n), path.empty() ? empty : path.c_str(), ImGuiComboFlags_HeightLarge)) {
            if (ImGui::Selectable(empty, path.empty())) {
                path.clear();
                changed = true;
            }
            for (const std::string& item : list) {
                if (ImGui::Selectable(item.c_str(), item == path)) {
                    path = item;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            if (type == AssetType::Message)
                ImGui::SetTooltip("Shion がビルドで書き出した .sobt（Output/<言語名>/ のフォルダを Assets の中に置く）\nアセットのパネルからドラッグしても入れられます");
            else
                ImGui::SetTooltip("アセットのパネルからドラッグしても入れられます");
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kAssetPayload)) {
                const std::string dropped(static_cast<const char*>(p->Data), static_cast<size_t>(p->DataSize));
                if (std::find(list.begin(), list.end(), dropped) != list.end()) {
                    path = dropped;
                    changed = true;
                }
            }
            ImGui::EndDragDropTarget();
        }
        if (type == AssetType::UIShader) {
            // シェーダーを変えたら、値をそのシェーダーの既定値にする
            if (path != before && target) {
                target->shader = path;
                target->ResetShaderParams();
            }
            ImGui::SameLine(0.0f, 4.0f);
            if (IconButton(path.empty() ? ICON_FA_PLUS : ICON_FA_PEN_TO_SQUARE, path.empty() ? "新しいシェーダーを作る（シェーダーのパネル）" : "このシェーダーを編集する") && onEditShader) onEditShader(path);
        }
    }
    void Header(std::string_view title) override {
        if (m_Table) EndPropertyTable();
        m_Table = false;
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::kAccent);
        ImGui::SeparatorText(std::string(title).c_str());
        ImGui::PopStyleColor();
    }
    void Begin(const char* id) {
        m_Id = id;
        m_Index = 0;
    }
    void End() {
        if (m_Table) EndPropertyTable();
        m_Table = false;
    }

private:
    bool Row(std::string_view n) { return RowLabel(PropertyLabelJa(n), PropertyHelp(n)); }
    // 詳しい説明（ラベルにかざすと出る）
    static const char* PropertyHelp(std::string_view n) {
        static const std::pair<const char*, const char*> kHelp[] = {
            {"effect", "文字がずっと動き続ける（一部だけ動かすときはテキストの中で <wave>…</wave> などのタグを使う）"},
            {"effectStrength", "動きの大きさ（基準解像度の単位）"},
            {"reveal", "文字の出し方。編集中は全部出し、プレビュー（P）とゲームでは実際に出す"},
            {"revealSpeed", "1 秒に出す文字の数"},
            {"revealDelay", "表示してから出し始めるまでの秒"},
            {"revealFade", "1 文字が出きるまでの秒（ふわっと・ポップなど）"},
            {"revealAuto", "オフにすると「出した割合」の分だけ出す（アニメーションのタイムラインで動かせる）"},
            {"revealProgress", "自動で出さないとき、何割の文字を出すか（0〜1）"},
            {"revealOnShow", "表示し直すたび・文字が変わるたびに最初から出す"},
            {"revealSound", "文字を出すときに鳴らす音（会話の「ピピピ」）"},
            {"revealSoundEvery", "何文字ごとに音を鳴らすか"},
            {"richText", "<wave> <shake> <rainbow> <pulse> <swing> <glitch> <custom> <color=#RRGGBB> <alpha=0.5> <pause=0.5> <speed=2> を使う"},
            {"customReveal", "出し方が「自作」のときの動き（文字アニメーションのパネルで作る）"},
            {"customEffect", "動きが「自作」のとき・<custom> で囲んだ所の動き（文字アニメーションのパネルで作る）"},
            {"messageFile", "Shion のテキストのファイル（.sobt）。Shion の Output/<言語名>/ のフォルダを Assets の中に置いて選ぶ"},
            {"messageLabel", "Shion のラベル。設定するとそのテキストを表示する（見つからなければ「テキスト」を表示）\nゲームでは ShionText::SetLanguage で言語を切り替え、ui->SetMessage(\"名前\", \"ラベル\") で変えられる"},
            {"shader", "UI のシェーダー（.uishader）。シェーダーのパネルで作る・編集する"},
            {"shaderChildren", "子のウィジェットにも同じシェーダーを掛ける"},
        };
        for (const auto& [k, v] : kHelp) {
            if (n == k) return v;
        }
        return nullptr;
    }
    bool RowLabel(const std::string& label, const char* tooltip = nullptr) {
        if (!m_Table) {
            m_Table = BeginPropertyTable(std::format("{}#{}", m_Id, m_Index++).c_str());
            if (!m_Table) return false;
        }
        PropertyLabel(label, tooltip);
        return true;
    }
    // シェーダーの @param（使っていない番号は出さない）
    void ShaderParamRow(int slot, Vec4& v) {
        auto sh = target && !target->shader.empty() ? UIShader::Get(target->shader) : nullptr;
        const UIShaderParam* p = nullptr;
        if (sh) {
            for (const UIShaderParam& q : sh->GetParams()) {
                if (q.slot == slot) p = &q;
            }
        }
        if (!p) return;
        const std::string tip = std::format("シェーダーの {}（アニメーションで動かせます。右クリックで既定値）", p->name);
        if (!RowLabel(p->label, tip.c_str())) return;
        const std::string id = std::format("##sp{}", slot);
        switch (p->type) {
        case UIShaderParam::Type::Float: changed |= ImGui::SliderFloat(id.c_str(), &v.x, p->min, p->max, "%.3f"); break;
        case UIShaderParam::Type::Vec2: changed |= ImGui::DragFloat2(id.c_str(), &v.x, std::max((p->max - p->min) / 200.0f, 0.001f), p->min, p->max, "%.3f"); break;
        case UIShaderParam::Type::Color: changed |= ColorField(id.c_str(), &v.x, true); break;
        case UIShaderParam::Type::Toggle: {
            bool on = v.x > 0.5f;
            if (ImGui::Checkbox(id.c_str(), &on)) {
                v.x = on ? 1.0f : 0.0f;
                changed = true;
            }
            break;
        }
        }
        if (ImGui::BeginPopupContextItem((id + "menu").c_str())) {
            if (ImGui::MenuItem(ICON_FA_ROTATE_LEFT "  既定値に戻す")) {
                v = p->defaultValue;
                changed = true;
            }
            ImGui::EndPopup();
        }
    }
    const char* Id(std::string_view n) {
        m_IdBuffer = "##" + std::string(n);
        return m_IdBuffer.c_str();
    }

    // Shion のラベル: ファイルのラベルから選ぶ（かざすと本文）・表示される文字・プレビューの言語
    void MessageLabelRow(std::string_view n, std::string& v) {
        const auto* text = dynamic_cast<const UIText*>(target);
        const std::string file = text ? text->messageFile : std::string();
        if (!Row(n)) return;
        const std::vector<std::string> labels = file.empty() ? std::vector<std::string>() : ShionText::GetLabels(file);
        const char* none = "(使わない: テキストを表示)";
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo(Id(n), v.empty() ? none : v.c_str(), ImGuiComboFlags_HeightLarge)) {
            if (ImGui::IsWindowAppearing()) {
                m_LabelFilter.clear();
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##label-filter", ICON_FA_MAGNIFYING_GLASS " ラベル・本文で絞り込む", &m_LabelFilter);
            if (ImGui::Selectable(none, v.empty())) {
                v.clear();
                changed = true;
            }
            if (file.empty()) ImGui::TextDisabled("先に「Shion のファイル」を選んでください");
            else if (labels.empty()) ImGui::TextDisabled("ラベルがありません %s", ShionText::GetError(file).c_str());
            for (const std::string& label : labels) {
                const std::string body = ShionText::GetText(file, label).value_or("");
                if (!m_LabelFilter.empty() && !ContainsText(label, m_LabelFilter) && !ContainsText(body, m_LabelFilter)) continue;
                const std::string item = std::format("{}   {}##{}", label, OneLine(body, 28), label);
                if (ImGui::Selectable(item.c_str(), label == v)) {
                    v = label;
                    changed = true;
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", body.c_str());
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("右クリックでラベルを直接書けます（まだファイルに無いラベルも）");
        // 直接書く（見つからない間は「テキスト」を表示）
        if (ImGui::BeginPopupContextItem("##label-menu")) {
            ImGui::TextDisabled("ラベルを書く");
            ImGui::SetNextItemWidth(240.0f);
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            changed |= ImGui::InputText("##label-edit", &v);
            ImGui::EndPopup();
        }
        if (!v.empty()) {
            ImGui::PushTextWrapPos(0.0f);
            if (file.empty()) {
                ImGui::TextColored(Theme::kWarning, ICON_FA_TRIANGLE_EXCLAMATION " Shion のファイルを選んでください");
            } else if (auto body = ShionText::GetText(file, v)) {
                ImGui::TextDisabled("%s", OneLine(*body, 60).c_str());
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", body->c_str());
            } else {
                const std::string error = ShionText::GetError(file);
                ImGui::TextColored(Theme::kWarning, ICON_FA_TRIANGLE_EXCLAMATION " %s",
                                   error.empty() ? "このラベルはファイルにありません（「テキスト」を表示）" : error.c_str());
            }
            ImGui::PopTextWrapPos();
        }

        // プレビューの言語（Output/<言語名>/ が 2 つ以上あるとき。ゲームでは ShionText::SetLanguage）
        const std::vector<std::string> languages = file.empty() ? std::vector<std::string>() : ShionText::GetLanguages(file);
        if (languages.size() < 2) return;
        if (!RowLabel("プレビューの言語", "Aqua で表示する言語（保存されない。すべてのテキストで同じ）\nゲームでは ShionText::SetLanguage(\"言語名\") で切り替える")) return;
        const std::string current = ShionText::GetLanguage();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##shion-language", current.empty() ? "(ファイルのまま)" : current.c_str())) {
            if (ImGui::Selectable("(ファイルのまま)", current.empty())) ShionText::SetLanguage("");
            for (const std::string& lang : languages) {
                if (ImGui::Selectable(lang.c_str(), lang == current)) ShionText::SetLanguage(lang);
            }
            ImGui::EndCombo();
        }
    }
    // 改行を ⏎ にして、長ければ切る（一覧に 1 行で出す）
    static std::string OneLine(const std::string& s, size_t maxChars) {
        std::string out;
        size_t chars = 0;
        for (size_t i = 0; i < s.size();) {
            if (chars >= maxChars) {
                out += "…";
                break;
            }
            const size_t from = i;
            if (Ruto::Utf8::Next(s, i) == '\n') out += " ⏎ ";
            else out.append(s, from, i - from);
            ++chars;
        }
        return out;
    }
    static bool ContainsText(const std::string& text, const std::string& query) {
        auto lower = [](std::string x) {
            std::transform(x.begin(), x.end(), x.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return x;
        };
        return lower(text).find(lower(query)) != std::string::npos;
    }

    Project& m_Project;
    std::string m_Id;
    std::string m_IdBuffer;
    int m_Index = 0;
    bool m_Table = false;
    std::string m_LabelFilter; // ラベルの一覧の絞り込み
};

} // namespace

// ---------------------------------------------------------------------------
// 編集操作
// ---------------------------------------------------------------------------

Widget* App::AddWidget(Document& doc, const std::string& type, Widget* parent, int index, const Vec2* canvasPos) {
    WidgetAsset& asset = *doc.asset;
    if (!parent) parent = asset.root.get();
    if (!parent) return nullptr;
    if (!parent->CanAddChild()) {
        // 子を持てないなら、その後ろに兄弟として入れる
        if (!parent->GetParent()) return nullptr;
        index = parent->GetParent()->IndexOf(parent) + 1;
        parent = parent->GetParent();
        if (!parent->CanAddChild()) return nullptr;
    }
    auto w = WidgetRegistry::Create(type);
    if (!w) return nullptr;
    std::string base = type.starts_with("UI") ? type.substr(2) : type;
    w->name = UniqueWidgetName(asset.root.get(), base);
    if (dynamic_cast<UICanvasPanel*>(parent)) {
        w->slot.size = DefaultCanvasSize(type);
        w->slot.pivot = {0.5f, 0.5f};
        if (type == "UIText" || type == "UIToggle") w->slot.autoSize = true;
        // 置く場所（ドロップした所、なければ親の中央）
        Vec2 pos{asset.referenceResolution.x * 0.5f, asset.referenceResolution.y * 0.5f};
        if (Widget* pd = doc.view ? Resolve(doc.view->GetRoot(), PathOf(parent)) : nullptr) pos = {pd->GetRect().w * 0.5f, pd->GetRect().h * 0.5f};
        if (canvasPos) pos = *canvasPos;
        w->slot.position = {std::round(pos.x), std::round(pos.y)};
        if (type == "UICapture") {
            // キャプチャー（ポーズなどの背景）: 親いっぱいに広げて、一番奥に置く
            w->slot.anchorMin = {0.0f, 0.0f};
            w->slot.anchorMax = {1.0f, 1.0f};
            w->slot.size = {0.0f, 0.0f};
            w->slot.position = {0.0f, 0.0f};
            if (index < 0) index = 0;
        }
    } else if (type == "UIText") {
        w->slot.hAlign = HAlign::Fill;
    }
    if (auto* b = dynamic_cast<UIBorder*>(w.get()); b && dynamic_cast<UICanvasPanel*>(parent)) b->contentPadding = {24, 24, 24, 24};
    Widget* raw = parent->AddChild(std::move(w), index);
    doc.ui.selection = PathOf(raw);
    doc.ui.hasSelection = true;
    doc.ui.extraSelection.clear();
    doc.Changed();
    return raw;
}

void App::DeleteWidget(Document& doc, const WidgetPath& path) {
    WidgetAsset& asset = *doc.asset;
    Widget* w = Resolve(asset.root.get(), path);
    if (!w || !w->GetParent()) return; // ルートは消さない
    // このウィジェットのアニメーションのトラックも消す
    std::set<std::string> names;
    w->ForEach([&](Widget& c) { names.insert(c.name); });
    for (WidgetAnimation& a : asset.animations) std::erase_if(a.tracks, [&](const WidgetAnimTrack& t) { return names.contains(t.widget); });
    Widget* parent = w->GetParent();
    parent->RemoveChild(w);
    doc.ui.selection = PathOf(parent);
    doc.ui.hasSelection = true;
    doc.Changed();
}

void App::DuplicateWidget(Document& doc, const WidgetPath& path) {
    WidgetAsset& asset = *doc.asset;
    Widget* w = Resolve(asset.root.get(), path);
    if (!w || !w->GetParent() || !w->GetParent()->CanAddChild()) return;
    auto copy = w->Clone();
    copy->ForEach([&](Widget& c) { c.name = UniqueWidgetName(asset.root.get(), c.name); });
    if (IsCanvasChild(*w)) copy->slot.position = copy->slot.position + Vec2{20.0f, 20.0f};
    Widget* parent = w->GetParent();
    Widget* raw = parent->AddChild(std::move(copy), parent->IndexOf(w) + 1);
    doc.ui.selection = PathOf(raw);
    doc.ui.extraSelection.clear();
    doc.Changed();
}

void App::WrapWidget(Document& doc, const WidgetPath& path, const std::string& type) {
    WidgetAsset& asset = *doc.asset;
    Widget* w = Resolve(asset.root.get(), path);
    if (!w || !w->GetParent()) return;
    auto container = WidgetRegistry::Create(type);
    if (!container || container->GetMaxChildren() == 0) return;
    container->name = UniqueWidgetName(asset.root.get(), type.starts_with("UI") ? type.substr(2) : type);
    container->slot = w->slot; // 包んだものが今の場所を引き継ぐ
    if (auto* b = dynamic_cast<UIBorder*>(container.get())) {
        b->shadow = false;
        b->contentPadding = {12, 12, 12, 12};
    }
    Widget* parent = w->GetParent();
    const int index = parent->IndexOf(w);
    auto moved = parent->RemoveChild(w);
    moved->slot = WidgetSlot{};
    Widget* c = parent->AddChild(std::move(container), index);
    c->AddChild(std::move(moved));
    doc.ui.selection = PathOf(c);
    doc.Changed();
}

void App::UnwrapWidget(Document& doc, const WidgetPath& path) {
    Widget* w = Resolve(doc.asset->root.get(), path);
    if (!w || !w->GetParent() || w->children.empty()) return;
    Widget* parent = w->GetParent();
    int index = parent->IndexOf(w);
    const WidgetSlot slot = w->slot;
    auto container = parent->RemoveChild(w);
    bool first = true;
    while (!container->children.empty()) {
        auto child = container->RemoveChild(container->children.front().get());
        if (first && dynamic_cast<UICanvasPanel*>(parent)) child->slot = slot;
        first = false;
        parent->AddChild(std::move(child), index++);
    }
    doc.ui.selection = PathOf(parent);
    doc.Changed();
}

void App::CopyWidget(Document& doc, const WidgetPath& path) {
    if (Widget* w = Resolve(doc.asset->root.get(), path)) {
        doc.clipboard = WidgetAsset::SerializeWidget(*w).Dump(0);
        Platform::SetClipboardText(doc.clipboard); // 別の文書・別の Aqua にも貼れるように
    }
}

void App::PasteWidget(Document& doc) {
    WidgetAsset& asset = *doc.asset;
    std::string text = Platform::GetClipboardText();
    if (text.empty() || text.front() != '{') text = doc.clipboard;
    auto json = Json::Parse(text);
    if (!json || !(*json)["type"].IsString()) return;
    auto w = WidgetAsset::DeserializeWidget(*json);
    if (!w) return;
    Widget* target = doc.ui.hasSelection ? Resolve(asset.root.get(), doc.ui.selection) : asset.root.get();
    if (!target) target = asset.root.get();
    int index = -1;
    if (!target->CanAddChild() && target->GetParent()) {
        index = target->GetParent()->IndexOf(target) + 1;
        target = target->GetParent();
    }
    if (!target->CanAddChild()) return;
    w->ForEach([&](Widget& c) { c.name = UniqueWidgetName(asset.root.get(), c.name); });
    Widget* raw = target->AddChild(std::move(w), index);
    doc.ui.selection = PathOf(raw);
    doc.ui.hasSelection = true;
    doc.ui.extraSelection.clear();
    doc.Changed();
}

void App::RenameWidget(Document& doc, Widget& w, std::string name) {
    WidgetAsset& asset = *doc.asset;
    if (name.empty() || name == w.name) return;
    if (asset.root->Find(name)) name = UniqueWidgetName(asset.root.get(), name);
    for (WidgetGroup& g : asset.groups) {
        for (std::string& m : g.members) {
            if (m == w.name) m = name;
        }
    }
    for (std::set<std::string>* names : {&doc.lockedWidgets, &doc.editorHidden}) {
        if (names->erase(w.name)) names->insert(name);
    }
    for (WidgetAnimation& a : asset.animations) {
        for (WidgetAnimTrack& t : a.tracks) {
            if (t.widget == w.name) t.widget = name;
        }
    }
    w.name = name;
    doc.Changed();
}

bool App::DropAsset(Document& doc, const std::string& assetPath, Widget* target, const Vec2& localPos) {
    const std::string ext = LowerExtOf(assetPath);
    WidgetAsset& asset = *doc.asset;
    if (!target) target = asset.root.get();
    auto select = [&](Widget* w) {
        doc.ui.selection = PathOf(w);
        doc.ui.hasSelection = true;
    };
    if (IsImageExt(ext)) {
        // 画像を持てるもの（Image・Button・Border）に落としたら差し替える。それ以外なら Image を作る
        if (auto* img = dynamic_cast<UIImage*>(target)) {
            img->texture = assetPath;
        } else if (auto* b = dynamic_cast<UIButton*>(target)) {
            b->texture = assetPath;
        } else if (auto* bd = dynamic_cast<UIBorder*>(target); bd && !ImGui::GetIO().KeyShift) {
            bd->texture = assetPath;
        } else {
            Widget* parent = target;
            while (parent && !(parent->CanAddChild() && !parent->generated)) parent = parent->GetParent();
            auto* created = dynamic_cast<UIImage*>(AddWidget(doc, "UIImage", parent ? parent : asset.root.get(), -1, &localPos));
            if (!created) return false;
            created->texture = assetPath;
            if (auto t = Assets::GetTexture(assetPath)) {
                created->size = {static_cast<float>(t->GetWidth()), static_cast<float>(t->GetHeight())};
                if (IsCanvasChild(*created)) created->slot.size = created->size;
            }
            created->name = UniqueWidgetName(asset.root.get(), ToIdentifier(FileSystem::ToUtf8(FileSystem::FromUtf8(assetPath).stem())));
            doc.Changed();
            return true;
        }
        select(target);
        doc.Changed();
        return true;
    }
    if (IsFontExt(ext)) {
        if (auto* t = dynamic_cast<UIText*>(target)) {
            t->font = assetPath;
            select(target);
            doc.Changed();
            return true;
        }
        Notify("フォントはテキストに落としてください", true);
        return false;
    }
    if (IsAudioExt(ext)) {
        if (auto* b = dynamic_cast<UIButton*>(target)) {
            b->clickSound = assetPath;
            select(target);
            doc.Changed();
            return true;
        }
        Notify("音はボタンに落としてください（クリックの音になります）", true);
        return false;
    }
    if (IsShaderExt(ext)) {
        // シェーダー: 落としたウィジェットに掛ける（ルートなら UI 全体）
        target->shader = assetPath;
        target->ResetShaderParams();
        select(target);
        doc.Changed();
        Notify(std::format("{} にシェーダーを掛けました", target->name));
        return true;
    }
    if (IsUIExt(ext)) {
        if (assetPath == doc.output) {
            Notify("自分自身は部品として入れられません", true);
            return false;
        }
        if (m_Project.KindOf(assetPath, LayoutKind::Part) == LayoutKind::Body) {
            Notify("Body は部品として入れられません（一覧の右クリックで「パーツにする」と入れられます）", true);
            return false;
        }
        if (LayoutUses(assetPath, doc.output)) {
            Notify("そのパーツはこのレイアウトを中で使っているので入れられません（入れ子が輪になります）", true);
            return false;
        }
        Widget* parent = target;
        while (parent && !(parent->CanAddChild() && !parent->generated)) parent = parent->GetParent();
        auto* uw = dynamic_cast<UIUserWidget*>(AddWidget(doc, "UIUserWidget", parent ? parent : asset.root.get(), -1, &localPos));
        if (!uw) return false;
        uw->asset = assetPath;
        uw->name = UniqueWidgetName(asset.root.get(), ToIdentifier(FileSystem::ToUtf8(FileSystem::FromUtf8(assetPath).stem())));
        if (auto loaded = WidgetAsset::Load(assetPath); loaded && IsCanvasChild(*uw)) uw->slot.size = loaded->referenceResolution * 0.5f;
        doc.Changed();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// 文書のタブとキャンバスの窓
// ---------------------------------------------------------------------------

void App::DrawDocumentArea() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {6.0f, 4.0f});
    const bool visible = ImGui::Begin(ICON_FA_OBJECT_GROUP "  キャンバス###Canvas", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    if (!visible) {
        if (Document* doc = ActiveDocument()) doc->ui.canvas = {};
        ImGui::End();
        return;
    }
    if (m_Docs.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled(ICON_FA_OBJECT_GROUP "  レイアウトを開いてください");
        ImGui::TextDisabled("左の「レイアウト」から選ぶか、新しいレイアウトを作ります。");
        ImGui::Spacing();
        if (ImGui::Button(ICON_FA_DESKTOP "  新しい Body")) {
            m_NewKind = LayoutKind::Body;
            m_OpenNewPopup = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_PUZZLE_PIECE "  新しいパーツ")) {
            m_NewKind = LayoutKind::Part;
            m_OpenNewPopup = true;
        }
        const auto& sources = m_Project.GetSources();
        if (!sources.empty()) {
            ImGui::Spacing();
            ImGui::SeparatorText("レイアウト");
            for (const auto& s : sources) {
                if (ImGui::Selectable(std::format("{}  {}  →  {}{}", LayoutKindIcon(s.kind), FileSystem::ToUtf8(s.path.stem()), m_Project.IsLibrary() ? "" : "Assets/", s.output).c_str()))
                    OpenDocument(s.path);
            }
        }
        ImGui::End();
        return;
    }
    int closeIndex = -1;
    if (ImGui::BeginTabBar("##docs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_FittingPolicyScroll)) {
        for (size_t i = 0; i < m_Docs.size(); ++i) {
            Document& doc = *m_Docs[i];
            bool open = true;
            ImGuiTabItemFlags flags = doc.HasPending() ? ImGuiTabItemFlags_UnsavedDocument : 0;
            if (m_FocusTab == static_cast<int>(i)) flags |= ImGuiTabItemFlags_SetSelected;
            const std::string label = std::format("{}  {}###doc{}", LayoutKindIcon(doc.kind), doc.GetTitle(), static_cast<const void*>(&doc));
            if (ImGui::BeginTabItem(label.c_str(), &open, flags)) {
                if (m_FocusTab < 0 || m_FocusTab == static_cast<int>(i)) m_Active = static_cast<int>(i);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                    ImGui::SetTooltip("%s\n→ Assets/%s", FileSystem::ToUtf8(doc.sourcePath).c_str(), doc.output.c_str());
                if (m_Active == static_cast<int>(i)) {
                    DrawToolbar(doc);
                    ImGui::BeginChild("##canvas-area", {0.0f, 0.0f}, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                    DrawCanvas(doc);
                    ImGui::EndChild();
                }
                ImGui::EndTabItem();
            }
            if (!open) closeIndex = static_cast<int>(i);
        }
        ImGui::EndTabBar();
    }
    m_FocusTab = -1;
    Document* doc = ActiveDocument();
    if (doc) {
        doc->ui.focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        if (doc->ui.focused) HandleDesignerKeys(*doc);
        if (doc->ui.hasSelection && !Resolve(doc->asset->root.get(), doc->ui.selection)) doc->ui.hasSelection = false;
        // 一緒に選んでいるもの: 消えたもの・最初に選んだものと同じものは外す
        {
            Widget* primary = doc->ui.hasSelection ? Resolve(doc->asset->root.get(), doc->ui.selection) : nullptr;
            if (!primary) doc->ui.extraSelection.clear();
            std::erase_if(doc->ui.extraSelection, [&](const std::string& name) {
                Widget* w = doc->asset->root->Find(name);
                return !w || w == primary || !w->GetParent();
            });
        }
        // マウスを離していて入力中でもなければ確定（元に戻すの単位）・保存・書き出し
        const bool idle = !ImGui::IsAnyItemActive() && !ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if (doc->HasPending() && idle) doc->Commit();
    }
    ImGui::End();
    if (closeIndex >= 0) CloseDocument(static_cast<size_t>(closeIndex));
}

void App::HandleDesignerKeys(Document& doc, bool timeline) {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) return;
    WidgetAsset& asset = *doc.asset;
    auto pressed = [](ImGuiKey k, bool repeat = false) { return ImGui::IsKeyPressed(k, repeat); };
    if (doc.ui.hasSelection && !timeline) {
        if (pressed(ImGuiKey_Delete)) DeleteWidget(doc, doc.ui.selection);
        if (io.KeyCtrl && pressed(ImGuiKey_D)) DuplicateWidget(doc, doc.ui.selection);
        if (io.KeyCtrl && pressed(ImGuiKey_C)) CopyWidget(doc, doc.ui.selection);
        if (io.KeyCtrl && pressed(ImGuiKey_X) && !doc.ui.selection.empty()) {
            CopyWidget(doc, doc.ui.selection);
            DeleteWidget(doc, doc.ui.selection);
        }
        if (pressed(ImGuiKey_F2)) {
            doc.ui.renaming = doc.ui.selection;
            doc.ui.isRenaming = true;
            if (Widget* w = Resolve(asset.root.get(), doc.ui.selection)) doc.ui.renameBuffer = w->name;
        }
        // 矢印キーで 1 ずつ（Shift で 10）動かす
        if (Widget* w = Resolve(asset.root.get(), doc.ui.selection); w && IsCanvasChild(*w) && !io.KeyCtrl) {
            const float step = io.KeyShift ? 10.0f : 1.0f;
            Vec2 d{0.0f, 0.0f};
            if (pressed(ImGuiKey_LeftArrow, true)) d.x -= step;
            if (pressed(ImGuiKey_RightArrow, true)) d.x += step;
            if (pressed(ImGuiKey_UpArrow, true)) d.y -= step;
            if (pressed(ImGuiKey_DownArrow, true)) d.y += step;
            if (d.x != 0.0f || d.y != 0.0f) {
                w->slot.position = w->slot.position + d;
                doc.Changed();
            }
        }
    }
    if (io.KeyCtrl && pressed(ImGuiKey_V) && !timeline) PasteWidget(doc);
    if (!io.KeyCtrl && pressed(ImGuiKey_F)) doc.ui.zoom = 0.0f;
    if (!io.KeyCtrl && pressed(ImGuiKey_P)) {
        doc.ui.preview = !doc.ui.preview;
        if (doc.view) doc.view->SetAsset(doc.asset);
    }
    if (!io.KeyCtrl && pressed(ImGuiKey_G)) doc.ui.showGrid = !doc.ui.showGrid;
    if (pressed(ImGuiKey_Escape) && !timeline) {
        doc.ui.hasSelection = false;
        doc.ui.extraSelection.clear();
    }
}

void App::DrawToolbar(Document& doc) {
    WidgetAsset& asset = *doc.asset;
    const auto presets = ResolutionPresets();
    // 解像度
    ImGui::SetNextItemWidth(250.0f * m_Scale);
    const int preset = std::clamp(doc.ui.resolution, 0, static_cast<int>(presets.size()) - 1);
    const std::string current = preset == 0 ? std::format(ICON_FA_DESKTOP "  基準解像度 {:.0f} × {:.0f}", asset.referenceResolution.x, asset.referenceResolution.y)
                                            : std::string(presets[static_cast<size_t>(preset)].label);
    if (ImGui::BeginCombo("##res", current.c_str())) {
        for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
            const std::string label = i == 0 ? std::format("基準解像度（{:.0f} × {:.0f}）", asset.referenceResolution.x, asset.referenceResolution.y)
                                             : presets[static_cast<size_t>(i)].label;
            if (ImGui::Selectable(label.c_str(), i == preset)) {
                doc.ui.resolution = i;
                doc.ui.zoom = 0.0f;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("いろいろな画面の大きさで、アンカーと拡大縮小（Canvas Scaler）の結果を確かめる");
    ImGui::SameLine();
    if (IconButton(ICON_FA_GEAR, "レイアウトの設定（種類・基準解像度）")) ImGui::OpenPopup("ui-settings");
    if (ImGui::BeginPopup("ui-settings")) {
        ImGui::SeparatorText("レイアウトの設定");
        int kind = doc.IsPart() ? 1 : 0;
        ImGui::TextUnformatted("種類");
        ImGui::SameLine();
        if (ImGui::RadioButton(ICON_FA_DESKTOP " Body（画面）", &kind, 0)) doc.SetKind(LayoutKind::Body);
        ImGui::SameLine();
        if (ImGui::RadioButton(ICON_FA_PUZZLE_PIECE " パーツ（部品）", &kind, 1)) doc.SetKind(LayoutKind::Part);
        int res[2] = {static_cast<int>(asset.referenceResolution.x), static_cast<int>(asset.referenceResolution.y)};
        if (ImGui::DragInt2(doc.IsPart() ? "パーツの大きさ" : "基準解像度", res, 1.0f, 16, 8192)) {
            asset.referenceResolution = {static_cast<float>(std::max(res[0], 16)), static_cast<float>(std::max(res[1], 16))};
            doc.Changed();
        }
        ImGui::TextDisabled("%s", doc.IsPart() ? "部品として入れたときの最初の大きさ（入れた先で変えられます）" : "この大きさでデザインし、実際の画面に合わせて拡大縮小されます");
        ImGui::Spacing();
        ImGui::TextDisabled("%s", m_Project.IsLibrary() ? "書き出し先（ライブラリからの相対パス）" : "書き出し先（Assets からの相対パス）");
        std::string out = doc.output;
        ImGui::SetNextItemWidth(280.0f * m_Scale);
        if (ImGui::InputText("##output", &out, ImGuiInputTextFlags_EnterReturnsTrue) && !out.empty()) {
            if (!out.ends_with(".aqua")) out += ".aqua";
            doc.output = out;
            doc.asset->path = out;
            doc.Save();
            doc.Export();
            m_Project.Refresh(true);
            Notify("書き出し先を変えました: " + out);
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f * m_Scale);
    int zoomPct = static_cast<int>(std::lround(doc.ui.zoom * 100.0f));
    if (ImGui::DragInt("##zoom", &zoomPct, 1.0f, 5, 400, "%d%%")) doc.ui.zoom = std::clamp(static_cast<float>(zoomPct) / 100.0f, 0.05f, 4.0f);
    ImGui::SameLine();
    if (IconButton(ICON_FA_EXPAND, "全体を表示（F）")) {
        doc.ui.zoom = 0.0f;
        doc.ui.pan = {0.0f, 0.0f};
    }
    ImGui::SameLine();
    if (IconButton(ICON_FA_BORDER_ALL, "グリッド（G）", doc.ui.showGrid)) doc.ui.showGrid = !doc.ui.showGrid;
    ImGui::SameLine();
    if (IconButton(ICON_FA_MAGNET, "吸い付き（親・兄弟の端と中央。Alt で一時的に無効）", doc.ui.snap)) doc.ui.snap = !doc.ui.snap;
    ImGui::SameLine();
    ImGui::BeginDisabled(!doc.CanUndo());
    if (IconButton(ICON_FA_ROTATE_LEFT, "元に戻す（Ctrl+Z）")) doc.Undo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!doc.CanRedo());
    if (IconButton(ICON_FA_ROTATE_RIGHT, "やり直す（Ctrl+Y）")) doc.Redo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, doc.ui.preview ? Theme::kAccent : Theme::kFrame);
    if (ImGui::Button(doc.ui.preview ? ICON_FA_CIRCLE_PLAY "  プレビュー中（P で編集に戻る）" : ICON_FA_CIRCLE_PLAY "  プレビュー（P）")) {
        doc.ui.preview = !doc.ui.preview;
        doc.ui.animPlaying = false;
        if (doc.view) doc.view->SetAsset(doc.asset); // 作り直して編集前の状態に
    }
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    if (doc.ui.preview && !doc.ui.lastEvent.empty() && ImGui::GetTime() - doc.ui.lastEventTime < 4.0) {
        ImGui::TextColored(Theme::kSuccess, ICON_FA_BOLT "  %s", doc.ui.lastEvent.c_str());
    } else if (doc.ui.preview) {
        ImGui::TextDisabled("ボタン・スライダー・キーボード / ゲームパッドのフォーカス移動を試せます");
    } else {
        ImGui::TextDisabled("ドラッグで移動・四隅でリサイズ・Shift + クリックで一緒に選ぶ・ホイールで拡大・中ボタンで移動");
    }
}

// ---------------------------------------------------------------------------
// パレット・階層
// ---------------------------------------------------------------------------

void App::DrawPalette(Document* doc) {
    SearchField("##palette-search", m_PaletteSearch, "検索");
    std::string lastCategory;
    const std::string& q = m_PaletteSearch;
    for (const WidgetTypeInfo& info : WidgetRegistry::GetAll()) {
        if (!q.empty() && info.label.find(q) == std::string::npos && info.name.find(q) == std::string::npos) continue;
        if (info.category != lastCategory) {
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::kAccent);
            ImGui::SeparatorText(info.category.c_str());
            ImGui::PopStyleColor();
            lastCategory = info.category;
        }
        ImGui::PushID(info.name.c_str());
        const ImVec2 rowPos = ImGui::GetCursorScreenPos();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetFontSize() * 1.7f);
        if (ImGui::Selectable(info.label.c_str(), false) && doc) {
            // クリック: 選択中の入れ物（なければその親、ルート）に追加
            Widget* target = doc->ui.hasSelection ? Resolve(doc->asset->root.get(), doc->ui.selection) : doc->asset->root.get();
            AddWidget(*doc, info.name, target);
        }
        ImGui::GetWindowDrawList()->AddText({rowPos.x + 2.0f, rowPos.y}, ToU32(WidgetIconColor(info.name)), WidgetIcon(info.name));
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("%s\n%s\nクリックで追加・キャンバスや階層へドラッグ", info.label.c_str(), info.help.c_str());
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(kTypePayload, info.name.data(), info.name.size());
            ImGui::Text("%s  %s", WidgetIcon(info.name), info.label.c_str());
            ImGui::EndDragDropSource();
        }
        ImGui::PopID();
    }

    // パーツ（このワークスペースのパーツを部品として入れる）
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kAccent);
    ImGui::SeparatorText("パーツ");
    ImGui::PopStyleColor();
    int shown = 0;
    for (const Project::SourceInfo& s : m_Project.GetSources()) {
        if (s.kind != LayoutKind::Part) continue;
        const std::string name = FileSystem::ToUtf8(s.path.stem());
        if (!q.empty() && name.find(q) == std::string::npos) continue;
        ++shown;
        const bool self = doc && (s.output == doc->output || LayoutUses(s.output, doc->output));
        ImGui::PushID(s.output.c_str());
        ImGui::BeginDisabled(self);
        const ImVec2 rowPos = ImGui::GetCursorScreenPos();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetFontSize() * 1.7f);
        if (ImGui::Selectable(name.c_str(), false) && doc) {
            Widget* target = doc->ui.hasSelection ? Resolve(doc->asset->root.get(), doc->ui.selection) : doc->asset->root.get();
            const Vec2 center = doc->asset->referenceResolution * 0.5f;
            DropAsset(*doc, s.output, target ? target : doc->asset->root.get(), center);
        }
        ImGui::GetWindowDrawList()->AddText({rowPos.x + 2.0f, rowPos.y}, ToU32(Theme::kCoral), ICON_FA_PUZZLE_PIECE);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", self ? "このレイアウト自身（か、これを使っているパーツ）なので入れられません" : "パーツ（部品）\nクリックで追加・キャンバスや階層へドラッグ");
        if (!self && ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(kAssetPayload, s.output.data(), s.output.size());
            ImGui::Text(ICON_FA_PUZZLE_PIECE "  %s", name.c_str());
            ImGui::EndDragDropSource();
        }
        ImGui::PopID();
    }
    if (shown == 0 && q.empty()) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("パーツを作ると、ここから部品として入れられます");
        ImGui::PopTextWrapPos();
    }
}

void App::DrawHierarchy(Document& doc) {
    WidgetAsset& asset = *doc.asset;
    if (!asset.root) return;
    WidgetPath path;
    DrawHierarchyNode(doc, *asset.root, path);
    // 何もない所をクリックで選択解除
    ImGui::Dummy({ImGui::GetContentRegionAvail().x, std::max(ImGui::GetContentRegionAvail().y, 20.0f)});
    if (ImGui::IsItemClicked()) doc.ui.hasSelection = false;
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kTypePayload))
            AddWidget(doc, std::string(static_cast<const char*>(p->Data), static_cast<size_t>(p->DataSize)), asset.root.get());
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kAssetPayload))
            DropAsset(doc, std::string(static_cast<const char*>(p->Data), static_cast<size_t>(p->DataSize)), asset.root.get(),
                      asset.referenceResolution * 0.5f);
        ImGui::EndDragDropTarget();
    }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) HandleDesignerKeys(doc);
}

void App::DrawHierarchyNode(Document& doc, Widget& w, WidgetPath& path) {
    WidgetAsset& asset = *doc.asset;
    const bool selected = (doc.ui.hasSelection && doc.ui.selection == path) ||
                          (w.GetParent() && std::find(doc.ui.extraSelection.begin(), doc.ui.extraSelection.end(), w.name) != doc.ui.extraSelection.end());
    // AllowOverlap: 行の右の鍵・目のボタンを押せるように（行より後に置いたボタンが優先）
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding |
                               ImGuiTreeNodeFlags_AllowOverlap;
    if (selected) flags |= ImGuiTreeNodeFlags_Selected;
    bool hasChildren = false;
    for (const auto& c : w.children) hasChildren |= !c->generated;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
    ImGui::PushID(PathKey(path).c_str());
    const bool renaming = doc.ui.isRenaming && doc.ui.renaming == path;
    const bool editorHidden = doc.editorHidden.contains(w.name);
    const std::string label = std::string(WidgetIcon(w.GetTypeName())) + "  " + (renaming ? std::string() : (w.name.empty() ? TypeLabel(w.GetTypeName()) : w.name)) +
                              (editorHidden && !renaming ? "  " ICON_FA_EYE_LOW_VISION : "");
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4.0f, 2.0f});
    ImGui::PushStyleColor(ImGuiCol_Text, w.visibility == WidgetVisibility::Collapsed || w.visibility == WidgetVisibility::Hidden || editorHidden ? Theme::kTextDim : Theme::kText);
    const bool open = ImGui::TreeNodeEx("##node", flags, "%s", label.c_str());
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    const ImRect itemRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) ImGui::OpenPopup("node-menu"); // 行の右クリック（後ろのボタンではなく）
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        if (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift) {
            ToggleExtraSelection(doc, path); // 一緒に選ぶ（整列パネルで揃える）
        } else {
            doc.ui.selection = path;
            doc.ui.hasSelection = true;
            doc.ui.extraSelection.clear();
        }
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && w.GetParent()) {
        doc.ui.renaming = path;
        doc.ui.isRenaming = true;
        doc.ui.renameBuffer = w.name;
    }
    if (ImGui::IsItemHovered()) {
        doc.ui.hover = path;
        doc.ui.hasHover = true;
    }
    // 編集のときだけのロック（鍵のアイコン）と、表示・非表示の切り替え（目のアイコン）
    if (w.GetParent()) {
        const bool locked = doc.lockedWidgets.contains(w.name);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::GetFrameHeight() * 2.1f);
        ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
        ImGui::PushStyleColor(ImGuiCol_Border, {0, 0, 0, 0});
        ImGui::PushStyleColor(ImGuiCol_Text, locked ? Theme::kCoral : ImVec4(Theme::kTextDim.x, Theme::kTextDim.y, Theme::kTextDim.z, 0.35f));
        if (ImGui::SmallButton(locked ? ICON_FA_LOCK "##lock" : ICON_FA_LOCK_OPEN "##lock")) {
            if (locked) doc.lockedWidgets.erase(w.name);
            else doc.lockedWidgets.insert(w.name);
            doc.Save();
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("ロック: キャンバスで選べない・動かせない（編集のときだけ。ゲームには関係ない）");
    }
    if (w.GetParent()) {
        const bool hidden = w.visibility == WidgetVisibility::Collapsed || w.visibility == WidgetVisibility::Hidden;
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::GetFrameHeight());
        ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
        ImGui::PushStyleColor(ImGuiCol_Border, {0, 0, 0, 0});
        ImGui::PushStyleColor(ImGuiCol_Text, hidden ? Theme::kTextDim : Theme::kText);
        if (ImGui::SmallButton(hidden ? ICON_FA_EYE_SLASH : ICON_FA_EYE)) {
            w.visibility = hidden ? WidgetVisibility::Visible : WidgetVisibility::Hidden;
            doc.Changed();
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("表示する / しない（場所は取る）");
    }
    if (!renaming && !w.style.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(Theme::kAccentDim, ICON_FA_BRUSH " %s", w.style.c_str());
    }
    if (renaming) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (!ImGui::IsAnyItemActive()) ImGui::SetKeyboardFocusHere();
        const bool done = ImGui::InputText("##rename", &doc.ui.renameBuffer, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        if (done || ImGui::IsItemDeactivated()) {
            if (!ImGui::IsKeyPressed(ImGuiKey_Escape)) RenameWidget(doc, w, doc.ui.renameBuffer);
            doc.ui.isRenaming = false;
        }
    }
    // ドラッグ & ドロップ（上 1/4: 前、下 1/4: 後ろ、真ん中: 子にする）
    if (w.GetParent() && ImGui::BeginDragDropSource()) {
        const std::string key = PathKey(path);
        ImGui::SetDragDropPayload(kNodePayload, key.data(), key.size());
        ImGui::Text("%s  %s", WidgetIcon(w.GetTypeName()), w.name.c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        const float y = ImGui::GetMousePos().y;
        const float h = itemRect.GetHeight();
        int zone = 1;
        if (w.GetParent()) zone = y < itemRect.Min.y + h * 0.28f ? 0 : (y > itemRect.Max.y - h * 0.28f ? 2 : 1);
        if (zone == 1 && !w.CanAddChild()) zone = 2;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImU32 col = ToU32(Theme::kAccent);
        if (zone == 0) dl->AddLine(itemRect.Min, {itemRect.Max.x, itemRect.Min.y}, col, 2.0f);
        else if (zone == 2) dl->AddLine({itemRect.Min.x, itemRect.Max.y}, itemRect.Max, col, 2.0f);
        else dl->AddRect(itemRect.Min, itemRect.Max, col, 3.0f, 0, 2.0f);
        Widget* parent = zone == 1 ? &w : w.GetParent();
        const int index = zone == 1 ? -1 : (w.GetParent() ? w.GetParent()->IndexOf(&w) + (zone == 2 ? 1 : 0) : -1);
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kTypePayload, ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
            AddWidget(doc, std::string(static_cast<const char*>(p->Data), static_cast<size_t>(p->DataSize)), parent, index);
        }
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kAssetPayload, ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
            Widget* docW = doc.view ? Resolve(doc.view->GetRoot(), path) : nullptr;
            const Vec2 local = docW ? Vec2{docW->GetRect().w * 0.5f, docW->GetRect().h * 0.5f} : asset.referenceResolution * 0.5f;
            DropAsset(doc, std::string(static_cast<const char*>(p->Data), static_cast<size_t>(p->DataSize)), &w, local);
        }
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kNodePayload, ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
            const WidgetPath fromPath = PathFromKey(std::string(static_cast<const char*>(p->Data), static_cast<size_t>(p->DataSize)));
            Widget* from = Resolve(asset.root.get(), fromPath);
            if (from && from != &w && from != parent && !parent->IsDescendantOf(from) && from->GetParent() && parent->CanAddChild()) {
                int idx = index;
                if (idx >= 0 && from->GetParent() == parent && parent->IndexOf(from) < idx) --idx;
                auto moved = from->GetParent()->RemoveChild(from);
                const bool toCanvas = dynamic_cast<UICanvasPanel*>(parent) != nullptr;
                if (toCanvas && moved->slot.size.x <= 0.0f) moved->slot.size = DefaultCanvasSize(moved->GetTypeName());
                Widget* raw = parent->AddChild(std::move(moved), idx);
                doc.ui.selection = PathOf(raw);
                doc.ui.hasSelection = true;
                doc.Changed();
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::BeginPopup("node-menu")) {
        doc.ui.selection = path;
        doc.ui.hasSelection = true;
        DrawWidgetMenu(doc, path);
        ImGui::EndPopup();
    }
    if (open) {
        for (size_t i = 0; i < w.children.size(); ++i) {
            if (w.children[i]->generated) continue;
            path.push_back(static_cast<int>(i));
            DrawHierarchyNode(doc, *w.children[i], path);
            path.pop_back();
            if (i >= w.children.size()) break; // 編集で消えたとき
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void App::DrawWidgetMenu(Document& doc, const WidgetPath& path) {
    Widget* w = Resolve(doc.asset->root.get(), path);
    if (!w) return;
    const bool isRoot = w->GetParent() == nullptr;
    ImGui::TextColored(WidgetIconColor(w->GetTypeName()), "%s  %s", WidgetIcon(w->GetTypeName()), w->name.c_str());
    ImGui::Separator();
    if (!isRoot) {
        const bool locked = doc.lockedWidgets.contains(w->name);
        if (ImGui::MenuItem(locked ? ICON_FA_LOCK_OPEN "  ロックを外す" : ICON_FA_LOCK "  ロック（キャンバスで選べなくする）")) {
            if (locked) doc.lockedWidgets.erase(w->name);
            else doc.lockedWidgets.insert(w->name);
            doc.Save();
        }
        const bool hidden = doc.editorHidden.contains(w->name);
        if (ImGui::MenuItem(hidden ? ICON_FA_EYE "  編集中も表示する" : ICON_FA_EYE_LOW_VISION "  編集中だけ隠す（ゲームでは表示）")) {
            if (hidden) doc.editorHidden.erase(w->name);
            else doc.editorHidden.insert(w->name);
            doc.Save();
            if (doc.view) doc.view->SetAsset(doc.asset);
        }
        ImGui::Separator();
    }
    if (w->CanAddChild() && ImGui::BeginMenu(ICON_FA_PLUS "  子を追加")) {
        for (const WidgetTypeInfo& info : WidgetRegistry::GetAll()) {
            if (ImGui::MenuItem((std::string(WidgetIcon(info.name)) + "  " + info.label).c_str())) AddWidget(doc, info.name, w);
        }
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem(ICON_FA_PEN "  名前を変更", "F2", false, !isRoot)) {
        doc.ui.renaming = path;
        doc.ui.isRenaming = true;
        doc.ui.renameBuffer = w->name;
    }
    if (ImGui::MenuItem(ICON_FA_CLONE "  複製", "Ctrl+D", false, !isRoot)) DuplicateWidget(doc, path);
    if (ImGui::MenuItem(ICON_FA_COPY "  コピー", "Ctrl+C")) CopyWidget(doc, path);
    if (ImGui::MenuItem(ICON_FA_PASTE "  貼り付け", "Ctrl+V")) PasteWidget(doc);
    ImGui::Separator();
    if (ImGui::BeginMenu(ICON_FA_OBJECT_GROUP "  ラップ（入れ物で包む）", !isRoot)) {
        for (const WidgetTypeInfo& info : WidgetRegistry::GetAll()) {
            auto probe = info.create();
            if (probe->GetMaxChildren() == 0) continue;
            if (ImGui::MenuItem((std::string(WidgetIcon(info.name)) + "  " + info.label).c_str())) WrapWidget(doc, path, info.name);
        }
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem(ICON_FA_OBJECT_UNGROUP "  中身を取り出す（入れ物を外す）", nullptr, false, !isRoot && !w->children.empty())) UnwrapWidget(doc, path);
    if (!isRoot) {
        Widget* parent = w->GetParent();
        const int index = parent->IndexOf(w);
        if (ImGui::MenuItem(ICON_FA_ARROW_UP "  前へ（奥へ）", nullptr, false, index > 0)) {
            std::swap(parent->children[static_cast<size_t>(index)], parent->children[static_cast<size_t>(index - 1)]);
            doc.ui.selection = PathOf(parent->children[static_cast<size_t>(index - 1)].get());
            doc.Changed();
        }
        if (ImGui::MenuItem(ICON_FA_ARROW_DOWN "  後ろへ（手前へ）", nullptr, false, index + 1 < static_cast<int>(parent->children.size()))) {
            std::swap(parent->children[static_cast<size_t>(index)], parent->children[static_cast<size_t>(index + 1)]);
            doc.ui.selection = PathOf(parent->children[static_cast<size_t>(index + 1)].get());
            doc.Changed();
        }
    }
    if (auto* uw = dynamic_cast<UIUserWidget*>(w); uw && !uw->asset.empty()) {
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_PUZZLE_PIECE "  この部品の UI を開く")) OpenDocument(Assets::GetPath(uw->asset));
    }
    ImGui::Separator();
    if (ImGui::MenuItem(ICON_FA_TRASH "  削除", "Delete", false, !isRoot)) DeleteWidget(doc, path);
}

// ---------------------------------------------------------------------------
// 吸い付き
// ---------------------------------------------------------------------------

UI::Rect App::Snap(Document& doc, Widget& w, const UI::Rect& rect, int handle) {
    doc.ui.guides.clear();
    if (!doc.ui.snap || ImGui::GetIO().KeyAlt || ImGui::GetIO().KeyCtrl) return rect;
    const Widget* parent = w.GetParent();
    if (!parent) return rect;
    const float uiScale = std::max(doc.view ? doc.view->GetScale() : 1.0f, 1e-4f);
    const float threshold = 6.0f / uiScale;

    // 吸い付く先の線（親の端・中央、兄弟の端・中央）
    struct Line {
        float pos, from, to;
    };
    std::vector<Line> xs, ys;
    auto addRect = [&](const UI::Rect& r) {
        xs.push_back({r.x, r.y, r.Bottom()});
        xs.push_back({r.x + r.w * 0.5f, r.y, r.Bottom()});
        xs.push_back({r.Right(), r.y, r.Bottom()});
        ys.push_back({r.y, r.x, r.Right()});
        ys.push_back({r.y + r.h * 0.5f, r.x, r.Right()});
        ys.push_back({r.Bottom(), r.x, r.Right()});
    };
    addRect(parent->GetRect());
    for (const auto& c : parent->children) {
        if (c.get() == &w || c->generated || !c->IsVisible()) continue;
        // まとめて動かしているものには吸い付かない（一緒に動くので）
        if (std::find(doc.ui.extraSelection.begin(), doc.ui.extraSelection.end(), c->name) != doc.ui.extraSelection.end()) continue;
        addRect(c->GetRect());
    }

    UI::Rect r = rect;
    // 動かす辺（-1: 動かさない、0: 左/上、1: 中央、2: 右/下）
    std::vector<int> moveX, moveY;
    if (handle < 0) {
        moveX = {0, 1, 2};
        moveY = {0, 1, 2};
    } else {
        if (handle == 0 || handle == 6 || handle == 7) moveX = {0};
        if (handle == 2 || handle == 3 || handle == 4) moveX = {2};
        if (handle == 0 || handle == 1 || handle == 2) moveY = {0};
        if (handle == 4 || handle == 5 || handle == 6) moveY = {2};
    }
    auto edge = [](float start, float size, int which) { return which == 0 ? start : which == 1 ? start + size * 0.5f : start + size; };

    auto snapAxis = [&](bool vertical, const std::vector<Line>& lines, const std::vector<int>& moving) {
        float best = threshold;
        float delta = 0.0f;
        int bestWhich = -1;
        const Line* bestLine = nullptr;
        for (int which : moving) {
            const float e = vertical ? edge(r.x, r.w, which) : edge(r.y, r.h, which);
            for (const Line& l : lines) {
                const float d = l.pos - e;
                if (std::abs(d) < best) {
                    best = std::abs(d);
                    delta = d;
                    bestWhich = which;
                    bestLine = &l;
                }
            }
        }
        if (!bestLine) return;
        float& start = vertical ? r.x : r.y;
        float& size = vertical ? r.w : r.h;
        if (handle < 0) {
            start += delta;
        } else if (bestWhich == 0) {
            start += delta;
            size -= delta;
        } else {
            size += delta;
        }
        const float a0 = vertical ? r.y : r.x, a1 = vertical ? r.Bottom() : r.Right();
        doc.ui.guides.push_back({vertical, bestLine->pos, std::min(a0, bestLine->from), std::max(a1, bestLine->to)});
    };
    snapAxis(true, xs, moveX);
    snapAxis(false, ys, moveY);
    r.w = std::max(r.w, 1.0f);
    r.h = std::max(r.h, 1.0f);
    return r;
}

// ---------------------------------------------------------------------------
// キャンバス
// ---------------------------------------------------------------------------

void App::DrawCanvas(Document& doc) {
    WidgetAsset& asset = *doc.asset;
    DesignerState& ui = doc.ui;
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 cpos = ImGui::GetCursorScreenPos();
    const ImVec2 csize = ImGui::GetContentRegionAvail();
    if (csize.x < 20.0f || csize.y < 20.0f) return;
    ui.canvas = {cpos.x, cpos.y, csize.x, csize.y};
    ImGui::InvisibleButton("##canvas", csize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 解像度と倍率
    const auto presets = ResolutionPresets();
    const int preset = std::clamp(ui.resolution, 0, static_cast<int>(presets.size()) - 1);
    const Vec2 res = preset == 0 ? asset.referenceResolution : Vec2{presets[static_cast<size_t>(preset)].w, presets[static_cast<size_t>(preset)].h};
    if (ui.zoom <= 0.0f) {
        ui.zoom = std::clamp(std::min((csize.x - 48.0f) / res.x, (csize.y - 48.0f) / res.y), 0.05f, 4.0f);
        ui.pan = {0.0f, 0.0f};
    }
    const float zoom = ui.zoom;
    const ImVec2 ssize{std::round(res.x * zoom), std::round(res.y * zoom)};
    const ImVec2 spos{std::round(cpos.x + (csize.x - ssize.x) * 0.5f + ui.pan.x), std::round(cpos.y + (csize.y - ssize.y) * 0.5f + ui.pan.y)};
    ui.screen = {spos.x, spos.y, ssize.x, ssize.y};
    ui.screenZoom = zoom; // 後でホイールで zoom が変わっても、このフレームは screen と同じ倍率で描く
    // 大きさが変わったら、ImGui にテクスチャを渡す前に作り直す（後で作り直すと、このフレームの描画が消えたテクスチャを指す）
    if (doc.framebuffer) doc.framebuffer->Resize(FramebufferSize(ssize.x), FramebufferSize(ssize.y));
    const float uiScale = zoom * CanvasScale(asset, res); // 基準解像度の 1 単位の画面上の大きさ
    // 倍率が変わったフレームは、選択・ホバーの枠が前の倍率のレイアウトで描かれないように先に並べ直す
    if (doc.view && doc.view->GetScale() != uiScale) {
        doc.view->Relayout({0.0f, 0.0f, static_cast<float>(FramebufferSize(ssize.x)), static_cast<float>(FramebufferSize(ssize.y))}, uiScale);
    }

    // 背景（チェック柄）と画面
    dl->PushClipRect(cpos, {cpos.x + csize.x, cpos.y + csize.y}, true);
    dl->AddRectFilled(cpos, {cpos.x + csize.x, cpos.y + csize.y}, IM_COL32(9, 20, 26, 255));
    const float cell = 16.0f;
    for (float y = cpos.y; y < cpos.y + csize.y; y += cell) {
        for (float x = cpos.x + (static_cast<int>((y - cpos.y) / cell) % 2) * cell; x < cpos.x + csize.x; x += cell * 2.0f)
            dl->AddRectFilled({x, y}, {x + cell, y + cell}, IM_COL32(12, 26, 33, 255));
    }
    dl->AddRectFilled({spos.x + 6, spos.y + 10}, {spos.x + ssize.x + 6, spos.y + ssize.y + 10}, IM_COL32(0, 0, 0, 90), 4.0f);
    if (doc.framebuffer) dl->AddImage(ImTextureRef(TextureId(*doc.framebuffer->GetColorTexture())), spos, {spos.x + ssize.x, spos.y + ssize.y}, {0, 1}, {1, 0});
    dl->AddRect({spos.x - 1, spos.y - 1}, {spos.x + ssize.x + 1, spos.y + ssize.y + 1}, IM_COL32(255, 255, 255, 60));
    dl->AddText({spos.x, spos.y - ImGui::GetTextLineHeight() - 4.0f}, ToU32(Theme::kTextDim), std::format("{:.0f} × {:.0f}  ·  {:.0f}%", res.x, res.y, zoom * 100.0f).c_str());
    // グリッド（基準解像度で 50 ごと）
    if (ui.showGrid && !ui.preview && uiScale * 50.0f > 8.0f) {
        dl->PushClipRect(spos, {spos.x + ssize.x, spos.y + ssize.y}, true);
        for (float x = 0.0f; x * uiScale < ssize.x; x += 50.0f)
            dl->AddLine({spos.x + x * uiScale, spos.y}, {spos.x + x * uiScale, spos.y + ssize.y}, IM_COL32(255, 255, 255, static_cast<int>(x) % 250 == 0 ? 22 : 9));
        for (float y = 0.0f; y * uiScale < ssize.y; y += 50.0f)
            dl->AddLine({spos.x, spos.y + y * uiScale}, {spos.x + ssize.x, spos.y + y * uiScale}, IM_COL32(255, 255, 255, static_cast<int>(y) % 250 == 0 ? 22 : 9));
        dl->PopClipRect();
    }

    Widget* docRoot = doc.view ? doc.view->GetRoot() : nullptr;
    auto toScreen = [&](const UI::Rect& r) { return ImRect({spos.x + r.x, spos.y + r.y}, {spos.x + r.x + r.w, spos.y + r.y + r.h}); };
    // キャンバス上の点を、ウィジェットの回転（祖先の分も含む）で回した位置にする
    auto screenPoint = [&](const Widget& w, ImVec2 p) {
        if (!w.HasScreenTransform()) return p;
        const Vec4 q = w.GetScreenTransform() * Vec4{p.x - spos.x, p.y - spos.y, 0.0f, 1.0f};
        return ImVec2{q.x + spos.x, q.y + spos.y};
    };
    // ウィジェットの枠（回っていれば回した四角形）
    auto outline = [&](const Widget& w, const ImRect& r, ImU32 col, float thickness) {
        if (!w.HasScreenTransform()) {
            dl->AddRect(r.Min, r.Max, col, 0.0f, 0, thickness);
            return;
        }
        dl->AddQuad(screenPoint(w, r.Min), screenPoint(w, {r.Max.x, r.Min.y}), screenPoint(w, r.Max), screenPoint(w, {r.Min.x, r.Max.y}), col, thickness);
    };
    const ImVec2 mouse = io.MousePos;
    const Vec2 mousePx{mouse.x - spos.x, mouse.y - spos.y};

    // ホバー
    Widget* hoverDoc = nullptr;
    if (hovered && docRoot && !ui.preview && ui.drag == DesignerState::Drag::None) hoverDoc = doc.view->HitTest(mousePx, false);
    // ロックしたもの（と、その中）はキャンバスで選ばない: ロックされていない親にする
    if (hoverDoc && !ui.hasHover) {
        while (hoverDoc) {
            Widget* a = Resolve(asset.root.get(), DesignPathOf(hoverDoc));
            if (!a || !doc.IsLocked(*a)) break;
            hoverDoc = hoverDoc->GetParent();
        }
    }
    if (hoverDoc && hoverDoc == docRoot) hoverDoc = nullptr;
    if (ui.hasHover) {
        // 階層でホバー中のもの
        if (Widget* hw = Resolve(docRoot, ui.hover)) hoverDoc = hw;
        ui.hasHover = false;
    }

    Widget* selDoc = ui.hasSelection ? Resolve(docRoot, ui.selection) : nullptr;
    Widget* selAsset = ui.hasSelection ? Resolve(asset.root.get(), ui.selection) : nullptr;
    const bool canTransform = selAsset && selDoc && IsCanvasChild(*selAsset) && !ui.preview && !doc.IsLocked(*selAsset);

    // リサイズのつまみ（8 方向）
    int handleHover = -1;
    ImVec2 handles[8];
    if (canTransform) {
        const ImRect r = toScreen(selDoc->GetScreenRect());
        const ImVec2 c = r.GetCenter();
        handles[0] = r.Min; handles[1] = {c.x, r.Min.y}; handles[2] = {r.Max.x, r.Min.y}; handles[3] = {r.Max.x, c.y};
        handles[4] = r.Max; handles[5] = {c.x, r.Max.y}; handles[6] = {r.Min.x, r.Max.y}; handles[7] = {r.Min.x, c.y};
        if (hovered && ui.drag == DesignerState::Drag::None) {
            for (int i = 0; i < 8; ++i) {
                if (std::abs(mouse.x - handles[i].x) <= 6.0f && std::abs(mouse.y - handles[i].y) <= 6.0f) handleHover = i;
            }
        }
        if (handleHover >= 0)
            ImGui::SetMouseCursor(handleHover % 4 == 1 ? ImGuiMouseCursor_ResizeNS : handleHover % 4 == 3 ? ImGuiMouseCursor_ResizeEW
                                  : (handleHover == 0 || handleHover == 4) ? ImGuiMouseCursor_ResizeNWSE : ImGuiMouseCursor_ResizeNESW);
    }

    // ---- 操作 ----
    if (!ui.preview) {
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !(io.KeyAlt || ImGui::IsKeyDown(ImGuiKey_Space))) {
            ui.dragMouse = {mouse.x, mouse.y};
            ui.dragMoved = false;
            if (handleHover >= 0) {
                ui.drag = DesignerState::Drag::Resize;
                ui.handle = handleHover;
            } else if (io.KeyShift && hoverDoc) {
                // Shift + クリック: 一緒に選ぶ / 外す（整列パネルで揃える）
                ToggleExtraSelection(doc, DesignPathOf(hoverDoc));
            } else {
                Widget* target = hoverDoc;
                // 選択中のものの中をクリックしたら、そのまま動かせる（重なっていても）
                if (selDoc && selDoc->GetScreenRect().Contains(mousePx) && target && target->IsDescendantOf(selDoc) && !io.KeyCtrl) target = selDoc;
                // 一緒に選んでいるものをクリックしたときは、そのまま（まとめて選んだまま）
                const bool inGroup = target && std::find(ui.extraSelection.begin(), ui.extraSelection.end(), target->name) != ui.extraSelection.end();
                if (inGroup && selAsset) {
                    // クリックしたものを最初に（今までの最初のものは一緒に選んだ側へ）
                    std::erase(ui.extraSelection, target->name);
                    ui.extraSelection.push_back(selAsset->name);
                } else if (target != selDoc) {
                    ui.extraSelection.clear();
                }
                if (target) {
                    ui.selection = DesignPathOf(target);
                    ui.hasSelection = true;
                } else {
                    ui.hasSelection = false;
                }
                selAsset = ui.hasSelection ? Resolve(asset.root.get(), ui.selection) : nullptr;
                selDoc = ui.hasSelection ? Resolve(docRoot, ui.selection) : nullptr;
                if (selAsset && selDoc && IsCanvasChild(*selAsset) && !doc.IsLocked(*selAsset)) ui.drag = DesignerState::Drag::Move;
            }
            if (ui.drag != DesignerState::Drag::None && selAsset && selDoc) {
                ui.dragSlot = selAsset->slot;
                ui.dragRect = selDoc->GetRect();
            }
            // 一緒に選んでいるものも、まとめて動かす
            ui.dragGroup.clear();
            if (ui.drag == DesignerState::Drag::Move && docRoot) {
                for (const std::string& name : ui.extraSelection) {
                    Widget* ea = asset.root->Find(name);
                    Widget* ev = ea && IsCanvasChild(*ea) ? Resolve(docRoot, PathOf(ea)) : nullptr;
                    if (ev && ev->GetParent()) ui.dragGroup.push_back({name, ea->slot, ev->GetRect(), ev->GetParent()->GetRect()});
                }
            }
        }
        if (active && ImGui::IsMouseDown(ImGuiMouseButton_Left) && (ui.drag == DesignerState::Drag::Move || ui.drag == DesignerState::Drag::Resize) && selAsset && selDoc) {
            const float k = std::max(uiScale, 1e-4f);
            Vec2 d{(mouse.x - ui.dragMouse.x) / k, (mouse.y - ui.dragMouse.y) / k};
            if (std::abs(d.x) + std::abs(d.y) > 0.5f) ui.dragMoved = true;
            if (ui.dragMoved) {
                if (io.KeyCtrl) d = {std::round(d.x / 10.0f) * 10.0f, std::round(d.y / 10.0f) * 10.0f};
                UI::Rect r = ui.dragRect;
                if (ui.drag == DesignerState::Drag::Move) {
                    if (io.KeyShift) (std::abs(d.x) > std::abs(d.y) ? d.y : d.x) = 0.0f; // 縦か横だけ
                    r.x += d.x;
                    r.y += d.y;
                    r = Snap(doc, *selDoc, r, -1);
                } else {
                    const int h = ui.handle;
                    if (h == 0 || h == 6 || h == 7) { r.x += d.x; r.w -= d.x; }
                    if (h == 2 || h == 3 || h == 4) r.w += d.x;
                    if (h == 0 || h == 1 || h == 2) { r.y += d.y; r.h -= d.y; }
                    if (h == 4 || h == 5 || h == 6) r.h += d.y;
                    r.w = std::max(r.w, 1.0f);
                    r.h = std::max(r.h, 1.0f);
                    r = Snap(doc, *selDoc, r, h);
                    selAsset->slot.autoSize = false;
                }
                const Widget* parentDoc = selDoc->GetParent();
                WidgetSlot slot = ui.dragSlot;
                slot.autoSize = selAsset->slot.autoSize;
                SetSlotRect(slot, parentDoc ? parentDoc->GetRect() : UI::Rect{0, 0, res.x, res.y}, r);
                selAsset->slot = slot;
                if (ui.drag == DesignerState::Drag::Move) {
                    const float mx = r.x - ui.dragRect.x, my = r.y - ui.dragRect.y; // 吸い付いた後の動き
                    for (const DesignerState::GroupDrag& g : ui.dragGroup) {
                        Widget* ea = asset.root->Find(g.name);
                        if (!ea) continue;
                        WidgetSlot gs = g.slot;
                        SetSlotRect(gs, g.parent, {g.rect.x + mx, g.rect.y + my, g.rect.w, g.rect.h});
                        ea->slot = gs;
                    }
                }
                doc.Changed();
                ImGui::SetTooltip("位置 %.0f, %.0f   大きさ %.0f × %.0f", r.x, r.y, r.w, r.h);
            }
        }
        if (ui.drag != DesignerState::Drag::None && ui.drag != DesignerState::Drag::Pan && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            ui.drag = DesignerState::Drag::None;
            ui.guides.clear();
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            if (hoverDoc) {
                ui.selection = DesignPathOf(hoverDoc);
                ui.hasSelection = true;
            } else {
                ui.selection.clear();
                ui.hasSelection = true;
            }
            ImGui::OpenPopup("canvas-menu");
        }
    }
    // 移動（中ボタン・Alt / Space + 左ドラッグ）
    if (active && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f) || ((io.KeyAlt || ImGui::IsKeyDown(ImGuiKey_Space)) && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)))) {
        ui.pan = ui.pan + Vec2{io.MouseDelta.x, io.MouseDelta.y};
    }
    // ホイールで拡大縮小（マウスの位置を中心に）
    if (hovered && io.MouseWheel != 0.0f && !ui.preview) {
        const float nz = std::clamp(ui.zoom * std::pow(1.12f, io.MouseWheel), 0.05f, 4.0f);
        const Vec2 center{cpos.x + csize.x * 0.5f + ui.pan.x, cpos.y + csize.y * 0.5f + ui.pan.y};
        const Vec2 rel{mouse.x - center.x, mouse.y - center.y};
        ui.pan = ui.pan + rel * (1.0f - nz / ui.zoom);
        ui.zoom = nz;
    }

    // ドロップ（パレットのウィジェット・アセット）
    if (ImGui::BeginDragDropTarget()) {
        Widget* hitDoc = docRoot ? doc.view->HitTest(mousePx, false) : nullptr;
        // 置く先: マウスの下で子を持てる一番手前（部品の中身ではなく、文書のウィジェット）
        Widget* targetDoc = hitDoc;
        while (targetDoc && !(targetDoc->CanAddChild() && !targetDoc->generated)) targetDoc = targetDoc->GetParent();
        if (!targetDoc) targetDoc = docRoot;
        if (targetDoc) {
            const ImRect tr = toScreen(targetDoc->GetScreenRect());
            dl->AddRect(tr.Min, tr.Max, ToU32(Theme::kAccent), 0.0f, 0, 2.0f);
        }
        const float k = std::max(uiScale, 1e-4f);
        auto localIn = [&](Widget* w) {
            const UI::Rect pr = w ? w->GetScreenRect() : UI::Rect{};
            return Vec2{(mousePx.x - pr.x) / k, (mousePx.y - pr.y) / k};
        };
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kTypePayload)) {
            const std::string type(static_cast<const char*>(p->Data), static_cast<size_t>(p->DataSize));
            Widget* target = targetDoc ? Resolve(asset.root.get(), DesignPathOf(targetDoc)) : asset.root.get();
            const Vec2 local = localIn(targetDoc);
            if (target) AddWidget(doc, type, target, -1, &local);
        }
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kAssetPayload)) {
            const std::string path(static_cast<const char*>(p->Data), static_cast<size_t>(p->DataSize));
            // 画像・フォント・音はマウスの下のウィジェットに入れる（入れられなければ新しく作る）
            Widget* under = hitDoc ? Resolve(asset.root.get(), DesignPathOf(hitDoc)) : nullptr;
            Widget* container = targetDoc ? Resolve(asset.root.get(), DesignPathOf(targetDoc)) : asset.root.get();
            const Vec2 local = localIn(targetDoc);
            const std::string ext = LowerExtOf(path);
            if (IsUIExt(ext) || (IsImageExt(ext) && !dynamic_cast<UIImage*>(under) && !dynamic_cast<UIButton*>(under)))
                DropAsset(doc, path, container, local);
            else
                DropAsset(doc, path, under ? under : container, local);
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::BeginPopup("canvas-menu")) {
        DrawWidgetMenu(doc, ui.selection);
        ImGui::EndPopup();
    }

    // ---- 重ねて描く（選択・ホバー・アンカー・吸い付きの線） ----
    if (!ui.preview) {
        // Null・Bounding ペインは何も描かないので、デザイン中だけ四隅のかぎと基準点（ピボット）の十字で場所を示す
        // （Bounding ペインは当たりの範囲を薄く塗る）。回っていれば回した位置に描く
        if (docRoot) {
            docRoot->ForEach([&](Widget& w) {
                const std::string_view type = w.GetTypeName();
                const bool bounding = type == "UIBoundingPane";
                if ((type != "UINullPane" && !bounding) || !w.IsVisible()) return;
                const ImU32 mark = bounding ? Theme::U32(Theme::kCoral, 0.9f) : Theme::U32(Theme::kTextDim, 0.75f);
                const ImRect r = toScreen(w.GetScreenRect());
                auto pt = [&](float x, float y) { return screenPoint(w, {x, y}); };
                if (bounding) {
                    dl->AddQuadFilled(pt(r.Min.x, r.Min.y), pt(r.Max.x, r.Min.y), pt(r.Max.x, r.Max.y), pt(r.Min.x, r.Max.y), Theme::U32(Theme::kCoral, 0.12f));
                }
                const float len = std::min({10.0f, r.GetWidth() * 0.5f, r.GetHeight() * 0.5f});
                for (int i = 0; i < 4; ++i) {
                    const float cx = i & 1 ? r.Max.x : r.Min.x, cy = i & 2 ? r.Max.y : r.Min.y;
                    const float sx = i & 1 ? -1.0f : 1.0f, sy = i & 2 ? -1.0f : 1.0f;
                    dl->AddLine(pt(cx, cy), pt(cx + len * sx, cy), mark, 1.0f);
                    dl->AddLine(pt(cx, cy), pt(cx, cy + len * sy), mark, 1.0f);
                }
                const float px = r.Min.x + r.GetWidth() * w.slot.pivot.x, py = r.Min.y + r.GetHeight() * w.slot.pivot.y;
                dl->AddLine(pt(px - 6.0f, py), pt(px + 6.0f, py), mark, 1.0f);
                dl->AddLine(pt(px, py - 6.0f), pt(px, py + 6.0f), mark, 1.0f);
            });
        }
        if (hoverDoc && hoverDoc != selDoc) {
            const ImRect r = toScreen(hoverDoc->GetScreenRect());
            outline(*hoverDoc, r, Theme::U32(Theme::kAccent, 0.55f), 1.0f);
        }
        // 一緒に選んでいるもの（整列の対象）
        for (const std::string& name : ui.extraSelection) {
            Widget* ea = asset.root->Find(name);
            Widget* ew = ea && docRoot ? Resolve(docRoot, PathOf(ea)) : nullptr;
            if (!ew) continue;
            const ImRect r = toScreen(ew->GetScreenRect());
            outline(*ew, r, ToU32(Theme::kAccent), 1.5f);
            if (!ew->HasScreenTransform()) dl->AddRectFilled(r.Min, r.Max, Theme::U32(Theme::kAccent, 0.11f));
        }
        if (selDoc) {
            const ImRect r = toScreen(selDoc->GetScreenRect());
            const ImU32 accent = ToU32(Theme::kAccent);
            // 親の範囲（薄く）
            if (Widget* pd = selDoc->GetParent()) {
                const ImRect pr = toScreen(pd->GetScreenRect());
                dl->AddRect(pr.Min, pr.Max, IM_COL32(255, 255, 255, 40), 0.0f, 0, 1.0f);
            }
            if (selDoc->HasScreenTransform()) {
                // 回っているとき: 回った枠（太線）と、つまみ・アンカーの基準になる回る前の枠（薄く）
                outline(*selDoc, r, accent, 2.0f);
                dl->AddRect(r.Min, r.Max, Theme::U32(Theme::kAccent, 0.35f), 0.0f, 0, 1.0f);
            } else {
                dl->AddRect(r.Min, r.Max, accent, 0.0f, 0, 2.0f);
            }
            // 名前の札
            const std::string tag = std::string(WidgetIcon(selDoc->GetTypeName())) + " " + selDoc->name;
            const ImVec2 ts = ImGui::CalcTextSize(tag.c_str());
            const ImVec2 tp{r.Min.x, std::max(r.Min.y - ts.y - 6.0f, spos.y - ts.y - 6.0f)};
            dl->AddRectFilled({tp.x, tp.y}, {tp.x + ts.x + 10.0f, tp.y + ts.y + 4.0f}, accent, 3.0f);
            dl->AddText({tp.x + 5.0f, tp.y + 2.0f}, Theme::U32(Theme::kAccentInk), tag.c_str());
            if (canTransform) {
                // アンカー（Unity の花びら）
                if (Widget* pd = selDoc->GetParent()) {
                    const UI::Rect pr = pd->GetScreenRect();
                    const WidgetSlot& sl = selAsset->slot;
                    const float ax[2] = {spos.x + pr.x + sl.anchorMin.x * pr.w, spos.x + pr.x + sl.anchorMax.x * pr.w};
                    const float ay[2] = {spos.y + pr.y + sl.anchorMin.y * pr.h, spos.y + pr.y + sl.anchorMax.y * pr.h};
                    const ImU32 ac = IM_COL32(255, 255, 255, 220);
                    const float a = 9.0f;
                    dl->AddTriangleFilled({ax[0], ay[0]}, {ax[0] - a, ay[0] - a * 0.4f}, {ax[0] - a * 0.4f, ay[0] - a}, ac);
                    dl->AddTriangleFilled({ax[1], ay[0]}, {ax[1] + a * 0.4f, ay[0] - a}, {ax[1] + a, ay[0] - a * 0.4f}, ac);
                    dl->AddTriangleFilled({ax[0], ay[1]}, {ax[0] - a * 0.4f, ay[1] + a}, {ax[0] - a, ay[1] + a * 0.4f}, ac);
                    dl->AddTriangleFilled({ax[1], ay[1]}, {ax[1] + a, ay[1] + a * 0.4f}, {ax[1] + a * 0.4f, ay[1] + a}, ac);
                    // ピボット
                    const ImVec2 pv{r.Min.x + r.GetWidth() * sl.pivot.x, r.Min.y + r.GetHeight() * sl.pivot.y};
                    dl->AddCircle(pv, 5.0f, IM_COL32(255, 255, 255, 230), 0, 2.0f);
                }
                for (int i = 0; i < 8; ++i) {
                    dl->AddRectFilled({handles[i].x - 4, handles[i].y - 4}, {handles[i].x + 4, handles[i].y + 4}, i == handleHover ? accent : IM_COL32(255, 255, 255, 255));
                    dl->AddRect({handles[i].x - 4, handles[i].y - 4}, {handles[i].x + 4, handles[i].y + 4}, accent);
                }
            } else if (selAsset && selAsset->GetParent()) {
                // Box の中: スロットのパディングを薄く表示
                const Vec4& p = selAsset->slot.padding;
                if (p.x + p.y + p.z + p.w > 0.0f) {
                    const float k = uiScale;
                    dl->AddRect({r.Min.x - p.x * k, r.Min.y - p.y * k}, {r.Max.x + p.z * k, r.Max.y + p.w * k}, IM_COL32(255, 200, 80, 120), 0.0f, 0, 1.0f);
                }
            }
        }
        // 吸い付きの線（基準解像度の単位 → 画面）
        const float sx = spos.x, sy = spos.y;
        for (const SnapGuide& g : ui.guides) {
            const ImU32 gc = IM_COL32(255, 80, 200, 230);
            if (g.vertical) dl->AddLine({sx + g.pos * uiScale, sy + g.from * uiScale - 8.0f}, {sx + g.pos * uiScale, sy + g.to * uiScale + 8.0f}, gc, 1.0f);
            else dl->AddLine({sx + g.from * uiScale - 8.0f, sy + g.pos * uiScale}, {sx + g.to * uiScale + 8.0f, sy + g.pos * uiScale}, gc, 1.0f);
        }
    }
    if (ui.preview) dl->AddRect({spos.x - 3, spos.y - 3}, {spos.x + ssize.x + 3, spos.y + ssize.y + 3}, ToU32(Theme::kAccent), 4.0f, 0, 2.0f);
    dl->PopClipRect();
}

// ---------------------------------------------------------------------------
// 詳細
// ---------------------------------------------------------------------------

void App::DrawSlot(Document& doc, Widget& w) {
    Widget* parent = w.GetParent();
    if (!parent) return;
    WidgetSlot& s = w.slot;
    bool changed = false;
    const bool canvas = dynamic_cast<UICanvasPanel*>(parent) != nullptr;
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kAccent);
    ImGui::SeparatorText(std::format("スロット（親: {}）", TypeLabel(parent->GetTypeName())).c_str());
    ImGui::PopStyleColor();
    if (canvas) {
        Widget* docW = doc.view ? Resolve(doc.view->GetRoot(), PathOf(&w)) : nullptr;
        const UI::Rect parentRect = docW && docW->GetParent() ? docW->GetParent()->GetRect() : UI::Rect{0, 0, doc.asset->referenceResolution.x, doc.asset->referenceResolution.y};
        const UI::Rect current = SlotRect(s, parentRect, docW ? docW->GetDesiredSize() : s.size);
        // アンカーのプリセット（Unity の 4×4 の表）
        const float bs = ImGui::GetFrameHeight() * 2.2f;
        auto drawAnchorIcon = [](ImDrawList* dl, ImVec2 mn, float size, Vec2 amin, Vec2 amax, bool hot) {
            const ImU32 frame = hot ? ToU32(Theme::kAccent) : IM_COL32(150, 155, 170, 255);
            dl->AddRect({mn.x + 4, mn.y + 4}, {mn.x + size - 4, mn.y + size - 4}, frame, 2.0f);
            const float x0 = mn.x + 4 + (size - 8) * amin.x, x1 = mn.x + 4 + (size - 8) * amax.x;
            const float y0 = mn.y + 4 + (size - 8) * amin.y, y1 = mn.y + 4 + (size - 8) * amax.y;
            const ImU32 red = IM_COL32(240, 90, 90, 255);
            if (x1 - x0 > 1.0f) dl->AddLine({x0, (y0 + y1) * 0.5f}, {x1, (y0 + y1) * 0.5f}, red, 2.0f);
            if (y1 - y0 > 1.0f) dl->AddLine({(x0 + x1) * 0.5f, y0}, {(x0 + x1) * 0.5f, y1}, red, 2.0f);
            const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
            dl->AddRectFilled({cx - 2.5f, cy - 2.5f}, {cx + 2.5f, cy + 2.5f}, IM_COL32(255, 255, 255, 230));
        };
        ImGui::InvisibleButton("##anchor", {bs, bs});
        drawAnchorIcon(ImGui::GetWindowDrawList(), ImGui::GetItemRectMin(), bs, s.anchorMin, s.anchorMax, ImGui::IsItemHovered());
        if (ImGui::IsItemClicked()) ImGui::OpenPopup("anchor-presets");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("アンカーのプリセット（Shift を押しながら選ぶと位置とピボットも合わせる）");
        if (ImGui::BeginPopup("anchor-presets")) {
            ImGui::TextDisabled("アンカー（Shift: ピボットと位置も合わせる）");
            static const float kVals[3] = {0.0f, 0.5f, 1.0f};
            const float cellSize = ImGui::GetFrameHeight() * 2.0f;
            for (int row = 0; row < 4; ++row) {
                for (int col = 0; col < 4; ++col) {
                    if (col > 0) ImGui::SameLine();
                    Vec2 amin{col < 3 ? kVals[col] : 0.0f, row < 3 ? kVals[row] : 0.0f};
                    Vec2 amax{col < 3 ? kVals[col] : 1.0f, row < 3 ? kVals[row] : 1.0f};
                    ImGui::PushID(row * 4 + col);
                    ImGui::InvisibleButton("##p", {cellSize, cellSize});
                    const bool hot = ImGui::IsItemHovered();
                    drawAnchorIcon(ImGui::GetWindowDrawList(), ImGui::GetItemRectMin(), cellSize, amin, amax, hot);
                    if (ImGui::IsItemClicked()) {
                        s.anchorMin = amin;
                        s.anchorMax = amax;
                        if (ImGui::GetIO().KeyShift) {
                            // その角・辺にぴったり付ける
                            s.pivot = {col < 3 ? kVals[col] : 0.5f, row < 3 ? kVals[row] : 0.5f};
                            s.position = {0.0f, 0.0f};
                            if (col == 3) s.size.x = 0.0f;
                            if (row == 3) s.size.y = 0.0f;
                            if (col < 3 && s.size.x <= 0.0f) s.size.x = current.w;
                            if (row < 3 && s.size.y <= 0.0f) s.size.y = current.h;
                        } else {
                            SetSlotRect(s, parentRect, current); // 見た目の位置は変えない
                        }
                        changed = true;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopID();
                }
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        ImGui::BeginGroup();
        const bool stretchX = s.anchorMin.x != s.anchorMax.x, stretchY = s.anchorMin.y != s.anchorMax.y;
        ImGui::TextDisabled("%s %s", stretchX ? "横: 引き伸ばし" : "横: 固定", stretchY ? "/ 縦: 引き伸ばし" : "/ 縦: 固定");
        ImGui::TextDisabled("今の位置 %.0f, %.0f  大きさ %.0f × %.0f", current.x - parentRect.x, current.y - parentRect.y, current.w, current.h);
        ImGui::EndGroup();
        if (BeginPropertyTable("slot-canvas")) {
            // 引き伸ばす軸は Left / Right（Top / Bottom）、そうでなければ位置と大きさ（Unity と同じ表示）
            auto axis = [&](int a, bool stretch, const char* posLabel, const char* sizeLabel, const char* minLabel, const char* maxLabel) {
                float& pos = a == 0 ? s.position.x : s.position.y;
                float& size = a == 0 ? s.size.x : s.size.y;
                const float pivot = a == 0 ? s.pivot.x : s.pivot.y;
                if (!stretch) {
                    PropertyLabel(posLabel);
                    changed |= ImGui::DragFloat(std::format("##pos{}", a).c_str(), &pos, 1.0f, 0.0f, 0.0f, "%.0f");
                    PropertyLabel(sizeLabel);
                    ImGui::BeginDisabled(s.autoSize);
                    changed |= ImGui::DragFloat(std::format("##size{}", a).c_str(), &size, 1.0f, 0.0f, 100000.0f, "%.0f");
                    ImGui::EndDisabled();
                } else {
                    float offMin = pos - size * pivot;           // 左（上）の余白
                    float offMax = -(pos + size * (1.0f - pivot)); // 右（下）の余白
                    PropertyLabel(minLabel);
                    bool c1 = ImGui::DragFloat(std::format("##min{}", a).c_str(), &offMin, 1.0f, 0.0f, 0.0f, "%.0f");
                    PropertyLabel(maxLabel);
                    bool c2 = ImGui::DragFloat(std::format("##max{}", a).c_str(), &offMax, 1.0f, 0.0f, 0.0f, "%.0f");
                    if (c1 || c2) {
                        size = -offMax - offMin;
                        pos = offMin + size * pivot;
                        changed = true;
                    }
                }
            };
            axis(0, stretchX, "X", "幅", "左の余白", "右の余白");
            axis(1, stretchY, "Y", "高さ", "上の余白", "下の余白");
            PropertyLabel("アンカー（最小）", "親の中の割合（0〜1）");
            changed |= DragVec("##amin", &s.anchorMin.x, 2, 0.01f, 0.0f, "%.2f");
            PropertyLabel("アンカー（最大）");
            changed |= DragVec("##amax", &s.anchorMax.x, 2, 0.01f, 1.0f, "%.2f");
            PropertyLabel("ピボット", "自分の中の基準点（0〜1）");
            changed |= DragVec("##pivot", &s.pivot.x, 2, 0.01f, 0.5f, "%.2f");
            PropertyLabel("中身に合わせる", "テキストなどの中身の大きさにする");
            changed |= ImGui::Checkbox("##auto", &s.autoSize);
            PropertyLabel("Z オーダー", "大きいほど手前");
            changed |= ImGui::DragInt("##z", &s.zOrder, 0.2f);
            EndPropertyTable();
        }
    } else {
        const bool box = dynamic_cast<UIHorizontalBox*>(parent) || dynamic_cast<UIVerticalBox*>(parent) || dynamic_cast<UIScrollBox*>(parent);
        if (BeginPropertyTable("slot-box")) {
            PropertyLabel("余白（左上右下）", "このウィジェットの外側の余白");
            changed |= DragVec("##pad", &s.padding.x, 4, 0.5f, 0.0f, "%.0f");
            // 揃え（アイコンのボタン）
            PropertyLabel("横の揃え");
            const char* hIcons[] = {ICON_FA_ARROWS_LEFT_RIGHT_TO_LINE, ICON_FA_ALIGN_LEFT, ICON_FA_ALIGN_CENTER, ICON_FA_ALIGN_RIGHT};
            const char* hTips[] = {"いっぱいに広げる (Fill)", "左", "中央", "右"};
            for (int i = 0; i < 4; ++i) {
                if (i) ImGui::SameLine(0.0f, 2.0f);
                ImGui::PushID(i);
                if (IconButton(hIcons[i], hTips[i], static_cast<int>(s.hAlign) == i)) {
                    s.hAlign = static_cast<HAlign>(i);
                    changed = true;
                }
                ImGui::PopID();
            }
            PropertyLabel("縦の揃え");
            const char* vLabels[] = {"Fill", "上", "中", "下"};
            const char* vTips[] = {"いっぱいに広げる (Fill)", "上", "中央", "下"};
            for (int i = 0; i < 4; ++i) {
                if (i) ImGui::SameLine(0.0f, 2.0f);
                ImGui::PushID(10 + i);
                if (IconButton(vLabels[i], vTips[i], static_cast<int>(s.vAlign) == i)) {
                    s.vAlign = static_cast<VAlign>(i);
                    changed = true;
                }
                ImGui::PopID();
            }
            if (box) {
                PropertyLabel("大きさ", "Auto: 中身の大きさ / Fill: 残りを分け合う");
                int rule = static_cast<int>(s.sizeRule);
                const char* rules[] = {"Auto（中身に合わせる）", "Fill（残りを使う）"};
                if (ImGui::Combo("##rule", &rule, rules, 2)) {
                    s.sizeRule = static_cast<SizeRule>(rule);
                    changed = true;
                }
                if (s.sizeRule == SizeRule::Fill) {
                    PropertyLabel("Fill の割合");
                    changed |= ImGui::DragFloat("##weight", &s.fillWeight, 0.05f, 0.0f, 100.0f, "%.2f");
                }
            }
            EndPropertyTable();
        }
    }
    if (changed) doc.Changed();
}

void App::DrawPartOverrides(Document& doc, UIUserWidget& uw) {
    auto part = WidgetAsset::Load(uw.asset);
    if (!part || !part->root) return;
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kAccent);
    ImGui::SeparatorText("パーツの上書き");
    ImGui::PopStyleColor();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("中のウィジェットを選んで値を変えると、ここに置いたパーツだけが変わります（パーツの元の値と違うものだけ保存）");
    ImGui::PopTextWrapPos();

    Json all = Json::MakeObject();
    if (!uw.overrides.empty()) {
        if (auto parsed = Json::Parse(uw.overrides); parsed && parsed->IsObject()) all = std::move(*parsed);
    }
    const Json& readOnly = all; // 読むだけ（無いキーを作らない）
    auto save = [&]() {
        uw.overrides = all.Size() == 0 ? std::string() : all.Dump(0);
        doc.Changed();
    };

    // 中のウィジェット（名前の付いたもの）
    std::vector<Widget*> inner;
    part->root->ForEach([&](Widget& w) {
        if (&w != part->root.get() && !w.generated && !w.name.empty()) inner.push_back(&w);
    });
    Widget* original = nullptr;
    for (Widget* w : inner) {
        if (w->name == m_OverrideTarget) original = w;
    }
    const std::string preview = original ? std::format("{}  {}", WidgetIcon(original->GetTypeName()), original->name) : std::string("（中のウィジェットを選ぶ）");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##override-target", preview.c_str(), ImGuiComboFlags_HeightLarge)) {
        for (Widget* w : inner) {
            const bool overridden = readOnly[w->name].IsObject();
            const std::string label = std::format("{}  {}{}", WidgetIcon(w->GetTypeName()), w->name, overridden ? "  ●" : "");
            if (ImGui::Selectable(label.c_str(), w == original)) m_OverrideTarget = w->name;
        }
        ImGui::EndCombo();
    }

    // 上書きしているもの
    std::string clear;
    for (const auto& [name, props] : all.Items()) {
        if (!props.IsObject()) continue;
        ImGui::PushID(name.c_str());
        if (IconButton(ICON_FA_XMARK, "この上書きを消す（パーツの値に戻す）")) clear = name;
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        std::string keys;
        for (const auto& [k, v] : props.Items()) keys += (keys.empty() ? "" : "・") + PropertyLabelJa(k);
        if (ImGui::Selectable(std::format("{}: {}", name, keys).c_str(), name == m_OverrideTarget)) m_OverrideTarget = name;
        ImGui::PopID();
    }
    if (!clear.empty()) {
        Json rest = Json::MakeObject();
        for (const auto& [k, v] : all.Items()) {
            if (k != clear) rest[k] = v;
        }
        all = std::move(rest);
        save();
        return;
    }
    if (!original) return;

    // パーツの値に上書きを重ねたものを編集し、元の値と違うものだけを上書きにする
    auto edited = original->Clone();
    if (readOnly[original->name].IsObject()) {
        const Json& props = readOnly[original->name];
        SceneSerializer::DeserializeProperties(props, [&](PropertyVisitor& v) { edited->ReflectCommon(v); });
        SceneSerializer::DeserializeProperties(props, [&](PropertyVisitor& v) { edited->Reflect(v); });
    }
    DesignerVisitor v(m_Project);
    v.target = edited.get();
    v.onEditShader = [this](const std::string& path) { OpenShaderEditor(path); };
    v.onEditTextAnim = [this](int slot) { OpenTextAnimEditor(slot); };
    v.Begin("override");
    edited->Reflect(v);
    v.Header("共通");
    edited->ReflectCommon(v);
    v.End();
    if (readOnly[original->name].IsObject() && ImGui::Button(ICON_FA_ROTATE_LEFT "  このウィジェットをパーツの値に戻す", {-FLT_MIN, 0.0f})) {
        clear = original->name;
        Json rest = Json::MakeObject();
        for (const auto& [k, val] : all.Items()) {
            if (k != clear) rest[k] = val;
        }
        all = std::move(rest);
        save();
        return;
    }
    if (!v.changed) return;
    const Json before = WidgetAsset::SerializeWidget(*original);
    const Json after = WidgetAsset::SerializeWidget(*edited);
    Json diff = Json::MakeObject();
    for (const char* group : {"common", "properties"}) {
        for (const auto& [k, val] : after[group].Items()) {
            if (before[group][k].Dump(0) != val.Dump(0)) diff[k] = val;
        }
    }
    if (diff.Size() == 0) {
        Json rest = Json::MakeObject();
        for (const auto& [k, val] : all.Items()) {
            if (k != original->name) rest[k] = val;
        }
        all = std::move(rest);
    } else {
        all[original->name] = std::move(diff);
    }
    save();
}

void App::DrawDetails(Document& doc) {
    WidgetAsset& asset = *doc.asset;
    DesignerState& ui = doc.ui;
    Widget* w = ui.hasSelection ? Resolve(asset.root.get(), ui.selection) : nullptr;
    if (!w) {
        ImGui::TextDisabled(ICON_FA_ARROW_POINTER "  ウィジェットを選ぶと、ここで編集できます");
        ImGui::Spacing();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("・パレットからキャンバスや階層にドラッグして置きます");
        ImGui::TextDisabled("・Canvas Panel の子はアンカーとピボットで、Box の子は余白・揃え・Auto / Fill で並びます");
        ImGui::TextDisabled("・テキストの {変数名} や「変数と連動」で、スクリプトの値を表示できます（「変数」のパネルで試せます）");
        ImGui::TextDisabled("・アセットのパネルから画像・フォント・音・UI をドラッグして入れられます");
        ImGui::TextDisabled("・スクリプト: GetComponent<UIDocument>()->OnClick(AquaUI::%s::Widgets::名前, ...)", ToIdentifier(doc.GetTitle()).c_str());
        ImGui::PopTextWrapPos();
        return;
    }
    // ---- 名前・種類 ----
    ImGui::PushFont(GetBoldFont(), ImGui::GetStyle().FontSizeBase * 1.2f);
    ImGui::TextColored(WidgetIconColor(w->GetTypeName()), "%s", WidgetIcon(w->GetTypeName()));
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    std::string name = w->name;
    if (ImGui::InputText("##name", &name, ImGuiInputTextFlags_EnterReturnsTrue) || (ImGui::IsItemDeactivatedAfterEdit() && name != w->name)) RenameWidget(doc, *w, name);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("スクリプトから Find(\"名前\") で探すときの名前");
    if (const WidgetTypeInfo* info = WidgetRegistry::Find(w->GetTypeName())) ImGui::TextDisabled("%s · %s", info->label.c_str(), info->help.c_str());

    // ---- スタイル ----
    if (w->GetParent()) {
        ImGui::Spacing();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(ICON_FA_BRUSH);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() * 2.6f);
        if (ImGui::BeginCombo("##style", w->style.empty() ? "スタイルなし" : w->style.c_str())) {
            if (ImGui::Selectable("スタイルなし", w->style.empty())) {
                w->style.clear();
                doc.Changed();
            }
            for (const WidgetStyle& st : asset.styles) {
                if (st.type != w->GetTypeName()) continue;
                if (ImGui::Selectable(st.name.c_str(), st.name == w->style)) {
                    w->style = st.name;
                    asset.ApplyStyle(*w);
                    doc.Changed();
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("スタイル: 同じスタイルのウィジェットの見た目をまとめて変えられる");
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::BeginDisabled(w->style.empty());
        if (IconButton(ICON_FA_ARROWS_ROTATE, "このウィジェットの見た目をスタイルに反映（同じスタイルの全ウィジェットが変わる）")) {
            asset.UpdateStyleFrom(*w);
            doc.Changed();
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0.0f, 2.0f);
        if (IconButton(ICON_FA_PLUS, "今の見た目で新しいスタイルを作る")) {
            ImGui::OpenPopup("new-style");
            ui.renameBuffer = std::format("{}Style", w->GetTypeName() + 2);
        }
        if (ImGui::BeginPopup("new-style")) {
            ImGui::TextDisabled("新しいスタイルの名前");
            if (!ImGui::IsAnyItemActive()) ImGui::SetKeyboardFocusHere();
            const bool ok = ImGui::InputText("##sn", &ui.renameBuffer, ImGuiInputTextFlags_EnterReturnsTrue);
            if ((ok || ImGui::Button("作成")) && !ui.renameBuffer.empty()) {
                w->style = ui.renameBuffer;
                asset.UpdateStyleFrom(*w);
                doc.Changed();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    DrawSlot(doc, *w);

    // ---- プロパティ（録画中は、アニメーションした値を編集してキーにする） ----
    WidgetAnimation* anim = ui.animation >= 0 && ui.animation < static_cast<int>(asset.animations.size()) ? &asset.animations[static_cast<size_t>(ui.animation)] : nullptr;
    const bool recording = ui.animRecording && anim && !ui.preview;
    Widget* target = w;
    if (recording && doc.view) {
        if (Widget* dw = Resolve(doc.view->GetRoot(), ui.selection)) target = dw;
    }
    std::map<std::string, float> before;
    if (recording) {
        for (const auto& info : WidgetProperties::List(*target)) {
            float v = 0.0f;
            if (WidgetProperties::Get(*target, info.path, v)) before[info.path] = v;
        }
        ImGui::Spacing();
        ImGui::TextColored({0.95f, 0.35f, 0.35f, 1.0f}, ICON_FA_CIRCLE "  録画中: 数値を変えると「%s」の %.2f 秒にキーが打たれます", anim->name.c_str(), ui.animTime);
    }
    // テキストの演出のプリセット（いくつかの値をまとめて入れる）
    if (auto* text = dynamic_cast<UIText*>(target)) {
        ImGui::Spacing();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##text-preset", ICON_FA_WAND_MAGIC_SPARKLES "  テキストの演出（プリセット）")) {
            struct TextPreset {
                const char* label;
                TextReveal reveal;
                TextEffect effect;
                float revealSpeed, revealFade, strength;
            };
            static const TextPreset kPresets[] = {
                {"会話（1 文字ずつ）", TextReveal::Typewriter, TextEffect::None, 28.0f, 0.0f, 6.0f},
                {"ふわっと出す", TextReveal::Fade, TextEffect::None, 22.0f, 0.25f, 6.0f},
                {"ポップに出す", TextReveal::Pop, TextEffect::None, 18.0f, 0.3f, 6.0f},
                {"下から浮かぶ", TextReveal::Rise, TextEffect::None, 20.0f, 0.35f, 6.0f},
                {"落ちて弾む", TextReveal::Drop, TextEffect::None, 14.0f, 0.5f, 6.0f},
                {"回りながら出る", TextReveal::Spin, TextEffect::None, 16.0f, 0.35f, 6.0f},
                {"波打つタイトル", TextReveal::Pop, TextEffect::Wave, 14.0f, 0.3f, 8.0f},
                {"震える（警告・怒り）", TextReveal::None, TextEffect::Shake, 30.0f, 0.0f, 4.0f},
                {"虹色（レア）", TextReveal::None, TextEffect::Rainbow, 30.0f, 0.0f, 6.0f},
                {"脈打つ（注目）", TextReveal::None, TextEffect::Pulse, 30.0f, 0.0f, 6.0f},
                {"グリッチ（ノイズ）", TextReveal::None, TextEffect::Glitch, 30.0f, 0.0f, 8.0f},
                {"演出なし", TextReveal::None, TextEffect::None, 30.0f, 0.18f, 6.0f},
            };
            for (const TextPreset& p : kPresets) {
                if (ImGui::Selectable(p.label)) {
                    text->reveal = p.reveal;
                    text->effect = p.effect;
                    text->revealSpeed = p.revealSpeed;
                    text->revealFade = p.revealFade;
                    text->effectStrength = p.strength;
                    text->revealAuto = true;
                    doc.Changed();
                }
            }
            ImGui::EndCombo();
        }
        if (text->reveal != TextReveal::None && !ui.preview) {
            if (ImGui::Button(ICON_FA_CIRCLE_PLAY "  出し方をプレビューで見る（P）", {-FLT_MIN, 0.0f})) {
                ui.preview = true;
                if (doc.view) doc.view->SetAsset(doc.asset); // 作り直して最初から出す
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("編集中は全部の文字を出しています。プレビューでは実際の出し方で出します");
        }
    }

    DesignerVisitor v(m_Project);
    v.target = target;
    v.onEditShader = [this](const std::string& path) { OpenShaderEditor(path); };
    v.onEditTextAnim = [this](int slot) { OpenTextAnimEditor(slot); };
    v.Begin("props");
    target->Reflect(v);
    v.Header("共通");
    target->ReflectCommon(v);
    v.End();
    // シェーダーの状態（読めない・コンパイルできない・値を既定に戻す）
    if (!target->shader.empty()) {
        auto sh = UIShader::Get(target->shader);
        if (!sh) {
            ImGui::TextColored(Theme::kError, ICON_FA_TRIANGLE_EXCLAMATION "  シェーダーが見つかりません: %s", target->shader.c_str());
        } else if (!sh->IsValid()) {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextColored(Theme::kError, ICON_FA_TRIANGLE_EXCLAMATION "  シェーダーにエラーがあるので、ふつうに描いています");
            ImGui::PopTextWrapPos();
            if (ImGui::SmallButton(ICON_FA_PEN_TO_SQUARE " シェーダーを直す")) OpenShaderEditor(target->shader);
        } else if (ImGui::SmallButton(ICON_FA_ROTATE_LEFT " シェーダーの値を既定に戻す")) {
            target->ResetShaderParams();
            v.changed = true;
        }
    }
    if (v.changed) {
        if (recording) {
            for (const auto& info : WidgetProperties::List(*target)) {
                float now = 0.0f;
                if (!WidgetProperties::Get(*target, info.path, now)) continue;
                auto it = before.find(info.path);
                if (it == before.end() || std::abs(it->second - now) < 1e-6f) continue;
                WidgetAnimTrack* t = anim->FindTrack(w->name, info.path);
                if (!t) {
                    anim->tracks.push_back({w->name, info.path, TimelineCurve{info.path, {}}});
                    t = &anim->tracks.back();
                    // 最初のキーなら 0 秒に元の値も打っておく
                    if (ui.animTime > 1e-3f) t->curve.SetKey(0.0f, it->second);
                }
                t->curve.SetKey(ui.animTime, now);
            }
        }
        doc.Changed();
    }
    if (auto* uw = dynamic_cast<UIUserWidget*>(w); uw && !uw->asset.empty()) {
        ImGui::Spacing();
        if (ImGui::Button(ICON_FA_PUZZLE_PIECE "  この部品の UI を開く", {-FLT_MIN, 0.0f})) OpenDocument(Assets::GetPath(uw->asset));
        DrawPartOverrides(doc, *uw);
    }    if (std::string_view(w->GetTypeName()) == "UICapture") {
        // キャプチャー: 撮った画面はゲームのメモリ（テクスチャ）に持つだけで、ファイルには保存しない
        ImGui::Spacing();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled(ICON_FA_CAMERA "  ゲームでは、このウィジェットより先に描かれた画面（ゲーム・後ろの UI）を撮ってここに貼ります。"
                            "撮った画面はメモリに持つだけで、ファイルには保存しません。キャンバスでは見本を表示し、プレビュー（P）で実際に撮れます。");
        ImGui::TextDisabled("スクリプトで撮り直す: if (auto* c = dynamic_cast<UICapture*>(ui->Find(AquaUI::%s::Widgets::%s))) c->Capture();",
                            ToIdentifier(doc.GetTitle()).c_str(), ToIdentifier(w->name).c_str());
        ImGui::PopTextWrapPos();
    }
}

// ---------------------------------------------------------------------------
// 描画（フレームバッファへ）
// ---------------------------------------------------------------------------

void App::RenderDocument(Document& doc) {
    DesignerState& ui = doc.ui;
    if (!ui.screen.Valid() || !ui.canvas.Valid()) return;
    if (!doc.view) doc.view = std::make_unique<WidgetDocument>();
    if (doc.view->GetAsset() != doc.asset) doc.view->SetAsset(doc.asset);
    const int w = FramebufferSize(ui.screen.w);
    const int h = FramebufferSize(ui.screen.h);
    if (!doc.framebuffer) doc.framebuffer = std::make_unique<Framebuffer>(w, h);
    else if (doc.framebuffer->GetWidth() != w || doc.framebuffer->GetHeight() != h) doc.framebuffer->Resize(w, h);
    doc.framebuffer->Bind();
    RenderCommand::Clear({0.09f, 0.15f, 0.19f, 1.0f});
    Renderer2D::BeginScene(Mat4::Ortho(0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, -1.0f, 1.0f), true);
    if (!doc.uiContext) doc.uiContext = UI::CreateContext();
    UI::SetContext(doc.uiContext);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool inside = mouse.x >= ui.canvas.x && mouse.y >= ui.canvas.y && mouse.x < ui.canvas.x + ui.canvas.w && mouse.y < ui.canvas.y + ui.canvas.h;
    const bool accept = ui.preview && inside && ui.focused;
    UI::BeginFrame(static_cast<float>(w), static_cast<float>(h), {ui.screen.x, ui.screen.y}, accept);
    doc.view->designMode = !ui.preview;
    doc.view->interactive = ui.preview;
    doc.view->navigation = true;
    const WidgetAnimation* anim = ui.animation >= 0 && ui.animation < static_cast<int>(doc.asset->animations.size()) ? &doc.asset->animations[static_cast<size_t>(ui.animation)] : nullptr;
    doc.view->previewAnimation = anim && !ui.preview && !m_AnimTextMode ? anim->name : std::string();
    doc.view->previewTime = ui.animTime;
    // プレビュー用の変数（変数のパネルで入れた値。プレビューで変わった値はそのまま）
    for (const auto& [name, value] : doc.previewVariables) {
        if (!ui.preview || doc.view->GetVariables().find(name) == doc.view->GetVariables().end()) doc.view->SetVariable(name, value);
    }
    // 解像度を変えたときは Canvas Scaler の倍率（基準解像度との比）に表示の倍率をかける
    const auto presets = ResolutionPresets();
    const int preset = std::clamp(ui.resolution, 0, static_cast<int>(presets.size()) - 1);
    const Vec2 res = preset == 0 ? doc.asset->referenceResolution : Vec2{presets[static_cast<size_t>(preset)].w, presets[static_cast<size_t>(preset)].h};
    doc.view->Update({0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h)}, ImGui::GetIO().DeltaTime, ui.screenZoom * CanvasScale(*doc.asset, res));
    doc.view->RefreshUserWidgets(); // 部品の UI がほかのタブで書き出されたら作り直す
    std::vector<std::pair<Widget*, float>> hiddenNow; // 編集中だけ隠すもの（描いたら元の不透明度に戻す）
    if (!ui.preview) {
        for (const std::string& name : doc.editorHidden) {
            if (Widget* vw = doc.view->Find(name)) {
                hiddenNow.emplace_back(vw, vw->opacity);
                vw->opacity = 0.0f;
            }
        }
    }
    doc.view->Paint();
    for (auto& [vw, opacity] : hiddenNow) vw->opacity = opacity;
    for (const UIEvent& e : doc.view->TakeEvents()) {
        const char* kind = e.type == UIEventType::Click ? "クリック" : e.type == UIEventType::ValueChanged ? "値の変更" : e.type == UIEventType::Submit ? "決定"
                         : e.type == UIEventType::TextRevealed ? "テキストを出し終えた" : e.type == UIEventType::TextHidden ? "テキストを消し終えた" : nullptr;
        if (!kind) continue;
        ui.lastEvent = e.type == UIEventType::ValueChanged ? std::format("{}: {}  {}", kind, e.widget, e.text.empty() ? std::format("{:.2f}", e.value) : e.text)
                                                           : std::format("{}: {}（OnUIEvent の message: \"{}\"）", kind, e.widget, e.message);
        ui.lastEventTime = ImGui::GetTime();
    }
    UI::EndFrame();
    UI::SetContext(nullptr);
    Renderer2D::EndScene();
    Framebuffer::BindDefault(GetWindow().GetWidth(), GetWindow().GetHeight());
}

} // namespace AquaTool
