// パネル: 整列（選んだウィジェットを揃える・等間隔にする・大きさを揃える・格子に並べる）
//
//   対象: キャンバスで Shift + クリック・階層で Ctrl / Shift + クリックして一緒に選んだウィジェット。
//         入れ物（Canvas Panel）を 1 つだけ選んでいるときは、その子すべて。
//   Canvas Panel の子だけが動かせる（Box の子は Box が並べるので、余白・揃えで調整する）。
//   位置はアンカー・ピボットを変えずにスロットの値を計算し直す（解像度を変えても同じ付き方のまま）。

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

namespace AquaTool {

using namespace Ruto::EditorUI;

namespace {

// 整列のボタンの絵（ImGui の文字のアイコンには無いので線と四角で描く）
enum class Glyph { Left, HCenter, Right, Top, VCenter, Bottom, DistributeH, DistributeV, SpaceH, SpaceV, SameW, SameH, Row, Column, Grid };

bool GlyphButton(const char* id, Glyph g, const char* tooltip, float size) {
    ImGui::PushID(id);
    const bool pressed = ImGui::Button("##g", {size, size});
    const bool hovered = ImGui::IsItemHovered();
    const bool disabled = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0;
    const ImVec2 mn = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float pad = size * 0.18f, s = size - pad * 2.0f;
    auto P = [&](float x, float y) { return ImVec2{mn.x + pad + x * s, mn.y + pad + y * s}; };
    const ImU32 fill = ToU32(disabled ? Theme::kTextDim : (hovered ? Theme::kAccent : Theme::kText));
    const ImU32 line = IM_COL32(240, 90, 90, disabled ? 120 : 255);
    auto box = [&](float x0, float y0, float x1, float y1) { dl->AddRectFilled(P(x0, y0), P(x1, y1), fill, 1.0f); };
    auto vline = [&](float x) { dl->AddLine(P(x, -0.05f), P(x, 1.05f), line, 1.5f); };
    auto hline = [&](float y) { dl->AddLine(P(-0.05f, y), P(1.05f, y), line, 1.5f); };
    switch (g) {
    case Glyph::Left: vline(0.05f); box(0.05f, 0.18f, 0.9f, 0.42f); box(0.05f, 0.58f, 0.6f, 0.82f); break;
    case Glyph::HCenter: vline(0.5f); box(0.1f, 0.18f, 0.9f, 0.42f); box(0.25f, 0.58f, 0.75f, 0.82f); break;
    case Glyph::Right: vline(0.95f); box(0.1f, 0.18f, 0.95f, 0.42f); box(0.4f, 0.58f, 0.95f, 0.82f); break;
    case Glyph::Top: hline(0.05f); box(0.18f, 0.05f, 0.42f, 0.9f); box(0.58f, 0.05f, 0.82f, 0.6f); break;
    case Glyph::VCenter: hline(0.5f); box(0.18f, 0.1f, 0.42f, 0.9f); box(0.58f, 0.25f, 0.82f, 0.75f); break;
    case Glyph::Bottom: hline(0.95f); box(0.18f, 0.1f, 0.42f, 0.95f); box(0.58f, 0.4f, 0.82f, 0.95f); break;
    case Glyph::DistributeH: box(0.0f, 0.25f, 0.18f, 0.75f); box(0.41f, 0.1f, 0.59f, 0.9f); box(0.82f, 0.3f, 1.0f, 0.7f); break;
    case Glyph::DistributeV: box(0.25f, 0.0f, 0.75f, 0.18f); box(0.1f, 0.41f, 0.9f, 0.59f); box(0.3f, 0.82f, 0.7f, 1.0f); break;
    case Glyph::SpaceH:
        box(0.0f, 0.2f, 0.3f, 0.8f); box(0.7f, 0.2f, 1.0f, 0.8f);
        dl->AddLine(P(0.36f, 0.5f), P(0.64f, 0.5f), line, 1.5f);
        dl->AddLine(P(0.36f, 0.35f), P(0.36f, 0.65f), line, 1.5f);
        dl->AddLine(P(0.64f, 0.35f), P(0.64f, 0.65f), line, 1.5f);
        break;
    case Glyph::SpaceV:
        box(0.2f, 0.0f, 0.8f, 0.3f); box(0.2f, 0.7f, 0.8f, 1.0f);
        dl->AddLine(P(0.5f, 0.36f), P(0.5f, 0.64f), line, 1.5f);
        dl->AddLine(P(0.35f, 0.36f), P(0.65f, 0.36f), line, 1.5f);
        dl->AddLine(P(0.35f, 0.64f), P(0.65f, 0.64f), line, 1.5f);
        break;
    case Glyph::SameW: vline(0.1f); vline(0.9f); box(0.1f, 0.15f, 0.9f, 0.4f); box(0.1f, 0.6f, 0.9f, 0.85f); break;
    case Glyph::SameH: hline(0.1f); hline(0.9f); box(0.15f, 0.1f, 0.4f, 0.9f); box(0.6f, 0.1f, 0.85f, 0.9f); break;
    case Glyph::Row: box(0.0f, 0.3f, 0.28f, 0.7f); box(0.36f, 0.3f, 0.64f, 0.7f); box(0.72f, 0.3f, 1.0f, 0.7f); break;
    case Glyph::Column: box(0.3f, 0.0f, 0.7f, 0.28f); box(0.3f, 0.36f, 0.7f, 0.64f); box(0.3f, 0.72f, 0.7f, 1.0f); break;
    case Glyph::Grid:
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 3; ++x) box(x * 0.37f, y * 0.37f, x * 0.37f + 0.26f, y * 0.37f + 0.26f);
        break;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", tooltip);
    ImGui::PopID();
    return pressed;
}

// 動かす 1 つ分（文書のウィジェットと、レイアウトの済んだ表示用の複製）
struct Item {
    Widget* asset = nullptr;
    Widget* view = nullptr;
    UI::Rect rect;    // 今の矩形（基準解像度の単位・キャンバス全体の座標）
    UI::Rect parent;  // 親の矩形
};

} // namespace

// ---------------------------------------------------------------------------
// 一緒に選ぶ
// ---------------------------------------------------------------------------

void App::ToggleExtraSelection(Document& doc, const WidgetPath& path) {
    Widget* w = Resolve(doc.asset->root.get(), path);
    if (!w || !w->GetParent() || w->name.empty()) return;
    DesignerState& ui = doc.ui;
    Widget* primary = ui.hasSelection ? Resolve(doc.asset->root.get(), ui.selection) : nullptr;
    if (!primary || !primary->GetParent()) {
        // まだ何も選んでいなければ、普通に選ぶ
        ui.selection = path;
        ui.hasSelection = true;
        ui.extraSelection.clear();
        return;
    }
    if (primary == w) {
        // 最初に選んだものを外したら、一緒に選んでいた次のものが最初になる
        if (ui.extraSelection.empty()) {
            ui.hasSelection = false;
            return;
        }
        Widget* next = doc.asset->root->Find(ui.extraSelection.front());
        ui.extraSelection.erase(ui.extraSelection.begin());
        if (next) ui.selection = PathOf(next);
        return;
    }
    auto it = std::find(ui.extraSelection.begin(), ui.extraSelection.end(), w->name);
    if (it != ui.extraSelection.end()) ui.extraSelection.erase(it);
    else ui.extraSelection.push_back(w->name);
}

std::vector<Widget*> App::ArrangeTargets(Document& doc, bool* children) {
    if (children) *children = false;
    std::vector<Widget*> out;
    WidgetAsset& asset = *doc.asset;
    Widget* primary = doc.ui.hasSelection ? Resolve(asset.root.get(), doc.ui.selection) : nullptr;
    if (!primary) return out;
    if (doc.ui.extraSelection.empty() && dynamic_cast<UICanvasPanel*>(primary)) {
        // 入れ物だけを選んでいる → その子すべて
        for (const auto& c : primary->children) {
            if (!c->generated) out.push_back(c.get());
        }
        if (children) *children = true;
        return out;
    }
    if (IsCanvasChild(*primary)) out.push_back(primary);
    for (const std::string& name : doc.ui.extraSelection) {
        Widget* w = asset.root->Find(name);
        if (w && w != primary && IsCanvasChild(*w) && std::find(out.begin(), out.end(), w) == out.end()) out.push_back(w);
    }
    return out;
}

// ---------------------------------------------------------------------------
// パネル
// ---------------------------------------------------------------------------

void App::DrawArrangePanel(Document& doc) {
    WidgetAsset& asset = *doc.asset;
    DesignerState& ui = doc.ui;
    Widget* docRoot = doc.view ? doc.view->GetRoot() : nullptr;
    bool childrenMode = false;
    const std::vector<Widget*> targets = ArrangeTargets(doc, &childrenMode);
    Widget* primary = ui.hasSelection ? Resolve(asset.root.get(), ui.selection) : nullptr;

    // ---- 対象 ----
    ImGui::PushTextWrapPos(0.0f);
    if (!primary) {
        ImGui::TextDisabled(ICON_FA_ARROW_POINTER "  ウィジェットを選んでください");
    } else if (childrenMode) {
        ImGui::TextColored(Theme::kAccent, ICON_FA_OBJECT_GROUP "  %s の子 %d 個を並べます", primary->name.c_str(), static_cast<int>(targets.size()));
    } else {
        ImGui::TextColored(Theme::kAccent, ICON_FA_ARROW_POINTER "  %d 個を選んでいます", static_cast<int>(targets.size()));
    }
    ImGui::TextDisabled("キャンバスで Shift + クリック・階層で Ctrl + クリックで一緒に選べます。入れ物（Canvas Panel）だけを選ぶと、その子すべてが対象です。");
    ImGui::PopTextWrapPos();
    if (primary && primary->GetParent()) {
        if (ImGui::SmallButton(ICON_FA_OBJECT_GROUP " 兄弟をすべて選ぶ")) {
            ui.extraSelection.clear();
            for (const auto& c : primary->GetParent()->children) {
                if (c.get() != primary && !c->generated && !c->name.empty()) ui.extraSelection.push_back(c->name);
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("同じ親の中のウィジェットをまとめて選ぶ");
        ImGui::SameLine();
    }
    ImGui::BeginDisabled(ui.extraSelection.empty());
    if (ImGui::SmallButton(ICON_FA_XMARK " 1 つだけにする")) ui.extraSelection.clear();
    ImGui::EndDisabled();
    if (primary && primary->GetParent() && !IsCanvasChild(*primary) && !childrenMode) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(Theme::kWarning, ICON_FA_CIRCLE_INFO "  %s の子は %s が並べます。詳細の「余白・揃え」で調整してください。",
                           TypeLabel(primary->GetParent()->GetTypeName()), TypeLabel(primary->GetParent()->GetTypeName()));
        ImGui::PopTextWrapPos();
    }

    // 動かせるもの（表示用の複製で今の矩形を測る）
    std::vector<Item> items;
    if (docRoot) {
        for (Widget* a : targets) {
            if (!IsCanvasChild(*a)) continue;
            Widget* v = Resolve(docRoot, PathOf(a));
            if (!v || !v->GetParent()) continue;
            items.push_back({a, v, v->GetRect(), v->GetParent()->GetRect()});
        }
    }
    const int n = static_cast<int>(items.size());
    Item* key = nullptr; // 基準（最初に選んだもの）
    for (Item& it : items) {
        if (it.asset == primary) key = &it;
    }

    auto apply = [&](Item& it, UI::Rect r, bool resized) {
        WidgetSlot slot = it.asset->slot;
        if (resized) slot.autoSize = false;
        if (!resized && slot.autoSize) {
            r.w = it.rect.w;
            r.h = it.rect.h;
        }
        SetSlotRect(slot, it.parent, r);
        it.asset->slot = slot;
        it.rect = r;
    };
    UI::Rect bounds{};
    if (n > 0) {
        float x0 = items[0].rect.x, y0 = items[0].rect.y, x1 = items[0].rect.Right(), y1 = items[0].rect.Bottom();
        for (const Item& it : items) {
            x0 = std::min(x0, it.rect.x);
            y0 = std::min(y0, it.rect.y);
            x1 = std::max(x1, it.rect.Right());
            y1 = std::max(y1, it.rect.Bottom());
        }
        bounds = {x0, y0, x1 - x0, y1 - y0};
    }
    // 揃える先（0: 選んだものの範囲 / 1: 親 / 2: 最初に選んだもの）。1 つだけ・子すべてのときは親
    int relative = m_ArrangeRelative;
    if (relative == 0 && (n < 2)) relative = 1;
    if (relative == 2 && (!key || childrenMode)) relative = childrenMode ? 1 : 0;
    auto refFor = [&](const Item& it) { return relative == 1 ? it.parent : relative == 2 ? key->rect : bounds; };

    const float bs = std::round(ImGui::GetFrameHeight() * 1.45f);
    const float gap = 3.0f * m_Scale;
    bool changed = false;

    // ---- 揃える ----
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kAccent);
    ImGui::SeparatorText("揃える");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-FLT_MIN);
    const char* relatives[] = {"選んだものの範囲に揃える", "親に揃える", "最初に選んだものに揃える"};
    ImGui::Combo("##relative", &m_ArrangeRelative, relatives, IM_ARRAYSIZE(relatives));
    ImGui::BeginDisabled(n == 0);
    struct AlignDef {
        const char* id;
        Glyph glyph;
        const char* tip;
    };
    const AlignDef aligns[] = {
        {"l", Glyph::Left, "左を揃える"},     {"hc", Glyph::HCenter, "横の中央を揃える"}, {"r", Glyph::Right, "右を揃える"},
        {"t", Glyph::Top, "上を揃える"},       {"vc", Glyph::VCenter, "縦の中央を揃える"}, {"b", Glyph::Bottom, "下を揃える"},
    };
    for (int i = 0; i < 6; ++i) {
        if (i > 0) ImGui::SameLine(0.0f, i == 3 ? gap * 4.0f : gap);
        if (GlyphButton(aligns[i].id, aligns[i].glyph, aligns[i].tip, bs)) {
            for (Item& it : items) {
                if (relative == 2 && &it == key) continue;
                const UI::Rect ref = refFor(it);
                UI::Rect r = it.rect;
                switch (i) {
                case 0: r.x = ref.x; break;
                case 1: r.x = ref.x + (ref.w - r.w) * 0.5f; break;
                case 2: r.x = ref.Right() - r.w; break;
                case 3: r.y = ref.y; break;
                case 4: r.y = ref.y + (ref.h - r.h) * 0.5f; break;
                case 5: r.y = ref.Bottom() - r.h; break;
                }
                apply(it, r, false);
            }
            changed = true;
        }
    }
    ImGui::EndDisabled();

