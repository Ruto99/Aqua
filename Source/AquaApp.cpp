#include "AquaApp.h"

#include <Ruto/EditorUI/EditorUI.h>
#include <Ruto/EditorUI/Widgets.h>

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <chrono>
#include <format>

namespace AquaTool {

using namespace Ruto::EditorUI;

namespace {

constexpr size_t kMaxLogLines = 2000;
constexpr size_t kMaxRecent = 12;

ApplicationDesc MakeDesc() {
    ApplicationDesc desc;
    desc.window.title = "Aqua";
    desc.window.width = 1600;
    desc.window.height = 940;
    desc.window.minWidth = 1000;
    desc.window.minHeight = 620;
    desc.window.msaaSamples = 0;
    desc.uiFontSize = 15.0f;
    desc.updateScene = false; // シーンは使わない（UI だけ）
    desc.iconFile = Platform::GetExecutableDir() / "Aqua.png"; // ビルドで実行ファイルの隣にコピーされる
    return desc;
}

fs::path AquaDataDir() {
    return Platform::GetUserDataDir() / "Aqua";
}

std::string TimeOfDay() {
    const auto now = std::chrono::current_zone()->to_local(std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
    return std::format("{:%H:%M:%S}", now);
}

} // namespace

App::App(StartupOptions options) : Application(MakeDesc()), m_Options(std::move(options)) {}

App::~App() = default;

// ---------------------------------------------------------------------------
// 起動・終了
// ---------------------------------------------------------------------------

void App::OnStart() {
    std::error_code ec;
    fs::create_directories(AquaDataDir(), ec);
    // RutoEngine 風だった前のレイアウト（Layout.ini）は使わず、Aqua のレイアウトから始める
    const fs::path ini = AquaDataDir() / "AquaLayout.ini";
    m_ResetLayout = !fs::exists(ini, ec);
    // この Aqua.exe の場所を登録する（RutoEditor・RutoLauncher がどこにあっても見つけて起動できるように）
    Ruto::Aqua::RegisterExecutable(Platform::GetExecutableDir() / "Aqua.exe");
    EditorUI::InitOptions options;
    options.iniPath = ini;
    EditorUI::Init(GetWindow(), options);
    m_Scale = EditorUI::GetScale();
    ApplyAquaStyle(m_Scale);

    m_LogSink = Log::AddSink([this](Log::Level level, std::string_view message) {
        m_Log.push_back({level, std::string(message)});
        if (m_Log.size() > kMaxLogLines) m_Log.pop_front();
        m_LogScroll = true;
    });
    Log::Info("Aqua を起動しました");

    if (!m_Options.project.empty()) {
        if (OpenProject(m_Options.project)) {
            for (const fs::path& f : m_Options.open) OpenDocument(f);
        }
    } else if (!m_Options.open.empty()) {
        // ファイルだけ渡されたら、そのファイルを含むプロジェクトを探す（Assets か AquaUI の親）
        for (fs::path dir = m_Options.open.front().parent_path(); !dir.empty() && dir != dir.root_path(); dir = dir.parent_path()) {
            if (fs::is_directory(dir / "Assets", ec) && OpenProject(dir)) {
                for (const fs::path& f : m_Options.open) OpenDocument(f);
                break;
            }
        }
    }
}

void App::OnShutdown() {
    for (auto& doc : m_Docs) {
        if (doc->HasPending()) doc->Commit();
    }
    m_Docs.clear(); // OpenGL のリソースを EditorUI より先に手放す
    m_Link.reset();
    if (m_LogSink >= 0) Log::RemoveSink(m_LogSink);
    EditorUI::Shutdown();
}

bool App::OnCloseRequested() {
    // 編集はいつも保存されているので、確定していない分だけ保存して閉じる
    for (auto& doc : m_Docs) {
        if (doc->HasPending()) doc->Commit();
    }
    return true;
}

// ---------------------------------------------------------------------------
// プロジェクト・文書
// ---------------------------------------------------------------------------

bool App::OpenProject(const fs::path& dir) {
    CloseProject();
    std::string error;
    if (!m_Project.Open(dir, &error)) {
        m_Error = error;
        Log::Error("{}", error);
        return false;
    }
    m_Error.clear();
    m_Link = Ruto::Aqua::LinkHost::Create(m_Project.GetDir());
    if (!m_Link) {
        // 同じプロジェクトを開いている Aqua がある → そちらに任せて閉じる
        Ruto::Aqua::PostRequest(m_Project.GetDir(), {});
        for (const fs::path& f : m_Options.open) Ruto::Aqua::PostRequest(m_Project.GetDir(), f);
        m_Options.open.clear();
        Log::Warn("このプロジェクトはほかの Aqua で開かれています");
        Quit();
        return false;
    }
    AddRecentProject(m_Project.GetDir());
    GetWindow().SetTitle(std::format("Aqua — {}", m_Project.GetName()));
    Log::Info("プロジェクトを開きました: {}", FileSystem::ToUtf8(m_Project.GetDir()));
    WriteSession();
    return true;
}

void App::CloseProject() {
    for (auto& doc : m_Docs) {
        if (doc->HasPending()) doc->Commit();
    }
    m_Docs.clear();
    m_Active = -1;
    m_Link.reset();
    m_Project = Project{};
    GetWindow().SetTitle("Aqua");
}

Document* App::ActiveDocument() {
    if (m_Active < 0 || m_Active >= static_cast<int>(m_Docs.size())) return nullptr;
    return m_Docs[static_cast<size_t>(m_Active)].get();
}

Document* App::OpenDocument(const fs::path& file) {
    if (!m_Project.IsOpen() || file.empty()) return nullptr;
    std::error_code ec;
    const fs::path abs = fs::weakly_canonical(file, ec);
    // もう開いている（同じ編集用ファイル・同じ書き出し先）ならそのタブを前に
    const std::string rel = m_Project.MakeAssetRelative(abs);
    for (size_t i = 0; i < m_Docs.size(); ++i) {
        const Document& d = *m_Docs[i];
        if (fs::equivalent(d.sourcePath, abs, ec) || (!rel.empty() && d.output == rel)) {
            m_Active = static_cast<int>(i);
            m_FocusTab = m_Active;
            return m_Docs[i].get();
        }
    }
    std::string error;
    auto doc = Document::Open(m_Project, abs, &error);
    if (!doc) {
        Notify(error, true);
        Log::Error("{}", error);
        return nullptr;
    }
    // .rui を取り込んだときなど、開いた後の編集用ファイルが開いている文書と同じこともある
    for (size_t i = 0; i < m_Docs.size(); ++i) {
        if (fs::equivalent(m_Docs[i]->sourcePath, doc->sourcePath, ec)) {
            m_Active = static_cast<int>(i);
            m_FocusTab = m_Active;
            return m_Docs[i].get();
        }
    }
    m_Docs.push_back(std::move(doc));
    m_Active = static_cast<int>(m_Docs.size()) - 1;
    m_FocusTab = m_Active;
    WriteSession();
    return m_Docs.back().get();
}

void App::CloseDocument(size_t index) {
    if (index >= m_Docs.size()) return;
    if (m_Docs[index]->HasPending()) m_Docs[index]->Commit();
    m_Docs.erase(m_Docs.begin() + static_cast<std::ptrdiff_t>(index));
    if (m_Active >= static_cast<int>(m_Docs.size())) m_Active = static_cast<int>(m_Docs.size()) - 1;
    WriteSession();
}

void App::NewDocument(const std::string& name) {
    if (!m_Project.IsOpen()) return;
    std::string base = name.empty() ? std::string("NewUI") : name;
    std::erase_if(base, [](char c) { return c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|'; });
    if (base.empty()) base = "NewUI";
    fs::path folder = m_Project.GetLayout().SourceDir();
    if (!m_NewFolder.empty()) folder /= FileSystem::FromUtf8(m_NewFolder);
    fs::path source = folder / FileSystem::FromUtf8(base + Ruto::Aqua::kSourceExtension);
    std::error_code ec;
    for (int n = 1; fs::exists(source, ec) || !m_Project.FindSourceFor(m_Project.DefaultOutputFor(source)).empty(); ++n)
        source = folder / FileSystem::FromUtf8(std::format("{}{}{}", base, n, Ruto::Aqua::kSourceExtension));
    auto doc = Document::CreateNew(m_Project, source, m_Project.DefaultOutputFor(source));
    Log::Info("UI を作成しました: {} → Assets/{}", FileSystem::ToUtf8(source.filename()), doc->output);
    m_Docs.push_back(std::move(doc));
    m_Active = static_cast<int>(m_Docs.size()) - 1;
    m_FocusTab = m_Active;
    WriteSession();
}

void App::ExportAll() {
    if (!m_Project.IsOpen()) return;
    m_Project.Refresh(true);
    int ok = 0, failed = 0;
    for (const Project::SourceInfo& s : m_Project.GetSources()) {
        Document* open = nullptr;
        std::error_code ec;
        for (auto& d : m_Docs) {
            if (fs::equivalent(d->sourcePath, s.path, ec)) open = d.get();
        }
        if (open) {
            if (open->HasPending()) open->Commit();
            open->Export() ? ++ok : ++failed;
            continue;
        }
        std::string error;
        auto doc = Document::Open(m_Project, s.path, &error);
        if (doc && doc->Export()) {
            ++ok;
        } else {
            ++failed;
            Log::Error("{}", error.empty() ? "書き出せませんでした: " + FileSystem::ToUtf8(s.path) : error);
        }
    }
    Notify(std::format("{} 個の UI を書き出しました{}", ok, failed ? std::format("（{} 個は失敗）", failed) : std::string()), failed > 0);
}

void App::HandleLinkRequests() {
    if (!m_Link) return;
    const double now = Platform::GetTime();
    if (now - m_LastLinkPoll < 0.3) return;
    m_LastLinkPoll = now;
    bool any = false;
    for (const fs::path& f : m_Link->TakeRequests()) {
        any = true;
        if (!f.empty()) OpenDocument(f);
    }
    if (any) GetWindow().Raise();
}

void App::WriteSession() {
    if (!m_Link) return;
    m_LastSession = Platform::GetTime();
    Json j = Json::MakeObject();
    j["app"] = "Aqua";
    j["heartbeat"] = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    Json docs = Json::MakeArray();
    for (const auto& d : m_Docs) {
        Json dj = Json::MakeObject();
        dj["source"] = FileSystem::ToUtf8(d->sourcePath);
        dj["output"] = d->output;
        docs.Push(std::move(dj));
    }
    j["documents"] = std::move(docs);
    if (Document* doc = ActiveDocument()) j["active"] = doc->output;
    m_Link->WriteSession(j);
}

std::vector<fs::path> App::LoadRecentProjects() const {
    std::vector<fs::path> out;
    auto add = [&](const fs::path& p) {
        if (p.empty() || std::find(out.begin(), out.end(), p) != out.end()) return;
        out.push_back(p);
    };
    if (const auto text = FileSystem::ReadTextFile(AquaDataDir() / "recent.json")) {
        if (const auto j = Json::Parse(*text)) {
            for (const Json& e : (*j)["projects"].Elements()) add(FileSystem::FromUtf8(e.AsString()));
        }
    }
    // RutoLauncher が知っているプロジェクトも出す
    if (const auto text = FileSystem::ReadTextFile(Platform::GetUserDataDir() / "projects.json")) {
        if (const auto j = Json::Parse(*text)) {
            for (const Json& e : (*j)["projects"].Elements()) add(FileSystem::FromUtf8(e["path"].AsString()));
        }
    }
    return out;
}

void App::AddRecentProject(const fs::path& dir) {
    std::vector<fs::path> list;
    if (const auto text = FileSystem::ReadTextFile(AquaDataDir() / "recent.json")) {
        if (const auto j = Json::Parse(*text)) {
            for (const Json& e : (*j)["projects"].Elements()) list.push_back(FileSystem::FromUtf8(e.AsString()));
        }
    }
    std::erase(list, dir);
    list.insert(list.begin(), dir);
    if (list.size() > kMaxRecent) list.resize(kMaxRecent);
    Json j = Json::MakeObject();
    Json arr = Json::MakeArray();
    for (const fs::path& p : list) arr.Push(FileSystem::ToUtf8(p));
    j["projects"] = std::move(arr);
    FileSystem::WriteTextFile(AquaDataDir() / "recent.json", j.Dump(2));
}

void App::Notify(std::string text, bool error) {
    m_Notice = std::move(text);
    m_NoticeError = error;
    m_NoticeTime = Platform::GetTime();
}

// ---------------------------------------------------------------------------
// 毎フレーム
// ---------------------------------------------------------------------------

void App::OnUpdate(float /*dt*/) {
    const double now = Platform::GetTime();
    HandleLinkRequests();
    if (m_Link && now - m_LastSession > 2.0) WriteSession();
    if (m_Project.IsOpen() && now - m_LastEditorCheck > 1.0) {
        m_LastEditorCheck = now;
        m_EditorRunning = Platform::FileLock::IsLocked(m_Project.GetDir() / "build" / "editor.lock");
    }

    EditorUI::BeginFrame();
    DrawUI();
    if (Document* doc = ActiveDocument()) {
        doc->Update(now);
        RenderDocument(*doc);
    }
    for (auto& d : m_Docs) {
        if (d.get() != ActiveDocument()) d->Update(now);
    }
    EditorUI::EndFrame();
}

void App::OnUIRender() {
    EditorUI::Render();
}

void App::DrawUI() {
    ImGui::GetStyle().FontSizeBase = std::round(15.0f * m_Scale);
    HandleShortcuts();
    DrawMenuBar();
    const float statusH = std::round(26.0f * m_Scale);
    if (!m_Project.IsOpen()) {
        DrawWelcome();
        DrawStatusBar(statusH);
        DrawPopups();
        return;
    }
    DrawDockSpace();

    // パネル
    if (ImGui::Begin(ICON_FA_FOLDER_TREE "  プロジェクト###Project")) DrawProjectPanel();
    ImGui::End();
    if (ImGui::Begin(ICON_FA_PALETTE "  パレット###Palette")) DrawPalette(ActiveDocument());
    ImGui::End();
    if (ImGui::Begin(ICON_FA_LIST "  階層###Hierarchy")) {
        if (Document* doc = ActiveDocument()) DrawHierarchy(*doc);
        else ImGui::TextDisabled("UI を開くと、ここにウィジェットの木が表示されます");
    }
    ImGui::End();
    DrawDocumentArea();
    if (ImGui::Begin(ICON_FA_SLIDERS "  詳細###Details")) {
        if (Document* doc = ActiveDocument()) DrawDetails(*doc);
    }
    ImGui::End();
    if (ImGui::Begin(ICON_FA_BRUSH "  スタイル###Styles")) {
        if (Document* doc = ActiveDocument()) DrawStylesPanel(*doc);
    }
    ImGui::End();
    if (ImGui::Begin(ICON_FA_SQUARE_ROOT_VARIABLE "  変数###Variables")) {
        if (Document* doc = ActiveDocument()) DrawVariablesPanel(*doc);
    }
    ImGui::End();
    if (ImGui::Begin(ICON_FA_WAND_MAGIC_SPARKLES "  アニメーション###Animation", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        if (Document* doc = ActiveDocument()) {
            DrawAnimations(*doc);
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) HandleDesignerKeys(*doc); // キーの削除など
        }
    }
    ImGui::End();
    if (ImGui::Begin(ICON_FA_IMAGES "  アセット###Assets")) DrawAssetsPanel();
    ImGui::End();
    // 後から足したパネルは、前のレイアウト（Layout.ini）で開いても既定の場所にドッキングする
    auto dockWith = [](const char* existing) {
        if (ImGuiWindow* w = ImGui::FindWindowByName(existing); w && w->DockId) ImGui::SetNextWindowDockID(w->DockId, ImGuiCond_FirstUseEver);
    };
    dockWith("###Assets");
    if (m_FocusShaderPanel) {
        if (ImGuiWindow* w = ImGui::FindWindowByName("###Shader")) {
            if (w->DockNode && w->DockNode->TabBar) w->DockNode->TabBar->NextSelectedTabId = w->TabId;
            ImGui::FocusWindow(w);
        }
        m_FocusShaderPanel = false;
    }
    if (ImGui::Begin(ICON_FA_FILL_DRIP "  シェーダー###Shader")) DrawShaderPanel();
    ImGui::End();
    dockWith("###Details");
    if (ImGui::Begin(ICON_FA_TABLE_CELLS "  整列###Arrange")) {
        if (Document* doc = ActiveDocument()) {
            DrawArrangePanel(*doc);
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) HandleDesignerKeys(*doc);
        } else {
            ImGui::TextDisabled("UI を開くと、選んだウィジェットを揃えたり並べたりできます");
        }
    }
    ImGui::End();
    if (ImGui::Begin(ICON_FA_TERMINAL "  ログ###Log")) DrawLogPanel();
    ImGui::End();
    if (m_DefaultTabsFrames > 0 && --m_DefaultTabsFrames == 0) {
        for (const char* name : {"###Palette", "###Details", "###Animation", "###Canvas"}) {
            ImGuiWindow* w = ImGui::FindWindowByName(name);
            if (!w) continue;
            if (w->DockNode && w->DockNode->TabBar) w->DockNode->TabBar->NextSelectedTabId = w->TabId;
            ImGui::FocusWindow(w);
        }
    }

    DrawStatusBar(statusH);
    DrawPopups();
}

// ---------------------------------------------------------------------------
// メニュー
// ---------------------------------------------------------------------------

void App::DrawMenuBar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {8.0f * m_Scale, 6.0f * m_Scale});
    if (!ImGui::BeginMainMenuBar()) {
        ImGui::PopStyleVar();
        return;
    }
    ImGui::PopStyleVar();
    DrawMenuBrand();
    Document* doc = ActiveDocument();
    const bool open = m_Project.IsOpen();

