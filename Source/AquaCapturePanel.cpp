// パネル: キャプチャー（ゲームの画面を撮って、キャンバスの背景に敷く）
//
//   ・ウィンドウ（RutoEditor のゲームビュー・実行中のゲーム）か画面全体を撮る。待ち時間を付けると、その間にゲームへ切り替えられる
//   ・クリップボードの画像・画像ファイル・アセットの画像（ドラッグ）も背景にできる
//   ・撮った画像は AquaUI/Captures に残り、ほかの UI の背景にも使える
//   ・背景は見た目を合わせるための下絵なので、.aqs にだけ保存して .aqua（ゲーム）には書き出さない

#include "AquaApp.h"

#include <Ruto/EditorUI/EditorUI.h>
#include <Ruto/EditorUI/Widgets.h>

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>

namespace AquaTool {

using namespace Ruto::EditorUI;

namespace {

constexpr size_t kMaxCaptureFiles = 60;

std::string LowerExt(const fs::path& p) {
    std::string e = FileSystem::ToUtf8(p.extension());
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return e;
}

bool IsImageFile(const fs::path& p) {
    const std::string e = LowerExt(p);
    return e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".bmp" || e == ".tga";
}

std::string Timestamp() {
    const auto now = std::chrono::current_zone()->to_local(std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
    return std::format("{:%Y%m%d_%H%M%S}", now);
}

bool Contains(const std::string& text, const std::string& word) {
    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    };
    return !word.empty() && lower(text).find(lower(word)) != std::string::npos;
}

// 画像を枠の中に縦横比を保って描く
void DrawFitted(ImDrawList* dl, const Texture& tex, ImVec2 mn, ImVec2 size) {
    if (tex.GetWidth() <= 0 || tex.GetHeight() <= 0) return;
    const float aspect = static_cast<float>(tex.GetWidth()) / static_cast<float>(tex.GetHeight());
    float w = size.x, h = size.x / aspect;
    if (h > size.y) {
        h = size.y;
        w = size.y * aspect;
    }
    const ImVec2 a{mn.x + (size.x - w) * 0.5f, mn.y + (size.y - h) * 0.5f};
    dl->AddImage(ImTextureRef(TextureId(tex)), a, {a.x + w, a.y + h}, {0, 1}, {1, 0});
}

} // namespace

fs::path App::CaptureDir() const {
    return m_Project.GetLayout().SourceDir() / "Captures";
}

// ---------------------------------------------------------------------------
// 背景を設定する
// ---------------------------------------------------------------------------

void App::SetBackground(Document& doc, const fs::path& file) {
    std::error_code ec;
    fs::path rel = fs::relative(file, m_Project.GetDir(), ec);
    if (ec || rel.empty() || FileSystem::ToGenericUtf8(rel).starts_with("..")) rel = file; // プロジェクトの外ならそのまま
    doc.background.image = FileSystem::ToGenericUtf8(rel);
    doc.background.visible = true;
    doc.Save(); // 下絵の設定は元に戻すの対象にしない（変数の値と同じ）
    EnsureBackgroundTexture(doc);
    if (!doc.backgroundTexture) Notify("画像を読めませんでした: " + FileSystem::ToUtf8(file), true);
}

bool App::UseCapturedImage(Document& doc, const Ruto::Image& image) {
    if (!image.IsValid()) return false;
    Ruto::Image img = image;
    // 4K の画面などは基準解像度まで縮めて保存する（ファイルを小さく、描画を軽く）
    const Vec2 ref = doc.asset->referenceResolution;
    const float k = std::min(ref.x / static_cast<float>(img.width), ref.y / static_cast<float>(img.height));
    if (m_CaptureShrink && k < 1.0f) {
        img = img.Resized(std::max(1, static_cast<int>(std::lround(img.width * k))), std::max(1, static_cast<int>(std::lround(img.height * k))));
    }
    const fs::path file = CaptureDir() / FileSystem::FromUtf8(std::format("{}_{}.bmp", ToIdentifier(doc.GetTitle()), Timestamp()));
    fs::path unique = file;
    std::error_code ec;
    for (int n = 2; fs::exists(unique, ec); ++n) unique.replace_filename(FileSystem::FromUtf8(std::format("{}_{}.bmp", FileSystem::ToUtf8(file.stem()), n)));
    if (!Capture::SaveBmp(unique, img)) {
        Notify("撮った画像を保存できません: " + FileSystem::ToUtf8(unique), true);
        return false;
    }
    m_CaptureFilesTime = -100.0;
    SetBackground(doc, unique);
    Log::Info("キャプチャーを背景にしました: {}（{} × {}）", FileSystem::ToUtf8(unique.filename()), img.width, img.height);
    Notify(std::format("背景にしました（{} × {}）", img.width, img.height));
    return true;
}

bool App::UseImageFile(Document& doc, const fs::path& file) {
    if (!IsImageFile(file)) {
        Notify("画像（PNG / JPG / BMP / TGA）を選んでください", true);
        return false;
    }
    std::error_code ec;
    const fs::path abs = fs::weakly_canonical(file, ec);
    const fs::path rel = fs::relative(abs, m_Project.GetDir(), ec);
    if (!ec && !rel.empty() && !FileSystem::ToGenericUtf8(rel).starts_with("..")) {
        SetBackground(doc, abs); // プロジェクトの中の画像はそのまま指す
    } else {
        // プロジェクトの外の画像は Captures にコピーする（ほかの PC でも同じ背景になるように）
        fs::path copy = CaptureDir() / abs.filename();
        fs::create_directories(CaptureDir(), ec);
        for (int n = 2; fs::exists(copy, ec); ++n) copy.replace_filename(FileSystem::FromUtf8(std::format("{}_{}{}", FileSystem::ToUtf8(abs.stem()), n, FileSystem::ToUtf8(abs.extension()))));
        if (!fs::copy_file(abs, copy, ec)) {
            Notify("画像をコピーできません: " + FileSystem::ToUtf8(abs), true);
            return false;
        }
        m_CaptureFilesTime = -100.0;
        SetBackground(doc, copy);
    }
    Notify("背景にしました: " + FileSystem::ToUtf8(abs.filename()));
    return doc.backgroundTexture != nullptr;
}

void App::EnsureBackgroundTexture(Document& doc) {
    if (doc.backgroundLoaded == doc.background.image) return;
    doc.backgroundLoaded = doc.background.image;
    doc.backgroundTexture.reset();
    if (doc.background.image.empty()) return;
    fs::path p = FileSystem::FromUtf8(doc.background.image);
    if (p.is_relative()) p = m_Project.GetDir() / p;
    doc.backgroundTexture = Texture::Load(p, TextureFilter::Linear, TextureWrap::Clamp);
    if (!doc.backgroundTexture) Log::Warn("背景の画像を読めません: {}", FileSystem::ToUtf8(p));
}

// フレームバッファ（width × height）に背景を描く。uiScale: 基準解像度の 1 単位のフレームバッファ上の大きさ
void App::PaintBackground(Document& doc, int width, int height, float uiScale) {
    const CaptureBackground& bg = doc.background;
    if (bg.image.empty() || !bg.visible || (doc.ui.preview && !bg.inPreview)) return;
    EnsureBackgroundTexture(doc);
    if (!doc.backgroundTexture || bg.opacity <= 0.0f) return;
    const float W = static_cast<float>(width), H = static_cast<float>(height);
    const float tw = static_cast<float>(doc.backgroundTexture->GetWidth()), th = static_cast<float>(doc.backgroundTexture->GetHeight());
    if (tw <= 0.0f || th <= 0.0f) return;
    const float ia = tw / th, sa = W / H;
    Vec2 size{W, H};
    switch (bg.fit) {
    case CaptureBackground::Contain: size = ia > sa ? Vec2{W, W / ia} : Vec2{H * ia, H}; break;
    case CaptureBackground::Cover: size = ia > sa ? Vec2{H * ia, H} : Vec2{W, W / ia}; break;
    case CaptureBackground::Original: size = Vec2{tw, th} * uiScale; break; // 画像の 1 画素 = 基準解像度の 1 単位
    default: break;
    }
    size = size * bg.scale;
    const Vec3 center{W * 0.5f + bg.offset.x * uiScale, H * 0.5f + bg.offset.y * uiScale, 0.0f};
    Renderer2D::DrawQuad(center, size, doc.backgroundTexture, Vec4{1.0f, 1.0f, 1.0f, bg.opacity});
}

// ---------------------------------------------------------------------------
// 撮る
// ---------------------------------------------------------------------------

void App::StartCapture(Document& doc) {
    m_CaptureDoc = doc.sourcePath;
    const double now = Platform::GetTime();
    double delay = static_cast<double>(m_CaptureDelay);
    if (m_CaptureSource == CaptureSource::Screen && m_CaptureHideAqua) {
        Capture::HideOwnWindows();
        m_CaptureHidden = true;
        delay = std::max(delay, 0.6); // 隠れるのを待つ
    }
    m_CaptureAt = now + delay;
}

void App::UpdateCapture(double now) {
    if (m_CaptureAt < 0.0 || now < m_CaptureAt) return;
    m_CaptureAt = -1.0;
    std::string error;
    std::optional<Ruto::Image> img = m_CaptureSource == CaptureSource::Window ? Capture::CaptureWindow(m_CaptureWindow, &error)
                                                                               : Capture::CaptureScreenUnderCursor(&error);
    if (m_CaptureHidden) {
        Capture::RestoreOwnWindows();
        m_CaptureHidden = false;
    }
    Document* target = nullptr;
    std::error_code ec;
    for (auto& d : m_Docs) {
        if (fs::equivalent(d->sourcePath, m_CaptureDoc, ec)) target = d.get();
    }
    if (!target) {
        Notify("撮る前に UI が閉じられました", true);
        return;
    }
    if (!img) {
        Notify(error.empty() ? "撮れませんでした" : error, true);
        Log::Error("キャプチャー: {}", error);
        return;
    }
    UseCapturedImage(*target, *img);
}

// ---------------------------------------------------------------------------
// パネル
// ---------------------------------------------------------------------------

void App::DrawBackgroundSettings(Document& doc) {
    CaptureBackground& bg = doc.background;
    EnsureBackgroundTexture(doc);
    if (bg.image.empty()) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("背景はまだありません。左で撮るか、右の撮った画像・アセットの画像を選ぶと、キャンバスの UI の後ろに表示されます。");
        ImGui::PopTextWrapPos();
        return;
    }
    // 小さく見せる
    const float thumbH = std::round(72.0f * m_Scale);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float thumbW = std::min(ImGui::GetContentRegionAvail().x, thumbH * 16.0f / 9.0f);
    ImGui::Dummy({thumbW, thumbH});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, {p.x + thumbW, p.y + thumbH}, ToU32(Theme::kFrame), 4.0f);
    if (doc.backgroundTexture) DrawFitted(dl, *doc.backgroundTexture, p, {thumbW, thumbH});
    else dl->AddText({p.x + 8, p.y + 8}, ToU32(Theme::kError), ICON_FA_TRIANGLE_EXCLAMATION " 読めません");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("%s%s", bg.image.c_str(),
                          doc.backgroundTexture ? std::format("\n{} × {}", doc.backgroundTexture->GetWidth(), doc.backgroundTexture->GetHeight()).c_str() : "");
    }
    ImGui::SameLine();
    ImGui::BeginGroup();
    bool changed = false;
    changed |= ImGui::Checkbox(ICON_FA_EYE " 表示する", &bg.visible);
    changed |= ImGui::Checkbox(ICON_FA_CIRCLE_PLAY " プレビュー中も", &bg.inPreview);
    if (ImGui::Button(ICON_FA_TRASH " 外す")) {
        bg = CaptureBackground{};
        changed = true;
    }
    ImGui::EndGroup();
    if (!bg.image.empty() && BeginPropertyTable("bg-settings")) {
        PropertyLabel("不透明度", "薄くすると UI との重なりが見やすくなります");
        changed |= ImGui::SliderFloat("##opacity", &bg.opacity, 0.0f, 1.0f, "%.2f");
        PropertyLabel("合わせ方");
        const char* fits[] = {"画面いっぱいに引き伸ばす", "画面に収める（縦横比を保つ）", "画面を埋める（はみ出しは切る）", "原寸（1 画素 = 基準解像度の 1）"};
        changed |= ImGui::Combo("##fit", &bg.fit, fits, IM_ARRAYSIZE(fits));
        PropertyLabel("倍率");
        changed |= ImGui::DragFloat("##scale", &bg.scale, 0.005f, 0.05f, 20.0f, "%.3f");
        PropertyLabel("ずらす", "基準解像度の単位");
        changed |= DragVec("##offset", &bg.offset.x, 2, 1.0f, 0.0f, "%.0f");
        EndPropertyTable();
    }
    if (ImGui::SmallButton(ICON_FA_ROTATE_LEFT " 位置と倍率を元に戻す")) {
        bg.scale = 1.0f;
        bg.offset = {0.0f, 0.0f};
        changed = true;
    }
    if (changed) doc.Save();
}

