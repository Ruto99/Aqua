#include "AquaDocument.h"

#include "AquaXml.h"

#include <Ruto/Aqua/AquaFormat.h>

#include <IconsFontAwesome6.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <set>
#include <utility>

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

// アセットのプロパティ（画像・フォント・音・シェーダー・パーツ）に prefix を付ける / 名前を集める
class AssetPathVisitor final : public PropertyVisitor {
public:
    std::string prefix;
    std::vector<std::string>* deps = nullptr;   // 付ける前のパス
    std::vector<std::string>* names = nullptr;  // プロパティの名前（スタイルの書き換え用）
    void Asset(std::string_view n, std::string& v, AssetType) override {
        if (names) names->emplace_back(n);
        if (v.empty() || v.front() == '@') return; // 組み込みの画像
        if (deps && std::find(deps->begin(), deps->end(), v) == deps->end()) deps->push_back(v);
        v = prefix + v;
    }
    void Property(std::string_view n, std::string& v) override {
        if (n != "frames") return;
        // コマ送りの画像（1 行に 1 つ）
        std::string out;
        size_t start = 0;
        while (start <= v.size()) {
            size_t end = v.find('\n', start);
            if (end == std::string::npos) end = v.size();
            std::string line = v.substr(start, end - start);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty() && line.front() != '@') {
                if (deps && std::find(deps->begin(), deps->end(), line) == deps->end()) deps->push_back(line);
                line = prefix + line;
            }
            out += line;
            if (end < v.size()) out += '\n';
            start = end + 1;
        }
        v = out;
    }
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
};

bool IsLayoutExt(const std::string& path) {
    std::string e = path.substr(std::min(path.size(), path.find_last_of('.') == std::string::npos ? path.size() : path.find_last_of('.')));
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return e == ".aqua" || e == ".rui";
}

// 送り先の記録（ライブラリのフォルダ → 送り先）。ユーザーごとの設定フォルダに置く
fs::path SendTargetsFile() {
    return Platform::GetUserDataDir() / "Aqua" / "send-targets.json";
}

std::string LibraryKey(const fs::path& dir) {
    std::string k = FileSystem::ToGenericUtf8(dir);
    std::transform(k.begin(), k.end(), k.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return k;
}

// 中身が違うか新しいときだけコピーする（ライブ反映で何度も呼ばれる）
bool CopyIfChanged(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    if (!fs::is_regular_file(from, ec)) return false;
    if (fs::exists(to, ec) && fs::file_size(to, ec) == fs::file_size(from, ec) && fs::last_write_time(to, ec) >= fs::last_write_time(from, ec)) return true;
    fs::create_directories(to.parent_path(), ec);
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    return !ec;
}

} // namespace

// ---------------------------------------------------------------------------
// レイアウトの種類・ワークスペース
// ---------------------------------------------------------------------------

const char* LayoutKindName(LayoutKind kind) { return kind == LayoutKind::Part ? "part" : "body"; }
const char* LayoutKindLabel(LayoutKind kind) { return kind == LayoutKind::Part ? "パーツ" : "Body"; }
const char* LayoutKindIcon(LayoutKind kind) { return kind == LayoutKind::Part ? ICON_FA_PUZZLE_PIECE : ICON_FA_DESKTOP; }

LayoutKind ParseLayoutKind(std::string_view name, LayoutKind fallback) {
    if (name == "part") return LayoutKind::Part;
    if (name == "body") return LayoutKind::Body;
    return fallback;
}

bool IsGameProjectDir(const fs::path& dir) {
    std::error_code ec;
    return fs::is_directory(dir / "Assets", ec);
}