    if (ImGui::BeginMenu("ファイル")) {
        if (ImGui::MenuItem(ICON_FA_FILE_CIRCLE_PLUS "  新しい UI", "Ctrl+N", false, open)) m_OpenNewPopup = true;
        if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  UI を開く...", "Ctrl+O", false, open)) {
            Platform::ShowOpenFileDialog(m_Project.GetLayout().SourceDir(), "Aqua の UI（.aqs / .aqua / .rui）", "aqs;aqua;rui",
                                         [this](std::optional<fs::path> p) {
                                             if (p) OpenDocument(*p);
                                         });
        }
        if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK "  保存して書き出す", "Ctrl+S", false, doc != nullptr)) {
            doc->Commit();
            doc->Save();
            doc->Export();
            Notify("保存して書き出しました: Assets/" + doc->output);
        }
        if (ImGui::MenuItem(ICON_FA_FILE_EXPORT "  すべての UI を書き出す", "Ctrl+Shift+E", false, open)) ExportAll();
        if (ImGui::MenuItem(ICON_FA_FILE_IMPORT "  .rui を取り込む...", nullptr, false, open)) {
            Platform::ShowOpenFileDialog(m_Project.GetLayout().AssetsDir(), "以前の UI（.rui）", "rui", [this](std::optional<fs::path> p) {
                if (p) OpenDocument(*p);
            });
        }
        if (ImGui::MenuItem(ICON_FA_XMARK "  閉じる", "Ctrl+W", false, doc != nullptr)) CloseDocument(static_cast<size_t>(m_Active));
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_FOLDER "  プロジェクトを開く...")) {
            Platform::ShowFolderDialog(m_Project.IsOpen() ? m_Project.GetDir() : Platform::GetDocumentsDir(), [this](std::optional<fs::path> p) {
                if (p) OpenProject(*p);
            });
        }
        if (ImGui::BeginMenu(ICON_FA_CLOCK_ROTATE_LEFT "  最近のプロジェクト")) {
            for (const fs::path& p : LoadRecentProjects()) {
                if (ImGui::MenuItem(FileSystem::ToUtf8(p).c_str())) OpenProject(p);
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(ICON_FA_DOOR_CLOSED "  プロジェクトを閉じる", nullptr, false, open)) CloseProject();
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_RIGHT_FROM_BRACKET "  終了")) {
            if (OnCloseRequested()) Quit();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("編集")) {
        if (ImGui::MenuItem(ICON_FA_ROTATE_LEFT "  元に戻す", "Ctrl+Z", false, doc && doc->CanUndo())) doc->Undo();
        if (ImGui::MenuItem(ICON_FA_ROTATE_RIGHT "  やり直す", "Ctrl+Y", false, doc && doc->CanRedo())) doc->Redo();
        ImGui::Separator();
        const bool sel = doc && doc->ui.hasSelection;
        if (ImGui::MenuItem(ICON_FA_SCISSORS "  切り取り", "Ctrl+X", false, sel && !doc->ui.selection.empty())) {
            CopyWidget(*doc, doc->ui.selection);
            DeleteWidget(*doc, doc->ui.selection);
        }
        if (ImGui::MenuItem(ICON_FA_COPY "  コピー", "Ctrl+C", false, sel)) CopyWidget(*doc, doc->ui.selection);
        if (ImGui::MenuItem(ICON_FA_PASTE "  貼り付け", "Ctrl+V", false, doc && !doc->clipboard.empty())) PasteWidget(*doc);
        if (ImGui::MenuItem(ICON_FA_CLONE "  複製", "Ctrl+D", false, sel && !doc->ui.selection.empty())) DuplicateWidget(*doc, doc->ui.selection);
        if (ImGui::MenuItem(ICON_FA_TRASH "  削除", "Delete", false, sel && !doc->ui.selection.empty())) DeleteWidget(*doc, doc->ui.selection);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("表示")) {
        if (doc) {
            ImGui::MenuItem(ICON_FA_BORDER_ALL "  グリッド", nullptr, &doc->ui.showGrid);
            ImGui::MenuItem(ICON_FA_MAGNET "  吸い付き（スナップ）", nullptr, &doc->ui.snap);
            if (ImGui::MenuItem(ICON_FA_EXPAND "  全体を表示", "F")) doc->ui.zoom = 0.0f;
            if (ImGui::MenuItem(ICON_FA_CIRCLE_PLAY "  プレビュー", "P", doc->ui.preview)) doc->ui.preview = !doc->ui.preview;
            ImGui::Separator();
        }
        if (ImGui::MenuItem(ICON_FA_TABLE_COLUMNS "  レイアウトを元に戻す")) m_ResetLayout = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("プロジェクト", open)) {
        if (ImGui::MenuItem(ICON_FA_GEAR "  Aqua の設定...")) m_OpenSettingsPopup = true;
        if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  エクスプローラーで開く")) Platform::OpenInFileBrowser(m_Project.GetDir());
        if (ImGui::MenuItem(ICON_FA_FILE_CODE "  AquaUI フォルダを開く")) Platform::OpenInFileBrowser(m_Project.GetLayout().SourceDir());
        if (ImGui::MenuItem(ICON_FA_ROTATE "  一覧を更新")) m_Project.Refresh(true);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("ヘルプ")) {
        if (ImGui::MenuItem(ICON_FA_CIRCLE_INFO "  Aqua について")) m_OpenAboutPopup = true;
        ImGui::EndMenu();
    }

    // 中央: 開いている UI の名前
    if (doc) {
        const std::string title = std::format("{}  {}", ICON_FA_LAYER_GROUP, doc->GetTitle());
        const float tw = ImGui::CalcTextSize(title.c_str()).x;
        const float x = (ImGui::GetWindowWidth() - tw) * 0.5f;
        if (x > ImGui::GetCursorPosX() + 20.0f * m_Scale) {
            ImGui::SetCursorPosX(x);
            ImGui::TextColored(Theme::kTextDim, "%s", title.c_str());
        }
    }

    // 右端: 連携の状態（丸い札）
    if (open) {
        const std::string status = m_EditorRunning ? ICON_FA_LINK "  ライブ反映中" : ICON_FA_LINK_SLASH "  RutoEditor 未起動";
        const ImVec2 ts = ImGui::CalcTextSize(status.c_str());
        const ImVec2 pad{10.0f * m_Scale, 3.0f * m_Scale};
        const float w = ts.x + pad.x * 2.0f;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - w - 10.0f * m_Scale);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float h = ImGui::GetFrameHeight();
        const ImVec2 min{p.x, p.y + (h - ts.y) * 0.5f - pad.y};
        const ImVec2 max{p.x + w, min.y + ts.y + pad.y * 2.0f};
        const ImVec4 col = m_EditorRunning ? Theme::kSuccess : Theme::kTextDim;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(min, max, Theme::U32(col, 0.14f), 100.0f);
        dl->AddRect(min, max, Theme::U32(col, 0.45f), 100.0f);
        dl->AddText({min.x + pad.x, min.y + pad.y}, Theme::U32(col), status.c_str());
        ImGui::Dummy({w, h});
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("編集すると Assets の .aqua がすぐ書き出され、\nRutoEditor のシーン・ゲームビュー・実行中のゲームに反映されます");
    }
    ImGui::EndMainMenuBar();
}

