// Aqua が自動生成したファイル（編集しないでください。Aqua で UI を変えると書き直されます）
//   AquaUI/StatusCard.aqs → Assets/UI/StatusCard.aqua
//
//   auto& ui = GetEntity().AddComponent<UIDocument>();
//   ui.asset = AquaUI::StatusCard::kAsset;
//   ui.OnClick(AquaUI::StatusCard::Widgets::名前, [] { ... });
#pragma once

namespace AquaUI::StatusCard {

// UIDocument::asset に入れるパス
inline constexpr const char* kAsset = "UI/StatusCard.aqua";
inline constexpr float kReferenceWidth = 1920.0f;
inline constexpr float kReferenceHeight = 1080.0f;

// ウィジェットの名前（Find・OnClick・SetText・SetVisible）
namespace Widgets {
inline constexpr const char* StatusCard = "StatusCard";
inline constexpr const char* StatusBody = "StatusBody";
inline constexpr const char* StatusHead = "StatusHead";
inline constexpr const char* StatusIcon = "StatusIcon";
inline constexpr const char* StatusNames = "StatusNames";
inline constexpr const char* StatusName = "StatusName";
inline constexpr const char* StatusClass = "StatusClass";
inline constexpr const char* StatusHpRow = "StatusHpRow";
inline constexpr const char* StatusHpLabel = "StatusHpLabel";
inline constexpr const char* StatusHp = "StatusHp";
inline constexpr const char* StatusMpRow = "StatusMpRow";
inline constexpr const char* StatusMpLabel = "StatusMpLabel";
inline constexpr const char* StatusMp = "StatusMp";
} // namespace Widgets

} // namespace AquaUI::StatusCard
