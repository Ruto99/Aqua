// Aqua が自動生成したファイル（編集しないでください。Aqua で UI を変えると書き直されます）
//   AquaUI/Binding.aqs → Assets/UI/Binding.aqua
//
//   auto& ui = GetEntity().AddComponent<UIDocument>();
//   ui.asset = AquaUI::Binding::kAsset;
//   ui.OnClick(AquaUI::Binding::Widgets::名前, [] { ... });
#pragma once

namespace AquaUI::Binding {

// UIDocument::asset に入れるパス
inline constexpr const char* kAsset = "UI/Binding.aqua";
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
inline constexpr const char* Controls = "Controls";
inline constexpr const char* ControlsBody = "ControlsBody";
inline constexpr const char* ControlsTitle = "ControlsTitle";
inline constexpr const char* VolumeRow = "VolumeRow";
inline constexpr const char* VolumeRowLabel = "VolumeRowLabel";
inline constexpr const char* VolumeRowCtl = "VolumeRowCtl";
inline constexpr const char* VolumeSlider = "VolumeSlider";
inline constexpr const char* VolumeRowValue = "VolumeRowValue";
inline constexpr const char* HpRow = "HpRow";
inline constexpr const char* HpRowLabel = "HpRowLabel";
inline constexpr const char* HpRowCtl = "HpRowCtl";
inline constexpr const char* HpSlider = "HpSlider";
inline constexpr const char* HpRowValue = "HpRowValue";
inline constexpr const char* SecretRow = "SecretRow";
inline constexpr const char* SecretRowLabel = "SecretRowLabel";
inline constexpr const char* SecretRowCtl = "SecretRowCtl";
inline constexpr const char* SecretToggle = "SecretToggle";
inline constexpr const char* SecretRowValue = "SecretRowValue";
inline constexpr const char* NameRow = "NameRow";
inline constexpr const char* NameRowLabel = "NameRowLabel";
inline constexpr const char* NameRowCtl = "NameRowCtl";
inline constexpr const char* NameInput = "NameInput";
inline constexpr const char* NameRowValue = "NameRowValue";
inline constexpr const char* Display = "Display";
inline constexpr const char* DisplaySize = "DisplaySize";
inline constexpr const char* DisplayBody = "DisplayBody";
inline constexpr const char* DisplayTitle = "DisplayTitle";
inline constexpr const char* Greeting = "Greeting";
inline constexpr const char* VolumeText = "VolumeText";
inline constexpr const char* HpLine = "HpLine";
inline constexpr const char* HpText = "HpText";
inline constexpr const char* HpBarSize = "HpBarSize";
inline constexpr const char* HpBar = "HpBar";
inline constexpr const char* Secret = "Secret";
inline constexpr const char* SecretText = "SecretText";
inline constexpr const char* ScoreText = "ScoreText";
inline constexpr const char* ScoreButtons = "ScoreButtons";
inline constexpr const char* AddScore = "AddScore";
inline constexpr const char* ResetScore = "ResetScore";
inline constexpr const char* DisplayNote = "DisplayNote";
} // namespace Widgets

// 変数の名前（SetVariable）
namespace Variables {
inline constexpr const char* 変数 = "変数";
inline constexpr const char* volume = "volume";
inline constexpr const char* hp = "hp";
inline constexpr const char* showSecret = "showSecret";
inline constexpr const char* playerName = "playerName";
inline constexpr const char* score = "score";
inline constexpr const char* 変数名 = "変数名";
} // namespace Variables

// ボタンのクリック時のメッセージ（OnUIEvent の e.message）
namespace Messages {
inline constexpr const char* back = "back";
inline constexpr const char* score_add = "score-add";
inline constexpr const char* score_reset = "score-reset";
} // namespace Messages

} // namespace AquaUI::Binding