fs::path FindWorkspaceFor(const fs::path& file) {
    std::error_code ec;
    const fs::path abs = fs::weakly_canonical(fs::absolute(file, ec), ec);
    auto isUnder = [](const fs::path& p, const fs::path& dir) {
        const std::string a = FileSystem::ToGenericUtf8(p), d = FileSystem::ToGenericUtf8(dir) + "/";
        return a.size() > d.size() && std::equal(d.begin(), d.end(), a.begin(), [](char x, char y) { return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y)); });
    };
    for (fs::path dir = abs.parent_path(); !dir.empty(); dir = dir.parent_path()) {
        if (fs::exists(dir / kLibraryFile, ec)) return dir;
        if (IsGameProjectDir(dir) && (isUnder(abs, dir / "AquaUI") || isUnder(abs, dir / "Assets"))) return dir;
        if (dir == dir.root_path()) break;
    }
    fs::path dir = abs.parent_path();
    const std::string name = FileSystem::ToUtf8(dir.filename());
    if ((name == "Body" || name == "Parts") && dir.has_parent_path()) dir = dir.parent_path();
    return dir;
}

void PrefixAssetPaths(WidgetAsset& asset, const std::string& prefix, std::vector<std::string>* deps) {
    AssetPathVisitor v;
    v.prefix = prefix;
    v.deps = deps;
    if (asset.root) {
        asset.root->ForEach([&](Widget& w) {
            if (w.generated) return;
            w.ReflectCommon(v);
            w.Reflect(v);
        });
    }
    // スタイルの値（ウィジェットの型から、どのキーがアセットかを調べる）
    for (WidgetStyle& st : asset.styles) {
        auto probe = WidgetRegistry::Create(st.type);
        if (!probe) continue;
        std::vector<std::string> names;
        AssetPathVisitor nv;
        nv.names = &names;
        probe->ReflectCommon(nv);
        probe->Reflect(nv);
        for (const std::string& key : names) {
            const Json& current = std::as_const(st.properties)[key]; // 読むだけ（無いキーを作らない）
            if (!current.IsString()) continue;
            const std::string value = current.AsString();
            if (value.empty() || value.front() == '@') continue;
            if (deps && std::find(deps->begin(), deps->end(), value) == deps->end()) deps->push_back(value);
            st.properties[key] = prefix + value;
        }
    }
}

// ---------------------------------------------------------------------------
// 編集用ファイル（.aqs）
// ---------------------------------------------------------------------------

std::optional<Json> ReadSourceFile(const fs::path& file, std::string* error, bool* wasJson) {
    if (wasJson) *wasJson = false;
    const auto text = FileSystem::ReadTextFile(file);
    if (!text) {
        if (error) *error = "開けません: " + FileSystem::ToUtf8(file);
        return std::nullopt;
    }
    // 以前の形式（JSON）は { で始まる
    std::string_view body = *text;
    if (body.starts_with("\xEF\xBB\xBF")) body.remove_prefix(3);
    while (!body.empty() && (body.front() == ' ' || body.front() == '\t' || body.front() == '\r' || body.front() == '\n')) body.remove_prefix(1);
    std::string e;
    std::optional<Json> json;
    if (body.starts_with("{")) {
        json = Json::Parse(body, &e);
        if (json && wasJson) *wasJson = true;
    } else {
        json = SourceFromXml(body, &e);
    }
    if (!json && error) *error = FileSystem::ToUtf8(file) + ": " + e;
    return json;
}

bool WriteSourceFile(const fs::path& file, const Json& source) {
    return WriteIfChanged(file, SourceToXml(source));
}

// ---------------------------------------------------------------------------
// プロジェクト
// ---------------------------------------------------------------------------