void App::DrawMenuBrand() {
    // メニューの左端: アクアの札に「Aqua」
    const float h = ImGui::GetFrameHeight();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const char* label = ICON_FA_DROPLET " Aqua";
    ImGui::PushFont(GetBoldFont(), ImGui::GetStyle().FontSizeBase);
    const ImVec2 ts = ImGui::CalcTextSize(label);
    const ImVec2 pad{9.0f * m_Scale, 3.0f * m_Scale};
    const ImVec2 min{p.x, p.y + (h - ts.y) * 0.5f - pad.y};
    const ImVec2 max{p.x + ts.x + pad.x * 2.0f, min.y + ts.y + pad.y * 2.0f};
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(min, max, Theme::U32(Theme::kAccent), 6.0f * m_Scale);
    dl->AddText({min.x + pad.x, min.y + pad.y}, Theme::U32(Theme::kAccentInk), label);
    ImGui::PopFont();
    ImGui::Dummy({max.x - min.x, h});
    ImGui::SameLine(0.0f, 10.0f * m_Scale);
}

void App::HandleShortcuts() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;
    Document* doc = ActiveDocument();
    auto pressed = [](ImGuiKey k) { return ImGui::IsKeyPressed(k, false); };
    if (io.KeyCtrl && pressed(ImGuiKey_N) && m_Project.IsOpen()) m_OpenNewPopup = true;
    if (io.KeyCtrl && pressed(ImGuiKey_O) && m_Project.IsOpen()) {
        Platform::ShowOpenFileDialog(m_Project.GetLayout().SourceDir(), "Aqua の UI（.aqs / .aqua / .rui）", "aqs;aqua;rui", [this](std::optional<fs::path> p) {
            if (p) OpenDocument(*p);
        });
    }
    if (io.KeyCtrl && io.KeyShift && pressed(ImGuiKey_E)) ExportAll();
    if (!doc) return;
    if (io.KeyCtrl && pressed(ImGuiKey_S)) {
        doc->Commit();
        doc->Save();
        doc->Export();
        Notify("保存して書き出しました: Assets/" + doc->output);
    }
    if (io.KeyCtrl && pressed(ImGuiKey_W)) {
        CloseDocument(static_cast<size_t>(m_Active));
        return;
    }
    if (io.KeyCtrl && !io.KeyShift && pressed(ImGuiKey_Z)) doc->Undo();
    if (io.KeyCtrl && (pressed(ImGuiKey_Y) || (io.KeyShift && pressed(ImGuiKey_Z)))) doc->Redo();
    if (io.KeyCtrl && pressed(ImGuiKey_Tab) && !m_Docs.empty()) {
        m_Active = (m_Active + 1) % static_cast<int>(m_Docs.size());
        m_FocusTab = m_Active;
    }
}

