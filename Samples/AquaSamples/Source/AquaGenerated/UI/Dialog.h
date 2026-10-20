// Aqua が自動生成したファイル（編集しないでください。Aqua で UI を変えると書き直されます）
//   AquaUI/Dialog.aqs → Assets/UI/Dialog.aqua
//
//   auto& ui = GetEntity().AddComponent<UIDocument>();
//   ui.asset = AquaUI::Dialog::kAsset;
//   ui.OnClick(AquaUI::Dialog::Widgets::名前, [] { ... });
#pragma once

namespace AquaUI::Dialog {

// UIDocument::asset に入れるパス
inline constexpr const char* kAsset = "UI/Dialog.aqua";
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
inline constexpr const char* DialogHint = "DialogHint";
inline constexpr const char* DialogHintSize = "DialogHintSize";
inline constexpr const char* DialogHintBody = "DialogHintBody";
inline constexpr const char* DialogHintTitle = "DialogHintTitle";
inline constexpr const char* DialogHintText = "DialogHintText";
inline constexpr const char* DialogWindow = "DialogWindow";
inline constexpr const char* DialogRow = "DialogRow";
inline constexpr const char* Portrait = "Portrait";
inline constexpr const char* PortraitBg = "PortraitBg";
inline constexpr const char* PortraitFace = "PortraitFace";
inline constexpr const char* DialogBody = "DialogBody";
inline constexpr const char* Speaker = "Speaker";
inline constexpr const char* DialogText = "DialogText";
inline constexpr const char* NextMark = "NextMark";
inline constexpr const char* NextButton = "NextButton";
} // namespace Widgets

// 変数の名前（SetVariable）
namespace Variables {
inline constexpr const char* speaker = "speaker";
inline constexpr const char* talking = "talking";
} // namespace Variables

// ボタンのクリック時のメッセージ（OnUIEvent の e.message）
namespace Messages {
inline constexpr const char* back = "back";
inline constexpr const char* dialog_next = "dialog-next";
} // namespace Messages

} // namespace AquaUI::Dialog
