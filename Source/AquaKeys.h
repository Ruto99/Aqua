#pragma once

// タイムラインのキーの編集（ウィジェットのアニメーション・文字アニメーションで共通）
//
//   ・選ぶ: クリック / Ctrl・Shift + クリックで足す・外す / 空いた所をドラッグで囲んで選ぶ / Ctrl+A ですべて
//   ・動かす: 選んだキーをまとめてドラッグ / ← → で 1 コマずつ（Shift で 10 コマ）
//   ・コピー（Ctrl+C）・切り取り（Ctrl+X）・貼り付け（Ctrl+V。今の時刻に、間隔を保って）・複製（Ctrl+D）・削除（Delete）
//   ・前 / 次のキーへ（, / .）・最初 / 最後へ（Home / End）・選んだキーの時間を反転（逆再生の動きを作る）

#include "AquaCommon.h"

#include <functional>
#include <span>
#include <string>
#include <vector>

namespace AquaTool {

// キーは行（トラック）の番号と時刻で指す（並べ替えても変わらない）
struct KeyRef {
    int row = -1;
    float time = 0.0f;
};

struct KeySelection {
    std::vector<KeyRef> keys; // 最後のものが「今のキー」（値の欄で編集する）

    bool Has(int row, float time) const;
    void Add(int row, float time);
    void Remove(int row, float time);
    void Clear() { keys.clear(); }
    bool Empty() const { return keys.empty(); }
    const KeyRef* Primary() const { return keys.empty() ? nullptr : &keys.back(); }

    // ドラッグ・囲んで選ぶ（KeyTimelineArea が使う）
    bool dragging = false;
    float dragAnchor = 0.0f;
    float dragDelta = 0.0f;
    std::vector<std::pair<KeyRef, TimelineKey>> dragKeys; // ドラッグを始めたときのキー
    bool boxing = false;
    bool boxAdd = false;
    ImVec2 boxStart{0.0f, 0.0f};
};

// コピーしたキー（時刻はいちばん前のキーからの秒）
struct KeyClip {
    std::string id; // 行の名前（トラックなら "ウィジェット\nプロパティ"、文字アニメーションならチャンネル）
    TimelineKey key;
};
struct KeyClipboard {
    std::string kind; // "widget" / "text"（違う種類には貼らない）
    std::vector<KeyClip> keys;
};

struct KeyTimelineArgs {
    std::span<TimelineCurve* const> rows; // nullptr の行はキーなし
    float rowH = 20.0f;
    ImVec2 pos{0.0f, 0.0f};               // キーの範囲の左上（行の名前の右）
    float width = 100.0f;
    std::function<float(float)> timeToX;
    std::function<float(float)> xToTime;
    std::function<float(float)> snap;
    float* playhead = nullptr;
    bool* playing = nullptr;
    float scale = 1.0f;
};

struct KeyTimelineResult {
    bool changed = false;
    bool doubleClick = false;  // 空いた所のダブルクリック（キーを打つ）
    int row = -1;              // ダブルクリック・右クリックの行
    float time = 0.0f;         // その時刻
    bool keyMenu = false;      // キーの右クリック（選択は更新済み）
    bool emptyMenu = false;    // 空いた所の右クリック
};

// 行の範囲のキーを描いて、選ぶ・ドラッグ・囲んで選ぶ・ダブルクリック・右クリックを扱う
KeyTimelineResult KeyTimelineArea(const char* id, const KeyTimelineArgs& args, KeySelection& sel);

// ---- 選んだキーの操作（変えたら true） ----
bool DeleteKeys(std::span<TimelineCurve* const> rows, KeySelection& sel);
void CopyKeys(std::span<TimelineCurve* const> rows, std::span<const std::string> ids, const KeySelection& sel, KeyClipboard& clip, const std::string& kind);
// at の時刻に貼る。行は rowOf(id) で探す。コピーしたのが 1 行だけで fallbackRow >= 0 なら、その行に貼る。貼ったキーを選ぶ
bool PasteKeys(std::span<TimelineCurve* const> rows, const KeyClipboard& clip, float at, const std::function<int(const std::string&)>& rowOf, int fallbackRow,
               KeySelection& sel);
bool MoveKeys(std::span<TimelineCurve* const> rows, KeySelection& sel, float delta);
bool ReverseKeys(std::span<TimelineCurve* const> rows, KeySelection& sel); // 選んだ範囲の中で時間を反転
bool SetKeysInterpolation(std::span<TimelineCurve* const> rows, const KeySelection& sel, KeyInterpolation interpolation);
void SelectAllKeys(std::span<TimelineCurve* const> rows, KeySelection& sel, int onlyRow = -1);
// t より後（dir > 0）/ 前（dir < 0）のいちばん近いキーの時刻（無ければ t）
float AdjacentKeyTime(std::span<TimelineCurve* const> rows, float t, int dir);
// 選んだキーのいちばん前・いちばん後ろの時刻
bool SelectedRange(const KeySelection& sel, float& first, float& last);
// 選んだキーを探す（無ければ nullptr）
TimelineKey* FindSelectedKey(std::span<TimelineCurve* const> rows, const KeyRef& ref);
// 行の中の重なったキー（同じ時刻）をまとめる
void SortRow(TimelineCurve& c);

// タイムラインのショートカット（パネルを使っているときに読む）
enum class KeyShortcut { None, Copy, Cut, Paste, Duplicate, Delete, SelectAll, Deselect, NudgeLeft, NudgeRight, PrevKey, NextKey, Home, End };
KeyShortcut ReadKeyShortcut();

// キーの右クリックのメニューの中身（共通の項目）。押されたショートカットを返す（呼んだ側で実行）
KeyShortcut KeyMenuItems(std::span<TimelineCurve* const> rows, KeySelection& sel, bool& changed, bool canPaste);

} // namespace AquaTool