// ---------------------------------------------------------------------------
// ドッキング
// ---------------------------------------------------------------------------

void App::DrawDockSpace() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float top = ImGui::GetFrameHeight();
    const float bottom = std::round(26.0f * m_Scale);
    ImGui::SetNextWindowPos({vp->Pos.x, vp->Pos.y + top});
    ImGui::SetNextWindowSize({vp->Size.x, vp->Size.y - top - bottom});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {6.0f * m_Scale, 6.0f * m_Scale});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::kBackground);
    ImGui::Begin("##AquaDockHost", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
    const ImGuiID id = ImGui::GetID("AquaDockSpace");
    if (m_ResetLayout || !ImGui::DockBuilderGetNode(id)) {
        BuildDefaultLayout(id);
        m_ResetLayout = false;
    }
    ImGui::DockSpace(id, {0.0f, 0.0f}, ImGuiDockNodeFlags_None);
    ImGui::End();
}

void App::BuildDefaultLayout(ImGuiID id) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::DockBuilderRemoveNode(id);
    ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(id, vp->Size);
    // 左: 部品（パレット・プロジェクト）／中央: キャンバス／右: 階層の下に詳細／下: アニメーションなど
    ImGuiID center = id;
    const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.15f, nullptr, &center);
    ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.27f, nullptr, &center);
    const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.30f, nullptr, &center);
    const ImGuiID rightTop = ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.36f, nullptr, &right);
    ImGui::DockBuilderDockWindow("###Palette", left);
    ImGui::DockBuilderDockWindow("###Project", left);
    ImGui::DockBuilderDockWindow("###Hierarchy", rightTop);
    ImGui::DockBuilderDockWindow("###Canvas", center);
    ImGui::DockBuilderDockWindow("###Details", right);
    ImGui::DockBuilderDockWindow("###Styles", right);
    ImGui::DockBuilderDockWindow("###Variables", right);
    ImGui::DockBuilderDockWindow("###Arrange", right);
    ImGui::DockBuilderDockWindow("###Animation", bottom);
    ImGui::DockBuilderDockWindow("###Assets", bottom);
    ImGui::DockBuilderDockWindow("###Shader", bottom);
    ImGui::DockBuilderDockWindow("###Log", bottom);
    ImGui::DockBuilderFinish(id);
    m_DefaultTabsFrames = 4; // 窓とタブができてから、最初に見せるタブを選ぶ
}

