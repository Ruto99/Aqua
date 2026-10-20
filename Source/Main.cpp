// Aqua — RutoEngine の UI を作るツール
//
//   Aqua.exe                                       … ようこそ画面（最近のプロジェクト・RutoLauncher のプロジェクト）
//   Aqua.exe --project <ProjectDir> [--open <file>] … プロジェクトを開く（file は .aqs / .aqua / .rui）
//   Aqua.exe <file>                                … ファイルを含むプロジェクトを開いて、そのファイルを開く
//
//   コマンドライン（ウィンドウを出さない。CI やビルドの前処理向け）
//   Aqua.exe export <ProjectDir>                    … AquaUI のすべての UI を Assets に書き出す
//   Aqua.exe export <file.aqs> [--project <dir>]    … 1 つだけ書き出す
//   Aqua.exe import <file.rui> --project <dir>      … 以前の UI（.rui）を取り込んで .aqs と .aqua を作る
//   Aqua.exe info <file.aqua>                       … バイナリの中身（情報・依存・文書）を JSON で出す
//   Aqua.exe to-json <file.aqua> [out.json]         … バイナリを JSON に戻す
//   Aqua.exe from-json <file.json> <out.aqua>       … JSON（.rui と同じ形）をバイナリにする

#include "AquaApp.h"

#include <Ruto/Aqua/AquaFormat.h>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <format>
#include <set>
#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;
using namespace Ruto;

namespace {

std::vector<std::string> GetUtf8Args(int argc, char** argv) {
    std::vector<std::string> args;
#if defined(_WIN32)
    (void)argc;
    (void)argv;
    int count = 0;
    LPWSTR* wargs = CommandLineToArgvW(GetCommandLineW(), &count);
    for (int i = 1; i < count; ++i) {
        int len = WideCharToMultiByte(CP_UTF8, 0, wargs[i], -1, nullptr, 0, nullptr, nullptr);
        std::string s(static_cast<size_t>(len > 0 ? len - 1 : 0), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wargs[i], -1, s.data(), len, nullptr, nullptr);
        args.push_back(std::move(s));
    }
    LocalFree(wargs);
#else
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
#endif
    return args;
}

void AttachParentConsole() {
#if defined(_WIN32)
    // リリース版はウィンドウアプリなので、コマンドラインから呼ばれたときは親のコンソールに出力する
    if (GetConsoleWindow() == nullptr && AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* f = nullptr;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
    }
    SetConsoleOutputCP(CP_UTF8);
#endif
}

void Print(const std::string& text) {
    std::fputs(text.c_str(), stdout);
    std::fputs("\n", stdout);
    std::fflush(stdout);
}

bool HasFlag(const std::vector<std::string>& args, const char* name) {
    return std::find(args.begin(), args.end(), name) != args.end();
}

std::string OptionValue(const std::vector<std::string>& args, const char* name) {
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == name) return args[i + 1];
    }
    return {};
}

int Usage() {
    Print("usage:");
    Print("  Aqua [--project <Dir>] [--open <file>]          open the UI tool (Dir: a library folder or a RutoEngine project)");
    Print("  Aqua new-library <Dir>                          make a layout library (Body / Parts / Images / Fonts / Shaders)");
    Print("  Aqua export <Dir | file.aqs> [--project <dir>]");
    Print("  Aqua import <file.rui> --project <dir> [--part]");
    Print("  Aqua send <LibraryDir> [--to <RutoEngineProject>] [--folder <UI/Name>]   send all layouts to a RutoEngine project");
    Print("  Aqua info <file.aqua>");
    Print("  Aqua to-json <file.aqua> [out.json]");
    Print("  Aqua from-json <file.json> <out.aqua>");
    return 1;
}

