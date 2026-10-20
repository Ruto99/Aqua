#include "AquaDocument.h"

#include <Ruto/Aqua/AquaFormat.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <set>

namespace AquaTool {

namespace {

constexpr char kGenerator[] = "Aqua " AQUA_VERSION;
constexpr double kListRefreshSeconds = 0.5;
constexpr double kLiveExportSeconds = 0.1;
constexpr size_t kMaxUndo = 200;

std::string LowerExt(const fs::path& p) {
    std::string e = FileSystem::ToUtf8(p.extension());
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return e;
}

std::string NowIso8601() {
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    return std::format("{:%Y-%m-%dT%H:%M:%SZ}", now);
}

// "a/b/c.ext" から拡張子を取り替える
std::string ReplaceExt(const std::string& rel, std::string_view ext) {
    const size_t slash = rel.find_last_of('/');
    const size_t dot = rel.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) return rel.substr(0, dot) + std::string(ext);
    return rel + std::string(ext);
}

std::optional<Json> ReadJsonFile(const fs::path& file, std::string* error) {
    const auto text = FileSystem::ReadTextFile(file);
    if (!text) {
        if (error) *error = "開けません: " + FileSystem::ToUtf8(file);
        return std::nullopt;
    }
    std::string e;
    auto json = Json::Parse(*text, &e);
    if (!json && error) *error = FileSystem::ToUtf8(file) + ": " + e;
    return json;
}

// 内容が同じなら書かない（ゲームのビルドを無駄にやり直させない）
bool WriteIfChanged(const fs::path& file, const std::string& text) {
    if (const auto old = FileSystem::ReadTextFile(file); old && *old == text) return true;
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    return FileSystem::WriteTextFile(file, text);
}

// テキストのプロパティ（text・bind・visibleBind・placeholder）を集める
class StringCollector final : public PropertyVisitor {
public:
    std::vector<std::pair<std::string, std::string>>* out = nullptr;
    void Property(std::string_view n, std::string& v) override { out->emplace_back(std::string(n), v); }
    void Property(std::string_view, bool&) override {}
    void Property(std::string_view, int&) override {}
    void Property(std::string_view, float&) override {}
    void Property(std::string_view, Vec2&) override {}
    void Property(std::string_view, Vec3&) override {}
    void Property(std::string_view, Vec4&) override {}
    void Property(std::string_view, EntityRef&) override {}
    void Range(std::string_view, float&, float, float) override {}
    void Color(std::string_view, Vec3&) override {}
    void Color(std::string_view, Vec4&) override {}
    void Enum(std::string_view, int&, std::span<const char* const>) override {}
    void Asset(std::string_view, std::string&, AssetType) override {}
};

} // namespace

// ---------------------------------------------------------------------------
// プロジェクト
// ---------------------------------------------------------------------------

bool Project::Open(const fs::path& dir, std::string* error) {
    std::error_code ec;
    const fs::path abs = fs::weakly_canonical(dir, ec);
    if (!fs::is_directory(abs / "Assets", ec)) {
        if (error) *error = "Assets フォルダがありません（RutoEngine のプロジェクトのフォルダを選んでください）: " + FileSystem::ToUtf8(abs);
        return false;
    }
    m_Dir = abs;
    m_Layout = Ruto::Aqua::ProjectLayout{abs};
    Assets::SetRoot(m_Layout.AssetsDir());
    Assets::ClearCache();
    WidgetAsset::ClearCache();

    // プロジェクトの名前と画面の大きさ（新しい UI の基準解像度）
    m_Name = FileSystem::ToUtf8(abs.filename());
    for (const auto& entry : fs::directory_iterator(abs, ec)) {
        if (entry.path().extension() == ".rutoproject") m_Name = FileSystem::ToUtf8(entry.path().stem());
    }
    ProjectSettings& ps = ProjectSettings::Get();
    if (ps.Load(abs / "ProjectSettings" / "Project.json")) {
        if (!ps.productName.empty()) m_Name = ps.productName;
        if (ps.windowWidth >= 320 && ps.windowHeight >= 240) m_DefaultResolution = {static_cast<float>(ps.windowWidth), static_cast<float>(ps.windowHeight)};
    }

    settings = {};
    if (const auto json = ReadJsonFile(m_Layout.SettingsFile(), nullptr)) {
        const Json& j = *json;
        settings.outputFolder = j["outputFolder"].AsString(settings.outputFolder);
        settings.generateHeaders = j["generateHeaders"].AsBool(settings.generateHeaders);
        settings.liveExport = j["liveExport"].AsBool(settings.liveExport);
    }
    fs::create_directories(m_Layout.SourceDir(), ec);
    Refresh(true);
    return true;
}