// ---------------------------------------------------------------------------
// 状態の行・ようこそ・ログ・ポップアップ
// ---------------------------------------------------------------------------

void App::DrawStatusBar(float height) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({vp->Pos.x, vp->Pos.y + vp->Size.y - height});
    ImGui::SetNextWindowSize({vp->Size.x, height});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.0f * m_Scale, 4.0f * m_Scale});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::kBackground);
    ImGui::Begin("##AquaStatus", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    {
        // 上の縁に、左からアクアが薄れていく線（Aqua の目印）
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 wp = ImGui::GetWindowPos();
        const float w = ImGui::GetWindowWidth();
        const float t = std::max(1.0f, std::round(2.0f * m_Scale));
        dl->AddRectFilledMultiColor(wp, {wp.x + w, wp.y + t}, Theme::U32(Theme::kAccent), Theme::U32(Theme::kAccent, 0.0f),
                                    Theme::U32(Theme::kAccent, 0.0f), Theme::U32(Theme::kAccent));
    }
    const double now = Platform::GetTime();
    if (!m_Notice.empty() && now - m_NoticeTime < 5.0) {
        ImGui::TextColored(m_NoticeError ? Theme::kError : Theme::kSuccess, "%s  %s", m_NoticeError ? ICON_FA_TRIANGLE_EXCLAMATION : ICON_FA_CHECK, m_Notice.c_str());
    } else if (Document* doc = ActiveDocument()) {
        if (!doc->GetLastError().empty()) {
            ImGui::TextColored(Theme::kError, ICON_FA_TRIANGLE_EXCLAMATION "  %s", doc->GetLastError().c_str());
        } else {
            const double ago = now - doc->GetLastExportTime();
            ImGui::TextColored(Theme::kTextDim, ICON_FA_FILE_EXPORT "  Assets/%s  ·  %s", doc->output.c_str(),
                               ago < 2.0 ? "書き出しました" : doc->HasPending() ? "編集中..." : "保存・書き出し済み");
        }
    } else if (m_Project.IsOpen()) {
        ImGui::TextColored(Theme::kTextDim, ICON_FA_FOLDER "  %s", FileSystem::ToUtf8(m_Project.GetDir()).c_str());
    } else {
        ImGui::TextColored(Theme::kTextDim, "プロジェクトを開いてください");
    }
    ImGui::SameLine();
    const std::string fps = std::format("{:.0f} FPS", GetFPS());
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::CalcTextSize(fps.c_str()).x - 12.0f * m_Scale);
    ImGui::TextColored(Theme::kTextDim, "%s", fps.c_str());
    ImGui::End();
}

