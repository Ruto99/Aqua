// Aqua が自動生成したファイル（編集しないでください。Aqua で UI を変えると書き直されます）
//   AquaUI/TextAnimation.aqs → Assets/UI/TextAnimation.aqua
//
//   auto& ui = GetEntity().AddComponent<UIDocument>();
//   ui.asset = AquaUI::TextAnimation::kAsset;
//   ui.OnClick(AquaUI::TextAnimation::Widgets::名前, [] { ... });
#pragma once

namespace AquaUI::TextAnimation {

// UIDocument::asset に入れるパス
inline constexpr const char* kAsset = "UI/TextAnimation.aqua";
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
inline constexpr const char* Columns = "Columns";
inline constexpr const char* EffectCard = "EffectCard";
inline constexpr const char* EffectCardSize = "EffectCardSize";
inline constexpr const char* EffectCardBody = "EffectCardBody";
inline constexpr const char* EffectCardTitle = "EffectCardTitle";
inline constexpr const char* EffectWave = "EffectWave";
inline constexpr const char* EffectShake = "EffectShake";
inline constexpr const char* EffectRainbow = "EffectRainbow";
inline constexpr const char* EffectPulse = "EffectPulse";
inline constexpr const char* EffectSwing = "EffectSwing";
inline constexpr const char* EffectGlitch = "EffectGlitch";
inline constexpr const char* EffectNote = "EffectNote";
inline constexpr const char* RevealCard = "RevealCard";
inline constexpr const char* RevealCardSize = "RevealCardSize";
inline constexpr const char* RevealCardBody = "RevealCardBody";
inline constexpr const char* RevealCardTitle = "RevealCardTitle";
inline constexpr const char* RevealTypewriter = "RevealTypewriter";
inline constexpr const char* RevealFade = "RevealFade";
inline constexpr const char* RevealPop = "RevealPop";
inline constexpr const char* RevealRise = "RevealRise";
inline constexpr const char* RevealDrop = "RevealDrop";
inline constexpr const char* RevealSpin = "RevealSpin";
inline constexpr const char* RevealButtons = "RevealButtons";
inline constexpr const char* Replay = "Replay";
inline constexpr const char* RevealNote = "RevealNote";
inline constexpr const char* RichHolder = "RichHolder";
inline constexpr const char* RichCard = "RichCard";
inline constexpr const char* RichCardSize = "RichCardSize";
inline constexpr const char* RichCardBody = "RichCardBody";
inline constexpr const char* RichCardTitle = "RichCardTitle";
inline constexpr const char* RichText = "RichText";
inline constexpr const char* RichNote = "RichNote";
} // namespace Widgets

// ボタンのクリック時のメッセージ（OnUIEvent の e.message）
namespace Messages {
inline constexpr const char* back = "back";
inline constexpr const char* restart_text = "restart-text";
} // namespace Messages

} // namespace AquaUI::TextAnimation