void Project::SaveSettings() const {
    if (!IsOpen()) return;
    Json j = Json::MakeObject();
    j["outputFolder"] = settings.outputFolder;
    j["generateHeaders"] = settings.generateHeaders;
    j["liveExport"] = settings.liveExport;
    WriteIfChanged(m_Layout.SettingsFile(), j.Dump(2) + "\n");
}

void Project::Refresh(bool force) {
    const double now = Platform::GetTime();
    if (!force && now - m_RefreshTime < kListRefreshSeconds) return;
    m_RefreshTime = now;
    if (!IsOpen()) return;

    m_Sources.clear();
    std::error_code ec;
    if (fs::is_directory(m_Layout.SourceDir(), ec)) {
        for (auto it = fs::recursive_directory_iterator(m_Layout.SourceDir(), ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) break;
            if (!it->is_regular_file(ec) || LowerExt(it->path()) != Ruto::Aqua::kSourceExtension) continue;
            SourceInfo info{it->path(), {}};
            if (const auto json = ReadJsonFile(it->path(), nullptr)) info.output = (*json)["output"].AsString();
            if (info.output.empty()) info.output = DefaultOutputFor(it->path());
            m_Sources.push_back(std::move(info));
        }
    }
    std::sort(m_Sources.begin(), m_Sources.end(), [](const SourceInfo& a, const SourceInfo& b) { return a.path < b.path; });

    m_AssetLists.clear();
    auto& tex = m_AssetLists[AssetType::Texture];
    tex = Assets::GetBuiltinTextureNames();
    for (std::string& f : Assets::FindFiles({".png", ".jpg", ".jpeg", ".bmp", ".tga"})) tex.push_back(std::move(f));
    m_AssetLists[AssetType::Font] = Assets::FindFiles({".ttf", ".otf", ".ttc", ".bdf", ".fnt", ".rfont"});
    m_AssetLists[AssetType::Audio] = Assets::FindFiles({".wav", ".ogg"});
    m_AssetLists[AssetType::Widget] = Assets::FindFiles({".aqua", ".rui"});
    m_AllAssets = Assets::FindFiles({".png", ".jpg", ".jpeg", ".bmp", ".tga", ".ttf", ".otf", ".ttc", ".bdf", ".fnt", ".rfont", ".wav", ".ogg", ".aqua", ".rui"});
}

const std::vector<std::string>& Project::GetAssets(AssetType type) {
    Refresh();
    return m_AssetLists[type];
}

const std::vector<std::string>& Project::GetAllAssets() {
    Refresh();
    return m_AllAssets;
}

fs::path Project::FindSourceFor(const std::string& output) const {
    for (const SourceInfo& s : m_Sources) {
        if (s.output == output) return s.path;
    }
    return {};
}

std::string Project::DefaultOutputFor(const fs::path& source) const {
    std::error_code ec;
    fs::path rel = fs::relative(source, m_Layout.SourceDir(), ec);
    if (ec || rel.empty() || FileSystem::ToGenericUtf8(rel).starts_with("..")) rel = source.filename();
    rel.replace_extension(Ruto::Aqua::kExtension);
    const std::string folder = settings.outputFolder.empty() ? std::string() : settings.outputFolder + "/";
    return folder + FileSystem::ToGenericUtf8(rel);
}

fs::path Project::DefaultSourceFor(const std::string& output) const {
    std::string rel = ReplaceExt(output, Ruto::Aqua::kSourceExtension);
    const std::string folder = settings.outputFolder + "/";
    if (!settings.outputFolder.empty() && rel.starts_with(folder)) rel = rel.substr(folder.size());
    return m_Layout.SourceDir() / FileSystem::FromUtf8(rel);
}

std::string Project::MakeAssetRelative(const fs::path& absolute) const {
    return Assets::MakeRelative(absolute);
}

