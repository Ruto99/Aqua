// Aqua が自動生成したファイル（編集しないでください。Aqua で UI を変えると書き直されます）
//   AquaUI/Capture.aqs → Assets/UI/Capture.aqua
//
//   auto& ui = GetEntity().AddComponent<UIDocument>();
//   ui.asset = AquaUI::Capture::kAsset;
//   ui.OnClick(AquaUI::Capture::Widgets::名前, [] { ... });
#pragma once

namespace AquaUI::Capture {

// UIDocument::asset に入れるパス
inline constexpr const char* kAsset = "UI/Capture.aqua";
inline constexpr float kReferenceWidth = 1920.0f;
inline constexpr float kReferenceHeight = 1080.0f;

// ウィジェットの名前（Find・OnClick・SetText・SetVisible）
namespace Widgets {
inline constexpr const char* Header = "Header";
inline constexpr const char* BackButton = "BackButton";
inline constexpr const char* Titles = "Titles";
inline constexpr const char* Title = "Title";
inline constexpr const char* Subtitle = "Subtitle";
inline constexpr const char* GlassPanel = "GlassPanel";
inline constexpr const char* GlassCapture = "GlassCapture";
inline constexpr const char* GlassFrame = "GlassFrame";
inline constexpr const char* GlassBodySize = "GlassBodySize";
inline constexpr const char* GlassBody = "GlassBody";
inline constexpr const char* GlassTitle = "GlassTitle";
inline constexpr const char* GlassNote = "GlassNote";
inline constexpr const char* GlassButtons = "GlassButtons";
inline constexpr const char* PauseButton = "PauseButton";
inline constexpr const char* ShotButton = "ShotButton";
inline constexpr const char* SnapshotCard = "SnapshotCard";
inline constexpr const char* SnapshotCardSize = "SnapshotCardSize";
inline constexpr const char* SnapshotCardBody = "SnapshotCardBody";
inline constexpr const char* SnapshotCardTitle = "SnapshotCardTitle";
inline constexpr const char* SnapshotSize = "SnapshotSize";
inline constexpr const char* Snapshot = "Snapshot";
inline constexpr const char* SnapshotNote = "SnapshotNote";
inline constexpr const char* PauseLayer = "PauseLayer";
inline constexpr const char* PauseCapture = "PauseCapture";
inline constexpr const char* PauseSize = "PauseSize";
inline constexpr const char* PauseMenu = "PauseMenu";
inline constexpr const char* PauseBody = "PauseBody";
inline constexpr const char* PauseTitle = "PauseTitle";
inline constexpr const char* PauseNote = "PauseNote";
inline constexpr const char* ResumeButton = "ResumeButton";
inline constexpr const char* PauseBackButton = "PauseBackButton";
} // namespace Widgets

// ボタンのクリック時のメッセージ（OnUIEvent の e.message）
namespace Messages {
inline constexpr const char* back = "back";
inline constexpr const char* pause = "pause";
inline constexpr const char* snapshot = "snapshot";
inline constexpr const char* resume = "resume";
} // namespace Messages

} // namespace AquaUI::Capture
