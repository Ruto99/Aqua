// Aqua が自動生成したファイル（編集しないでください。Aqua で UI を変えると書き直されます）
//   AquaUI/Transform.aqs → Assets/UI/Transform.aqua
//
//   auto& ui = GetEntity().AddComponent<UIDocument>();
//   ui.asset = AquaUI::Transform::kAsset;
//   ui.OnClick(AquaUI::Transform::Widgets::名前, [] { ... });
#pragma once

namespace AquaUI::Transform {

// UIDocument::asset に入れるパス
inline constexpr const char* kAsset = "UI/Transform.aqua";
inline constexpr float kReferenceWidth = 1920.0f;
inline constexpr float kReferenceHeight = 1080.0f;

// ウィジェットの名前（Find・OnClick・SetText・SetVisible）
namespace Widgets {
inline constexpr const char* Background = "Background";
inline constexpr const char* Header = "Header";
inline constexpr const char* BackButton = "BackButton";
inline constexpr const char* Titles = "Titles";
inline constexpr const char* Title = "Title";
inline constexpr const char* Subtitle = "Subtitle";
inline constexpr const char* RotateCard = "RotateCard";
inline constexpr const char* RotateCardSize = "RotateCardSize";
inline constexpr const char* RotateCardBody = "RotateCardBody";
inline constexpr const char* RotateCardTitle = "RotateCardTitle";
inline constexpr const char* RotateRow = "RotateRow";
inline constexpr const char* SpinArea = "SpinArea";
inline constexpr const char* SpinCanvas = "SpinCanvas";
inline constexpr const char* Spinner = "Spinner";
inline constexpr const char* Dot0 = "Dot0";
inline constexpr const char* Dot1 = "Dot1";
inline constexpr const char* Dot2 = "Dot2";
inline constexpr const char* Dot3 = "Dot3";
inline constexpr const char* ClockArea = "ClockArea";
inline constexpr const char* ClockCanvas = "ClockCanvas";
inline constexpr const char* Dial = "Dial";
inline constexpr const char* Hand = "Hand";
inline constexpr const char* TiltedButton = "TiltedButton";
inline constexpr const char* RotateNote = "RotateNote";
inline constexpr const char* ColorCard = "ColorCard";
inline constexpr const char* ColorCardSize = "ColorCardSize";
inline constexpr const char* ColorCardBody = "ColorCardBody";
inline constexpr const char* ColorCardTitle = "ColorCardTitle";
inline constexpr const char* GradBox = "GradBox";
inline constexpr const char* GradText = "GradText";
inline constexpr const char* GradPanel = "GradPanel";
inline constexpr const char* GradPanelText = "GradPanelText";
inline constexpr const char* ColorNote = "ColorNote";
inline constexpr const char* HitCard = "HitCard";
inline constexpr const char* HitCardSize = "HitCardSize";
inline constexpr const char* HitCardBody = "HitCardBody";
inline constexpr const char* HitCardTitle = "HitCardTitle";
inline constexpr const char* TreasureArea = "TreasureArea";
inline constexpr const char* TreasureCanvas = "TreasureCanvas";
inline constexpr const char* Treasure = "Treasure";
inline constexpr const char* Lock = "Lock";
inline constexpr const char* TreasureLabel = "TreasureLabel";
inline constexpr const char* LockHit = "LockHit";
inline constexpr const char* HitCount = "HitCount";
inline constexpr const char* ShopItem = "ShopItem";
inline constexpr const char* ShopItemBody = "ShopItemBody";
inline constexpr const char* ShopItemName = "ShopItemName";
inline constexpr const char* ShopItemPrice = "ShopItemPrice";
inline constexpr const char* HitNote = "HitNote";
} // namespace Widgets

// アニメーションの名前（PlayAnimation）
namespace Animations {
inline constexpr const char* Loop = "Loop";
} // namespace Animations

// 変数の名前（SetVariable）
namespace Variables {
inline constexpr const char* hits = "hits";
inline constexpr const char* itemName = "itemName";
inline constexpr const char* itemPrice = "itemPrice";
} // namespace Variables

// ボタンのクリック時のメッセージ（OnUIEvent の e.message）
namespace Messages {
inline constexpr const char* back = "back";
inline constexpr const char* bounding_hit = "bounding-hit";
} // namespace Messages

} // namespace AquaUI::Transform
