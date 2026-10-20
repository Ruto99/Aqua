// Aqua が自動生成したファイル（編集しないでください。Aqua で UI を変えると書き直されます）
//   AquaUI/TextExtras.aqs → Assets/UI/TextExtras.aqua
//
//   auto& ui = GetEntity().AddComponent<UIDocument>();
//   ui.asset = AquaUI::TextExtras::kAsset;
//   ui.OnClick(AquaUI::TextExtras::Widgets::名前, [] { ... });
#pragma once

namespace AquaUI::TextExtras {

// UIDocument::asset に入れるパス
inline constexpr const char* kAsset = "UI/TextExtras.aqua";
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
inline constexpr const char* SpacingCard = "SpacingCard";
inline constexpr const char* SpacingCardSize = "SpacingCardSize";
inline constexpr const char* SpacingCardBody = "SpacingCardBody";
inline constexpr const char* SpacingCardTitle = "SpacingCardTitle";
inline constexpr const char* Tight = "Tight";
inline constexpr const char* Normal = "Normal";
inline constexpr const char* Wide = "Wide";
inline constexpr const char* LineRow = "LineRow";
inline constexpr const char* Line0Box = "Line0Box";
inline constexpr const char* Line0 = "Line0";
inline constexpr const char* Line1Box = "Line1Box";
inline constexpr const char* Line1 = "Line1";
inline constexpr const char* FitCard = "FitCard";
inline constexpr const char* FitCardSize = "FitCardSize";
inline constexpr const char* FitCardBody = "FitCardBody";
inline constexpr const char* FitCardTitle = "FitCardTitle";
inline constexpr const char* FitOffSize = "FitOffSize";
inline constexpr const char* FitOffCol = "FitOffCol";
inline constexpr const char* FitOff = "FitOff";
inline constexpr const char* FitOffText = "FitOffText";
inline constexpr const char* FitOffLabel = "FitOffLabel";
inline constexpr const char* FitOnSize = "FitOnSize";
inline constexpr const char* FitOnCol = "FitOnCol";
inline constexpr const char* FitOn = "FitOn";
inline constexpr const char* FitOnText = "FitOnText";
inline constexpr const char* FitOnLabel = "FitOnLabel";
inline constexpr const char* GradText = "GradText";
inline constexpr const char* FitNote = "FitNote";
inline constexpr const char* BlackWhiteCard = "BlackWhiteCard";
inline constexpr const char* BlackWhiteCardSize = "BlackWhiteCardSize";
inline constexpr const char* BlackWhiteCardBody = "BlackWhiteCardBody";
inline constexpr const char* BlackWhiteCardTitle = "BlackWhiteCardTitle";
inline constexpr const char* BwRow1 = "BwRow1";
inline constexpr const char* BwOriginal = "BwOriginal";
inline constexpr const char* BwNavyAqua = "BwNavyAqua";
inline constexpr const char* BwCoralGold = "BwCoralGold";
inline constexpr const char* BwRow2 = "BwRow2";
inline constexpr const char* BwSoft = "BwSoft";
inline constexpr const char* BwSoftAqua = "BwSoftAqua";
inline constexpr const char* BwSoftGlow = "BwSoftGlow";
inline constexpr const char* BwNote = "BwNote";
} // namespace Widgets

// ボタンのクリック時のメッセージ（OnUIEvent の e.message）
namespace Messages {
inline constexpr const char* back = "back";
} // namespace Messages

} // namespace AquaUI::TextExtras
