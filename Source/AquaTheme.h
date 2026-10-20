#pragma once

// Aqua の見た目（RutoEngine のエディターと見分けがつくように、深い海の色 + アクア色のアクセント）
//
// AquaTool の中で Theme:: と書くと、RutoEngine の EditorUI::Theme ではなくこちらが使われる。

#include <imgui.h>

namespace AquaTool {

namespace Theme {
inline const ImVec4 kBackground{0.031f, 0.063f, 0.082f, 1.0f};   // いちばん奥（パネルの隙間・メニュー）
inline const ImVec4 kPanel{0.055f, 0.102f, 0.129f, 1.0f};        // パネル
inline const ImVec4 kFrame{0.082f, 0.149f, 0.184f, 1.0f};        // 入力欄・ボタン
inline const ImVec4 kFrameHover{0.106f, 0.196f, 0.235f, 1.0f};
inline const ImVec4 kFrameActive{0.129f, 0.243f, 0.286f, 1.0f};
inline const ImVec4 kAccent{0.196f, 0.851f, 0.800f, 1.0f};       // アクア
inline const ImVec4 kAccentDim{0.196f, 0.851f, 0.800f, 0.40f};
inline const ImVec4 kAccentInk{0.016f, 0.098f, 0.110f, 1.0f};    // アクアの上に置く文字
inline const ImVec4 kCoral{1.0f, 0.494f, 0.416f, 1.0f};          // 2 つ目のアクセント（目印）
inline const ImVec4 kText{0.867f, 0.937f, 0.945f, 1.0f};
inline const ImVec4 kTextDim{0.463f, 0.592f, 0.631f, 1.0f};
inline const ImVec4 kBorder{0.110f, 0.204f, 0.243f, 1.0f};
inline const ImVec4 kWarning{1.0f, 0.776f, 0.333f, 1.0f};
inline const ImVec4 kError{1.0f, 0.420f, 0.420f, 1.0f};
inline const ImVec4 kSuccess{0.380f, 0.890f, 0.600f, 1.0f};

inline ImU32 U32(const ImVec4& c, float alpha = -1.0f) {
    return ImGui::ColorConvertFloat4ToU32({c.x, c.y, c.z, alpha < 0.0f ? c.w : alpha});
}
} // namespace Theme

// EditorUI::Init の後に呼ぶ（RutoEngine の既定の見た目を Aqua の見た目に置き換える）
void ApplyAquaStyle(float scale);

// EditorUI::IconButton の Aqua 版（選んでいるときはアクア色）
bool IconButton(const char* icon, const char* tooltip, bool selected = false, ImVec2 size = {0, 0});

// Aqua のロゴ（しずく + 文字）。size は文字の大きさ
void DrawLogo(float size);

} // namespace AquaTool
