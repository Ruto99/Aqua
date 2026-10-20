// タイムラインのキーの編集（ウィジェットのアニメーション・文字アニメーションで共通）

#include "AquaKeys.h"

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <set>

namespace AquaTool {

namespace {

constexpr float kSame = 1e-4f; // 同じ時刻とみなす差

int FindAt(const TimelineCurve& c, float time) {
    for (size_t i = 0; i < c.keys.size(); ++i) {
        if (std::abs(c.keys[i].time - time) < kSame) return static_cast<int>(i);
    }
    return -1;
}

// time にキーを入れる（同じ時刻にあれば置き換える）
void PutKey(TimelineCurve& c, TimelineKey k) {
    k.time = std::max(k.time, 0.0f);
    if (const int i = FindAt(c, k.time); i >= 0) c.keys[static_cast<size_t>(i)] = k;
    else c.keys.push_back(k);
    std::stable_sort(c.keys.begin(), c.keys.end(), [](const TimelineKey& a, const TimelineKey& b) { return a.time < b.time; });
}

bool EraseAt(TimelineCurve& c, float time) {
    if (const int i = FindAt(c, time); i >= 0) {
        c.keys.erase(c.keys.begin() + i);
        return true;
    }
    return false;
}

TimelineCurve* RowOf(std::span<TimelineCurve* const> rows, int row) {
    return row >= 0 && row < static_cast<int>(rows.size()) ? rows[static_cast<size_t>(row)] : nullptr;
}

void DrawDiamond(ImDrawList* dl, ImVec2 c, float r, ImU32 fill) {
    dl->AddQuadFilled({c.x, c.y - r}, {c.x + r, c.y}, {c.x, c.y + r}, {c.x - r, c.y}, fill);
    dl->AddQuad({c.x, c.y - r}, {c.x + r, c.y}, {c.x, c.y + r}, {c.x - r, c.y}, IM_COL32(20, 20, 24, 255));
}

} // namespace

// ---------------------------------------------------------------------------
// 選択
// ---------------------------------------------------------------------------

bool KeySelection::Has(int row, float time) const {
    return std::any_of(keys.begin(), keys.end(), [&](const KeyRef& k) { return k.row == row && std::abs(k.time - time) < kSame; });
}

void KeySelection::Add(int row, float time) {
    Remove(row, time);
    keys.push_back({row, time});
}

void KeySelection::Remove(int row, float time) {
    std::erase_if(keys, [&](const KeyRef& k) { return k.row == row && std::abs(k.time - time) < kSame; });
}

// ---------------------------------------------------------------------------
// 操作
// ---------------------------------------------------------------------------

TimelineKey* FindSelectedKey(std::span<TimelineCurve* const> rows, const KeyRef& ref) {
    TimelineCurve* c = RowOf(rows, ref.row);
    if (!c) return nullptr;
    const int i = FindAt(*c, ref.time);
    return i >= 0 ? &c->keys[static_cast<size_t>(i)] : nullptr;
}

void SortRow(TimelineCurve& c) {
    std::stable_sort(c.keys.begin(), c.keys.end(), [](const TimelineKey& a, const TimelineKey& b) { return a.time < b.time; });
    for (size_t i = 1; i < c.keys.size();) {
        if (std::abs(c.keys[i].time - c.keys[i - 1].time) < kSame) c.keys.erase(c.keys.begin() + static_cast<std::ptrdiff_t>(i - 1));
        else ++i;
    }
}

bool DeleteKeys(std::span<TimelineCurve* const> rows, KeySelection& sel) {
    bool changed = false;
    for (const KeyRef& k : sel.keys) {
        if (TimelineCurve* c = RowOf(rows, k.row)) changed |= EraseAt(*c, k.time);
    }
    sel.Clear();
    return changed;
}

bool SelectedRange(const KeySelection& sel, float& first, float& last) {
    if (sel.keys.empty()) return false;
    first = 1e30f;
    last = -1e30f;
    for (const KeyRef& k : sel.keys) {
        first = std::min(first, k.time);
        last = std::max(last, k.time);
    }
    return true;
}

void CopyKeys(std::span<TimelineCurve* const> rows, std::span<const std::string> ids, const KeySelection& sel, KeyClipboard& clip, const std::string& kind) {
    float first = 0.0f, last = 0.0f;
    if (!SelectedRange(sel, first, last)) return;
    clip.kind = kind;
    clip.keys.clear();
    for (const KeyRef& ref : sel.keys) {
        const TimelineKey* k = FindSelectedKey(rows, ref);
        if (!k || ref.row < 0 || ref.row >= static_cast<int>(ids.size())) continue;
        TimelineKey copy = *k;
        copy.time -= first;
        clip.keys.push_back({ids[static_cast<size_t>(ref.row)], copy});
    }
}

bool PasteKeys(std::span<TimelineCurve* const> rows, const KeyClipboard& clip, float at, const std::function<int(const std::string&)>& rowOf, int fallbackRow,
               KeySelection& sel) {
    std::set<std::string> ids;
    for (const KeyClip& k : clip.keys) ids.insert(k.id);
    const bool oneRow = ids.size() == 1 && fallbackRow >= 0;
    KeySelection pasted;
    for (const KeyClip& k : clip.keys) {
        const int row = oneRow ? fallbackRow : rowOf(k.id);
        TimelineCurve* c = RowOf(rows, row);
        if (!c) continue;
        TimelineKey key = k.key;
        key.time = std::max(at + key.time, 0.0f);
        PutKey(*c, key);
        pasted.Add(row, key.time);
    }
    if (pasted.Empty()) return false;
    sel.keys = pasted.keys;
    return true;
}

bool MoveKeys(std::span<TimelineCurve* const> rows, KeySelection& sel, float delta) {
    float first = 0.0f, last = 0.0f;
    if (!SelectedRange(sel, first, last)) return false;
    delta = std::max(delta, -first);
    if (std::abs(delta) < 1e-6f) return false;
    std::vector<std::pair<KeyRef, TimelineKey>> moving;
    for (const KeyRef& ref : sel.keys) {
        if (const TimelineKey* k = FindSelectedKey(rows, ref)) moving.push_back({ref, *k});
    }
    for (const auto& [ref, k] : moving) EraseAt(*rows[static_cast<size_t>(ref.row)], ref.time);
    sel.keys.clear();
    for (auto [ref, k] : moving) {
        k.time = std::max(k.time + delta, 0.0f);
        PutKey(*rows[static_cast<size_t>(ref.row)], k);
        sel.keys.push_back({ref.row, k.time});
    }
    return true;
}

bool ReverseKeys(std::span<TimelineCurve* const> rows, KeySelection& sel) {
    float first = 0.0f, last = 0.0f;
    if (!SelectedRange(sel, first, last) || last - first < kSame) return false;
    std::vector<std::pair<KeyRef, TimelineKey>> moving;
    for (const KeyRef& ref : sel.keys) {
        if (const TimelineKey* k = FindSelectedKey(rows, ref)) moving.push_back({ref, *k});
    }
    for (const auto& [ref, k] : moving) EraseAt(*rows[static_cast<size_t>(ref.row)], ref.time);
    // 補間は「次のキーまでのつなぎ方」なので、行ごとに 1 つずつずらして付け直す
    std::sort(moving.begin(), moving.end(), [](const auto& a, const auto& b) { return a.first.row != b.first.row ? a.first.row < b.first.row : a.second.time < b.second.time; });
    std::vector<std::pair<KeyRef, TimelineKey>> flipped = moving;
    for (size_t i = 0; i < moving.size(); ++i) {
        flipped[i].second.time = first + last - moving[i].second.time;
        const bool hasPrev = i > 0 && moving[i - 1].first.row == moving[i].first.row;
        flipped[i].second.interpolation = hasPrev ? moving[i - 1].second.interpolation : moving[i].second.interpolation;
    }
    sel.keys.clear();
    for (const auto& [ref, k] : flipped) {
        PutKey(*rows[static_cast<size_t>(ref.row)], k);
        sel.keys.push_back({ref.row, k.time});
    }
    return true;
}

bool SetKeysInterpolation(std::span<TimelineCurve* const> rows, const KeySelection& sel, KeyInterpolation interpolation) {
    bool changed = false;
    for (const KeyRef& ref : sel.keys) {
        if (TimelineKey* k = FindSelectedKey(rows, ref); k && k->interpolation != interpolation) {
            k->interpolation = interpolation;
            changed = true;
        }
    }
    return changed;
}

void SelectAllKeys(std::span<TimelineCurve* const> rows, KeySelection& sel, int onlyRow) {
    sel.Clear();
    for (int r = 0; r < static_cast<int>(rows.size()); ++r) {
        if (onlyRow >= 0 && r != onlyRow) continue;
        if (const TimelineCurve* c = rows[static_cast<size_t>(r)]) {
            for (const TimelineKey& k : c->keys) sel.keys.push_back({r, k.time});
        }
    }
}

float AdjacentKeyTime(std::span<TimelineCurve* const> rows, float t, int dir) {
    float best = t;
    float bestDist = 1e30f;
    for (const TimelineCurve* c : rows) {
        if (!c) continue;
        for (const TimelineKey& k : c->keys) {
            const float d = (k.time - t) * static_cast<float>(dir);
            if (d > kSame && d < bestDist) {
                bestDist = d;
                best = k.time;
            }
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// キーの範囲（描く・選ぶ・ドラッグ・囲んで選ぶ）
// ---------------------------------------------------------------------------

KeyTimelineResult KeyTimelineArea(const char* id, const KeyTimelineArgs& a, KeySelection& sel) {
    const ImGuiIO& io = ImGui::GetIO();
    KeyTimelineResult result;
    const int n = static_cast<int>(a.rows.size());
    const float height = std::max(a.rowH * static_cast<float>(n), 1.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::SetCursorScreenPos(a.pos);
    ImGui::InvisibleButton(id, {std::max(a.width, 1.0f), height}, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    const float pick = 6.0f * a.scale;
    auto rowAt = [&](float y) { return std::clamp(static_cast<int>(std::floor((y - a.pos.y) / a.rowH)), 0, std::max(n - 1, 0)); };

    // マウスの下のキー
    int hoverRow = -1;
    float hoverTime = 0.0f;
    if (hovered && n > 0) {
        const int r = rowAt(io.MousePos.y);
        if (const TimelineCurve* c = a.rows[static_cast<size_t>(r)]) {
            float bestDist = pick;
            for (const TimelineKey& k : c->keys) {
                const float d = std::abs(io.MousePos.x - a.timeToX(k.time));
                if (d <= bestDist) {
                    bestDist = d;
                    hoverRow = r;
                    hoverTime = k.time;
                }
            }
        }
    }

    // 押した
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        const bool additive = io.KeyCtrl || io.KeyShift;
        if (hoverRow >= 0) {
            if (additive) {
                if (sel.Has(hoverRow, hoverTime)) sel.Remove(hoverRow, hoverTime);
                else sel.Add(hoverRow, hoverTime);
            } else if (!sel.Has(hoverRow, hoverTime)) {
                sel.Clear();
                sel.Add(hoverRow, hoverTime);
            } else {
                sel.Add(hoverRow, hoverTime); // 今のキーにする
            }
            sel.dragging = sel.Has(hoverRow, hoverTime);
            sel.dragAnchor = a.snap(a.xToTime(io.MousePos.x));
            sel.dragDelta = 0.0f;
            sel.dragKeys.clear();
            for (const KeyRef& ref : sel.keys) {
                if (const TimelineKey* k = FindSelectedKey(a.rows, ref)) sel.dragKeys.push_back({ref, *k});
            }
            if (a.playhead) *a.playhead = hoverTime;
            if (a.playing) *a.playing = false;
        } else {
            sel.boxing = true;
            sel.boxAdd = additive;
            sel.boxStart = io.MousePos;
        }
    }
    // 空いた所のダブルクリック（キーを打つ）
    if (hovered && hoverRow < 0 && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && n > 0) {
        result.doubleClick = true;
        result.row = rowAt(io.MousePos.y);
        result.time = a.snap(a.xToTime(io.MousePos.x));
        sel.boxing = false;
    }
    // 選んだキーをまとめてドラッグ
    if (sel.dragging && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f) && !sel.dragKeys.empty()) {
        float first = 1e30f;
        for (const auto& [ref, k] : sel.dragKeys) first = std::min(first, k.time);
        const float delta = std::max(a.snap(a.xToTime(io.MousePos.x)) - sel.dragAnchor, -first);
        if (std::abs(delta - sel.dragDelta) > 1e-6f) {
            for (const auto& [ref, k] : sel.dragKeys) {
                if (TimelineCurve* c = RowOf(a.rows, ref.row)) EraseAt(*c, k.time + sel.dragDelta);
            }
            sel.keys.clear();
            for (const auto& [ref, k] : sel.dragKeys) {
                TimelineCurve* c = RowOf(a.rows, ref.row);
                if (!c) continue;
                TimelineKey moved = k;
                moved.time = std::max(k.time + delta, 0.0f);
                PutKey(*c, moved);
                sel.keys.push_back({ref.row, moved.time});
            }
            sel.dragDelta = delta;
            if (a.playhead && !sel.keys.empty()) *a.playhead = sel.keys.back().time;
            result.changed = true;
        }
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) sel.dragging = false;
    // 囲んで選ぶ（動かさずに離したら、その時刻へ）
    if (sel.boxing) {
        const ImVec2 p0 = sel.boxStart, p1 = io.MousePos;
        const bool moved = std::abs(p1.x - p0.x) > 3.0f || std::abs(p1.y - p0.y) > 3.0f;
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (moved) {
                const ImVec2 mn{std::min(p0.x, p1.x), std::min(p0.y, p1.y)}, mx{std::max(p0.x, p1.x), std::max(p0.y, p1.y)};
                dl->AddRectFilled(mn, mx, Theme::U32(Theme::kAccent, 0.15f));
                dl->AddRect(mn, mx, Theme::U32(Theme::kAccent, 0.8f));
            }
        } else {
            if (!moved) {
                if (!sel.boxAdd) sel.Clear();
                if (a.playhead) *a.playhead = a.snap(a.xToTime(p0.x));
                if (a.playing) *a.playing = false;
            } else {
                if (!sel.boxAdd) sel.Clear();
                const float t0 = a.xToTime(std::min(p0.x, p1.x)), t1 = a.xToTime(std::max(p0.x, p1.x));
                const float y0 = std::min(p0.y, p1.y), y1 = std::max(p0.y, p1.y);
                for (int r = 0; r < n; ++r) {
                    const float ry0 = a.pos.y + static_cast<float>(r) * a.rowH, ry1 = ry0 + a.rowH;
                    if (ry1 < y0 || ry0 > y1) continue;
                    if (const TimelineCurve* c = a.rows[static_cast<size_t>(r)]) {
                        for (const TimelineKey& k : c->keys) {
                            if (k.time >= t0 - kSame && k.time <= t1 + kSame && !sel.Has(r, k.time)) sel.keys.push_back({r, k.time});
                        }
                    }
                }
            }
            sel.boxing = false;
        }
    }
    // 右クリック
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && n > 0) {
        result.row = hoverRow >= 0 ? hoverRow : rowAt(io.MousePos.y);
        result.time = hoverRow >= 0 ? hoverTime : a.snap(a.xToTime(io.MousePos.x));
        if (hoverRow >= 0) {
            if (!sel.Has(hoverRow, hoverTime)) sel.Clear();
            sel.Add(hoverRow, hoverTime);
            result.keyMenu = true;
        } else {
            result.emptyMenu = true;
        }
    }

    // 描く
    dl->PushClipRect(a.pos, {a.pos.x + a.width, a.pos.y + height}, true);
    for (int r = 0; r < n; ++r) {
        const TimelineCurve* c = a.rows[static_cast<size_t>(r)];
        if (!c) continue;
        const float cy = a.pos.y + (static_cast<float>(r) + 0.5f) * a.rowH;
        const float radius = std::min(5.0f * a.scale, a.rowH * 0.36f);
        for (const TimelineKey& k : c->keys) {
            const bool isHover = r == hoverRow && std::abs(k.time - hoverTime) < kSame;
            const bool isSel = sel.Has(r, k.time);
            DrawDiamond(dl, {a.timeToX(k.time), cy}, isHover ? radius * 1.25f : radius, isSel ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 200, 80, 255));
        }
    }
    dl->PopClipRect();
    if (hoverRow >= 0 && !sel.dragging) {
        if (const TimelineCurve* c = a.rows[static_cast<size_t>(hoverRow)]) {
            const int i = FindAt(*c, hoverTime);
            if (i >= 0) ImGui::SetTooltip("%.3f 秒: %.3f\n（Ctrl / Shift + クリックで選ぶものを足す・ドラッグで動かす・右クリックでメニュー）", hoverTime, c->keys[static_cast<size_t>(i)].value);
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// ショートカット・メニュー
// ---------------------------------------------------------------------------

KeyShortcut ReadKeyShortcut() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || ImGui::IsAnyItemActive() || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) return KeyShortcut::None;
    auto pressed = [](ImGuiKey k, bool repeat = false) { return ImGui::IsKeyPressed(k, repeat); };
    if (io.KeyCtrl) {
        if (pressed(ImGuiKey_C)) return KeyShortcut::Copy;
        if (pressed(ImGuiKey_X)) return KeyShortcut::Cut;
        if (pressed(ImGuiKey_V)) return KeyShortcut::Paste;
        if (pressed(ImGuiKey_D)) return KeyShortcut::Duplicate;
        if (pressed(ImGuiKey_A)) return KeyShortcut::SelectAll;
        return KeyShortcut::None;
    }
    if (pressed(ImGuiKey_Delete) || pressed(ImGuiKey_Backspace)) return KeyShortcut::Delete;
    if (pressed(ImGuiKey_Escape)) return KeyShortcut::Deselect;
    if (pressed(ImGuiKey_LeftArrow, true)) return KeyShortcut::NudgeLeft;
    if (pressed(ImGuiKey_RightArrow, true)) return KeyShortcut::NudgeRight;
    if (pressed(ImGuiKey_Comma, true)) return KeyShortcut::PrevKey;
    if (pressed(ImGuiKey_Period, true)) return KeyShortcut::NextKey;
    if (pressed(ImGuiKey_Home)) return KeyShortcut::Home;
    if (pressed(ImGuiKey_End)) return KeyShortcut::End;
    return KeyShortcut::None;
}

KeyShortcut KeyMenuItems(std::span<TimelineCurve* const> rows, KeySelection& sel, bool& changed, bool canPaste) {
    KeyShortcut action = KeyShortcut::None;
    const int count = static_cast<int>(sel.keys.size());
    if (count == 1) {
        if (TimelineKey* k = FindSelectedKey(rows, sel.keys.front())) {
            ImGui::TextDisabled("キー %.3f 秒", k->time);
            ImGui::SetNextItemWidth(120.0f * ImGui::GetFontSize() / 16.0f);
            if (ImGui::DragFloat("値", &k->value, 0.01f, 0.0f, 0.0f, "%.3f")) changed = true;
        }
    } else if (count > 1) {
        ImGui::TextDisabled("キー %d 個", count);
    }
    if (count > 0) {
        const char* names[] = {"なめらか（スムーズ）", "まっすぐ（リニア）", "そのまま（コンスタント）"};
        const TimelineKey* first = FindSelectedKey(rows, sel.keys.front());
        for (int i = 0; i < 3; ++i) {
            if (ImGui::MenuItem(names[i], nullptr, first && static_cast<int>(first->interpolation) == i)) changed |= SetKeysInterpolation(rows, sel, static_cast<KeyInterpolation>(i));
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_COPY "  コピー", "Ctrl+C")) action = KeyShortcut::Copy;
        if (ImGui::MenuItem(ICON_FA_SCISSORS "  切り取り", "Ctrl+X")) action = KeyShortcut::Cut;
    }
    if (ImGui::MenuItem(ICON_FA_PASTE "  今の時刻に貼り付け", "Ctrl+V", false, canPaste)) action = KeyShortcut::Paste;
    if (count > 0) {
        if (ImGui::MenuItem(ICON_FA_CLONE "  今の時刻に複製", "Ctrl+D")) action = KeyShortcut::Duplicate;
        if (ImGui::MenuItem(ICON_FA_RIGHT_LEFT "  時間を反転（逆の動きにする）", nullptr, false, count > 1)) changed |= ReverseKeys(rows, sel);
        if (ImGui::MenuItem(ICON_FA_TRASH "  削除", "Delete")) action = KeyShortcut::Delete;
    }
    ImGui::Separator();
    if (ImGui::MenuItem(ICON_FA_OBJECT_GROUP "  すべてのキーを選ぶ", "Ctrl+A")) action = KeyShortcut::SelectAll;
    return action;
}

} // namespace AquaTool
