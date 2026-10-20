#include "AquaCapture.h"

#include <algorithm>
#include <cstring>
#include <fstream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#endif

namespace AquaTool::Capture {

using Ruto::Image;

bool SaveBmp(const std::filesystem::path& path, const Image& image) {
    if (!image.IsValid()) return false;
    const int w = image.width, h = image.height;
    const uint32_t rowSize = (static_cast<uint32_t>(w) * 3u + 3u) & ~3u;
    const uint32_t dataSize = rowSize * static_cast<uint32_t>(h);
    std::vector<uint8_t> out(54 + dataSize, 0);
    auto put16 = [&](size_t at, uint16_t v) { out[at] = static_cast<uint8_t>(v); out[at + 1] = static_cast<uint8_t>(v >> 8); };
    auto put32 = [&](size_t at, uint32_t v) { for (int i = 0; i < 4; ++i) out[at + static_cast<size_t>(i)] = static_cast<uint8_t>(v >> (8 * i)); };
    out[0] = 'B';
    out[1] = 'M';
    put32(2, static_cast<uint32_t>(out.size()));
    put32(10, 54);           // 画素の始まり
    put32(14, 40);           // BITMAPINFOHEADER
    put32(18, static_cast<uint32_t>(w));
    put32(22, static_cast<uint32_t>(h)); // 正 = 下の行から
    put16(26, 1);
    put16(28, 24);
    put32(34, dataSize);
    put32(38, 2835);         // 72 dpi
    put32(42, 2835);
    for (int y = 0; y < h; ++y) {
        const uint8_t* src = image.pixels.data() + static_cast<size_t>(h - 1 - y) * static_cast<size_t>(w) * 4;
        uint8_t* dst = out.data() + 54 + static_cast<size_t>(y) * rowSize;
        for (int x = 0; x < w; ++x) {
            dst[x * 3 + 0] = src[x * 4 + 2];
            dst[x * 3 + 1] = src[x * 4 + 1];
            dst[x * 3 + 2] = src[x * 4 + 0];
        }
    }
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
    return static_cast<bool>(f);
}

#if defined(_WIN32)

namespace {

#ifndef PW_CLIENTONLY
#define PW_CLIENTONLY 0x00000001
#endif
#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

std::string ToUtf8(const wchar_t* s, int len = -1) {
    if (!s) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, s, len, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s, len, out.data(), n, nullptr, nullptr);
    if (len < 0 && !out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

// 撮る間だけ、ほかのウィンドウの大きさを本当の画素で扱う（拡大表示の画面でも縮んだ画像にならないように）
class DpiScope {
public:
    DpiScope() {
        using Fn = DPI_AWARENESS_CONTEXT(WINAPI*)(DPI_AWARENESS_CONTEXT);
        if (HMODULE user = GetModuleHandleW(L"user32.dll")) {
            m_Set = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(user, "SetThreadDpiAwarenessContext")));
            if (m_Set) m_Old = m_Set(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }
    ~DpiScope() {
        if (m_Set && m_Old) m_Set(m_Old);
    }

private:
    DPI_AWARENESS_CONTEXT(WINAPI* m_Set)(DPI_AWARENESS_CONTEXT) = nullptr;
    DPI_AWARENESS_CONTEXT m_Old = nullptr;
};

// メモリの DC のビットマップ → Image（RGBA・左上から）
std::optional<Image> ReadBitmap(HDC dc, HBITMAP bmp, int w, int h) {
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // 上の行から
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    Image img;
    img.width = w;
    img.height = h;
    img.pixels.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    if (GetDIBits(dc, bmp, 0, static_cast<UINT>(h), img.pixels.data(), &bi, DIB_RGB_COLORS) == 0) return std::nullopt;
    for (size_t i = 0; i < img.pixels.size(); i += 4) {
        std::swap(img.pixels[i], img.pixels[i + 2]); // BGRA → RGBA
        img.pixels[i + 3] = 255;
    }
    return img;
}

bool IsAllBlack(const Image& img) {
    for (size_t i = 0; i < img.pixels.size(); i += 4) {
        if (img.pixels[i] | img.pixels[i + 1] | img.pixels[i + 2]) return false;
    }
    return true;
}

// 画面（デスクトップ）の一部を撮る（スクリーン座標）
std::optional<Image> CaptureScreenRect(int x, int y, int w, int h, std::string* error) {
    if (w <= 0 || h <= 0) {
        if (error) *error = "撮る範囲がありません";
        return std::nullopt;
    }
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, w, h);
    HGDIOBJ old = SelectObject(mem, bmp);
    const bool ok = BitBlt(mem, 0, 0, w, h, screen, x, y, SRCCOPY | CAPTUREBLT) != FALSE;
    SelectObject(mem, old);
    std::optional<Image> img = ok ? ReadBitmap(mem, bmp, w, h) : std::nullopt;
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    if (!img && error) *error = "画面を撮れませんでした";
    return img;
}

std::string ProcessName(HWND hwnd) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) return {};
    wchar_t buf[MAX_PATH];
    DWORD size = MAX_PATH;
    std::string name;
    if (QueryFullProcessImageNameW(proc, 0, buf, &size)) {
        const wchar_t* slash = wcsrchr(buf, L'\\');
        name = ToUtf8(slash ? slash + 1 : buf);
    }
    CloseHandle(proc);
    return name;
}