// ---------------------------------------------------------------------------
// 文書
// ---------------------------------------------------------------------------

Document::Document(Project& p) : project(p) {}

Document::~Document() {
    view.reset();
    framebuffer.reset();
    if (uiContext) UI::DestroyContext(uiContext);
}

std::unique_ptr<Document> Document::CreateNew(Project& project, const fs::path& source, const std::string& output) {
    auto doc = std::make_unique<Document>(project);
    doc->sourcePath = source;
    doc->output = output;
    doc->asset = std::shared_ptr<WidgetAsset>(WidgetAsset::CreateDefault().release());
    doc->asset->referenceResolution = project.GetDefaultResolution();
    doc->asset->path = output;
    doc->ResetHistory();
    doc->Save();
    doc->Export();
    project.Refresh(true);
    return doc;
}

std::unique_ptr<Document> Document::Open(Project& project, const fs::path& file, std::string* error) {
    auto doc = std::make_unique<Document>(project);
    doc->asset = std::make_shared<WidgetAsset>();
    const std::string ext = LowerExt(file);
    bool needSave = false;
    if (ext == Ruto::Aqua::kSourceExtension) {
        auto json = ReadJsonFile(file, error);
        if (!json) return nullptr;
        if ((*json)["format"].AsString() != "AquaSource") {
            if (error) *error = "Aqua の編集用ファイルではありません: " + FileSystem::ToUtf8(file);
            return nullptr;
        }
        doc->sourcePath = file;
        doc->output = (*json)["output"].AsString();
        if (doc->output.empty()) doc->output = project.DefaultOutputFor(file);
        doc->asset->FromJson((*json)["document"]);
        for (const auto& [k, v] : (*json)["preview"]["variables"].Items()) doc->previewVariables[k] = v.AsString();
    } else if (ext == Ruto::Aqua::kExtension) {
        // バイナリだけがあるとき（ほかの人が書き出した・編集用ファイルを消した）は、そこから編集用ファイルを作り直す
        auto bin = Ruto::Aqua::ReadFile(file, error);
        if (!bin) return nullptr;
        doc->output = project.MakeAssetRelative(file);
        if (doc->output.empty()) {
            if (error) *error = "プロジェクトの Assets の中の .aqua を選んでください";
            return nullptr;
        }
        doc->sourcePath = project.FindSourceFor(doc->output);
        if (doc->sourcePath.empty()) {
            doc->sourcePath = project.DefaultSourceFor(doc->output);
            needSave = true;
        }
        if (!needSave) return Open(project, doc->sourcePath, error);
        doc->asset->FromJson(bin->content);
    } else if (ext == ".rui") {
        // 以前の RutoEditor の UI デザイナーで作った UI を取り込む（.rui はそのまま残す）
        auto json = ReadJsonFile(file, error);
        if (!json) return nullptr;
        const std::string rel = project.MakeAssetRelative(file);
        doc->output = rel.empty() ? project.settings.outputFolder + "/" + FileSystem::ToUtf8(file.stem()) + ".aqua" : ReplaceExt(rel, ".aqua");
        if (const fs::path existing = project.FindSourceFor(doc->output); !existing.empty()) return Open(project, existing, error);
        doc->sourcePath = project.DefaultSourceFor(doc->output);
        doc->asset->FromJson(*json);
        needSave = true;
        Log::Info("{} を取り込みました → {}", rel.empty() ? FileSystem::ToUtf8(file) : rel, doc->output);
    } else {
        if (error) *error = "開けないファイルです（.aqs / .aqua / .rui）: " + FileSystem::ToUtf8(file);
        return nullptr;
    }
    doc->asset->path = doc->output;
    doc->ResetHistory();
    if (needSave) {
        // 別の編集用ファイルと名前がぶつかるなら番号を付ける
        std::error_code ec;
        for (int n = 1; fs::exists(doc->sourcePath, ec); ++n) {
            fs::path p = doc->sourcePath;
            p.replace_filename(FileSystem::FromUtf8(std::format("{}{}{}", FileSystem::ToUtf8(file.stem()), n, Ruto::Aqua::kSourceExtension)));
            doc->sourcePath = p;
        }
        doc->Save();
        doc->Export();
        project.Refresh(true);
    }
    return doc;
}