void App::DrawWelcome() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float top = ImGui::GetFrameHeight();
    ImGui::SetNextWindowPos({vp->Pos.x, vp->Pos.y + top});
    ImGui::SetNextWindowSize({vp->Size.x, vp->Size.y - top - std::round(26.0f * m_Scale)});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::kBackground);
    ImGui::Begin("##AquaWelcome", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);
    ImGui::PopStyleColor();

    // 上から差し込む光（アクアのグラデーション）
    ImDrawList* bg = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 ws = ImGui::GetWindowSize();
    const float glowH = std::min(ws.y, 320.0f * m_Scale);
    bg->AddRectFilledMultiColor(wp, {wp.x + ws.x, wp.y + glowH}, Theme::U32(Theme::kAccent, 0.16f), Theme::U32(Theme::kAccent, 0.05f),
                                Theme::U32(Theme::kAccent, 0.0f), Theme::U32(Theme::kAccent, 0.0f));

    const float w = std::min(ImGui::GetContentRegionAvail().x - 40.0f, 760.0f * m_Scale);
    ImGui::SetCursorPos({(ImGui::GetWindowWidth() - w) * 0.5f, 64.0f * m_Scale});
    ImGui::BeginGroup();
    DrawLogo(ImGui::GetStyle().FontSizeBase * 2.6f);
    ImGui::Spacing();
    ImGui::TextColored(Theme::kTextDim, "ゲームの UI（メニュー・HUD・設定画面）を見たまま作る、RutoEngine のための UI デザイナー");
    ImGui::Dummy({0.0f, 14.0f * m_Scale});
    if (!m_Error.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w);
        ImGui::TextColored(Theme::kError, ICON_FA_TRIANGLE_EXCLAMATION "  %s", m_Error.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
    }
    ImGui::PushStyleColor(ImGuiCol_Button, Theme::kAccent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.40f, 0.92f, 0.88f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::kAccent);
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kAccentInk);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 100.0f);
    ImGui::PushFont(GetBoldFont(), ImGui::GetStyle().FontSizeBase);
    if (ImGui::Button(ICON_FA_FOLDER_OPEN "  プロジェクトのフォルダを開く...", {std::min(w, 340.0f * m_Scale), 44.0f * m_Scale})) {
        Platform::ShowFolderDialog(Platform::GetDocumentsDir(), [this](std::optional<fs::path> p) {
            if (p) OpenProject(*p);
        });
    }
    ImGui::PopFont();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    ImGui::Dummy({0.0f, 20.0f * m_Scale});

    ImGui::TextColored(Theme::kAccent, ICON_FA_CLOCK_ROTATE_LEFT);
    ImGui::SameLine();
    ImGui::TextUnformatted("最近のプロジェクト");
    ImGui::SameLine();
    ImGui::TextDisabled("（RutoLauncher のプロジェクトも表示）");
    ImGui::Spacing();
    const auto recent = LoadRecentProjects();
    if (recent.empty()) ImGui::TextDisabled("まだありません。RutoLauncher でプロジェクトを作ってから開いてください");
    // 1 件ずつ角の丸いカードにする
    std::error_code ec;
    const float cardH = ImGui::GetTextLineHeight() * 2.0f + 18.0f * m_Scale;
    const float rounding = 10.0f * m_Scale;
    for (const fs::path& p : recent) {
        const bool exists = fs::is_directory(p / "Assets", ec);
        ImGui::PushID(FileSystem::ToUtf8(p).c_str());
        const ImVec2 cp = ImGui::GetCursorScreenPos();
        ImGui::BeginDisabled(!exists);
        const bool clicked = ImGui::InvisibleButton("##card", {w, cardH});
        ImGui::EndDisabled();
        const bool hovered = exists && ImGui::IsItemHovered();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 cmax{cp.x + w, cp.y + cardH};
        dl->AddRectFilled(cp, cmax, Theme::U32(hovered ? Theme::kFrameHover : Theme::kPanel), rounding);
        if (hovered) {
            dl->AddRect(cp, cmax, Theme::U32(Theme::kAccent, 0.7f), rounding, 0, 1.5f);
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        // 左の丸いアイコン
        const float pad = 9.0f * m_Scale;
        const float r = (cardH - pad * 2.0f) * 0.5f;
        const ImVec2 ic{cp.x + pad + r, cp.y + cardH * 0.5f};
        const ImVec4 iconCol = exists ? Theme::kAccent : Theme::kTextDim;
        dl->AddCircleFilled(ic, r, Theme::U32(iconCol, 0.16f));
        const ImVec2 is = ImGui::CalcTextSize(ICON_FA_FOLDER);
        dl->AddText({ic.x - is.x * 0.5f, ic.y - is.y * 0.5f}, Theme::U32(iconCol), ICON_FA_FOLDER);
        const float tx = ic.x + r + 12.0f * m_Scale;
        const float lh = ImGui::GetTextLineHeight();
        const std::string name = FileSystem::ToUtf8(p.filename());
        const std::string path = FileSystem::ToUtf8(p) + (exists ? "" : "（見つかりません）");
        dl->PushClipRect(cp, {cmax.x - pad, cmax.y}, true);
        dl->AddText({tx, ic.y - lh}, Theme::U32(exists ? Theme::kText : Theme::kTextDim), name.c_str());
        dl->AddText({tx, ic.y}, Theme::U32(Theme::kTextDim), path.c_str());
        dl->PopClipRect();
        if (exists) {
            const ImVec2 as = ImGui::CalcTextSize(ICON_FA_ARROW_RIGHT);
            dl->AddText({cmax.x - pad - as.x - 6.0f * m_Scale, ic.y - as.y * 0.5f}, Theme::U32(hovered ? Theme::kAccent : Theme::kTextDim, hovered ? 1.0f : 0.5f),
                        ICON_FA_ARROW_RIGHT);
        }
        ImGui::PopID();
        ImGui::Dummy({0.0f, 2.0f * m_Scale});
        if (clicked && exists) {
            OpenProject(p); // 一覧が変わるので最後に開く
            break;
        }
    }
    ImGui::EndGroup();
    ImGui::End();
}