bool IsCloaked(HWND hwnd) {
    BOOL cloaked = FALSE;
    return SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked;
}

std::vector<HWND> s_Hidden;

} // namespace

bool IsSupported() { return true; }

std::vector<WindowInfo> ListWindows() {
    DpiScope dpi;
    std::vector<WindowInfo> out;
    EnumWindows(
        [](HWND hwnd, LPARAM param) -> BOOL {
            auto& list = *reinterpret_cast<std::vector<WindowInfo>*>(param);
            if (!IsWindowVisible(hwnd) || IsIconic(hwnd) || IsCloaked(hwnd)) return TRUE;
            if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
            if (GetWindow(hwnd, GW_OWNER)) return TRUE; // ダイアログなど
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid == GetCurrentProcessId()) return TRUE;
            wchar_t title[512];
            const int len = GetWindowTextW(hwnd, title, 512);
            if (len <= 0) return TRUE;
            RECT rc{};
            GetClientRect(hwnd, &rc);
            if (rc.right - rc.left < 64 || rc.bottom - rc.top < 64) return TRUE;
            WindowInfo info;
            info.handle = reinterpret_cast<uintptr_t>(hwnd);
            info.title = ToUtf8(title, len);
            info.process = ProcessName(hwnd);
            info.width = rc.right - rc.left;
            info.height = rc.bottom - rc.top;
            if (info.process == "explorer.exe" && info.title == "Program Manager") return TRUE;
            list.push_back(std::move(info));
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&out));
    return out;
}

std::optional<Image> CaptureWindow(uintptr_t handle, std::string* error) {
    DpiScope dpi;
    HWND hwnd = reinterpret_cast<HWND>(handle);
    if (!IsWindow(hwnd)) {
        if (error) *error = "ウィンドウが見つかりません（閉じられました）";
        return std::nullopt;
    }
    if (IsIconic(hwnd)) {
        if (error) *error = "ウィンドウが最小化されています。元に戻してから撮ってください";
        return std::nullopt;
    }
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
        if (error) *error = "ウィンドウの中身がありません";
        return std::nullopt;
    }
    // まずウィンドウ自身に描かせる（後ろに隠れていても撮れる。DirectX / OpenGL も PW_RENDERFULLCONTENT で撮れる）
    HDC wdc = GetDC(hwnd);
    HDC mem = CreateCompatibleDC(wdc);
    HBITMAP bmp = CreateCompatibleBitmap(wdc, w, h);
    HGDIOBJ old = SelectObject(mem, bmp);
    const bool printed = PrintWindow(hwnd, mem, PW_CLIENTONLY | PW_RENDERFULLCONTENT) != FALSE;
    SelectObject(mem, old);
    std::optional<Image> img = printed ? ReadBitmap(mem, bmp, w, h) : std::nullopt;
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(hwnd, wdc);
    if (img && !IsAllBlack(*img)) return img;
    // 撮れなかった（真っ黒）ときは、画面に見えている所をそのまま撮る
    POINT origin{0, 0};
    ClientToScreen(hwnd, &origin);
    return CaptureScreenRect(origin.x, origin.y, w, h, error);
}

