// 文字アニメーション: 1 文字に掛ける動きをキーフレームで作る（テキストの「出し方」「動き」の「自作」）
//   アニメーションのパネルの中で編集する（左の一覧の「文字アニメーション」を選ぶと、右側がこの編集になる）
//
//   ・出し方: 文字が出るときの動き（1 文字ずつ順に動き出す。出す速さはテキストの「出す速さ」）
//   ・動き:   ずっと繰り返す動き（文字ごとに少しずつずらせる。テキストの中の <custom>…</custom> でも使える）
//   ・10 の行（ずれ・大きさ・回転・不透明度・色）をいつもすべて表示する。ダブルクリックでキーを打つ
//   ・キーの選択・まとめて動かす・コピー / 貼り付け・複製・反転などは AquaKeys（アニメーションのパネルと共通）
//   ・上の見本で、再生・一時停止・逆再生・繰り返しを確かめられる
//   ・ひな形から作る・作ったものを名前を付けて保存して、ほかのテキスト・レイアウトでも使う
//   データは UIText の customReveal・customEffect に JSON で入る（.aqs / .aqua にそのまま保存され、ゲームでも同じに動く）

#include "AquaApp.h"
#include "AquaKeys.h"

#include <Ruto/EditorUI/EditorUI.h>
#include <Ruto/EditorUI/Widgets.h>

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <set>
#include <fstream>
#include <sstream>