void App::DrawLogPanel() {
    if (ImGui::SmallButton(ICON_FA_TRASH " クリア")) m_Log.clear();
    ImGui::Separator();
    ImGui::BeginChild("##log-lines");
    for (const LogLine& l : m_Log) {
        const ImVec4 c = l.level == Log::Level::Error ? Theme::kError : l.level == Log::Level::Warn ? Theme::kWarning : Theme::kText;
        ImGui::TextColored(c, "%s", l.text.c_str());
    }
    if (m_LogScroll) {
        ImGui::SetScrollHereY(1.0f);
        m_LogScroll = false;
    }
    ImGui::EndChild();
}

void App::DrawPopups() {
    if (m_OpenNewPopup) {
        ImGui::OpenPopup("新しい UI");
        m_NewName = "NewUI";
        m_NewFolder.clear();
        m_OpenNewPopup = false;
    }
    if (ImGui::BeginPopupModal("新しい UI", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextDisabled("名前（スクリプトからは AquaUI::名前 で使えます）");
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(360.0f * m_Scale);
        const bool enter = ImGui::InputText("##name", &m_NewName, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::TextDisabled("フォルダ（AquaUI の中。空でよい）");
        ImGui::SetNextItemWidth(360.0f * m_Scale);
        ImGui::InputTextWithHint("##folder", "例: Menu", &m_NewFolder);
        if (m_Project.IsOpen()) {
            const fs::path src = m_Project.GetLayout().SourceDir() / FileSystem::FromUtf8(m_NewFolder) / FileSystem::FromUtf8(m_NewName + ".aqs");
            ImGui::TextDisabled(ICON_FA_ARROW_RIGHT "  Assets/%s", m_Project.DefaultOutputFor(src).c_str());
        }
        ImGui::Spacing();
        if (ImGui::Button("作成", {120.0f * m_Scale, 0.0f}) || enter) {
            NewDocument(m_NewName);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("キャンセル", {120.0f * m_Scale, 0.0f}) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (m_OpenSettingsPopup) {
        ImGui::OpenPopup("Aqua の設定");
        m_OpenSettingsPopup = false;
    }
    if (ImGui::BeginPopupModal("Aqua の設定", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ProjectSettingsData& s = m_Project.settings;
        bool changed = false;
        ImGui::TextDisabled("プロジェクトごとの設定（ProjectSettings/Aqua.json）");
        ImGui::Spacing();
        ImGui::SetNextItemWidth(260.0f * m_Scale);
        changed |= ImGui::InputText("書き出し先（Assets の中のフォルダ）", &s.outputFolder);
        changed |= ImGui::Checkbox("スクリプト用のヘッダーを書き出す（Source/AquaGenerated）", &s.generateHeaders);
        changed |= ImGui::Checkbox("ドラッグ中もライブで書き出す（RutoEditor・ゲームにすぐ反映）", &s.liveExport);
        if (changed) m_Project.SaveSettings();
        ImGui::Spacing();
        if (ImGui::Button("閉じる", {120.0f * m_Scale, 0.0f}) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (m_OpenAboutPopup) {
        ImGui::OpenPopup("Aqua について");
        m_OpenAboutPopup = false;
    }
    if (ImGui::BeginPopupModal("Aqua について", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        DrawLogo(ImGui::GetStyle().FontSizeBase * 1.6f);
        ImGui::TextDisabled("Aqua %s · RutoEngine の UI を作るツール（UI バイナリ フォーマット %d）", AQUA_VERSION, static_cast<int>(Ruto::Aqua::kFormatVersion));
        ImGui::Spacing();
        ImGui::BulletText("AquaUI/*.aqs … 編集用ファイル（バージョン管理向けの XML）");
        ImGui::BulletText("Assets/UI/*.aqua … 書き出した UI（ゲームが読むバイナリ）");
        ImGui::BulletText("Source/AquaGenerated/*.h … スクリプト用の名前の定数");
        ImGui::BulletText("編集するとすぐ書き出され、RutoEditor・実行中のゲームに反映されます");
        ImGui::BulletText("ゲームでは UIDocument の asset に .aqua を指定して表示します");
        ImGui::Spacing();
        if (ImGui::Button("閉じる", {120.0f * m_Scale, 0.0f}) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

} // namespace AquaTool
