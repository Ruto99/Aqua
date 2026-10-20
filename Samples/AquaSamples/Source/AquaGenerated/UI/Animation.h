// Aqua が自動生成したファイル（編集しないでください。Aqua で UI を変えると書き直されます）
//   AquaUI/Animation.aqs → Assets/UI/Animation.aqua
//
//   auto& ui = GetEntity().AddComponent<UIDocument>();
//   ui.asset = AquaUI::Animation::kAsset;
//   ui.OnClick(AquaUI::Animation::Widgets::名前, [] { ... });
#pragma once

namespace AquaUI::Animation {

// UIDocument::asset に入れるパス
inline constexpr const char* kAsset = "UI/Animation.aqua";
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
inline constexpr const char* SlidePanel = "SlidePanel";
inline constexpr const char* SlidePanelSize = "SlidePanelSize";
inline constexpr const char* SlidePanelBody = "SlidePanelBody";
inline constexpr const char* SlidePanelTitle = "SlidePanelTitle";
inline constexpr const char* SlideNote = "SlideNote";
inline constexpr const char* Heart = "Heart";
inline constexpr const char* HeartLabel = "HeartLabel";
inline constexpr const char* Bobber = "Bobber";
inline constexpr const char* BobLabel = "BobLabel";
inline constexpr const char* ShakeTarget = "ShakeTarget";
inline constexpr const char* ShakeText = "ShakeText";
inline constexpr const char* Gauge = "Gauge";
inline constexpr const char* AnimButtons = "AnimButtons";
inline constexpr const char* PlayIntro = "PlayIntro";
inline constexpr const char* PlayShake = "PlayShake";
inline constexpr const char* PlayGauge = "PlayGauge";
} // namespace Widgets

// アニメーションの名前（PlayAnimation）
namespace Animations {
inline constexpr const char* Intro = "Intro";
inline constexpr const char* Loop = "Loop";
inline constexpr const char* Shake = "Shake";
inline constexpr const char* Fill = "Fill";
} // namespace Animations

// ボタンのクリック時のメッセージ（OnUIEvent の e.message）
namespace Messages {
inline constexpr const char* back = "back";
inline constexpr const char* anim_Intro = "anim:Intro";
inline constexpr const char* anim_Shake = "anim:Shake";
inline constexpr const char* anim_Fill = "anim:Fill";
} // namespace Messages

} // namespace AquaUI::Animation