namespace AquaTool {

using namespace Ruto::EditorUI;

namespace {

struct Channel {
    const char* property;
    const char* label;
    const char* help;
    float speed; // 値のドラッグの細かさ
};

const Channel kChannels[] = {
    {"offset.x", "横のずれ", "文字の大きさに対する割合（1 で 1 文字分。右が +）", 0.01f},
    {"offset.y", "縦のずれ", "文字の大きさに対する割合（1 で 1 文字分。下が +）", 0.01f},
    {"scale", "大きさ", "1 が元の大きさ（横・縦の大きさにも掛かる）", 0.01f},
    {"scale.x", "横の大きさ", "1 が元の幅", 0.01f},
    {"scale.y", "縦の大きさ", "1 が元の高さ", 0.01f},
    {"rotation", "回転（度）", "時計回りの角度", 0.5f},
    {"alpha", "不透明度", "0 で見えない・1 でそのまま", 0.01f},
    {"color.r", "色（赤）", "文字の色に掛ける（1 でそのまま）", 0.01f},
    {"color.g", "色（緑）", "文字の色に掛ける（1 でそのまま）", 0.01f},
    {"color.b", "色（青）", "文字の色に掛ける（1 でそのまま）", 0.01f},
};

const Channel* FindChannel(std::string_view property) {
    for (const Channel& c : kChannels) {
        if (property == c.property) return &c;
    }
    return nullptr;
}

TimelineCurve& CurveOf(TextCharAnimation& a, std::string_view property) {
    if (TimelineCurve* c = a.Find(property)) return *c;
    a.curves.push_back({std::string(property), {}});
    return a.curves.back();
}

float ValueAt(const TextCharAnimation& a, std::string_view property, float t) {
    const TimelineCurve* c = a.Find(property);
    return c && !c->keys.empty() ? c->Evaluate(t) : TextCharAnimation::DefaultValue(property);
}

// ---- ひな形 ----
struct KeyDef {
    const char* property;
    std::initializer_list<std::pair<float, float>> keys;
    KeyInterpolation interpolation = KeyInterpolation::Smooth;
};
struct Template {
    const char* label;
    int slot; // 0: 出し方 / 1: 動き
    float pivot;
    float charDelay;
    std::initializer_list<KeyDef> curves;
};

const Template kTemplates[] = {
    // 出し方
    {"ぴょんと跳ねて出る", 0, 1.0f, 0.0f,
     {{"offset.y", {{0.0f, 0.6f}, {0.15f, -0.35f}, {0.3f, 0.0f}}}, {"scale.y", {{0.0f, 0.6f}, {0.15f, 1.15f}, {0.3f, 1.0f}}},
      {"alpha", {{0.0f, 0.0f}, {0.1f, 1.0f}}, KeyInterpolation::Linear}}},
    {"横から滑り込む", 0, 0.5f, 0.0f, {{"offset.x", {{0.0f, 1.5f}, {0.35f, 0.0f}}}, {"alpha", {{0.0f, 0.0f}, {0.2f, 1.0f}}, KeyInterpolation::Linear}}},
    {"大きく出て縮む", 0, 0.5f, 0.0f, {{"scale", {{0.0f, 2.5f}, {0.3f, 1.0f}}}, {"alpha", {{0.0f, 0.0f}, {0.15f, 1.0f}}, KeyInterpolation::Linear}}},
    {"回りながら落ちる", 0, 0.5f, 0.0f,
     {{"offset.y", {{0.0f, -1.5f}, {0.4f, 0.0f}}}, {"rotation", {{0.0f, -180.0f}, {0.4f, 0.0f}}}, {"alpha", {{0.0f, 0.0f}, {0.15f, 1.0f}}, KeyInterpolation::Linear}}},
    {"ぼよんと伸び縮み", 0, 1.0f, 0.0f,
     {{"scale.x", {{0.0f, 0.0f}, {0.12f, 1.4f}, {0.24f, 0.85f}, {0.36f, 1.0f}}}, {"scale.y", {{0.0f, 0.0f}, {0.12f, 0.7f}, {0.24f, 1.15f}, {0.36f, 1.0f}}}}},
    {"光ってから色が付く", 0, 0.5f, 0.0f,
     {{"color.r", {{0.0f, 3.0f}, {0.4f, 1.0f}}}, {"color.g", {{0.0f, 3.0f}, {0.4f, 1.0f}}}, {"color.b", {{0.0f, 3.0f}, {0.4f, 1.0f}}},
      {"scale", {{0.0f, 1.3f}, {0.4f, 1.0f}}}, {"alpha", {{0.0f, 0.0f}, {0.1f, 1.0f}}, KeyInterpolation::Linear}}},
    // 動き
    {"ぴょんぴょん", 1, 1.0f, 0.08f, {{"offset.y", {{0.0f, 0.0f}, {0.2f, -0.25f}, {0.4f, 0.0f}, {1.0f, 0.0f}}}}},
    {"ゆらゆら", 1, 1.0f, 0.1f, {{"rotation", {{0.0f, -8.0f}, {0.6f, 8.0f}, {1.2f, -8.0f}}}}},
    {"ドキドキ", 1, 0.5f, 0.0f, {{"scale", {{0.0f, 1.0f}, {0.1f, 1.25f}, {0.25f, 1.0f}, {0.8f, 1.0f}}}}},
    {"点滅", 1, 0.5f, 0.05f, {{"alpha", {{0.0f, 1.0f}, {0.4f, 0.2f}, {0.8f, 1.0f}}}}},
    {"くるりと回る", 1, 0.5f, 0.08f, {{"rotation", {{0.0f, 0.0f}, {0.5f, 360.0f}, {2.0f, 360.0f}}, KeyInterpolation::Linear}}},
    {"伸び縮み", 1, 1.0f, 0.06f, {{"scale.y", {{0.0f, 1.0f}, {0.25f, 1.3f}, {0.5f, 1.0f}}}, {"scale.x", {{0.0f, 1.0f}, {0.25f, 0.85f}, {0.5f, 1.0f}}}}},
};

TextCharAnimation MakeTemplate(const Template& t) {
    TextCharAnimation a;
    a.pivot = t.pivot;
    a.charDelay = t.charDelay;
    for (const KeyDef& d : t.curves) {
        TimelineCurve c{d.property, {}};
        for (const auto& [time, value] : d.keys) c.keys.push_back({time, value, d.interpolation});
        a.curves.push_back(std::move(c));
    }
    return a;
}

// ---- 保存したもの（ユーザーの設定フォルダの TextAnimations.json。どのレイアウトからも使える） ----
struct SavedAnim {
    std::string name;
    int slot = 0;
    std::string data;
};

fs::path SavedFile() { return Platform::GetUserDataDir() / "Aqua" / "TextAnimations.json"; }

std::vector<SavedAnim> LoadSaved() {
    std::vector<SavedAnim> out;
    std::ifstream in(SavedFile(), std::ios::binary);
    if (!in) return out;
    std::stringstream ss;
    ss << in.rdbuf();
    const auto json = Json::Parse(ss.str());
    if (!json) return out;
    for (const Json& e : (*json)["animations"].Elements()) out.push_back({e["name"].AsString(), e["slot"].AsInt(0), e["data"].AsString()});
    return out;
}

void WriteSaved(const std::vector<SavedAnim>& list) {
    Json arr = Json::MakeArray();
    for (const SavedAnim& s : list) {
        Json e = Json::MakeObject();
        e["name"] = s.name;
        e["slot"] = s.slot;
        e["data"] = s.data;
        arr.Push(std::move(e));
    }
    Json root = Json::MakeObject();
    root["animations"] = std::move(arr);
    std::error_code ec;
    fs::create_directories(SavedFile().parent_path(), ec);
    std::ofstream out(SavedFile(), std::ios::binary | std::ios::trunc);
    out << root.Dump(2);
}

// UTF-8 を 1 文字ずつに分ける
std::vector<std::string> SplitChars(std::string_view s) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        const size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
        out.emplace_back(s.substr(i, std::min(len, s.size() - i)));
        i += len;
    }
    return out;
}

} // namespace

void App::OpenTextAnimEditor(int slot) {
    m_TextAnimSlot = std::clamp(slot, 0, 1);
    m_TextKeySel.Clear();
    m_AnimTextMode = true;
    m_FocusAnimPanel = true;
}

