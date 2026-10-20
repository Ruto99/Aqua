#pragma once

// 画面のキャプチャー（ゲームの画面を撮って、キャンバスの背景に敷く）
//
//   ・カーソルのある画面（モニター）全体を撮る
//   ・ウィンドウを選んで撮る（RutoEditor のゲームビュー・実行中のゲームなど。後ろに隠れていても撮れる）
//   ・クリップボードの画像を使う（Win + Shift + S などで撮ったもの）
//
// 撮れるのは Windows だけ（ほかの OS では IsSupported() が false。画像ファイルからは使える）

#include <Ruto/Ruto.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace AquaTool::Capture {

bool IsSupported();

struct WindowInfo {
    uintptr_t handle = 0; // HWND
    std::string title;    // UTF-8
    std::string process;  // 実行ファイル名（例: "RutoEditor.exe"）
    int width = 0, height = 0; // 中身（タイトルバーを除く）の大きさ
};

// 撮れるウィンドウの一覧（見えていて、名前があり、Aqua 自身ではないもの。手前から順に）
std::vector<WindowInfo> ListWindows();

// ウィンドウの中身（タイトルバー・枠を除く）を撮る
std::optional<Ruto::Image> CaptureWindow(uintptr_t handle, std::string* error = nullptr);
// マウスカーソルのある画面（モニター）全体を撮る
std::optional<Ruto::Image> CaptureScreenUnderCursor(std::string* error = nullptr);
// クリップボードの画像（無ければ nullopt）
std::optional<Ruto::Image> ClipboardImage(std::string* error = nullptr);

// Aqua のウィンドウを隠す / 戻す（画面全体を撮るときに写り込まないように）
void HideOwnWindows();
void RestoreOwnWindows();

// 24bit の BMP として保存する（Texture::Load・Image::FromFile で読める）
bool SaveBmp(const std::filesystem::path& path, const Ruto::Image& image);

} // namespace AquaTool::Capture
