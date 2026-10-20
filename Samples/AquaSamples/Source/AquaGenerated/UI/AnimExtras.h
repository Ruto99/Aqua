// Aqua が自動生成したファイル（編集しないでください。Aqua で UI を変えると書き直されます）
//   AquaUI/AnimExtras.aqs → Assets/UI/AnimExtras.aqua
//
//   auto& ui = GetEntity().AddComponent<UIDocument>();
//   ui.asset = AquaUI::AnimExtras::kAsset;
//   ui.OnClick(AquaUI::AnimExtras::Widgets::名前, [] { ... });
#pragma once

namespace AquaUI::AnimExtras {

// UIDocument::asset に入れるパス
inline constexpr const char* kAsset = "UI/AnimExtras.aqua";
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
inline constexpr const char* SectionCard = "SectionCard";
inline constexpr const char* SectionCardSize = "SectionCardSize";
inline constexpr const char* SectionCardBody = "SectionCardBody";
inline constexpr const char* SectionCardTitle = "SectionCardTitle";
inline constexpr const char* BannerArea = "BannerArea";
inline constexpr const char* BannerHolder = "BannerHolder";
inline constexpr const char* Banner = "Banner";
inline constexpr const char* BannerText = "BannerText";
inline constexpr const char* SectionButtons = "SectionButtons";
inline constexpr const char* PlayIn = "PlayIn";
inline constexpr const char* PlayIdle = "PlayIdle";
inline constexpr const char* PlayOut = "PlayOut";
inline constexpr const char* SectionNote = "SectionNote";
inline constexpr const char* GroupCard = "GroupCard";
inline constexpr const char* GroupCardSize = "GroupCardSize";
inline constexpr const char* GroupCardBody = "GroupCardBody";
inline constexpr const char* GroupCardTitle = "GroupCardTitle";
inline constexpr const char* StarArea = "StarArea";
inline constexpr const char* StarCanvas = "StarCanvas";
inline constexpr const char* Star0 = "Star0";
inline constexpr const char* Star1 = "Star1";
inline constexpr const char* Star2 = "Star2";
inline constexpr const char* Star3 = "Star3";
inline constexpr const char* Star4 = "Star4";
inline constexpr const char* Star5 = "Star5";
inline constexpr const char* GroupButtons = "GroupButtons";
inline constexpr const char* HideStars = "HideStars";
inline constexpr const char* ShowStars = "ShowStars";
inline constexpr const char* GroupNote = "GroupNote";
inline constexpr const char* FlipCard = "FlipCard";
inline constexpr const char* FlipCardSize = "FlipCardSize";
inline constexpr const char* FlipCardBody = "FlipCardBody";
inline constexpr const char* FlipCardTitle = "FlipCardTitle";
inline constexpr const char* FlipRow = "FlipRow";
inline constexpr const char* FlipAutoCol = "FlipAutoCol";
inline constexpr const char* FlipAuto = "FlipAuto";
inline constexpr const char* FlipAutoLabel = "FlipAutoLabel";
inline constexpr const char* FlipKeysCol = "FlipKeysCol";
inline constexpr const char* FlipKeys = "FlipKeys";
inline constexpr const char* FlipKeysLabel = "FlipKeysLabel";
inline constexpr const char* FlipNote = "FlipNote";
} // namespace Widgets

// アニメーションと区間の名前（PlayAnimation）
namespace Animations {
inline constexpr const char* Show = "Show";
inline constexpr const char* In = "In";
inline constexpr const char* Idle = "Idle";
inline constexpr const char* Out = "Out";
inline constexpr const char* Loop = "Loop";
} // namespace Animations

// グループの名前（FindGroup・SetGroupVisible）
namespace Groups {
inline constexpr const char* Stars = "Stars";
} // namespace Groups

// ボタンのクリック時のメッセージ（OnUIEvent の e.message）
namespace Messages {
inline constexpr const char* back = "back";
inline constexpr const char* anim_In = "anim:In";
inline constexpr const char* anim_Idle = "anim:Idle";
inline constexpr const char* anim_Out = "anim:Out";
inline constexpr const char* group_hide = "group-hide";
inline constexpr const char* group_show = "group-show";
} // namespace Messages

} // namespace AquaUI::AnimExtras
