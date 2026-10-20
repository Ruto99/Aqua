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
        // ファイルだけ渡されたら、そのファイルを含むワークスペース（ゲームのプロジェクトかライブラリ）を開く
        const auto files = m_Options.open;
        for (const fs::path& f : files) OpenAnyFile(f);
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
    if (m_Project.IsLibrary()) {
        // ライブラリは RutoEngine と関係なく開く（エディターからの依頼は受けない）
        AddRecentProject(m_Project.GetDir());
        GetWindow().SetTitle(std::format("Aqua — {}（ライブラリ）", m_Project.GetName()));
        Log::Info("ライブラリを開きました: {}", FileSystem::ToUtf8(m_Project.GetDir()));
        if (m_Project.sendTarget.Valid())
            Log::Info("送り先: {}（Assets/{}）", FileSystem::ToUtf8(m_Project.sendTarget.project), m_Project.sendTarget.folder);
        return true;
    }
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

Document* App::OpenAnyFile(const fs::path& file) {
    if (file.empty()) return nullptr;
    std::error_code ec;
    const fs::path ws = FindWorkspaceFor(file);
    if (!m_Project.IsOpen() || !fs::equivalent(ws, m_Project.GetDir(), ec)) {
        if (!OpenProject(ws)) return nullptr;
    }
    return OpenDocument(file);
}

void App::ShowOpenFileDialog() {
    const fs::path start = m_Project.IsOpen() ? m_Project.GetSourceDir() : Platform::GetDocumentsDir();
    Platform::ShowOpenFileDialog(start, "Aqua のレイアウト（.aqs / .aqua / .rui）", "aqs;aqua;rui", [this](std::optional<fs::path> p) {
        if (p) OpenAnyFile(*p);
    });
}

void App::ShowOpenFolderDialog() {
    Platform::ShowFolderDialog(m_Project.IsOpen() ? m_Project.GetDir() : Platform::GetDocumentsDir(), [this](std::optional<fs::path> p) {
        if (p) OpenProject(*p);
    });
}

void App::CreateLibraryAt(const fs::path& dir) {
    std::string error;
    if (!Project::CreateLibrary(dir, &error)) {
        m_Error = error;
        Notify(error, true);
        return;
    }
    if (OpenProject(dir)) Notify("ライブラリを作りました: " + FileSystem::ToUtf8(dir));
}

