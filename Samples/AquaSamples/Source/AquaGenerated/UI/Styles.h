// Aqua が自動生成したファイル（編集しないでください。Aqua で UI を変えると書き直されます）
//   AquaUI/Styles.aqs → Assets/UI/Styles.aqua
//
//   auto& ui = GetEntity().AddComponent<UIDocument>();
//   ui.asset = AquaUI::Styles::kAsset;
//   ui.OnClick(AquaUI::Styles::Widgets::名前, [] { ... });
#pragma once

namespace AquaUI::Styles {

// UIDocument::asset に入れるパス
inline constexpr const char* kAsset = "UI/Styles.aqua";
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
inline constexpr const char* StyleCard = "StyleCard";
inline constexpr const char* StyleCardSize = "StyleCardSize";
inline constexpr const char* StyleCardBody = "StyleCardBody";
inline constexpr const char* StyleCardTitle = "StyleCardTitle";
inline constexpr const char* StyleButtons = "StyleButtons";
inline constexpr const char* StyleHeading1 = "StyleHeading1";
inline constexpr const char* PrimaryRow = "PrimaryRow";
inline constexpr const char* Primary0 = "Primary0";
inline constexpr const char* Primary1 = "Primary1";
inline constexpr const char* Primary2 = "Primary2";
inline constexpr const char* StyleHeading2 = "StyleHeading2";
inline constexpr const char* DangerRow = "DangerRow";
inline constexpr const char* Danger0 = "Danger0";
inline constexpr const char* Danger1 = "Danger1";
inline constexpr const char* StyleHeading3 = "StyleHeading3";
inline constexpr const char* GhostRow = "GhostRow";
inline constexpr const char* Ghost0 = "Ghost0";
inline constexpr const char* Ghost1 = "Ghost1";
inline constexpr const char* Ghost2 = "Ghost2";
inline constexpr const char* StyleNote = "StyleNote";
inline constexpr const char* PartCard = "PartCard";
inline constexpr const char* PartCardSize = "PartCardSize";
inline constexpr const char* PartCardBody = "PartCardBody";
inline constexpr const char* PartCardTitle = "PartCardTitle";
inline constexpr const char* PartsSize = "PartsSize";
inline constexpr const char* Parts = "Parts";
inline constexpr const char* Member0 = "Member0";
inline constexpr const char* Member1 = "Member1";
inline constexpr const char* PartNote = "PartNote";
} // namespace Widgets

// ボタンのクリック時のメッセージ（OnUIEvent の e.message）
namespace Messages {
inline constexpr const char* back = "back";
inline constexpr const char* noop = "noop";
} // namespace Messages

} // namespace AquaUI::Styles
