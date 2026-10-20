// Aqua が自動生成したファイル（編集しないでください。Aqua で UI を変えると書き直されます）
//   AquaUI/Shaders.aqs → Assets/UI/Shaders.aqua
//
//   auto& ui = GetEntity().AddComponent<UIDocument>();
//   ui.asset = AquaUI::Shaders::kAsset;
//   ui.OnClick(AquaUI::Shaders::Widgets::名前, [] { ... });
#pragma once

namespace AquaUI::Shaders {

// UIDocument::asset に入れるパス
inline constexpr const char* kAsset = "UI/Shaders.aqua";
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
inline constexpr const char* ShaderGrid = "ShaderGrid";
inline constexpr const char* GrayscaleCard = "GrayscaleCard";
inline constexpr const char* GrayscaleCardBody = "GrayscaleCardBody";
inline constexpr const char* GrayscaleFaceSize = "GrayscaleFaceSize";
inline constexpr const char* GrayscaleFace = "GrayscaleFace";
inline constexpr const char* GrayscaleFaceBg = "GrayscaleFaceBg";
inline constexpr const char* GrayscaleFaceStripes = "GrayscaleFaceStripes";
inline constexpr const char* GrayscaleFaceS0 = "GrayscaleFaceS0";
inline constexpr const char* GrayscaleFaceS1 = "GrayscaleFaceS1";
inline constexpr const char* GrayscaleFaceS2 = "GrayscaleFaceS2";
inline constexpr const char* GrayscaleFaceS3 = "GrayscaleFaceS3";
inline constexpr const char* GrayscaleFaceText = "GrayscaleFaceText";
inline constexpr const char* GrayscaleLabel = "GrayscaleLabel";
inline constexpr const char* GrayscaleFile = "GrayscaleFile";
inline constexpr const char* SepiaCard = "SepiaCard";
inline constexpr const char* SepiaCardBody = "SepiaCardBody";
inline constexpr const char* SepiaFaceSize = "SepiaFaceSize";
inline constexpr const char* SepiaFace = "SepiaFace";
inline constexpr const char* SepiaFaceBg = "SepiaFaceBg";
inline constexpr const char* SepiaFaceStripes = "SepiaFaceStripes";
inline constexpr const char* SepiaFaceS0 = "SepiaFaceS0";
inline constexpr const char* SepiaFaceS1 = "SepiaFaceS1";
inline constexpr const char* SepiaFaceS2 = "SepiaFaceS2";
inline constexpr const char* SepiaFaceS3 = "SepiaFaceS3";
inline constexpr const char* SepiaFaceText = "SepiaFaceText";
inline constexpr const char* SepiaLabel = "SepiaLabel";
inline constexpr const char* SepiaFile = "SepiaFile";
inline constexpr const char* FlashCard = "FlashCard";
inline constexpr const char* FlashCardBody = "FlashCardBody";
inline constexpr const char* FlashFaceSize = "FlashFaceSize";
inline constexpr const char* FlashFace = "FlashFace";
inline constexpr const char* FlashFaceBg = "FlashFaceBg";
inline constexpr const char* FlashFaceStripes = "FlashFaceStripes";
inline constexpr const char* FlashFaceS0 = "FlashFaceS0";
inline constexpr const char* FlashFaceS1 = "FlashFaceS1";
inline constexpr const char* FlashFaceS2 = "FlashFaceS2";
inline constexpr const char* FlashFaceS3 = "FlashFaceS3";
inline constexpr const char* FlashFaceText = "FlashFaceText";
inline constexpr const char* FlashLabel = "FlashLabel";
inline constexpr const char* FlashFile = "FlashFile";
inline constexpr const char* ShineCard = "ShineCard";
inline constexpr const char* ShineCardBody = "ShineCardBody";
inline constexpr const char* ShineFaceSize = "ShineFaceSize";
inline constexpr const char* ShineFace = "ShineFace";
inline constexpr const char* ShineFaceBg = "ShineFaceBg";
inline constexpr const char* ShineFaceStripes = "ShineFaceStripes";
inline constexpr const char* ShineFaceS0 = "ShineFaceS0";
inline constexpr const char* ShineFaceS1 = "ShineFaceS1";
inline constexpr const char* ShineFaceS2 = "ShineFaceS2";
inline constexpr const char* ShineFaceS3 = "ShineFaceS3";
inline constexpr const char* ShineFaceText = "ShineFaceText";
inline constexpr const char* ShineLabel = "ShineLabel";
inline constexpr const char* ShineFile = "ShineFile";
inline constexpr const char* DissolveCard = "DissolveCard";
inline constexpr const char* DissolveCardBody = "DissolveCardBody";
inline constexpr const char* DissolveFaceSize = "DissolveFaceSize";
inline constexpr const char* DissolveFace = "DissolveFace";
inline constexpr const char* DissolveFaceBg = "DissolveFaceBg";
inline constexpr const char* DissolveFaceStripes = "DissolveFaceStripes";
inline constexpr const char* DissolveFaceS0 = "DissolveFaceS0";
inline constexpr const char* DissolveFaceS1 = "DissolveFaceS1";
inline constexpr const char* DissolveFaceS2 = "DissolveFaceS2";
inline constexpr const char* DissolveFaceS3 = "DissolveFaceS3";
inline constexpr const char* DissolveFaceText = "DissolveFaceText";
inline constexpr const char* DissolveLabel = "DissolveLabel";
inline constexpr const char* DissolveFile = "DissolveFile";
inline constexpr const char* GradientCard = "GradientCard";
inline constexpr const char* GradientCardBody = "GradientCardBody";
inline constexpr const char* GradientFaceSize = "GradientFaceSize";
inline constexpr const char* GradientFace = "GradientFace";
inline constexpr const char* GradientFaceBg = "GradientFaceBg";
inline constexpr const char* GradientFaceStripes = "GradientFaceStripes";
inline constexpr const char* GradientFaceS0 = "GradientFaceS0";
inline constexpr const char* GradientFaceS1 = "GradientFaceS1";
inline constexpr const char* GradientFaceS2 = "GradientFaceS2";
inline constexpr const char* GradientFaceS3 = "GradientFaceS3";
inline constexpr const char* GradientFaceText = "GradientFaceText";
inline constexpr const char* GradientLabel = "GradientLabel";
inline constexpr const char* GradientFile = "GradientFile";
inline constexpr const char* RainbowCard = "RainbowCard";
inline constexpr const char* RainbowCardBody = "RainbowCardBody";
inline constexpr const char* RainbowFaceSize = "RainbowFaceSize";
inline constexpr const char* RainbowFace = "RainbowFace";
inline constexpr const char* RainbowFaceBg = "RainbowFaceBg";
inline constexpr const char* RainbowFaceStripes = "RainbowFaceStripes";
inline constexpr const char* RainbowFaceS0 = "RainbowFaceS0";
inline constexpr const char* RainbowFaceS1 = "RainbowFaceS1";
inline constexpr const char* RainbowFaceS2 = "RainbowFaceS2";
inline constexpr const char* RainbowFaceS3 = "RainbowFaceS3";
inline constexpr const char* RainbowFaceText = "RainbowFaceText";
inline constexpr const char* RainbowLabel = "RainbowLabel";
inline constexpr const char* RainbowFile = "RainbowFile";
inline constexpr const char* HueShiftCard = "HueShiftCard";
inline constexpr const char* HueShiftCardBody = "HueShiftCardBody";
inline constexpr const char* HueShiftFaceSize = "HueShiftFaceSize";
inline constexpr const char* HueShiftFace = "HueShiftFace";
inline constexpr const char* HueShiftFaceBg = "HueShiftFaceBg";
inline constexpr const char* HueShiftFaceStripes = "HueShiftFaceStripes";
inline constexpr const char* HueShiftFaceS0 = "HueShiftFaceS0";
inline constexpr const char* HueShiftFaceS1 = "HueShiftFaceS1";
inline constexpr const char* HueShiftFaceS2 = "HueShiftFaceS2";
inline constexpr const char* HueShiftFaceS3 = "HueShiftFaceS3";
inline constexpr const char* HueShiftFaceText = "HueShiftFaceText";
inline constexpr const char* HueShiftLabel = "HueShiftLabel";
inline constexpr const char* HueShiftFile = "HueShiftFile";
inline constexpr const char* BlinkCard = "BlinkCard";
inline constexpr const char* BlinkCardBody = "BlinkCardBody";
inline constexpr const char* BlinkFaceSize = "BlinkFaceSize";
inline constexpr const char* BlinkFace = "BlinkFace";
inline constexpr const char* BlinkFaceBg = "BlinkFaceBg";
inline constexpr const char* BlinkFaceStripes = "BlinkFaceStripes";
inline constexpr const char* BlinkFaceS0 = "BlinkFaceS0";
inline constexpr const char* BlinkFaceS1 = "BlinkFaceS1";
inline constexpr const char* BlinkFaceS2 = "BlinkFaceS2";
inline constexpr const char* BlinkFaceS3 = "BlinkFaceS3";
inline constexpr const char* BlinkFaceText = "BlinkFaceText";
inline constexpr const char* BlinkLabel = "BlinkLabel";
inline constexpr const char* BlinkFile = "BlinkFile";
inline constexpr const char* ScanlinesCard = "ScanlinesCard";
inline constexpr const char* ScanlinesCardBody = "ScanlinesCardBody";
inline constexpr const char* ScanlinesFaceSize = "ScanlinesFaceSize";
inline constexpr const char* ScanlinesFace = "ScanlinesFace";
inline constexpr const char* ScanlinesFaceBg = "ScanlinesFaceBg";
inline constexpr const char* ScanlinesFaceStripes = "ScanlinesFaceStripes";
inline constexpr const char* ScanlinesFaceS0 = "ScanlinesFaceS0";
inline constexpr const char* ScanlinesFaceS1 = "ScanlinesFaceS1";
inline constexpr const char* ScanlinesFaceS2 = "ScanlinesFaceS2";
inline constexpr const char* ScanlinesFaceS3 = "ScanlinesFaceS3";
inline constexpr const char* ScanlinesFaceText = "ScanlinesFaceText";
inline constexpr const char* ScanlinesLabel = "ScanlinesLabel";
inline constexpr const char* ScanlinesFile = "ScanlinesFile";
inline constexpr const char* HologramCard = "HologramCard";
inline constexpr const char* HologramCardBody = "HologramCardBody";
inline constexpr const char* HologramFaceSize = "HologramFaceSize";
inline constexpr const char* HologramFace = "HologramFace";
inline constexpr const char* HologramFaceBg = "HologramFaceBg";
inline constexpr const char* HologramFaceStripes = "HologramFaceStripes";
inline constexpr const char* HologramFaceS0 = "HologramFaceS0";
inline constexpr const char* HologramFaceS1 = "HologramFaceS1";
inline constexpr const char* HologramFaceS2 = "HologramFaceS2";
inline constexpr const char* HologramFaceS3 = "HologramFaceS3";
inline constexpr const char* HologramFaceText = "HologramFaceText";
inline constexpr const char* HologramLabel = "HologramLabel";
inline constexpr const char* HologramFile = "HologramFile";
inline constexpr const char* VignetteCard = "VignetteCard";
inline constexpr const char* VignetteCardBody = "VignetteCardBody";
inline constexpr const char* VignetteFaceSize = "VignetteFaceSize";
inline constexpr const char* VignetteFace = "VignetteFace";
inline constexpr const char* VignetteFaceBg = "VignetteFaceBg";
inline constexpr const char* VignetteFaceStripes = "VignetteFaceStripes";
inline constexpr const char* VignetteFaceS0 = "VignetteFaceS0";
inline constexpr const char* VignetteFaceS1 = "VignetteFaceS1";
inline constexpr const char* VignetteFaceS2 = "VignetteFaceS2";
inline constexpr const char* VignetteFaceS3 = "VignetteFaceS3";
inline constexpr const char* VignetteFaceText = "VignetteFaceText";
inline constexpr const char* VignetteLabel = "VignetteLabel";
inline constexpr const char* VignetteFile = "VignetteFile";
inline constexpr const char* CustomCard = "CustomCard";
inline constexpr const char* CustomCardSize = "CustomCardSize";
inline constexpr const char* CustomCardBody = "CustomCardBody";
inline constexpr const char* CustomCardTitle = "CustomCardTitle";
inline constexpr const char* CausticsSize = "CausticsSize";
inline constexpr const char* CausticsFace = "CausticsFace";
inline constexpr const char* CustomNote = "CustomNote";
inline constexpr const char* ControlCard = "ControlCard";
inline constexpr const char* ControlCardSize = "ControlCardSize";
inline constexpr const char* ControlCardBody = "ControlCardBody";
inline constexpr const char* ControlCardTitle = "ControlCardTitle";
inline constexpr const char* ControlledSize = "ControlledSize";
inline constexpr const char* DissolveControlled = "DissolveControlled";
inline constexpr const char* DissolveControlledText = "DissolveControlledText";
inline constexpr const char* DissolveSlider = "DissolveSlider";
inline constexpr const char* ControlNote = "ControlNote";
} // namespace Widgets

// アニメーションの名前（PlayAnimation）
namespace Animations {
inline constexpr const char* Loop = "Loop";
} // namespace Animations

// 変数の名前（SetVariable）
namespace Variables {
inline constexpr const char* dissolve = "dissolve";
} // namespace Variables

// ボタンのクリック時のメッセージ（OnUIEvent の e.message）
namespace Messages {
inline constexpr const char* back = "back";
} // namespace Messages

} // namespace AquaUI::Shaders
