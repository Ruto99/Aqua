// Aqua が自動生成したファイル（編集しないでください。Aqua で UI を変えると書き直されます）
//   AquaUI/Menu.aqs → Assets/UI/Menu.aqua
//
//   auto& ui = GetEntity().AddComponent<UIDocument>();
//   ui.asset = AquaUI::Menu::kAsset;
//   ui.OnClick(AquaUI::Menu::Widgets::名前, [] { ... });
#pragma once

namespace AquaUI::Menu {

// UIDocument::asset に入れるパス
inline constexpr const char* kAsset = "UI/Menu.aqua";
inline constexpr float kReferenceWidth = 1920.0f;
inline constexpr float kReferenceHeight = 1080.0f;

// ウィジェットの名前（Find・OnClick・SetText・SetVisible）
namespace Widgets {
inline constexpr const char* Background = "Background";
inline constexpr const char* MenuTitle = "MenuTitle";
inline constexpr const char* MenuSubtitle = "MenuSubtitle";
inline constexpr const char* SampleGrid = "SampleGrid";
inline constexpr const char* OpenTextAnimation = "OpenTextAnimation";
inline constexpr const char* CardTextAnimationSize = "CardTextAnimationSize";
inline constexpr const char* CardTextAnimationContent = "CardTextAnimationContent";
inline constexpr const char* CardTextAnimationNumber = "CardTextAnimationNumber";
inline constexpr const char* CardTextAnimationLabel = "CardTextAnimationLabel";
inline constexpr const char* CardTextAnimationDesc = "CardTextAnimationDesc";
inline constexpr const char* OpenDialog = "OpenDialog";
inline constexpr const char* CardDialogSize = "CardDialogSize";
inline constexpr const char* CardDialogContent = "CardDialogContent";
inline constexpr const char* CardDialogNumber = "CardDialogNumber";
inline constexpr const char* CardDialogLabel = "CardDialogLabel";
inline constexpr const char* CardDialogDesc = "CardDialogDesc";
inline constexpr const char* OpenShaders = "OpenShaders";
inline constexpr const char* CardShadersSize = "CardShadersSize";
inline constexpr const char* CardShadersContent = "CardShadersContent";
inline constexpr const char* CardShadersNumber = "CardShadersNumber";
inline constexpr const char* CardShadersLabel = "CardShadersLabel";
inline constexpr const char* CardShadersDesc = "CardShadersDesc";
inline constexpr const char* OpenCapture = "OpenCapture";
inline constexpr const char* CardCaptureSize = "CardCaptureSize";
inline constexpr const char* CardCaptureContent = "CardCaptureContent";
inline constexpr const char* CardCaptureNumber = "CardCaptureNumber";
inline constexpr const char* CardCaptureLabel = "CardCaptureLabel";
inline constexpr const char* CardCaptureDesc = "CardCaptureDesc";
inline constexpr const char* OpenAnimation = "OpenAnimation";
inline constexpr const char* CardAnimationSize = "CardAnimationSize";
inline constexpr const char* CardAnimationContent = "CardAnimationContent";
inline constexpr const char* CardAnimationNumber = "CardAnimationNumber";
inline constexpr const char* CardAnimationLabel = "CardAnimationLabel";
inline constexpr const char* CardAnimationDesc = "CardAnimationDesc";
inline constexpr const char* OpenLayout = "OpenLayout";
inline constexpr const char* CardLayoutSize = "CardLayoutSize";
inline constexpr const char* CardLayoutContent = "CardLayoutContent";
inline constexpr const char* CardLayoutNumber = "CardLayoutNumber";
inline constexpr const char* CardLayoutLabel = "CardLayoutLabel";
inline constexpr const char* CardLayoutDesc = "CardLayoutDesc";
inline constexpr const char* OpenBinding = "OpenBinding";
inline constexpr const char* CardBindingSize = "CardBindingSize";
inline constexpr const char* CardBindingContent = "CardBindingContent";
inline constexpr const char* CardBindingNumber = "CardBindingNumber";
inline constexpr const char* CardBindingLabel = "CardBindingLabel";
inline constexpr const char* CardBindingDesc = "CardBindingDesc";
inline constexpr const char* OpenStyles = "OpenStyles";
inline constexpr const char* CardStylesSize = "CardStylesSize";
inline constexpr const char* CardStylesContent = "CardStylesContent";
inline constexpr const char* CardStylesNumber = "CardStylesNumber";
inline constexpr const char* CardStylesLabel = "CardStylesLabel";
inline constexpr const char* CardStylesDesc = "CardStylesDesc";
inline constexpr const char* OpenTransform = "OpenTransform";
inline constexpr const char* CardTransformSize = "CardTransformSize";
inline constexpr const char* CardTransformContent = "CardTransformContent";
inline constexpr const char* CardTransformNumber = "CardTransformNumber";
inline constexpr const char* CardTransformLabel = "CardTransformLabel";
inline constexpr const char* CardTransformDesc = "CardTransformDesc";
inline constexpr const char* OpenOverrides = "OpenOverrides";
inline constexpr const char* CardOverridesSize = "CardOverridesSize";
inline constexpr const char* CardOverridesContent = "CardOverridesContent";
inline constexpr const char* CardOverridesNumber = "CardOverridesNumber";
inline constexpr const char* CardOverridesLabel = "CardOverridesLabel";
inline constexpr const char* CardOverridesDesc = "CardOverridesDesc";
inline constexpr const char* OpenAnimExtras = "OpenAnimExtras";
inline constexpr const char* CardAnimExtrasSize = "CardAnimExtrasSize";
inline constexpr const char* CardAnimExtrasContent = "CardAnimExtrasContent";
inline constexpr const char* CardAnimExtrasNumber = "CardAnimExtrasNumber";
inline constexpr const char* CardAnimExtrasLabel = "CardAnimExtrasLabel";
inline constexpr const char* CardAnimExtrasDesc = "CardAnimExtrasDesc";
inline constexpr const char* OpenTextExtras = "OpenTextExtras";
inline constexpr const char* CardTextExtrasSize = "CardTextExtrasSize";
inline constexpr const char* CardTextExtrasContent = "CardTextExtrasContent";
inline constexpr const char* CardTextExtrasNumber = "CardTextExtrasNumber";
inline constexpr const char* CardTextExtrasLabel = "CardTextExtrasLabel";
inline constexpr const char* CardTextExtrasDesc = "CardTextExtrasDesc";
inline constexpr const char* MenuHint = "MenuHint";
} // namespace Widgets

// アニメーションと区間の名前（PlayAnimation）
namespace Animations {
inline constexpr const char* Intro = "Intro";
} // namespace Animations

// 変数の名前（SetVariable）
namespace Variables {
inline constexpr const char* 変数 = "変数";
} // namespace Variables

// ボタンのクリック時のメッセージ（OnUIEvent の e.message）
namespace Messages {
inline constexpr const char* open_UI_TextAnimation_aqua = "open:UI/TextAnimation.aqua";
inline constexpr const char* open_UI_Dialog_aqua = "open:UI/Dialog.aqua";
inline constexpr const char* open_UI_Shaders_aqua = "open:UI/Shaders.aqua";
inline constexpr const char* open_UI_Capture_aqua = "open:UI/Capture.aqua";
inline constexpr const char* open_UI_Animation_aqua = "open:UI/Animation.aqua";
inline constexpr const char* open_UI_Layout_aqua = "open:UI/Layout.aqua";
inline constexpr const char* open_UI_Binding_aqua = "open:UI/Binding.aqua";
inline constexpr const char* open_UI_Styles_aqua = "open:UI/Styles.aqua";
inline constexpr const char* open_UI_Transform_aqua = "open:UI/Transform.aqua";
inline constexpr const char* open_UI_Overrides_aqua = "open:UI/Overrides.aqua";
inline constexpr const char* open_UI_AnimExtras_aqua = "open:UI/AnimExtras.aqua";
inline constexpr const char* open_UI_TextExtras_aqua = "open:UI/TextExtras.aqua";
} // namespace Messages

} // namespace AquaUI::Menu