std::string Document::GetTitle() const {
    return FileSystem::ToUtf8(sourcePath.stem());
}

void Document::ResetHistory() {
    m_Undo.clear();
    m_Redo.clear();
    m_Committed = asset->ToJson().Dump(0);
    m_Pending = false;
}

void Document::Changed() {
    asset->MarkChanged();
    m_Pending = true;
    m_ExportPending = true;
}

void Document::Commit() {
    m_Pending = false;
    std::string now = asset->ToJson().Dump(0);
    if (now == m_Committed) {
        if (m_ExportPending) Export();
        return;
    }
    m_Undo.push_back(std::move(m_Committed));
    if (m_Undo.size() > kMaxUndo) m_Undo.erase(m_Undo.begin());
    m_Committed = std::move(now);
    m_Redo.clear();
    Save();
    Export();
}

void Document::Undo() {
    if (m_Pending) Commit();
    if (m_Undo.empty()) return;
    m_Redo.push_back(std::move(m_Committed));
    m_Committed = std::move(m_Undo.back());
    m_Undo.pop_back();
    if (auto json = Json::Parse(m_Committed)) asset->FromJson(*json);
    if (!Resolve(asset->root.get(), ui.selection)) ui.hasSelection = false;
    Save();
    Export();
}

void Document::Redo() {
    if (m_Redo.empty()) return;
    m_Undo.push_back(std::move(m_Committed));
    m_Committed = std::move(m_Redo.back());
    m_Redo.pop_back();
    if (auto json = Json::Parse(m_Committed)) asset->FromJson(*json);
    if (!Resolve(asset->root.get(), ui.selection)) ui.hasSelection = false;
    Save();
    Export();
}

bool Document::Save() {
    Json j = Json::MakeObject();
    j["format"] = "AquaSource";
    j["version"] = 1;
    j["output"] = output;
    j["generator"] = kGenerator;
    j["document"] = asset->ToJson();
    Json vars = Json::MakeObject();
    for (const auto& [k, v] : previewVariables) vars[k] = v;
    j["preview"]["variables"] = std::move(vars);
    std::error_code ec;
    fs::create_directories(sourcePath.parent_path(), ec);
    if (!FileSystem::WriteTextFile(sourcePath, j.Dump(2) + "\n")) {
        m_SaveError = "保存できません: " + FileSystem::ToUtf8(sourcePath);
        Log::Error("{}", m_SaveError);
        return false;
    }
    m_SaveError.clear();
    return true;
}

bool Document::Export() {
    m_ExportPending = false;
    m_LastExport = Platform::GetTime();
    Ruto::Aqua::Document bin;
    bin.meta["name"] = GetTitle();
    bin.meta["generator"] = kGenerator;
    std::error_code ec;
    bin.meta["source"] = FileSystem::ToGenericUtf8(fs::relative(sourcePath, project.GetDir(), ec));
    bin.meta["exportedAt"] = NowIso8601();
    Json res = Json::MakeArray();
    res.Push(asset->referenceResolution.x);
    res.Push(asset->referenceResolution.y);
    bin.meta["referenceResolution"] = std::move(res);
    bin.content = asset->ToJson();
    bin.dependencies = asset->CollectDependencies();
    std::string error;
    if (!Ruto::Aqua::WriteFile(Assets::GetPath(output), bin, &error)) {
        m_SaveError = "書き出せません: " + error;
        Log::Error("{}", m_SaveError);
        return false;
    }
    if (project.settings.generateHeaders) WriteHeader();
    return true;
}

void Document::Update(double now) {
    if (m_ExportPending && project.settings.liveExport && now - m_LastExport >= kLiveExportSeconds) Export();
}