bool Project::Open(const fs::path& dir, std::string* error) {
    std::error_code ec;
    const fs::path abs = fs::weakly_canonical(dir, ec);
    if (!fs::is_directory(abs, ec)) {
        if (error) *error = "フォルダがありません: " + FileSystem::ToUtf8(abs);
        return false;
    }
    m_Dir = abs;
    m_Layout = Ruto::Aqua::ProjectLayout{abs};
    m_Kind = IsGameProjectDir(abs) ? WorkspaceKind::GameProject : WorkspaceKind::Library;
    Assets::SetRoot(GetAssetRoot());
    Assets::ClearCache();
    WidgetAsset::ClearCache();
    m_DefaultResolution = {1920.0f, 1080.0f};
    settings = {};

    if (IsLibrary()) {
        // ライブラリ: 目印のファイル（名前）を読む。無ければ作る（次からファイルを 1 つ開いたときもここが見つかる）
        m_Name = FileSystem::ToUtf8(abs.filename());
        const fs::path marker = abs / kLibraryFile;
        if (const auto json = ReadJsonFile(marker, nullptr)) {
            m_Name = (*json)["name"].AsString(m_Name);
            const Json& res = (*json)["resolution"];
            if (res.Size() == 2) m_DefaultResolution = {static_cast<float>(res.At(0).AsNumber(1920.0)), static_cast<float>(res.At(1).AsNumber(1080.0))};
            settings.liveExport = (*json)["liveExport"].AsBool(true);
        } else {
            Json j = Json::MakeObject();
            j["format"] = "AquaLibrary";
            j["version"] = 1;
            j["name"] = m_Name;
            WriteIfChanged(marker, j.Dump(2) + "\n");
        }
        settings.generateHeaders = false; // ヘッダーは送り先のプロジェクトに書く
        LoadSendTarget();
        Refresh(true);
        return true;
    }

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

bool Project::CreateLibrary(const fs::path& dir, std::string* error) {
    std::error_code ec;
    for (const char* sub : {"Body", "Parts", "Images", "Fonts", "Shaders"}) fs::create_directories(dir / sub, ec);
    if (ec) {
        if (error) *error = "フォルダを作れません: " + FileSystem::ToUtf8(dir);
        return false;
    }
    if (!fs::exists(dir / kLibraryFile, ec)) {
        Json j = Json::MakeObject();
        j["format"] = "AquaLibrary";
        j["version"] = 1;
        j["name"] = FileSystem::ToUtf8(dir.filename());
        Json res = Json::MakeArray();
        res.Push(1920);
        res.Push(1080);
        j["resolution"] = std::move(res);
        if (!WriteIfChanged(dir / kLibraryFile, j.Dump(2) + "\n")) {
            if (error) *error = "書けません: " + FileSystem::ToUtf8(dir / kLibraryFile);
            return false;
        }
    }
    return true;
}

fs::path Project::GetAssetRoot() const { return IsLibrary() ? m_Dir : m_Layout.AssetsDir(); }
fs::path Project::GetSourceDir() const { return IsLibrary() ? m_Dir : m_Layout.SourceDir(); }

std::string Project::DefaultFolderFor(LayoutKind kind) const {
    if (IsLibrary()) return kind == LayoutKind::Part ? "Parts" : "Body";
    return kind == LayoutKind::Part ? "Parts" : "";
}

Vec2 Project::GetDefaultResolution(LayoutKind kind) const {
    return kind == LayoutKind::Part ? Vec2{480.0f, 270.0f} : m_DefaultResolution;
}

const Project::SourceInfo* Project::FindSourceInfo(const std::string& output) const {
    for (const SourceInfo& s : m_Sources) {
        if (s.output == output) return &s;
    }
    return nullptr;
}

LayoutKind Project::KindOf(const std::string& output, LayoutKind fallback) const {
    if (const SourceInfo* s = FindSourceInfo(output)) return s->kind;
    if (const auto bin = Ruto::Aqua::ReadFile(Assets::GetPath(output))) return ParseLayoutKind(bin->meta["kind"].AsString(), fallback);
    return fallback;
}

void Project::LoadSendTarget() {
    sendTarget = {};
    sendTarget.folder = "UI/" + ToIdentifier(m_Name);
    const auto json = ReadJsonFile(SendTargetsFile(), nullptr);
    if (!json) return;
    const Json& t = (*json)["libraries"][LibraryKey(m_Dir)];
    if (!t.IsObject()) return;
    sendTarget.project = FileSystem::FromUtf8(t["project"].AsString());
    sendTarget.folder = t["folder"].AsString(sendTarget.folder);
    sendTarget.live = t["live"].AsBool(true);
}

void Project::SaveSendTarget() const {
    if (!IsLibrary()) return;
    Json j = Json::MakeObject();
    if (const auto json = ReadJsonFile(SendTargetsFile(), nullptr); json && json->IsObject()) j = *json;
    if (!j["libraries"].IsObject()) j["libraries"] = Json::MakeObject();
    if (sendTarget.Valid()) {
        Json t = Json::MakeObject();
        t["library"] = FileSystem::ToUtf8(m_Dir);
        t["project"] = FileSystem::ToUtf8(sendTarget.project);
        t["folder"] = sendTarget.folder;
        t["live"] = sendTarget.live;
        j["libraries"][LibraryKey(m_Dir)] = std::move(t);
    } else {
        Json rest = Json::MakeObject();
        for (const auto& [k, v] : j["libraries"].Items()) {
            if (k != LibraryKey(m_Dir)) rest[k] = v;
        }
        j["libraries"] = std::move(rest);
    }
    WriteIfChanged(SendTargetsFile(), j.Dump(2) + "\n");
}

std::string Project::SendPathFor(const std::string& output) const {
    std::string folder = sendTarget.folder;
    while (!folder.empty() && (folder.back() == '/' || folder.back() == '\\')) folder.pop_back();
    return folder.empty() ? output : folder + "/" + output;
}

bool Project::SendLayout(const std::string& output, std::string* error, std::set<std::string>* done) const {
    if (!IsLibrary() || !sendTarget.Valid()) {
        if (error) *error = "送り先がありません";
        return false;
    }
    std::set<std::string> local;
    if (!done) done = &local;
    if (!done->insert(output).second) return true;
    if (!IsGameProjectDir(sendTarget.project)) {
        if (error) *error = "送り先が RutoEngine のプロジェクトではありません（Assets がありません）: " + FileSystem::ToUtf8(sendTarget.project);
        return false;
    }
    auto bin = Ruto::Aqua::ReadFile(m_Dir / FileSystem::FromUtf8(output), error);
    if (!bin) return false;
    std::string prefix = SendPathFor("");
    WidgetAsset asset;
    asset.FromJson(bin->content);
    std::vector<std::string> deps;
    PrefixAssetPaths(asset, prefix, &deps);
    bin->content = asset.ToJson();
    bin->dependencies.clear();
    for (const std::string& d : deps) bin->dependencies.push_back(prefix + d);
    bin->meta["library"] = m_Name;
    bin->meta["source"] = "library:" + m_Name + "/" + output;
    const fs::path assets = sendTarget.project / "Assets";
    const std::string sentPath = SendPathFor(output);
    if (!Ruto::Aqua::WriteFile(assets / FileSystem::FromUtf8(sentPath), *bin, error)) return false;
    // スクリプト用のヘッダー（送り先が書き出す設定なら）
    bool headers = true;
    if (const auto json = ReadJsonFile(sendTarget.project / "ProjectSettings" / "Aqua.json", nullptr)) headers = (*json)["generateHeaders"].AsBool(true);
    if (headers) {
        const std::string title = FileSystem::ToUtf8(FileSystem::FromUtf8(output).stem());
        WriteIfChanged(sendTarget.project / "Source" / "AquaGenerated" / FileSystem::FromUtf8(ReplaceExt(sentPath, ".h")),
                       GenerateLayoutHeader(asset, title, sentPath, "ライブラリ " + m_Name + " の " + ReplaceExt(output, Ruto::Aqua::kSourceExtension)));
    }
    // 使っているパーツ（送る）と素材（コピーする）
    bool ok = true;
    for (const std::string& d : deps) {
        if (IsLayoutExt(d)) {
            ok &= SendLayout(d, error, done);
        } else if (!CopyIfChanged(m_Dir / FileSystem::FromUtf8(d), assets / FileSystem::FromUtf8(prefix + d))) {
            Log::Warn("送れませんでした（ライブラリにありません）: {}", d);
        }
    }
    return ok;
}

void Project::SaveSettings() const {
    if (!IsOpen()) return;
    if (IsLibrary()) {
        Json j = Json::MakeObject();
        if (const auto json = ReadJsonFile(m_Dir / kLibraryFile, nullptr); json && json->IsObject()) j = *json;
        j["liveExport"] = settings.liveExport;
        WriteIfChanged(m_Dir / kLibraryFile, j.Dump(2) + "\n");
        return;
    }
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
    const fs::path srcDir = GetSourceDir();
    if (fs::is_directory(srcDir, ec)) {
        for (auto it = fs::recursive_directory_iterator(srcDir, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) break;
            // ライブラリでは隠しフォルダ・build は見ない
            const std::string name = FileSystem::ToUtf8(it->path().filename());
            if (it->is_directory(ec) && (name.starts_with(".") || name == "build")) {
                it.disable_recursion_pending();
                continue;
            }
            if (!it->is_regular_file(ec) || LowerExt(it->path()) != Ruto::Aqua::kSourceExtension) continue;
            SourceInfo info{it->path(), {}};
            LayoutKind fallback = LayoutKind::Body;
            if (IsLibrary() && FileSystem::ToGenericUtf8(fs::relative(it->path(), srcDir, ec)).starts_with("Parts/")) fallback = LayoutKind::Part;
            info.kind = fallback;
            if (const auto json = ReadSourceFile(it->path(), nullptr)) {
                info.output = (*json)["output"].AsString();
                info.kind = ParseLayoutKind((*json)["kind"].AsString(), fallback);
            }
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
    // 部品として入れられるのはパーツだけ（編集用ファイルの無い .aqua・.rui は種類が分からないので出す）
    auto& widgets = m_AssetLists[AssetType::Widget];
    for (const SourceInfo& s : m_Sources) {
        if (s.kind == LayoutKind::Part) widgets.push_back(s.output);
    }
    for (std::string& f : Assets::FindFiles({".aqua", ".rui"})) {
        if (!FindSourceInfo(f) && KindOf(f, LayoutKind::Part) == LayoutKind::Part) widgets.push_back(std::move(f));
    }
    m_AssetLists[AssetType::UIShader] = Assets::FindFiles({".uishader"});
    m_AllAssets = Assets::FindFiles({".png", ".jpg", ".jpeg", ".bmp", ".tga", ".ttf", ".otf", ".ttc", ".bdf", ".fnt", ".rfont", ".wav", ".ogg", ".aqua", ".rui", ".uishader"});
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
    if (IsLibrary()) {
        // ライブラリ: .aqs の隣に .aqua
        fs::path rel = fs::relative(source, m_Dir, ec);
        if (ec || rel.empty() || FileSystem::ToGenericUtf8(rel).starts_with("..")) rel = source.filename();
        rel.replace_extension(Ruto::Aqua::kExtension);
        return FileSystem::ToGenericUtf8(rel);
    }
    fs::path rel = fs::relative(source, m_Layout.SourceDir(), ec);
    if (ec || rel.empty() || FileSystem::ToGenericUtf8(rel).starts_with("..")) rel = source.filename();
    rel.replace_extension(Ruto::Aqua::kExtension);
    const std::string folder = settings.outputFolder.empty() ? std::string() : settings.outputFolder + "/";
    return folder + FileSystem::ToGenericUtf8(rel);
}

fs::path Project::DefaultSourceFor(const std::string& output) const {
    std::string rel = ReplaceExt(output, Ruto::Aqua::kSourceExtension);
    if (IsLibrary()) return m_Dir / FileSystem::FromUtf8(rel);
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

std::unique_ptr<Document> Document::CreateNew(Project& project, const fs::path& source, const std::string& output, LayoutKind kind) {
    auto doc = std::make_unique<Document>(project);
    doc->sourcePath = source;
    doc->output = output;
    doc->kind = kind;
    doc->asset = std::shared_ptr<WidgetAsset>(WidgetAsset::CreateDefault().release());
    doc->asset->referenceResolution = project.GetDefaultResolution(kind);
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
        bool wasJson = false;
        auto json = ReadSourceFile(file, error, &wasJson);
        if (!json) return nullptr;
        if ((*json)["format"].AsString() != "AquaSource") {
            if (error) *error = "Aqua の編集用ファイルではありません: " + FileSystem::ToUtf8(file);
            return nullptr;
        }
        doc->sourcePath = file;
        doc->output = (*json)["output"].AsString();
        if (doc->output.empty()) doc->output = project.DefaultOutputFor(file);
        doc->asset->FromJson((*json)["document"]);
        if (const Project::SourceInfo* info = project.FindSourceInfo(doc->output)) doc->kind = info->kind;
        doc->kind = ParseLayoutKind((*json)["kind"].AsString(), doc->kind);
        for (const auto& [k, v] : (*json)["preview"]["variables"].Items()) doc->previewVariables[k] = v.AsString();
        for (const Json& n : (*json)["editor"]["locked"].Elements()) doc->lockedWidgets.insert(n.AsString());
        for (const Json& n : (*json)["editor"]["hidden"].Elements()) doc->editorHidden.insert(n.AsString());
        if (wasJson) {
            // 以前の JSON の編集用ファイルは、開いたときに XML に書き直す
            doc->asset->path = doc->output;
            if (doc->Save()) Log::Info("{} を XML の形式に変えました", FileSystem::ToUtf8(file.filename()));
        }
    } else if (ext == Ruto::Aqua::kExtension) {
        // バイナリだけがあるとき（ほかの人が書き出した・編集用ファイルを消した）は、そこから編集用ファイルを作り直す
        auto bin = Ruto::Aqua::ReadFile(file, error);
        if (!bin) return nullptr;
        doc->output = project.MakeAssetRelative(file);
        if (doc->output.empty()) {
            if (error) *error = project.IsLibrary() ? "ライブラリの中の .aqua を選んでください" : "プロジェクトの Assets の中の .aqua を選んでください";
            return nullptr;
        }
        doc->sourcePath = project.FindSourceFor(doc->output);
        if (doc->sourcePath.empty()) {
            doc->sourcePath = project.DefaultSourceFor(doc->output);
            needSave = true;
        }
        if (!needSave) return Open(project, doc->sourcePath, error);
        doc->asset->FromJson(bin->content);
        doc->kind = ParseLayoutKind(bin->meta["kind"].AsString(), LayoutKind::Body);
    } else if (ext == ".rui") {
        // 以前の RutoEditor の UI デザイナーで作った UI を取り込む（.rui はそのまま残す）
        auto json = ReadJsonFile(file, error);
        if (!json) return nullptr;
        const std::string rel = project.MakeAssetRelative(file);
        // 種類は JSON の kind（Aqua のコマンドラインで作るときなど。無ければ Body）
        doc->kind = ParseLayoutKind((*json)["kind"].AsString(), LayoutKind::Body);
        const std::string folder = project.IsLibrary() ? project.DefaultFolderFor(doc->kind) : project.settings.outputFolder;
        doc->output = rel.empty() ? (folder.empty() ? std::string() : folder + "/") + FileSystem::ToUtf8(file.stem()) + ".aqua" : ReplaceExt(rel, ".aqua");
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

bool Document::IsLocked(const Widget& w) const {
    for (const Widget* p = &w; p; p = p->GetParent()) {
        if (!p->name.empty() && lockedWidgets.contains(p->name)) return true;
    }
    return false;
}

std::string Document::GetTitle() const {
    return FileSystem::ToUtf8(sourcePath.stem());
}

void Document::SetKind(LayoutKind k) {
    if (kind == k) return;
    kind = k;
    Save();
    Export();
    project.Refresh(true);
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
    j["version"] = 2; // 2 = XML
    j["output"] = output;
    j["kind"] = LayoutKindName(kind);
    j["generator"] = kGenerator;
    j["document"] = asset->ToJson();
    Json vars = Json::MakeObject();
    for (const auto& [k, v] : previewVariables) vars[k] = v;
    j["preview"]["variables"] = std::move(vars);
    if (!lockedWidgets.empty() || !editorHidden.empty()) {
        Json locked = Json::MakeArray(), hidden = Json::MakeArray();
        for (const std::string& n : lockedWidgets) locked.Push(n);
        for (const std::string& n : editorHidden) hidden.Push(n);
        j["editor"]["locked"] = std::move(locked);
        j["editor"]["hidden"] = std::move(hidden);
    }
    if (!WriteSourceFile(sourcePath, j)) {
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
    bin.meta["kind"] = LayoutKindName(kind);
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
    if (project.settings.generateHeaders && !project.IsLibrary()) WriteHeader();
    // ライブラリ: 送り先があり、ライブ反映ならそのまま RutoEngine に送る
    if (project.IsLibrary() && project.sendTarget.Valid() && project.sendTarget.live) {
        std::string sendError;
        if (!project.SendLayout(output, &sendError)) {
            m_SaveError = "送れません: " + sendError;
            Log::Error("{}", m_SaveError);
        }
    }
    return true;
}

void Document::Update(double now) {
    if (m_ExportPending && project.settings.liveExport && now - m_LastExport >= kLiveExportSeconds) Export();
}

std::vector<std::string> Document::CollectVariables() const {
    return CollectLayoutVariables(*asset);
}

std::vector<std::string> CollectLayoutVariables(const WidgetAsset& layout) {
    std::vector<std::string> vars;
    WidgetAsset* asset = const_cast<WidgetAsset*>(&layout); // Reflect は読むだけでも非 const
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
    std::error_code ec;
    return GenerateLayoutHeader(*asset, GetTitle(), output, FileSystem::ToGenericUtf8(fs::relative(sourcePath, project.GetDir(), ec)));
}

std::string GenerateLayoutHeader(const WidgetAsset& layout, const std::string& title, const std::string& output, const std::string& source) {
    const WidgetAsset* asset = &layout;
    const std::string ns = ToIdentifier(title);
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
            std::string message;
            if (auto* b = dynamic_cast<UIButton*>(&w)) message = b->clickMessage;
            else if (auto* bp = dynamic_cast<UIBoundingPane*>(&w)) message = bp->clickMessage;
            if (!message.empty() && std::find(messages.begin(), messages.end(), message) == messages.end()) messages.push_back(message);
        });
    }
    for (const WidgetAnimation& a : asset->animations) {
        animations.push_back(a.name);
        for (const WidgetAnimSection& sec : a.sections) {
            if (std::find(animations.begin(), animations.end(), sec.name) == animations.end()) animations.push_back(sec.name);
        }
    }
    std::vector<std::string> groups;
    for (const WidgetGroup& g : asset->groups) groups.push_back(g.name);
    emit("Widgets", "ウィジェットの名前（Find・OnClick・SetText・SetVisible）", widgets);
    emit("Animations", "アニメーションと区間の名前（PlayAnimation）", animations);
    emit("Groups", "グループの名前（FindGroup・SetGroupVisible）", groups);
    emit("Variables", "変数の名前（SetVariable）", CollectLayoutVariables(layout));
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