    // ---- 間隔 ----
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kAccent);
    ImGui::SeparatorText("間隔・大きさ");
    ImGui::PopStyleColor();
    // 等間隔（両端は動かさず、間を同じにする）
    auto distribute = [&](bool horizontal) {
        std::vector<Item*> order;
        for (Item& it : items) order.push_back(&it);
        std::sort(order.begin(), order.end(), [&](const Item* a, const Item* b) {
            return horizontal ? a->rect.x + a->rect.w * 0.5f < b->rect.x + b->rect.w * 0.5f : a->rect.y + a->rect.h * 0.5f < b->rect.y + b->rect.h * 0.5f;
        });
        float start = horizontal ? bounds.x : bounds.y, end = horizontal ? bounds.Right() : bounds.Bottom();
        if (relative == 1) {
            const UI::Rect p = order.front()->parent;
            start = horizontal ? p.x : p.y;
            end = horizontal ? p.Right() : p.Bottom();
        }
        float total = 0.0f;
        for (const Item* it : order) total += horizontal ? it->rect.w : it->rect.h;
        const float space = (end - start - total) / static_cast<float>(std::max(1, n - 1));
        float pos = start;
        for (Item* it : order) {
            UI::Rect r = it->rect;
            (horizontal ? r.x : r.y) = pos;
            pos += (horizontal ? r.w : r.h) + space;
            apply(*it, r, false);
        }
    };
    // 決めた間隔で詰めて並べる（並びの先頭は動かさない）
    auto pack = [&](bool horizontal) {
        std::vector<Item*> order;
        for (Item& it : items) order.push_back(&it);
        std::sort(order.begin(), order.end(), [&](const Item* a, const Item* b) { return horizontal ? a->rect.x < b->rect.x : a->rect.y < b->rect.y; });
        float pos = horizontal ? order.front()->rect.x : order.front()->rect.y;
        for (Item* it : order) {
            UI::Rect r = it->rect;
            (horizontal ? r.x : r.y) = pos;
            pos += (horizontal ? r.w : r.h) + m_ArrangeSpacing;
            apply(*it, r, false);
        }
    };
    // 大きさを揃える（最初に選んだもの、なければ一番大きいもの）
    auto sameSize = [&](bool width) {
        float size = 0.0f;
        if (key && !childrenMode) size = width ? key->rect.w : key->rect.h;
        else
            for (const Item& it : items) size = std::max(size, width ? it.rect.w : it.rect.h);
        for (Item& it : items) {
            UI::Rect r = it.rect;
            // 中心を保って大きさだけ変える
            if (width) {
                r.x += (r.w - size) * 0.5f;
                r.w = size;
            } else {
                r.y += (r.h - size) * 0.5f;
                r.h = size;
            }
            apply(it, r, true);
        }
    };
    ImGui::BeginDisabled(n < (relative == 1 ? 2 : 3));
    if (GlyphButton("dh", Glyph::DistributeH, "横に等間隔に並べる（両端はそのまま。3 個から）", bs)) {
        distribute(true);
        changed = true;
    }
    ImGui::SameLine(0.0f, gap);
    if (GlyphButton("dv", Glyph::DistributeV, "縦に等間隔に並べる（両端はそのまま。3 個から）", bs)) {
        distribute(false);
        changed = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, gap * 4.0f);
    ImGui::BeginDisabled(n < 2);
    if (GlyphButton("sh", Glyph::SpaceH, "決めた間隔で横に詰めて並べる", bs)) {
        pack(true);
        changed = true;
    }
    ImGui::SameLine(0.0f, gap);
    if (GlyphButton("sv", Glyph::SpaceV, "決めた間隔で縦に詰めて並べる", bs)) {
        pack(false);
        changed = true;
    }
    ImGui::SameLine(0.0f, gap);
    ImGui::SetNextItemWidth(std::max(60.0f * m_Scale, ImGui::GetContentRegionAvail().x - (bs + gap) * 2.0f - gap * 4.0f));
    ImGui::DragFloat("##spacing", &m_ArrangeSpacing, 0.5f, -10000.0f, 10000.0f, "間隔 %.0f");
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, gap * 4.0f);
    ImGui::BeginDisabled(n < 2);
    if (GlyphButton("sw", Glyph::SameW, "幅を揃える（最初に選んだもの、なければ一番大きいものに合わせる）", bs)) {
        sameSize(true);
        changed = true;
    }
    ImGui::SameLine(0.0f, gap);
    if (GlyphButton("sz", Glyph::SameH, "高さを揃える（最初に選んだもの、なければ一番大きいものに合わせる）", bs)) {
        sameSize(false);
        changed = true;
    }
    ImGui::EndDisabled();

    // ---- 自動で並べる（格子） ----
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kAccent);
    ImGui::SeparatorText("自動で並べる");
    ImGui::PopStyleColor();
    int layout = -1; // 押されたら 0: 横一列 / 1: 縦一列 / 2: 格子
    ImGui::BeginDisabled(n < 1);
    if (GlyphButton("row", Glyph::Row, "横一列に並べる", bs)) layout = 0;
    ImGui::SameLine(0.0f, gap);
    if (GlyphButton("col", Glyph::Column, "縦一列に並べる", bs)) layout = 1;
    ImGui::SameLine(0.0f, gap);
    if (GlyphButton("grid", Glyph::Grid, "列の数で折り返して格子に並べる", bs)) layout = 2;
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, gap * 4.0f);
    ImGui::SetNextItemWidth(std::max(70.0f * m_Scale, ImGui::GetContentRegionAvail().x));
    if (ImGui::DragInt("##columns", &m_ArrangeColumns, 0.1f, 1, 64, "%d 列")) m_ArrangeColumns = std::clamp(m_ArrangeColumns, 1, 64);
    if (BeginPropertyTable("arrange-grid")) {
        PropertyLabel("間隔（横・縦）");
        DragVec("##gap", m_ArrangeGap, 2, 0.5f, 16.0f, "%.0f");
        PropertyLabel("並べる順");
        const char* orders[] = {"今の位置の順（左上から）", "階層の順"};
        ImGui::Combo("##order", &m_ArrangeOrder, orders, IM_ARRAYSIZE(orders));
        PropertyLabel("置く場所");
        const char* origins[] = {"今の範囲の左上から", "親の左上から（余白を空ける）", "親の中央にまとめる"};
        ImGui::Combo("##origin", &m_ArrangeOrigin, origins, IM_ARRAYSIZE(origins));
        if (m_ArrangeOrigin == 1) {
            PropertyLabel("親からの余白");
            ImGui::DragFloat("##margin", &m_ArrangeMargin, 0.5f, 0.0f, 10000.0f, "%.0f");
        }
        PropertyLabel("ます目の大きさ");
        const char* cells[] = {"そのまま（ます目の中央に置く）", "一番大きいものに揃える"};
        ImGui::Combo("##cell", &m_ArrangeCellSize, cells, IM_ARRAYSIZE(cells));
        EndPropertyTable();
    }
    if (layout >= 0 && n > 0) {
        std::vector<Item*> order;
        for (Item& it : items) order.push_back(&it);
        if (m_ArrangeOrder == 0) {
            // 上から行ごと、行の中は左から（高さの半分までのずれは同じ行とみなす）
            float minH = order.front()->rect.h;
            for (const Item* it : order) minH = std::min(minH, it->rect.h);
            const float band = std::max(minH * 0.5f, 1.0f);
            std::stable_sort(order.begin(), order.end(), [&](const Item* a, const Item* b) {
                const float ra = std::floor((a->rect.y + a->rect.h * 0.5f) / band), rb = std::floor((b->rect.y + b->rect.h * 0.5f) / band);
                return ra != rb ? ra < rb : a->rect.x < b->rect.x;
            });
        } else {
            std::stable_sort(order.begin(), order.end(), [](const Item* a, const Item* b) {
                const WidgetPath pa = PathOf(a->asset), pb = PathOf(b->asset);
                return std::lexicographical_compare(pa.begin(), pa.end(), pb.begin(), pb.end());
            });
        }
        const int cols = layout == 0 ? n : layout == 1 ? 1 : std::clamp(m_ArrangeColumns, 1, n);
        const int rows = (n + cols - 1) / cols;
        float maxW = 0.0f, maxH = 0.0f;
        for (const Item* it : order) {
            maxW = std::max(maxW, it->rect.w);
            maxH = std::max(maxH, it->rect.h);
        }
        const bool uniform = m_ArrangeCellSize == 1;
        // 列の幅・行の高さ（その列・行で一番大きいもの）
        std::vector<float> colW(static_cast<size_t>(cols), 0.0f), rowH(static_cast<size_t>(rows), 0.0f);
        for (int i = 0; i < n; ++i) {
            const Item* it = order[static_cast<size_t>(i)];
            float& cw = colW[static_cast<size_t>(i % cols)];
            float& rh = rowH[static_cast<size_t>(i / cols)];
            cw = std::max(cw, uniform ? maxW : it->rect.w);
            rh = std::max(rh, uniform ? maxH : it->rect.h);
        }
        float totalW = m_ArrangeGap[0] * static_cast<float>(cols - 1), totalH = m_ArrangeGap[1] * static_cast<float>(rows - 1);
        for (float w : colW) totalW += w;
        for (float h : rowH) totalH += h;
        const UI::Rect parent = order.front()->parent;
        Vec2 origin{bounds.x, bounds.y};
        if (m_ArrangeOrigin == 1) origin = {parent.x + m_ArrangeMargin, parent.y + m_ArrangeMargin};
        if (m_ArrangeOrigin == 2) origin = {parent.x + (parent.w - totalW) * 0.5f, parent.y + (parent.h - totalH) * 0.5f};
        float y = origin.y;
        for (int row = 0; row < rows; ++row) {
            float x = origin.x;
            for (int col = 0; col < cols; ++col) {
                const int i = row * cols + col;
                if (i >= n) break;
                Item* it = order[static_cast<size_t>(i)];
                const float cw = colW[static_cast<size_t>(col)], rh = rowH[static_cast<size_t>(row)];
                UI::Rect r = it->rect;
                if (uniform) {
                    r.w = maxW;
                    r.h = maxH;
                }
                r.x = x + (cw - r.w) * 0.5f;
                r.y = y + (rh - r.h) * 0.5f;
                apply(*it, r, uniform);
                x += cw + m_ArrangeGap[0];
            }
            y += rowH[static_cast<size_t>(row)] + m_ArrangeGap[1];
        }
        changed = true;
    }
    if (changed) {
        doc.Changed();
        Notify(std::format("{} 個を並べました（Ctrl+Z で元に戻せます）", n));
    }
}

} // namespace AquaTool