std::vector<std::string> Document::CollectVariables() const {
    std::vector<std::string> vars;
    auto add = [&](std::string name) {
        while (!name.empty() && (name.front() == '!' || name.front() == ' ')) name.erase(name.begin());
        while (!name.empty() && name.back() == ' ') name.pop_back();
        if (!name.empty() && std::find(vars.begin(), vars.end(), name) == vars.end()) vars.push_back(std::move(name));
    };
    if (!asset->root) return vars;
    std::vector<std::pair<std::string, std::string>> strings;
    StringCollector c;
    c.out = &strings;
    asset->root->ForEach([&](Widget& w) {
        if (w.generated) return;
        w.ReflectCommon(c);
        w.Reflect(c);
    });
    for (const auto& [prop, value] : strings) {
        if (prop == "bind" || prop == "visibleBind") {
            add(value);
        } else if (prop == "text" || prop == "placeholder" || prop == "tooltip") {
            // {name} / {name:1}
            for (size_t open = value.find('{'); open != std::string::npos; open = value.find('{', open + 1)) {
                const size_t close = value.find('}', open);
                if (close == std::string::npos) break;
                std::string inner = value.substr(open + 1, close - open - 1);
                if (const size_t colon = inner.find(':'); colon != std::string::npos) inner.resize(colon);
                add(inner);
            }
        }
    }
    return vars;
}

std::string Document::GenerateHeader() const {
    const std::string ns = ToIdentifier(GetTitle());
    std::error_code ec;
    const std::string source = FileSystem::ToGenericUtf8(fs::relative(sourcePath, project.GetDir(), ec));
    std::string h;
    h += "// Aqua が自動生成したファイル（編集しないでください。Aqua で UI を変えると書き直されます）\n";
    h += std::format("//   {} → Assets/{}\n", source, output);
    h += "//\n";
    h += std::format("//   auto& ui = GetEntity().AddComponent<UIDocument>();\n");
    h += std::format("//   ui.asset = AquaUI::{}::kAsset;\n", ns);
    h += std::format("//   ui.OnClick(AquaUI::{}::Widgets::名前, [] {{ ... }});\n", ns);
    h += "#pragma once\n\n";
    h += std::format("namespace AquaUI::{} {{\n\n", ns);
    h += "// UIDocument::asset に入れるパス\n";
    h += std::format("inline constexpr const char* kAsset = \"{}\";\n", output);
    h += std::format("inline constexpr float kReferenceWidth = {:.0f}.0f;\n", asset->referenceResolution.x);
    h += std::format("inline constexpr float kReferenceHeight = {:.0f}.0f;\n", asset->referenceResolution.y);

    auto emit = [&](const char* name, const char* comment, const std::vector<std::string>& values) {
        if (values.empty()) return;
        h += std::format("\n// {}\nnamespace {} {{\n", comment, name);
        std::set<std::string> used;
        for (const std::string& v : values) {
            std::string id = ToIdentifier(v);
            for (int n = 2; used.contains(id); ++n) id = std::format("{}_{}", ToIdentifier(v), n);
            used.insert(id);
            std::string escaped;
            for (char c : v) {
                if (c == '"' || c == '\\') escaped += '\\';
                escaped += c;
            }
            h += std::format("inline constexpr const char* {} = \"{}\";\n", id, escaped);
        }
        h += std::format("}} // namespace {}\n", name);
    };

    std::vector<std::string> widgets, messages, animations;
    if (asset->root) {
        asset->root->ForEach([&](Widget& w) {
            if (w.generated || !w.GetParent() || w.name.empty()) return;
            if (std::find(widgets.begin(), widgets.end(), w.name) == widgets.end()) widgets.push_back(w.name);
            if (auto* b = dynamic_cast<UIButton*>(&w); b && !b->clickMessage.empty() &&
                std::find(messages.begin(), messages.end(), b->clickMessage) == messages.end())
                messages.push_back(b->clickMessage);
        });
    }
    for (const WidgetAnimation& a : asset->animations) animations.push_back(a.name);
    emit("Widgets", "ウィジェットの名前（Find・OnClick・SetText・SetVisible）", widgets);
    emit("Animations", "アニメーションの名前（PlayAnimation）", animations);
    emit("Variables", "変数の名前（SetVariable）", CollectVariables());
    emit("Messages", "ボタンのクリック時のメッセージ（OnUIEvent の e.message）", messages);
    h += std::format("\n}} // namespace AquaUI::{}\n", ns);
    return h;
}

bool Document::WriteHeader() {
    std::string rel = ReplaceExt(output, ".h");
    const fs::path file = project.GetLayout().GeneratedDir() / FileSystem::FromUtf8(rel);
    return WriteIfChanged(file, GenerateHeader());
}

} // namespace AquaTool
