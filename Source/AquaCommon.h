#pragma once

// Aqua の共通の部品（ウィジェットのアイコン・日本語名・ツリーの位置・スロットの計算など）

#include <Ruto/Ruto.h>

// テキストアニメーション・UI のシェーダー・キャプチャーは RutoEngine に EnginePatches を当てた SDK が要る
#if !__has_include(<Ruto/Engine/UIShader.h>)
#error "RutoEngine の SDK が古いです。RutoEngine で EnginePatches/*.patch を git am で当て、Setup.bat で SDK を作り直してください"
#endif
#include <Ruto/Engine/UIShader.h>

#include <imgui.h>

#include "AquaTheme.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace AquaTool {

using namespace Ruto;

inline constexpr const char* kTypePayload = "AQUA_WIDGET_TYPE";  // パレットからのドラッグ（型名）
inline constexpr const char* kNodePayload = "AQUA_WIDGET_NODE";  // 階層からのドラッグ（ツリーの位置）
inline constexpr const char* kAssetPayload = "AQUA_ASSET";       // アセットからのドラッグ（Assets からの相対パス）

const char* WidgetIcon(std::string_view type);
ImVec4 WidgetIconColor(std::string_view type);
const char* TypeLabel(std::string_view type);
std::string PropertyLabelJa(std::string_view property);
// 選択肢（enum）の日本語の表示名（知らないものはそのまま）
std::string EnumOptionLabelJa(std::string_view property, std::string_view option);
// アニメーションのトラックの表示名（シェーダーの値は @param の表示名にする）。使わない値なら空
std::string AnimPropertyLabel(Widget& w, const std::string& property, const std::string& channel);
// shaderParamN の N（違えば -1）
int ShaderParamSlot(std::string_view property);

// ツリーの中の位置（子の番号の並び）
using WidgetPath = std::vector<int>;
Widget* Resolve(Widget* root, const WidgetPath& path);
WidgetPath PathOf(const Widget* w);
// プレビューの複製の中のウィジェット → 文書の中の位置（部品の中身はその部品）
WidgetPath DesignPathOf(Widget* w);
std::string PathKey(const WidgetPath& p);
WidgetPath PathFromKey(const std::string& s);

bool IsCanvasChild(const Widget& w);
// Canvas Panel の子のスロットを、親の中の rect になるように合わせる（アンカー・ピボットはそのまま）
void SetSlotRect(WidgetSlot& s, const UI::Rect& parent, const UI::Rect& rect);
UI::Rect SlotRect(const WidgetSlot& s, const UI::Rect& parent, const Vec2& desired);
Vec2 DefaultCanvasSize(std::string_view type);
std::string UniqueWidgetName(Widget* root, const std::string& base);

struct ResolutionPreset {
    const char* label;
    float w, h;
};
std::span<const ResolutionPreset> ResolutionPresets();

// 解像度 res の画面での Canvas Scaler の倍率（基準解像度との比）
float CanvasScale(const WidgetAsset& asset, const Vec2& res);
// キャンバスの描画先の大きさ（画面上の大きさを丸めたもの）
int FramebufferSize(float screenSize);

// 文字列が C++ の識別子として使えるように直す（コード生成）
std::string ToIdentifier(std::string_view name);

} // namespace AquaTool