void App::SendAll() {
    if (!m_Project.IsLibrary() || !m_Project.sendTarget.Valid()) return;
    m_Project.Refresh(true);
    int ok = 0, failed = 0;
    std::set<std::string> done;
    for (const Project::SourceInfo& s : m_Project.GetSources()) {
        // 書き出しを最新にしてから送る
        std::error_code ec;
        bool exported = false;
        for (auto& d : m_Docs) {
            if (!fs::equivalent(d->sourcePath, s.path, ec)) continue;
            if (d->HasPending()) d->Commit();
            exported = true;
        }
        if (!exported) {
            if (auto doc = Document::Open(m_Project, s.path)) {
                const bool live = m_Project.sendTarget.live;
                m_Project.sendTarget.live = false; // 下で送る
                doc->Export();
                m_Project.sendTarget.live = live;
            }
        }
        std::string error;
        if (m_Project.SendLayout(s.output, &error, &done)) {
            ++ok;
        } else {
            ++failed;
            Log::Error("{}: {}", s.output, error);
        }
    }
    Notify(std::format("{} 個のレイアウトを {} に送りました{}", ok, FileSystem::ToUtf8(m_Project.sendTarget.project.filename()),
                       failed ? std::format("（{} 個は失敗）", failed) : std::string()),
           failed > 0);
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

void App::NewDocument(const std::string& name, LayoutKind kind) {
    if (!m_Project.IsOpen()) return;
    std::string base = name.empty() ? std::string("NewUI") : name;
    std::erase_if(base, [](char c) { return c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|'; });
    if (base.empty()) base = "NewUI";
    fs::path folder = m_Project.GetSourceDir();
    if (!m_NewFolder.empty()) folder /= FileSystem::FromUtf8(m_NewFolder);
    fs::path source = folder / FileSystem::FromUtf8(base + Ruto::Aqua::kSourceExtension);
    std::error_code ec;
    for (int n = 1; fs::exists(source, ec) || !m_Project.FindSourceFor(m_Project.DefaultOutputFor(source)).empty(); ++n)
        source = folder / FileSystem::FromUtf8(std::format("{}{}{}", base, n, Ruto::Aqua::kSourceExtension));
    auto doc = Document::CreateNew(m_Project, source, m_Project.DefaultOutputFor(source), kind);
    Log::Info("{}を作成しました: {} → {}", LayoutKindLabel(kind), FileSystem::ToUtf8(source.filename()), doc->output);
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
        const fs::path game = m_Project.IsLibrary() ? m_Project.sendTarget.project : m_Project.GetDir();
        m_EditorRunning = !game.empty() && Platform::FileLock::IsLocked(game / "build" / "editor.lock");
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
    if (ImGui::Begin(ICON_FA_FOLDER_TREE "  レイアウト###Project")) DrawProjectPanel();
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
    // 文字アニメーションの「作る / 編集」で、アニメーションのタブを前に出す
    if (m_FocusAnimPanel) {
        if (ImGuiWindow* w = ImGui::FindWindowByName("###Animation")) {
            if (w->DockNode && w->DockNode->TabBar) w->DockNode->TabBar->NextSelectedTabId = w->TabId;
            ImGui::FocusWindow(w);
        }
        m_FocusAnimPanel = false;
    }
    if (ImGui::Begin(ICON_FA_WAND_MAGIC_SPARKLES "  アニメーション###Animation", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        if (Document* doc = ActiveDocument()) {
            DrawAnimations(*doc);
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) HandleDesignerKeys(*doc, true); // P・F など（キーの操作はパネルの中）
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
    dockWith("###Details");
    if (ImGui::Begin(ICON_FA_OBJECT_GROUP "  グループ###Groups")) {
        if (Document* doc = ActiveDocument()) DrawGroupsPanel(*doc);
        else ImGui::TextDisabled("UI を開くと、ウィジェットをグループにまとめられます");
    }
    ImGui::End();
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
        for (const char* name : {"###Project", "###Details", "###Animation", "###Canvas"}) {
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
        if (ImGui::MenuItem(ICON_FA_FILE_CIRCLE_PLUS "  新しいレイアウト...", "Ctrl+N", false, open)) m_OpenNewPopup = true;
        if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  レイアウトを開く...", "Ctrl+O")) ShowOpenFileDialog();
        if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK "  保存して書き出す", "Ctrl+S", false, doc != nullptr)) {
            doc->Commit();
            doc->Save();
            doc->Export();
            Notify("保存して書き出しました: Assets/" + doc->output);
        }
        if (ImGui::MenuItem(ICON_FA_FILE_EXPORT "  すべての UI を書き出す", "Ctrl+Shift+E", false, open)) ExportAll();
        if (ImGui::MenuItem(ICON_FA_FILE_IMPORT "  .rui を取り込む...", nullptr, false, open)) {
            Platform::ShowOpenFileDialog(m_Project.GetAssetRoot(), "以前の UI（.rui）", "rui", [this](std::optional<fs::path> p) {
                if (p) OpenDocument(*p);
            });
        }
        if (ImGui::MenuItem(ICON_FA_XMARK "  閉じる", "Ctrl+W", false, doc != nullptr)) CloseDocument(static_cast<size_t>(m_Active));
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_FOLDER "  フォルダを開く（ライブラリ / ゲーム）...")) ShowOpenFolderDialog();
        if (ImGui::MenuItem(ICON_FA_BOOK "  新しいライブラリ...")) {
            Platform::ShowFolderDialog(Platform::GetDocumentsDir(), [this](std::optional<fs::path> p) {
                if (p) CreateLibraryAt(*p);
            });
        }
        if (ImGui::BeginMenu(ICON_FA_CLOCK_ROTATE_LEFT "  最近開いたもの")) {
            for (const fs::path& p : LoadRecentProjects()) {
                const char* icon = IsGameProjectDir(p) ? ICON_FA_GAMEPAD : ICON_FA_BOOK;
                if (ImGui::MenuItem(std::format("{}  {}", icon, FileSystem::ToUtf8(p)).c_str())) OpenProject(p);
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(ICON_FA_DOOR_CLOSED "  閉じる（ようこそ画面へ）", nullptr, false, open)) CloseProject();
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
    if (m_Project.IsLibrary()) {
        if (ImGui::BeginMenu("ライブラリ")) {
            if (ImGui::MenuItem(ICON_FA_PAPER_PLANE "  RutoEngine に送る...")) m_OpenSendPopup = true;
            if (ImGui::MenuItem(ICON_FA_ARROWS_ROTATE "  すべて送り直す", nullptr, false, m_Project.sendTarget.Valid())) SendAll();
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_GEAR "  Aqua の設定...")) m_OpenSettingsPopup = true;
            if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  エクスプローラーで開く")) Platform::OpenInFileBrowser(m_Project.GetDir());
            if (ImGui::MenuItem(ICON_FA_ROTATE "  一覧を更新")) m_Project.Refresh(true);
            ImGui::EndMenu();
        }
    } else if (ImGui::BeginMenu("プロジェクト", open)) {
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

    // 右端: 連携の状態（丸い札）。ライブラリは送り先（クリックで送り先の設定）
    if (open) {
        const bool library = m_Project.IsLibrary();
        const SendTarget& target = m_Project.sendTarget;
        std::string status;
        if (!library) status = m_EditorRunning ? ICON_FA_LINK "  ライブ反映中" : ICON_FA_LINK_SLASH "  RutoEditor 未起動";
        else if (!target.Valid()) status = ICON_FA_PAPER_PLANE "  RutoEngine に送る";
        else status = std::format("{}  {} に{}", ICON_FA_PAPER_PLANE, FileSystem::ToUtf8(target.project.filename()), target.live ? "ライブ反映中" : "送る（手動）");
        const ImVec2 ts = ImGui::CalcTextSize(status.c_str());
        const ImVec2 pad{10.0f * m_Scale, 3.0f * m_Scale};
        const float w = ts.x + pad.x * 2.0f;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - w - 10.0f * m_Scale);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float h = ImGui::GetFrameHeight();
        const ImVec2 min{p.x, p.y + (h - ts.y) * 0.5f - pad.y};
        const ImVec2 max{p.x + w, min.y + ts.y + pad.y * 2.0f};
        const ImVec4 col = library ? (target.Valid() && target.live ? Theme::kAccent : Theme::kTextDim) : (m_EditorRunning ? Theme::kSuccess : Theme::kTextDim);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(min, max, Theme::U32(col, 0.14f), 100.0f);
        dl->AddRect(min, max, Theme::U32(col, 0.45f), 100.0f);
        dl->AddText({min.x + pad.x, min.y + pad.y}, Theme::U32(col), status.c_str());
        if (library) {
            if (ImGui::InvisibleButton("##send-pill", {w, h})) m_OpenSendPopup = true;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", target.Valid() ? std::format("送り先: {}\nAssets/{}{}", FileSystem::ToUtf8(target.project), target.folder,
                                                                  target.live ? "\n編集するたびに送ります（実行中のゲームにもすぐ反映）" : "").c_str()
                                                     : "このライブラリのレイアウトを RutoEngine のゲームのプロジェクトに送ります");
        } else {
            ImGui::Dummy({w, h});
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("編集すると Assets の .aqua がすぐ書き出され、\nRutoEditor のシーン・ゲームビュー・実行中のゲームに反映されます");
        }
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
    if (io.KeyCtrl && pressed(ImGuiKey_O)) ShowOpenFileDialog();
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
    // 基本のレイアウト
    // 左: レイアウト一覧・パレット／中央: キャンバス／右: 階層の下に詳細・スタイル・変数・整列・グループ／
    // 下: アニメーション（文字アニメーションも）・アセット・シェーダー・ログ
    ImGuiID center = id;
    const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.15f, nullptr, &center);
    ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.27f, nullptr, &center);
    const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.30f, nullptr, &center);
    const ImGuiID rightTop = ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.36f, nullptr, &right);
    ImGui::DockBuilderDockWindow("###Project", left);
    ImGui::DockBuilderDockWindow("###Palette", left);
    ImGui::DockBuilderDockWindow("###Hierarchy", rightTop);
    ImGui::DockBuilderDockWindow("###Canvas", center);
    ImGui::DockBuilderDockWindow("###Details", right);
    ImGui::DockBuilderDockWindow("###Styles", right);
    ImGui::DockBuilderDockWindow("###Variables", right);
    ImGui::DockBuilderDockWindow("###Arrange", right);
    ImGui::DockBuilderDockWindow("###Groups", right);
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
            const char* state = ago < 2.0 ? "書き出しました" : doc->HasPending() ? "編集中..." : "保存・書き出し済み";
            if (m_Project.IsLibrary()) {
                const SendTarget& t = m_Project.sendTarget;
                const std::string sent = t.Valid() && t.live ? std::format("  ·  {} の Assets/{} に送信", FileSystem::ToUtf8(t.project.filename()), m_Project.SendPathFor(doc->output)) : std::string();
                ImGui::TextColored(Theme::kTextDim, "%s  %s  ·  %s%s", LayoutKindIcon(doc->kind), doc->output.c_str(), state, sent.c_str());
            } else {
                ImGui::TextColored(Theme::kTextDim, "%s  Assets/%s  ·  %s", LayoutKindIcon(doc->kind), doc->output.c_str(), state);
            }
        }
    } else if (m_Project.IsOpen()) {
        ImGui::TextColored(Theme::kTextDim, "%s  %s", m_Project.IsLibrary() ? ICON_FA_BOOK : ICON_FA_GAMEPAD, FileSystem::ToUtf8(m_Project.GetDir()).c_str());
    } else {
        ImGui::TextColored(Theme::kTextDim, "ライブラリかゲームのプロジェクトを開いてください");
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
    ImGui::TextColored(Theme::kTextDim, "ゲームの UI（メニュー・HUD・設定画面）を見たまま作る UI デザイナー。単体でも、RutoEngine とつないでも使えます");
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
    const ImVec2 buttonSize{std::min((w - 24.0f * m_Scale) / 3.0f, 250.0f * m_Scale), 44.0f * m_Scale};
    if (ImGui::Button(ICON_FA_FOLDER_OPEN "  フォルダを開く...", buttonSize)) ShowOpenFolderDialog();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("ライブラリ（好きなフォルダ）か、RutoEngine のゲームのプロジェクトを開きます");
    ImGui::PopFont();
    ImGui::PopStyleColor(4);
    ImGui::SameLine(0.0f, 12.0f * m_Scale);
    if (ImGui::Button(ICON_FA_BOOK "  新しいライブラリ...", buttonSize)) {
        Platform::ShowFolderDialog(Platform::GetDocumentsDir(), [this](std::optional<fs::path> p) {
            if (p) CreateLibraryAt(*p);
        });
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("選んだフォルダに Body・Parts・Images などを作り、レイアウトのライブラリにします（RutoEngine は要りません）");
    ImGui::SameLine(0.0f, 12.0f * m_Scale);
    if (ImGui::Button(ICON_FA_FILE "  レイアウトを開く...", buttonSize)) ShowOpenFileDialog();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(".aqs / .aqua を 1 つだけ開きます（そのフォルダをライブラリとして開きます）");
    ImGui::PopStyleVar();
    ImGui::Dummy({0.0f, 20.0f * m_Scale});

    ImGui::TextColored(Theme::kAccent, ICON_FA_CLOCK_ROTATE_LEFT);
    ImGui::SameLine();
    ImGui::TextUnformatted("最近開いたもの");
    ImGui::SameLine();
    ImGui::TextDisabled("（" ICON_FA_BOOK " ライブラリ・" ICON_FA_GAMEPAD " ゲームのプロジェクト。RutoLauncher のプロジェクトも表示）");
    ImGui::Spacing();
    const auto recent = LoadRecentProjects();
    if (recent.empty()) ImGui::TextDisabled("まだありません。「新しいライブラリ」で作るか、フォルダを開いてください");
    // 1 件ずつ角の丸いカードにする
    std::error_code ec;
    const float cardH = ImGui::GetTextLineHeight() * 2.0f + 18.0f * m_Scale;
    const float rounding = 10.0f * m_Scale;
    for (const fs::path& p : recent) {
        const bool exists = fs::is_directory(p, ec);
        const bool game = IsGameProjectDir(p);
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
        const char* kindIcon = game ? ICON_FA_GAMEPAD : ICON_FA_BOOK;
        const ImVec2 is = ImGui::CalcTextSize(kindIcon);
        dl->AddText({ic.x - is.x * 0.5f, ic.y - is.y * 0.5f}, Theme::U32(iconCol), kindIcon);
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

void App::DrawSendPopup() {
    if (m_OpenSendPopup) {
        ImGui::OpenPopup("RutoEngine に送る");
        m_OpenSendPopup = false;
        m_SendProjectInput = FileSystem::ToUtf8(m_Project.sendTarget.project);
        m_SendFolderInput = m_Project.sendTarget.folder;
    }
    if (!ImGui::BeginPopupModal("RutoEngine に送る", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    SendTarget& t = m_Project.sendTarget;
    const float w = 460.0f * m_Scale;
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w);
    ImGui::TextDisabled("このライブラリのレイアウトを、RutoEngine のゲームのプロジェクトの Assets に書き出します。"
                        "使っているパーツ・画像・フォント・シェーダーも一緒に送り、中のパスは送り先に合わせて書き換えます。");
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    ImGui::TextUnformatted("送り先のプロジェクト");
    ImGui::SetNextItemWidth(w - ImGui::GetFrameHeight() * 1.2f - 4.0f);
    if (ImGui::BeginCombo("##send-project", m_SendProjectInput.empty() ? "(選んでください)" : m_SendProjectInput.c_str(), ImGuiComboFlags_HeightLarge)) {
        for (const fs::path& p : LoadRecentProjects()) {
            if (!IsGameProjectDir(p)) continue;
            const std::string u = FileSystem::ToUtf8(p);
            if (ImGui::Selectable(std::format(ICON_FA_GAMEPAD "  {}##{}", FileSystem::ToUtf8(p.filename()), u).c_str(), u == m_SendProjectInput)) m_SendProjectInput = u;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", u.c_str());
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (IconButton(ICON_FA_FOLDER_OPEN, "フォルダを選ぶ")) {
        Platform::ShowFolderDialog(Platform::GetDocumentsDir(), [this](std::optional<fs::path> p) {
            if (p) m_SendProjectInput = FileSystem::ToUtf8(*p);
        });
    }
    const fs::path projectDir = FileSystem::FromUtf8(m_SendProjectInput);
    const bool validProject = !m_SendProjectInput.empty() && IsGameProjectDir(projectDir);
    if (!m_SendProjectInput.empty() && !validProject) ImGui::TextColored(Theme::kError, ICON_FA_TRIANGLE_EXCLAMATION "  Assets フォルダがありません（RutoEngine のプロジェクトを選んでください）");

    ImGui::Spacing();
    ImGui::TextUnformatted("送り先のフォルダ（Assets の中）");
    ImGui::SetNextItemWidth(w);
    ImGui::InputTextWithHint("##send-folder", "例: UI/MyLibrary", &m_SendFolderInput);
    ImGui::Checkbox("編集するたびに送る（RutoEditor・実行中のゲームにすぐ反映）", &t.live);
    if (validProject) {
        SendTarget preview = t;
        preview.project = projectDir;
        preview.folder = m_SendFolderInput;
        Project& pr = m_Project;
        const SendTarget saved = pr.sendTarget;
        pr.sendTarget = preview;
        if (Document* doc = ActiveDocument()) ImGui::TextDisabled(ICON_FA_ARROW_RIGHT "  例: %s → Assets/%s", doc->output.c_str(), pr.SendPathFor(doc->output).c_str());
        pr.sendTarget = saved;
    }
    ImGui::Spacing();
    ImGui::BeginDisabled(!validProject);
    ImGui::PushStyleColor(ImGuiCol_Button, Theme::kAccent);
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kAccentInk);
    if (ImGui::Button(ICON_FA_PAPER_PLANE "  送る（すべてのレイアウト）", {220.0f * m_Scale, 0.0f})) {
        t.project = projectDir;
        t.folder = m_SendFolderInput;
        m_Project.SaveSendTarget();
        SendAll();
        ImGui::CloseCurrentPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!t.Valid());
    if (ImGui::Button("送り先を外す", {120.0f * m_Scale, 0.0f})) {
        t = {};
        t.folder = "UI/" + ToIdentifier(m_Project.GetName());
        m_Project.SaveSendTarget();
        Notify("送り先を外しました");
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("閉じる", {100.0f * m_Scale, 0.0f}) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        if (t.Valid()) m_Project.SaveSendTarget(); // ライブ反映の切り替えだけ保存
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void App::DrawPopups() {
    DrawSendPopup();
    if (m_OpenNewPopup) {
        ImGui::OpenPopup("新しいレイアウト");
        m_NewName = m_NewKind == LayoutKind::Part ? "NewPart" : "NewUI";
        m_NewFolder = m_Project.DefaultFolderFor(m_NewKind);
        m_OpenNewPopup = false;
    }
    if (ImGui::BeginPopupModal("新しいレイアウト", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // 種類（Body: 画面のいちばん親 / パーツ: ほかのレイアウトに部品として入れる）
        for (LayoutKind k : {LayoutKind::Body, LayoutKind::Part}) {
            if (k == LayoutKind::Part) ImGui::SameLine();
            const bool selected = m_NewKind == k;
            ImGui::PushStyleColor(ImGuiCol_Button, selected ? Theme::kAccent : Theme::kFrame);
            ImGui::PushStyleColor(ImGuiCol_Text, selected ? Theme::kAccentInk : Theme::kText);
            if (ImGui::Button(std::format("{}  {}", LayoutKindIcon(k), LayoutKindLabel(k)).c_str(), {176.0f * m_Scale, 0.0f}) && !selected) {
                // フォルダが既定のままなら、種類に合わせて変える
                if (m_NewFolder == m_Project.DefaultFolderFor(m_NewKind)) m_NewFolder = m_Project.DefaultFolderFor(k);
                if (m_NewName == "NewUI" || m_NewName == "NewPart") m_NewName = k == LayoutKind::Part ? "NewPart" : "NewUI";
                m_NewKind = k;
            }
            ImGui::PopStyleColor(2);
        }
        ImGui::TextDisabled("%s", m_NewKind == LayoutKind::Part ? "ほかのレイアウト（Body・パーツ）に部品として入れて使い回す" : "画面のいちばん親のレイアウト（タイトル・HUD・ポーズ画面など）");
        ImGui::Spacing();
        ImGui::TextDisabled("名前（スクリプトからは AquaUI::名前 で使えます）");
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(360.0f * m_Scale);
        const bool enter = ImGui::InputText("##name", &m_NewName, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::TextDisabled(m_Project.IsLibrary() ? "フォルダ（ライブラリの中）" : "フォルダ（AquaUI の中。空でよい）");
        ImGui::SetNextItemWidth(360.0f * m_Scale);
        ImGui::InputTextWithHint("##folder", "例: Menu", &m_NewFolder);
        if (m_Project.IsOpen()) {
            const fs::path src = m_Project.GetSourceDir() / FileSystem::FromUtf8(m_NewFolder) / FileSystem::FromUtf8(m_NewName + ".aqs");
            ImGui::TextDisabled(ICON_FA_ARROW_RIGHT "  %s%s", m_Project.IsLibrary() ? "" : "Assets/", m_Project.DefaultOutputFor(src).c_str());
        }
        ImGui::Spacing();
        if (ImGui::Button("作成", {120.0f * m_Scale, 0.0f}) || enter) {
            NewDocument(m_NewName, m_NewKind);
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
        if (m_Project.IsLibrary()) {
            ImGui::TextDisabled("ライブラリの設定（%s）", kLibraryFile);
            ImGui::Spacing();
            changed |= ImGui::Checkbox("ドラッグ中もライブで書き出す（送り先のゲームにもすぐ反映）", &s.liveExport);
            ImGui::TextDisabled("RutoEngine への送り先は「ライブラリ > RutoEngine に送る」で決めます");
        } else {
            ImGui::TextDisabled("プロジェクトごとの設定（ProjectSettings/Aqua.json）");
            ImGui::Spacing();
            ImGui::SetNextItemWidth(260.0f * m_Scale);
            changed |= ImGui::InputText("書き出し先（Assets の中のフォルダ）", &s.outputFolder);
            changed |= ImGui::Checkbox("スクリプト用のヘッダーを書き出す（Source/AquaGenerated）", &s.generateHeaders);
            changed |= ImGui::Checkbox("ドラッグ中もライブで書き出す（RutoEditor・ゲームにすぐ反映）", &s.liveExport);
        }
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