void App::DrawTextAnimPanel(Document& doc) {
    const float s = m_Scale;
    const ImGuiIO& io = ImGui::GetIO();
    DesignerState& ui = doc.ui;
    Widget* sel = ui.hasSelection ? Resolve(doc.asset->root.get(), ui.selection) : nullptr;
    auto* text = dynamic_cast<UIText*>(sel);
    if (!text) {
        ImGui::TextDisabled("テキストを選ぶと、文字が出るときの動き・ずっと続く動きを、キーフレームで自由に作れます");
        ImGui::TextDisabled("左の「文字アニメーション」から出し方・動きを選びます。作った動きはゲームでも同じに動きます");
        return;
    }

    // ---- 編集するもの（出し方 / 動き） ----
    const bool revealSlot = m_TextAnimSlot == 0;
    std::string& source = revealSlot ? text->customReveal : text->customEffect;
    TextCharAnimation anim = TextCharAnimation::Parse(source);
    // すべての行（チャンネル）をいつも表示する（キーの無い行も）
    for (const Channel& ch : kChannels) CurveOf(anim, ch.property);
    std::vector<TimelineCurve*> rows;
    std::vector<std::string> rowIds;
    for (const Channel& ch : kChannels) {
        rows.push_back(anim.Find(ch.property));
        rowIds.emplace_back(ch.property);
    }
    KeySelection& ksel = m_TextKeySel;
    if (const std::string owner = std::format("{}#{}", text->name, m_TextAnimSlot); owner != m_TextKeyOwner) {
        m_TextKeyOwner = owner;
        ksel.Clear();
    }
    if (!ksel.dragging) std::erase_if(ksel.keys, [&](const KeyRef& k) { return !FindSelectedKey(rows, k); });
    bool changed = false;
    bool used = revealSlot ? text->reveal == TextReveal::Custom : text->effect == TextEffect::Custom;
    auto setUsed = [&](bool on) {
        if (revealSlot) text->reveal = on ? TextReveal::Custom : TextReveal::None;
        else text->effect = on ? TextEffect::Custom : TextEffect::None;
        used = on;
        changed = true;
    };
    // 動きを丸ごと入れ替える（ひな形・保存したもの）
    auto replaceAnim = [&](TextCharAnimation next) {
        anim = std::move(next);
        for (const Channel& ch : kChannels) CurveOf(anim, ch.property);
        for (size_t i = 0; i < std::size(kChannels); ++i) rows[i] = anim.Find(kChannels[i].property);
        ksel.Clear();
        m_TextAnimTime = 0.0f;
        m_TextAnimPlaying = true;
        m_TextAnimDir = 1.0f;
        if (!used) setUsed(true);
        changed = true;
    };

    const float len = std::max(anim.GetDuration(), 0.0f);
    std::string sample = m_TextAnimSample.empty() ? UIText::StripTags(text->text) : m_TextAnimSample;
    if (const size_t nl = sample.find('\n'); nl != std::string::npos) sample.resize(nl);
    std::vector<std::string> chars = SplitChars(sample);
    if (chars.size() > 40) chars.resize(40);
    if (chars.empty()) chars = SplitChars("サンプル");
    const int n = static_cast<int>(chars.size());
    const float step = 1.0f / std::max(text->revealSpeed, 0.5f);
    // 見本の全体の長さ（出し方: 最後の文字が出きって少し待つまで / 動き: 1 回分）
    const float total = revealSlot ? static_cast<float>(n - 1) * step + len + 0.6f : std::max(len, 0.01f);

    // ---- 再生（見本と、タイムラインの今の時刻） ----
    if (m_TextAnimPlaying && len > 1e-4f) {
        m_TextAnimTime += io.DeltaTime * m_TextAnimDir * (revealSlot ? 1.0f : text->effectSpeed);
        if (m_TextAnimTime > total || m_TextAnimTime < 0.0f) {
            if (m_TextAnimLoop) {
                m_TextAnimTime = m_TextAnimTime > total ? (revealSlot ? 0.0f : std::fmod(m_TextAnimTime, total)) : total;
            } else {
                m_TextAnimTime = std::clamp(m_TextAnimTime, 0.0f, total);
                m_TextAnimPlaying = false;
            }
        }
    }
    auto play = [&](float dir) {
        if (m_TextAnimPlaying && m_TextAnimDir == dir) {
            m_TextAnimPlaying = false;
            return;
        }
        if (dir > 0.0f && m_TextAnimTime >= total - 1e-3f) m_TextAnimTime = 0.0f;
        if (dir < 0.0f && m_TextAnimTime <= 1e-3f) m_TextAnimTime = total;
        m_TextAnimDir = dir;
        m_TextAnimPlaying = true;
    };

    // ---- 1 行目: 再生（アニメーションと同じ並び）・テキスト・使う・ひな形・保存・消す ----
    if (IconButton(ICON_FA_BACKWARD_STEP, "最初へ（Home）")) m_TextAnimTime = 0.0f;
    ImGui::SameLine(0.0f, 2.0f);
    const bool playingBack = m_TextAnimPlaying && m_TextAnimDir < 0.0f;
    if (IconButton(playingBack ? ICON_FA_PAUSE : ICON_FA_BACKWARD, playingBack ? "一時停止（Shift+Space）" : "逆再生（Shift+Space。出し方なら文字が消えていく）", playingBack)) play(-1.0f);
    ImGui::SameLine(0.0f, 2.0f);
    const bool playingFwd = m_TextAnimPlaying && m_TextAnimDir > 0.0f;
    if (IconButton(playingFwd ? ICON_FA_PAUSE : ICON_FA_PLAY, playingFwd ? "一時停止（Space）" : "再生（Space）", playingFwd)) play(1.0f);
    ImGui::SameLine(0.0f, 2.0f);
    if (IconButton(ICON_FA_FORWARD_STEP, "最後へ（End）")) m_TextAnimTime = total;
    ImGui::SameLine(0.0f, 2.0f);
    if (IconButton(ICON_FA_REPEAT, "繰り返す", m_TextAnimLoop)) m_TextAnimLoop = !m_TextAnimLoop;
    ImGui::SameLine(0.0f, 8.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(Theme::kAccent, ICON_FA_FONT "  %s の%s", text->name.c_str(), revealSlot ? "出し方（出るとき）" : "動き（ずっと）");
    ImGui::SameLine();
    bool useBox = used;
    if (ImGui::Checkbox(revealSlot ? "このテキストの出し方にする" : "このテキストの動きにする", &useBox)) setUsed(useBox);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip(revealSlot ? "テキストの「出し方」を「自作」にする（オフで「すぐ全部出す」）"
                                     : "テキストの「動き」を「自作」にする（一部だけ動かすなら、テキストの中で <custom>…</custom> で囲む）");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(170.0f * s);
    if (ImGui::BeginCombo("##ta-template", ICON_FA_WAND_MAGIC_SPARKLES "  ひな形・保存したもの", ImGuiComboFlags_HeightLarge)) {
        ImGui::TextDisabled("ひな形（今の動きと入れ替わる）");
        for (const Template& t : kTemplates) {
            if (t.slot != m_TextAnimSlot) continue;
            if (ImGui::Selectable(t.label)) replaceAnim(MakeTemplate(t));
        }
        const std::vector<SavedAnim> saved = LoadSaved();
        bool any = false;
        for (size_t i = 0; i < saved.size(); ++i) {
            if (saved[i].slot != m_TextAnimSlot) continue;
            if (!any) {
                ImGui::Separator();
                ImGui::TextDisabled("保存したもの（右クリックで削除）");
                any = true;
            }
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable((std::string(ICON_FA_FLOPPY_DISK "  ") + saved[i].name).c_str())) replaceAnim(TextCharAnimation::Parse(saved[i].data));
            if (ImGui::BeginPopupContextItem("saved-menu")) {
                if (ImGui::MenuItem(ICON_FA_TRASH "  削除")) {
                    std::vector<SavedAnim> next = saved;
                    next.erase(next.begin() + static_cast<std::ptrdiff_t>(i));
                    WriteSaved(next);
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine(0.0f, 2.0f);
    ImGui::BeginDisabled(anim.IsEmpty());
    if (IconButton(ICON_FA_FLOPPY_DISK, "名前を付けて保存（ほかのテキスト・レイアウトでも「ひな形・保存したもの」から使える）")) {
        m_TextAnimSaveName = revealSlot ? "MyReveal" : "MyEffect";
        ImGui::OpenPopup("ta-save");
    }
    ImGui::EndDisabled();
    if (ImGui::BeginPopup("ta-save")) {
        ImGui::TextDisabled("保存する名前");
        if (!ImGui::IsAnyItemActive()) ImGui::SetKeyboardFocusHere();
        const bool ok = ImGui::InputText("##ta-save-name", &m_TextAnimSaveName, ImGuiInputTextFlags_EnterReturnsTrue);
        if ((ok || ImGui::Button("保存")) && !m_TextAnimSaveName.empty()) {
            std::vector<SavedAnim> saved = LoadSaved();
            std::erase_if(saved, [&](const SavedAnim& a) { return a.name == m_TextAnimSaveName && a.slot == m_TextAnimSlot; });
            TextCharAnimation trimmed = anim;
            std::erase_if(trimmed.curves, [](const TimelineCurve& c) { return c.keys.empty(); });
            saved.push_back({m_TextAnimSaveName, m_TextAnimSlot, trimmed.ToString()});
            WriteSaved(saved);
            Notify(std::format("文字アニメーション「{}」を保存しました", m_TextAnimSaveName));
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine(0.0f, 2.0f);
    ImGui::BeginDisabled(anim.IsEmpty());
    if (IconButton(ICON_FA_TRASH, "キーをすべて消す")) {
        for (TimelineCurve* c : rows) c->keys.clear();
        ksel.Clear();
        changed = true;
    }
    ImGui::EndDisabled();


    // ==== 左: 設定・再生・見本 ====
    const float sideW = std::round(std::clamp(ImGui::GetContentRegionAvail().x * 0.32f, 230.0f * s, 380.0f * s));
    ImGui::BeginChild("##ta-side", {sideW, 0.0f}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
    {
        const float fieldW = ImGui::GetContentRegionAvail().x - 96.0f * s;
        auto label = [&](const char* t) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%s", t);
            ImGui::SameLine(96.0f * s);
            ImGui::SetNextItemWidth(fieldW);
        };
        label("中心");
        const char* pivots[] = {"文字の上", "文字の中央", "文字の下"};
        int pivotIndex = anim.pivot < 0.25f ? 0 : anim.pivot > 0.75f ? 2 : 1;
        if (ImGui::Combo("##ta-pivot", &pivotIndex, pivots, 3)) {
            anim.pivot = static_cast<float>(pivotIndex) * 0.5f;
            changed = true;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("回転・拡大の中心の高さ");
        if (revealSlot) {
            label("出す速さ");
            if (ImGui::DragFloat("##ta-speed", &text->revealSpeed, 0.2f, 0.5f, 200.0f, "%.1f 文字/秒")) changed = true;
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("1 秒に何文字ずつ動き出すか（テキストの「出す速さ」）");
        } else {
            label("文字のずれ");
            if (ImGui::DragFloat("##ta-delay", &anim.charDelay, 0.005f, 0.0f, 2.0f, "%.3f 秒")) changed = true;
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("隣の文字と動きをずらす秒（0 なら全部の文字がそろって動く）");
            label("速さ");
            if (ImGui::DragFloat("##ta-espeed", &text->effectSpeed, 0.01f, 0.05f, 10.0f, "x%.2f")) changed = true;
        }
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("長さ");
        ImGui::SameLine(96.0f * s);
        ImGui::Text("%.2f 秒", len);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##ta-sample", "見本の文字（空ならテキストの文字）", &m_TextAnimSample);

        // 見本（ImGui で 1 文字ずつ描く）
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const float availW = ImGui::GetContentRegionAvail().x;
        const float previewH = std::max(ImGui::GetContentRegionAvail().y - 2.0f, 40.0f * s);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p0, {p0.x + availW, p0.y + previewH}, Theme::U32(Theme::kBackground), 4.0f * s);
        ImFont* font = ImGui::GetFont();
        std::vector<float> widths;
        float lineW = 0.0f;
        float size = std::round(std::min(30.0f * s, previewH * 0.45f));
        for (const std::string& c : chars) lineW += font->CalcTextSizeA(size, FLT_MAX, 0.0f, c.c_str()).x;
        if (lineW > availW - 16.0f * s && lineW > 0.0f) size = std::max(std::floor(size * (availW - 16.0f * s) / lineW), 10.0f);
        lineW = 0.0f;
        for (const std::string& c : chars) {
            widths.push_back(font->CalcTextSizeA(size, FLT_MAX, 0.0f, c.c_str()).x);
            lineW += widths.back();
        }
        float x = p0.x + std::max((availW - lineW) * 0.5f, 8.0f * s);
        const float top = p0.y + (previewH - size) * 0.5f;
        const Vec4 base = text->color;
        dl->PushClipRect(p0, {p0.x + availW, p0.y + previewH}, true);
        for (int i = 0; i < n; ++i) {
            float t;
            bool visible = true;
            if (revealSlot) {
                t = m_TextAnimTime - static_cast<float>(i) * step;
                visible = t > 0.0f;
                t = std::clamp(t, 0.0f, len);
            } else {
                t = len > 1e-4f ? std::fmod(m_TextAnimTime - static_cast<float>(i) * anim.charDelay, len) : 0.0f;
                if (t < 0.0f) t += len;
            }
            const float w = widths[static_cast<size_t>(i)];
            if (visible) {
                const TextCharAnimation::Sample cs = anim.Evaluate(t);
                const ImVec2 pivot{x + w * 0.5f, top + std::clamp(anim.pivot, -1.0f, 2.0f) * size};
                const float r = cs.rotation * 3.14159265f / 180.0f;
                const float cr = std::cos(r), sr = std::sin(r);
                const Vec4 col{base.x * cs.color.x, base.y * cs.color.y, base.z * cs.color.z, std::clamp(base.w * cs.color.w, 0.0f, 1.0f)};
                const int v0 = dl->VtxBuffer.Size;
                dl->AddText(font, size, {x, top}, ImGui::ColorConvertFloat4ToU32({std::min(col.x, 1.0f), std::min(col.y, 1.0f), std::min(col.z, 1.0f), col.w}),
                            chars[static_cast<size_t>(i)].c_str());
                for (int k = v0; k < dl->VtxBuffer.Size; ++k) {
                    ImVec2& pos = dl->VtxBuffer[k].pos;
                    const float dx = (pos.x - pivot.x) * cs.scale.x, dy = (pos.y - pivot.y) * cs.scale.y;
                    pos = {pivot.x + dx * cr - dy * sr + cs.offset.x * size, pivot.y + dx * sr + dy * cr + cs.offset.y * size};
                }
            }
            x += w;
        }
        dl->PopClipRect();
        ImGui::Dummy({availW, previewH});
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ==== 右: 今のキー・タイムライン（10 の行をすべて表示） ====
    // 今の時刻（タイムラインの線）: 出し方は最初の文字の時刻、動きは 1 回分の中の時刻
    const float cursor = revealSlot ? std::max(m_TextAnimTime, 0.0f) : (len > 1e-4f ? std::fmod(m_TextAnimTime, len) : 0.0f);
    ImGui::BeginChild("##ta-main", {0.0f, 0.0f}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        // 今のキー（いちばん最後に選んだもの）
        ImGui::AlignTextToFramePadding();
        const KeyRef* prim = ksel.Primary();
        TimelineKey* pk = prim ? FindSelectedKey(rows, *prim) : nullptr;
        if (pk) {
            const Channel& ch = kChannels[static_cast<size_t>(prim->row)];
            if (ksel.keys.size() > 1) ImGui::TextColored(Theme::kAccent, ICON_FA_DIAMOND "  %s（ほか %d 個）", ch.label, static_cast<int>(ksel.keys.size()) - 1);
            else ImGui::TextColored(Theme::kAccent, ICON_FA_DIAMOND "  %s", ch.label);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f * s);
            float kt = pk->time;
            if (ImGui::DragFloat("##ta-ktime", &kt, 0.005f, 0.0f, 30.0f, "%.3f 秒")) {
                // 選んだキーをまとめてずらす
                changed |= MoveKeys(rows, ksel, std::max(kt, 0.0f) - prim->time);
            }
            if (const KeyRef* p2 = ksel.Primary(); TimelineKey* k2 = p2 ? FindSelectedKey(rows, *p2) : nullptr) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(90.0f * s);
                if (ImGui::DragFloat("##ta-kvalue", &k2->value, kChannels[static_cast<size_t>(p2->row)].speed, 0.0f, 0.0f, "%.3f")) changed = true;
                ImGui::SameLine();
                ImGui::SetNextItemWidth(110.0f * s);
                const char* interps[] = {"なめらか", "まっすぐ", "そのまま（段）"};
                int ii = static_cast<int>(k2->interpolation);
                if (ImGui::Combo("##ta-kinterp", &ii, interps, 3)) changed |= SetKeysInterpolation(rows, ksel, static_cast<KeyInterpolation>(ii));
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("次のキーまでのつなぎ方（選んだキーすべて）");
                ImGui::SameLine();
                if (IconButton(ICON_FA_TRASH, "選んだキーを削除（Delete）")) changed |= DeleteKeys(rows, ksel);
            }
        } else {
            ImGui::TextDisabled("ダブルクリックでキー・ドラッグで囲んで選ぶ・Ctrl+C / V でコピー / 貼り付け（%.2f 秒）", cursor);
        }

        const float labelW = std::round(96.0f * s);
        const float rulerH = std::round(18.0f * s);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float areaW = ImGui::GetContentRegionAvail().x;
        const int rowCount = static_cast<int>(std::size(kChannels));
        const float rowH = std::clamp(std::floor((ImGui::GetContentRegionAvail().y - rulerH - 2.0f) / static_cast<float>(rowCount)), std::round(11.0f * s), std::round(22.0f * s));
        ImDrawList* tl = ImGui::GetWindowDrawList();
        const float x0 = origin.x + labelW;
        auto timeToX = [&](float t) { return x0 + 8.0f + t * m_TextAnimZoom; };
        auto xToTime = [&](float x) { return std::max((x - x0 - 8.0f) / m_TextAnimZoom, 0.0f); };
        auto snapTime = [&](float t) { return io.KeyAlt ? t : std::round(t * 100.0f) / 100.0f; };
        // 目盛り
        tl->AddRectFilled({x0, origin.y}, {origin.x + areaW, origin.y + rulerH}, ToU32(Theme::kPanel));
        float tick = 0.05f;
        while (tick * m_TextAnimZoom < 45.0f * s) tick *= 2.0f;
        for (float t = 0.0f; timeToX(t) < origin.x + areaW; t += tick) {
            tl->AddLine({timeToX(t), origin.y + rulerH - 5.0f}, {timeToX(t), origin.y + rulerH}, ToU32(Theme::kTextDim));
            tl->AddText({timeToX(t) + 3.0f, origin.y + 1.0f}, ToU32(Theme::kTextDim), std::format("{:.2f}", t).c_str());
        }
        if (len > 0.0f) tl->AddRectFilled({timeToX(len), origin.y}, {origin.x + areaW, origin.y + rulerH}, Theme::U32(Theme::kBackground, 0.5f));
        ImGui::SetCursorScreenPos({x0, origin.y});
        ImGui::InvisibleButton("##ta-ruler", {std::max(areaW - labelW, 1.0f), rulerH});
        if (ImGui::IsItemActive()) {
            m_TextAnimTime = snapTime(xToTime(io.MousePos.x));
            m_TextAnimPlaying = false;
        }
        if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f && io.MousePos.x > x0) m_TextAnimZoom = std::clamp(m_TextAnimZoom * std::pow(1.15f, io.MouseWheel), 40.0f, 3000.0f);

        // 行の名前・背景・値の変化の線
        const float rowsTop = origin.y + rulerH;
        for (int ci = 0; ci < rowCount; ++ci) {
            const Channel& ch = kChannels[static_cast<size_t>(ci)];
            const float y = rowsTop + static_cast<float>(ci) * rowH;
            const TimelineCurve* curve = rows[static_cast<size_t>(ci)];
            const bool hasKeys = curve && !curve->keys.empty();
            if (ci % 2 == 0) tl->AddRectFilled({origin.x, y}, {origin.x + areaW, y + rowH}, Theme::U32(Theme::kPanel, 0.35f));
            tl->PushClipRect({origin.x, y}, {x0 - 2.0f, y + rowH}, true);
            tl->AddText({origin.x + 6.0f, y + (rowH - ImGui::GetFontSize()) * 0.5f}, ToU32(hasKeys ? Theme::kText : Theme::kTextDim), ch.label);
            tl->PopClipRect();
            ImGui::SetCursorScreenPos({origin.x, y});
            ImGui::PushID(ci);
            ImGui::InvisibleButton("##label", {labelW, rowH});
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s\n（ダブルクリックでこの行のキーをすべて選ぶ・右クリックでメニュー）", ch.help);
            if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) SelectAllKeys(rows, ksel, ci);
            if (ImGui::BeginPopupContextItem("row-menu")) {
                if (ImGui::MenuItem(ICON_FA_DIAMOND "  今の時刻にキーを打つ")) {
                    TimelineCurve& c = *rows[static_cast<size_t>(ci)];
                    const float v = ValueAt(anim, ch.property, cursor);
                    c.SetKey(cursor, v);
                    ksel.Clear();
                    ksel.Add(ci, cursor);
                    changed = true;
                }
                if (ImGui::MenuItem(ICON_FA_OBJECT_GROUP "  この行のキーをすべて選ぶ", nullptr, false, hasKeys)) SelectAllKeys(rows, ksel, ci);
                if (ImGui::MenuItem(ICON_FA_TRASH "  この行のキーをすべて消す", nullptr, false, hasKeys)) {
                    rows[static_cast<size_t>(ci)]->keys.clear();
                    ksel.Clear();
                    changed = true;
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
            if (hasKeys && curve->keys.size() >= 2) {
                float mn = 1e9f, mx = -1e9f;
                const float end = curve->GetEndTime();
                for (int k = 0; k <= 48; ++k) {
                    const float v = curve->Evaluate(end * static_cast<float>(k) / 48.0f);
                    mn = std::min(mn, v);
                    mx = std::max(mx, v);
                }
                if (mx - mn > 1e-5f) {
                    ImVec2 prev{};
                    for (int k = 0; k <= 48; ++k) {
                        const float t = end * static_cast<float>(k) / 48.0f;
                        const float v = curve->Evaluate(t);
                        const ImVec2 p{timeToX(t), y + rowH - 2.0f - (v - mn) / (mx - mn) * (rowH - 4.0f)};
                        if (k > 0) tl->AddLine(prev, p, Theme::U32(Theme::kAccent, 0.35f), 1.0f);
                        prev = p;
                    }
                }
            }
        }

        // キー
        KeyTimelineArgs args;
        args.rows = rows;
        args.rowH = rowH;
        args.pos = {x0, rowsTop};
        args.width = std::max(areaW - labelW, 1.0f);
        args.timeToX = timeToX;
        args.xToTime = xToTime;
        args.snap = snapTime;
        args.playhead = &m_TextAnimTime;
        args.playing = &m_TextAnimPlaying;
        args.scale = s;
        const KeyTimelineResult kr = KeyTimelineArea("##ta-keys", args, ksel);
        changed |= kr.changed;
        if (kr.doubleClick && kr.row >= 0 && kr.row < rowCount) {
            const char* property = kChannels[static_cast<size_t>(kr.row)].property;
            rows[static_cast<size_t>(kr.row)]->SetKey(kr.time, ValueAt(anim, property, kr.time));
            ksel.Clear();
            ksel.Add(kr.row, kr.time);
            changed = true;
        }
        if (kr.keyMenu || kr.emptyMenu) {
            if (kr.emptyMenu) m_TextAnimTime = kr.time;
            m_TextAnimPlaying = false;
            ImGui::OpenPopup("ta-key-menu");
        }
        KeyShortcut action = KeyShortcut::None;
        const bool canPaste = m_KeyClipboard.kind == "text" && !m_KeyClipboard.keys.empty();
        if (ImGui::BeginPopup("ta-key-menu")) {
            action = KeyMenuItems(rows, ksel, changed, canPaste);
            ImGui::EndPopup();
        }
        if (action == KeyShortcut::None && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) action = ReadKeyShortcut();
        const float pasteAt = revealSlot ? std::max(m_TextAnimTime, 0.0f) : cursor;
        auto rowOf = [&](const std::string& id) {
            const auto it = std::find(rowIds.begin(), rowIds.end(), id);
            return it == rowIds.end() ? -1 : static_cast<int>(it - rowIds.begin());
        };
        auto paste = [&](const KeyClipboard& clip, float at, bool toSelectedRow) {
            if (clip.kind != "text" || clip.keys.empty()) return;
            std::set<std::string> ids;
            for (const KeyClip& k : clip.keys) ids.insert(k.id);
            const KeyRef* p = ksel.Primary();
            const int fallback = toSelectedRow && ids.size() == 1 && p ? p->row : -1;
            changed |= PasteKeys(rows, clip, at, rowOf, fallback, ksel);
        };
        const float frame = 0.01f;
        switch (action) {
        case KeyShortcut::Copy: CopyKeys(rows, rowIds, ksel, m_KeyClipboard, "text"); break;
        case KeyShortcut::Cut:
            CopyKeys(rows, rowIds, ksel, m_KeyClipboard, "text");
            changed |= DeleteKeys(rows, ksel);
            break;
        case KeyShortcut::Paste: paste(m_KeyClipboard, pasteAt, true); break;
        case KeyShortcut::Duplicate: {
            float first = 0.0f, last = 0.0f;
            if (!SelectedRange(ksel, first, last)) break;
            KeyClipboard tmp;
            CopyKeys(rows, rowIds, ksel, tmp, "text");
            paste(tmp, std::abs(pasteAt - first) < 1e-3f ? last + 0.1f : pasteAt, false);
            break;
        }
        case KeyShortcut::Delete: changed |= DeleteKeys(rows, ksel); break;
        case KeyShortcut::SelectAll: SelectAllKeys(rows, ksel); break;
        case KeyShortcut::Deselect: ksel.Clear(); break;
        case KeyShortcut::NudgeLeft:
        case KeyShortcut::NudgeRight: {
            const float d = (action == KeyShortcut::NudgeLeft ? -frame : frame) * (io.KeyShift ? 10.0f : 1.0f);
            if (!ksel.Empty()) {
                changed |= MoveKeys(rows, ksel, d);
                if (const KeyRef* p = ksel.Primary()) m_TextAnimTime = p->time;
            } else {
                m_TextAnimTime = std::max(m_TextAnimTime + d, 0.0f);
            }
            m_TextAnimPlaying = false;
            break;
        }
        case KeyShortcut::PrevKey:
        case KeyShortcut::NextKey:
            m_TextAnimTime = AdjacentKeyTime(rows, cursor, action == KeyShortcut::NextKey ? 1 : -1);
            m_TextAnimPlaying = false;
            break;
        case KeyShortcut::Home:
            m_TextAnimTime = 0.0f;
            m_TextAnimPlaying = false;
            break;
        case KeyShortcut::End:
            m_TextAnimTime = total;
            m_TextAnimPlaying = false;
            break;
        default: break;
        }

        // 今の時刻の線
        const float rowsBottom = rowsTop + static_cast<float>(rowCount) * rowH;
        const float cx = timeToX(revealSlot ? std::min(cursor, std::max(len, cursor)) : cursor);
        tl->AddLine({cx, origin.y}, {cx, rowsBottom}, ToU32(Theme::kCoral), 1.5f);
        tl->AddLine({x0, origin.y}, {x0, rowsBottom}, Theme::U32(Theme::kTextDim, 0.4f));
        ImGui::SetCursorScreenPos({origin.x, rowsBottom});
        ImGui::Dummy({1.0f, 1.0f});
    }
    ImGui::EndChild();
    // Space で再生 / 一時停止、Shift+Space で逆再生
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space, false))
        play(io.KeyShift ? -1.0f : 1.0f);

    if (changed) {
        // 空の行は保存しない（JSON を短く）
        std::erase_if(anim.curves, [](const TimelineCurve& c) { return c.keys.empty(); });
        source = anim.IsEmpty() ? std::string() : anim.ToString();
        doc.Changed();
    }
}

} // namespace AquaTool