std::optional<Image> CaptureScreenUnderCursor(std::string* error) {
    DpiScope dpi;
    POINT cursor{};
    GetCursorPos(&cursor);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST), &mi)) {
        if (error) *error = "画面（モニター）が見つかりません";
        return std::nullopt;
    }
    const RECT& r = mi.rcMonitor;
    return CaptureScreenRect(r.left, r.top, r.right - r.left, r.bottom - r.top, error);
}

std::optional<Image> ClipboardImage(std::string* error) {
    if (!OpenClipboard(nullptr)) {
        if (error) *error = "クリップボードを開けません";
        return std::nullopt;
    }
    std::optional<Image> img;
    if (HANDLE h = GetClipboardData(CF_DIB)) {
        // DIB の前に BMP のファイルヘッダーを付けて、stb_image（Image::FromMemory）に読ませる
        if (const auto* bih = static_cast<const BITMAPINFOHEADER*>(GlobalLock(h))) {
            const size_t size = GlobalSize(h);
            uint32_t colors = bih->biClrUsed;
            if (colors == 0 && bih->biBitCount <= 8) colors = 1u << bih->biBitCount;
            uint32_t offset = 14 + bih->biSize + colors * 4;
            if (bih->biCompression == BI_BITFIELDS && bih->biSize == sizeof(BITMAPINFOHEADER)) offset += 12;
            std::vector<uint8_t> file(14 + size, 0);
            file[0] = 'B';
            file[1] = 'M';
            const uint32_t total = static_cast<uint32_t>(file.size());
            std::memcpy(file.data() + 2, &total, 4);
            std::memcpy(file.data() + 10, &offset, 4);
            std::memcpy(file.data() + 14, bih, size);
            GlobalUnlock(h);
            img = Image::FromMemory(file.data(), file.size());
            if (img) {
                for (size_t i = 3; i < img->pixels.size(); i += 4) img->pixels[i] = 255; // スクリーンショットは不透明
            }
        }
    } else if (HANDLE drop = GetClipboardData(CF_HDROP)) {
        // エクスプローラーでコピーした画像ファイル
        wchar_t path[MAX_PATH];
        if (DragQueryFileW(static_cast<HDROP>(drop), 0, path, MAX_PATH)) img = Image::FromFile(std::filesystem::path(path));
    }
    CloseClipboard();
    if (!img && error) *error = "クリップボードに画像がありません（Win + Shift + S などで撮ってから押してください）";
    return img;
}

void HideOwnWindows() {
    s_Hidden.clear();
    EnumWindows(
        [](HWND hwnd, LPARAM) -> BOOL {
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd) && !IsIconic(hwnd)) s_Hidden.push_back(hwnd);
            return TRUE;
        },
        0);
    for (HWND hwnd : s_Hidden) ShowWindow(hwnd, SW_MINIMIZE);
}

void RestoreOwnWindows() {
    for (HWND hwnd : s_Hidden) {
        ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
    }
    s_Hidden.clear();
}

#else // ほかの OS: 撮れない（画像ファイルからは使える）

bool IsSupported() { return false; }
std::vector<WindowInfo> ListWindows() { return {}; }
std::optional<Image> CaptureWindow(uintptr_t, std::string* error) {
    if (error) *error = "画面のキャプチャーは Windows だけで使えます";
    return std::nullopt;
}
std::optional<Image> CaptureScreenUnderCursor(std::string* error) { return CaptureWindow(0, error); }
std::optional<Image> ClipboardImage(std::string* error) { return CaptureWindow(0, error); }
void HideOwnWindows() {}
void RestoreOwnWindows() {}

#endif

} // namespace AquaTool::Capture