int RunCommandLine(const std::vector<std::string>& args) {
    const std::string& command = args[0];
    Log::AddSink([](Log::Level level, std::string_view message) {
        if (level != Log::Level::Info) Print(std::string(level == Log::Level::Error ? "error: " : "warning: ") + std::string(message));
    });

    if (command == "info" || command == "to-json") {
        if (args.size() < 2) return Usage();
        std::string error;
        auto doc = Aqua::ReadFile(FileSystem::FromUtf8(args[1]), &error);
        if (!doc) {
            Print("error: " + error);
            return 1;
        }
        if (command == "to-json") {
            const std::string text = doc->content.Dump(2) + "\n";
            if (args.size() > 2) return FileSystem::WriteTextFile(FileSystem::FromUtf8(args[2]), text) ? 0 : 1;
            Print(text);
            return 0;
        }
        Json j = Json::MakeObject();
        j["meta"] = doc->meta;
        Json deps = Json::MakeArray();
        for (const std::string& d : doc->dependencies) deps.Push(d);
        j["dependencies"] = std::move(deps);
        j["document"] = doc->content;
        Print(j.Dump(2));
        return 0;
    }

    if (command == "from-json") {
        if (args.size() < 3) return Usage();
        const auto text = FileSystem::ReadTextFile(FileSystem::FromUtf8(args[1]));
        std::string error;
        const auto json = text ? Json::Parse(*text, &error) : std::nullopt;
        if (!json) {
            Print("error: " + (text ? error : "cannot read " + args[1]));
            return 1;
        }
        WidgetAsset asset;
        asset.FromJson(*json);
        Aqua::Document doc;
        doc.meta["generator"] = "Aqua " AQUA_VERSION " (from-json)";
        doc.content = asset.ToJson();
        doc.dependencies = asset.CollectDependencies();
        if (!Aqua::WriteFile(FileSystem::FromUtf8(args[2]), doc, &error)) {
            Print("error: " + error);
            return 1;
        }
        Print("wrote: " + args[2]);
        return 0;
    }

    if (command == "new-library") {
        if (args.size() < 2) return Usage();
        std::string error;
        if (!AquaTool::Project::CreateLibrary(FileSystem::FromUtf8(args[1]), &error)) {
            Print("error: " + error);
            return 1;
        }
        Print("created: " + args[1]);
        return 0;
    }

    if (command == "send") {
        if (args.size() < 2) return Usage();
        AquaTool::Project library;
        std::string error;
        if (!library.Open(FileSystem::FromUtf8(args[1]), &error) || !library.IsLibrary()) {
            Print("error: " + (error.empty() ? args[1] + " is not a layout library" : error));
            return 1;
        }
        if (const std::string to = OptionValue(args, "--to"); !to.empty()) library.sendTarget.project = FileSystem::FromUtf8(to);
        if (const std::string folder = OptionValue(args, "--folder"); !folder.empty()) library.sendTarget.folder = folder;
        if (!library.sendTarget.Valid()) {
            Print("error: no send target (use --to <RutoEngineProject>)");
            return 1;
        }
        library.SaveSendTarget();
        library.sendTarget.live = false; // 書き出しのたびには送らず、下でまとめて送る
        std::set<std::string> done;
        int failed = 0;
        for (const auto& s : library.GetSources()) {
            auto doc = AquaTool::Document::Open(library, s.path, &error);
            if (!doc || !doc->Export() || !library.SendLayout(doc->output, &error, &done)) {
                Print("error: " + error);
                ++failed;
                continue;
            }
            Print(std::format("{} -> Assets/{}", doc->output, library.SendPathFor(doc->output)));
        }
        Print(std::format("{} sent, {} failed", library.GetSources().size() - static_cast<size_t>(failed), failed));
        return failed ? 1 : 0;
    }

    if (command == "export" || command == "import") {
        if (args.size() < 2) return Usage();
        const fs::path target = FileSystem::FromUtf8(args[1]);
        std::error_code ec;
        fs::path projectDir = FileSystem::FromUtf8(OptionValue(args, "--project"));
        const bool wholeProject = command == "export" && fs::is_directory(target, ec);
        if (projectDir.empty()) projectDir = wholeProject ? target : AquaTool::FindWorkspaceFor(target);
        if (projectDir.empty()) {
            Print("error: project folder was not found (use --project <dir>)");
            return 1;
        }
        AquaTool::Project project;
        std::string error;
        if (!project.Open(projectDir, &error)) {
            Print("error: " + error);
            return 1;
        }
        std::vector<fs::path> files;
        if (wholeProject) {
            for (const auto& s : project.GetSources()) files.push_back(s.path);
        } else {
            files.push_back(fs::absolute(target, ec));
        }
        int failed = 0;
        for (const fs::path& f : files) {
            auto doc = AquaTool::Document::Open(project, f, &error);
            if (!doc || !doc->Export()) {
                Print("error: " + (error.empty() ? FileSystem::ToUtf8(f) : error));
                ++failed;
                continue;
            }
            if (command == "import" && HasFlag(args, "--part")) doc->SetKind(AquaTool::LayoutKind::Part);
            Print(std::format("{} -> Assets/{}", FileSystem::ToUtf8(doc->sourcePath.filename()), doc->output));
        }
        Print(std::format("{} exported, {} failed", files.size() - static_cast<size_t>(failed), failed));
        return failed ? 1 : 0;
    }
    return command == "help" || command == "--help" ? (Usage(), 0) : Usage();
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args = GetUtf8Args(argc, argv);

    // コマンドライン（--project / --open / ファイル 1 つ以外）
    if (!args.empty() && !args[0].starts_with("--") && (args.size() > 1 || args[0] == "help")) {
        AttachParentConsole();
        // ウィンドウを出さないので OpenGL は無い。画像の読み込み（GetTexture）などは使わない
        Platform::Initialize();
        const int code = RunCommandLine(args);
        Platform::Shutdown();
        return code;
    }

    AquaTool::StartupOptions options;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--project" && i + 1 < args.size()) options.project = FileSystem::FromUtf8(args[++i]);
        else if (args[i] == "--open" && i + 1 < args.size()) options.open.push_back(FileSystem::FromUtf8(args[++i]));
        else if (!args[i].starts_with("--")) options.open.push_back(FileSystem::FromUtf8(args[i]));
    }
    // 同じプロジェクトの Aqua がもう動いていれば、そちらで開いてもらう
    if (!options.project.empty() && Aqua::IsRunning(options.project)) {
        Aqua::PostRequest(options.project, {});
        for (const fs::path& f : options.open) Aqua::PostRequest(options.project, f);
        return 0;
    }

    try {
        AquaTool::App app(std::move(options));
        app.Run();
    } catch (const std::exception& e) {
        Log::Error("Fatal error: {}", e.what());
        ShowErrorDialog("Aqua", e.what());
        return 1;
    }
    return 0;
}
