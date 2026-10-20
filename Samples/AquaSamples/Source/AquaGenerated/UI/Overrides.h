// Aqua が自動生成したファイル（編集しないでください。Aqua で UI を変えると書き直されます）
//   AquaUI/Overrides.aqs → Assets/UI/Overrides.aqua
//
//   auto& ui = GetEntity().AddComponent<UIDocument>();
//   ui.asset = AquaUI::Overrides::kAsset;
//   ui.OnClick(AquaUI::Overrides::Widgets::名前, [] { ... });
#pragma once

namespace AquaUI::Overrides {

// UIDocument::asset に入れるパス
inline constexpr const char* kAsset = "UI/Overrides.aqua";
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
inline constexpr const char* PartyCard = "PartyCard";
inline constexpr const char* PartyCardSize = "PartyCardSize";
inline constexpr const char* PartyCardBody = "PartyCardBody";
inline constexpr const char* PartyCardTitle = "PartyCardTitle";
inline constexpr const char* PartySize = "PartySize";
inline constexpr const char* Party = "Party";
inline constexpr const char* MemberAqua = "MemberAqua";
inline constexpr const char* MemberRuto = "MemberRuto";
inline constexpr const char* MemberCoral = "MemberCoral";
inline constexpr const char* PartyNote = "PartyNote";
inline constexpr const char* OverrideButtons = "OverrideButtons";
inline constexpr const char* RenameButton = "RenameButton";
inline constexpr const char* ResetButton = "ResetButton";
} // namespace Widgets

// ボタンのクリック時のメッセージ（OnUIEvent の e.message）
namespace Messages {
inline constexpr const char* back = "back";
inline constexpr const char* override_rename = "override-rename";
inline constexpr const char* override_reset = "override-reset";
} // namespace Messages

} // namespace AquaUI::Overrides