void App::DrawCapturePanel(Document& doc) {
    const double now = Platform::GetTime();
    const bool supported = Capture::IsSupported();
    const ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;
    if (!ImGui::BeginTable("##capture", 3, tableFlags, ImGui::GetContentRegionAvail())) return;
    ImGui::TableSetupColumn("撮る", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("背景", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("撮った画像", ImGuiTableColumnFlags_WidthStretch, 1.3f);
    ImGui::TableNextRow();

    // ---- 撮る ----
    ImGui::TableSetColumnIndex(0);
    ImGui::BeginChild("##capture-take");
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kAccent);
    ImGui::SeparatorText(ICON_FA_CAMERA "  ゲームの画面を撮る");
    ImGui::PopStyleColor();
    ImGui::BeginDisabled(!supported || m_CaptureAt >= 0.0);
    int source = static_cast<int>(m_CaptureSource);
    ImGui::RadioButton(ICON_FA_WINDOW_MAXIMIZE " ウィンドウ", &source, 0);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("RutoEditor のゲームビューや、実行中のゲームのウィンドウを撮ります（後ろに隠れていても撮れます）");
    ImGui::SameLine();
    ImGui::RadioButton(ICON_FA_DESKTOP " 画面全体", &source, 1);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("マウスカーソルのある画面（モニター）全体を撮ります");
    m_CaptureSource = static_cast<CaptureSource>(source);

    if (m_CaptureSource == CaptureSource::Window) {
        // 一覧は開いたときと、最初に表示したときに作り直す
        auto refresh = [&] {
            m_CaptureWindows = Capture::ListWindows();
            // ゲーム・RutoEditor らしいものを先に
            const std::string project = m_Project.GetName();
            std::stable_sort(m_CaptureWindows.begin(), m_CaptureWindows.end(), [&](const Capture::WindowInfo& a, const Capture::WindowInfo& b) {
                auto rank = [&](const Capture::WindowInfo& w) {
                    if (Contains(w.title, project) || Contains(w.process, project)) return 0;
                    if (Contains(w.title, "Ruto") || Contains(w.process, "Ruto")) return 1;
                    return 2;
                };
                return rank(a) < rank(b);
            });
            const bool stillThere = std::any_of(m_CaptureWindows.begin(), m_CaptureWindows.end(), [&](const auto& w) { return w.handle == m_CaptureWindow; });
            if (!stillThere && !m_CaptureWindows.empty()) {
                m_CaptureWindow = m_CaptureWindows.front().handle;
                m_CaptureWindowTitle = m_CaptureWindows.front().title;
            } else if (!stillThere) {
                m_CaptureWindow = 0;
                m_CaptureWindowTitle.clear();
            }
        };
        if (supported && m_CaptureWindows.empty() && m_CaptureWindow == 0) refresh();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x);
        const std::string preview = m_CaptureWindowTitle.empty() ? std::string("（ウィンドウを選ぶ）") : m_CaptureWindowTitle;
        if (ImGui::BeginCombo("##window", preview.c_str(), ImGuiComboFlags_HeightLarge)) {
            if (ImGui::IsWindowAppearing()) refresh();
            for (const Capture::WindowInfo& w : m_CaptureWindows) {
                ImGui::PushID(reinterpret_cast<const void*>(w.handle));
                const std::string label = std::format("{}  ·  {}  {} × {}", w.title, w.process, w.width, w.height);
                if (ImGui::Selectable(label.c_str(), w.handle == m_CaptureWindow)) {
                    m_CaptureWindow = w.handle;
                    m_CaptureWindowTitle = w.title;
                }
                ImGui::PopID();
            }
            if (m_CaptureWindows.empty()) ImGui::TextDisabled("撮れるウィンドウがありません");
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (IconButton(ICON_FA_ROTATE, "ウィンドウの一覧を更新")) refresh();
    } else {
        ImGui::Checkbox("撮る間 Aqua を隠す", &m_CaptureHideAqua);
    }
    ImGui::SetNextItemWidth(140.0f * m_Scale);
    const char* delays[] = {"すぐ撮る", "3 秒後に撮る", "5 秒後に撮る", "10 秒後に撮る"};
    const int delayValues[] = {0, 3, 5, 10};
    int delayIndex = 0;
    for (int i = 0; i < 4; ++i) {
        if (delayValues[i] == m_CaptureDelay) delayIndex = i;
    }
    if (ImGui::Combo("##delay", &delayIndex, delays, 4)) m_CaptureDelay = delayValues[delayIndex];
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("待つ間に、ゲームのメニューを開く・場面を切り替えるなどできます");
    ImGui::SameLine();
    ImGui::Checkbox("基準解像度まで縮める", &m_CaptureShrink);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("基準解像度（%.0f × %.0f）より大きい画像は縮めて保存します", doc.asset->referenceResolution.x, doc.asset->referenceResolution.y);
    ImGui::EndDisabled();

    ImGui::Spacing();
    if (m_CaptureAt >= 0.0) {
        ImGui::PushStyleColor(ImGuiCol_Button, Theme::kAccentDim);
        ImGui::Button(std::format(ICON_FA_STOPWATCH "  あと {:.1f} 秒で撮ります...", std::max(0.0, m_CaptureAt - now)).c_str(), {ImGui::GetContentRegionAvail().x - 90.0f * m_Scale, 32.0f * m_Scale});
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("やめる", {-FLT_MIN, 32.0f * m_Scale})) {
            m_CaptureAt = -1.0;
            if (m_CaptureHidden) {
                Capture::RestoreOwnWindows();
                m_CaptureHidden = false;
            }
        }
    } else {
        ImGui::BeginDisabled(!supported || (m_CaptureSource == CaptureSource::Window && m_CaptureWindow == 0));
        ImGui::PushStyleColor(ImGuiCol_Button, Theme::kAccentDim);
        if (ImGui::Button(ICON_FA_CAMERA "  撮って背景にする", {-FLT_MIN, 32.0f * m_Scale})) StartCapture(doc);
        ImGui::PopStyleColor();
        ImGui::EndDisabled();
    }
    if (!supported) ImGui::TextColored(Theme::kWarning, ICON_FA_TRIANGLE_EXCLAMATION "  画面のキャプチャーは Windows だけで使えます（画像ファイルは使えます）");
    ImGui::BeginDisabled(!supported);
    if (ImGui::Button(ICON_FA_PASTE "  クリップボードの画像")) {
        std::string error;
        if (auto img = Capture::ClipboardImage(&error)) UseCapturedImage(doc, *img);
        else Notify(error, true);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("Win + Shift + S・PrintScreen などで撮った画像や、コピーした画像ファイルを背景にします");
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_IMAGE "  画像ファイル...")) {
        const fs::path docPath = doc.sourcePath;
        Platform::ShowOpenFileDialog(m_Project.GetDir(), "画像（PNG / JPG / BMP / TGA）", "png;jpg;jpeg;bmp;tga", [this, docPath](std::optional<fs::path> p) {
            if (!p) return;
            std::error_code ec;
            for (auto& d : m_Docs) {
                if (fs::equivalent(d->sourcePath, docPath, ec)) UseImageFile(*d, *p);
            }
        });
    }
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("アセットの画像をこのパネルへドラッグしても背景にできます。背景は下絵なので、ゲーム（.aqua）には入りません。");
    ImGui::PopTextWrapPos();
    ImGui::EndChild();

    // ---- 今の背景 ----
    ImGui::TableSetColumnIndex(1);
    ImGui::BeginChild("##capture-background");
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kAccent);
    ImGui::SeparatorText(ICON_FA_IMAGE "  この UI の背景");
    ImGui::PopStyleColor();
    DrawBackgroundSettings(doc);
    ImGui::EndChild();

    // ---- 撮った画像 ----
    ImGui::TableSetColumnIndex(2);
    ImGui::BeginChild("##capture-files");
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kAccent);
    ImGui::SeparatorText(ICON_FA_IMAGES "  撮った画像（AquaUI/Captures）");
    ImGui::PopStyleColor();
    if (now - m_CaptureFilesTime > 1.0) {
        m_CaptureFilesTime = now;
        m_CaptureFiles.clear();
        std::error_code ec;
        std::vector<std::pair<fs::file_time_type, fs::path>> found;
        for (fs::directory_iterator it(CaptureDir(), ec), end; !ec && it != end; it.increment(ec)) {
            if (it->is_regular_file(ec) && IsImageFile(it->path())) found.emplace_back(it->last_write_time(ec), it->path());
        }
        std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first > b.first; }); // 新しい順
        for (size_t i = 0; i < found.size() && i < kMaxCaptureFiles; ++i) m_CaptureFiles.push_back(found[i].second);
        // もう無いファイルのサムネイルは手放す
        std::erase_if(m_CaptureThumbs, [&](const auto& kv) {
            return std::none_of(m_CaptureFiles.begin(), m_CaptureFiles.end(), [&](const fs::path& f) { return FileSystem::ToUtf8(f) == kv.first; });
        });
    }
    if (m_CaptureFiles.empty()) ImGui::TextDisabled("まだありません");
    const float cellW = std::round(128.0f * m_Scale), cellH = std::round(72.0f * m_Scale);
    const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / (cellW + ImGui::GetStyle().ItemSpacing.x)));
    std::error_code ec;
    fs::path removeFile;
    for (size_t i = 0; i < m_CaptureFiles.size(); ++i) {
        const fs::path& f = m_CaptureFiles[i];
        const std::string key = FileSystem::ToUtf8(f);
        if (i % static_cast<size_t>(columns) != 0) ImGui::SameLine();
        ImGui::PushID(key.c_str());
        auto [thumbIt, firstTime] = m_CaptureThumbs.try_emplace(key);
        if (firstTime) thumbIt->second = Texture::Load(f, TextureFilter::Linear, TextureWrap::Clamp); // 読めなくても毎フレーム読み直さない
        const std::shared_ptr<Texture>& thumb = thumbIt->second;
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##shot", {cellW, cellH});
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const bool hovered = ImGui::IsItemHovered();
        fs::path bgPath = FileSystem::FromUtf8(doc.background.image);
        if (bgPath.is_relative()) bgPath = m_Project.GetDir() / bgPath;
        const bool current = !doc.background.image.empty() && fs::equivalent(bgPath, f, ec);
        dl->AddRectFilled(p, {p.x + cellW, p.y + cellH}, ToU32(hovered ? Theme::kFrameHover : Theme::kFrame), 4.0f);
        if (thumb) DrawFitted(dl, *thumb, {p.x + 2, p.y + 2}, {cellW - 4, cellH - 4});
        if (current) dl->AddRect(p, {p.x + cellW, p.y + cellH}, ToU32(Theme::kAccent), 4.0f, 0, 2.0f);
        if (hovered) ImGui::SetTooltip("%s%s\nクリックでこの UI の背景にする", FileSystem::ToUtf8(f.filename()).c_str(),
                                       thumb ? std::format("\n{} × {}", thumb->GetWidth(), thumb->GetHeight()).c_str() : "");
        if (ImGui::IsItemClicked()) SetBackground(doc, f);
        if (ImGui::BeginPopupContextItem("shot-menu")) {
            if (ImGui::MenuItem(ICON_FA_IMAGE "  この UI の背景にする")) SetBackground(doc, f);
            if (ImGui::MenuItem(ICON_FA_FOLDER "  エクスプローラーで表示")) Platform::OpenInFileBrowser(f.parent_path());
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_TRASH "  削除")) removeFile = f;
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    if (!removeFile.empty()) {
        m_CaptureThumbs.erase(FileSystem::ToUtf8(removeFile));
        fs::remove(removeFile, ec);
        m_CaptureFilesTime = -100.0;
        // この画像を背景にしていた UI（開いているもの）からは外す
        for (auto& d : m_Docs) {
            fs::path p = FileSystem::FromUtf8(d->background.image);
            if (p.is_relative()) p = m_Project.GetDir() / p;
            if (!d->background.image.empty() && !fs::exists(p, ec)) {
                d->background = CaptureBackground{};
                d->Save();
            }
        }
    }
    ImGui::EndChild();
    ImGui::EndTable();

    // アセットの画像・エクスプローラーからのドロップ
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    if (ImGui::BeginDragDropTargetCustom(win->InnerRect, win->ID)) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kAssetPayload)) {
            const std::string rel(static_cast<const char*>(p->Data), static_cast<size_t>(p->DataSize));
            UseImageFile(doc, Assets::GetPath(rel));
        }
        ImGui::EndDragDropTarget();
    }
}

} // namespace AquaTool
