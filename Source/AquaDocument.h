#pragma once

// Aqua のワークスペース（プロジェクト・ライブラリ）と文書
//
//   ワークスペースは 2 種類:
//     ゲームのプロジェクト = RutoEngine のゲームのフォルダ（Assets がある所）。編集用は AquaUI/*.aqs、書き出しは Assets/**/*.aqua
//     ライブラリ           = RutoEngine と関係のない好きなフォルダ（aqua-library.json が目印）。
//                            Body/・Parts/ の .aqs の隣に .aqua を書き出す。画像・フォント・シェーダーもフォルダの中に置き、
//                            参照はすべてライブラリのフォルダからの相対パス。.aqs を 1 つだけ開いたときはそのフォルダがライブラリ
//   文書 = 1 つのレイアウト。編集用の .aqs（XML）と書き出しの .aqua の組。種類は 2 つ:
//     Body   … 画面のいちばん親のレイアウト（タイトル・HUD・ポーズ画面など）。部品としては入れられない
//     パーツ … ほかのレイアウト（Body・パーツ）に部品（UIUserWidget）として入れて使い回すレイアウト
//
//   編集するたびに .aqs を保存して .aqua を書き出す（ドラッグ中も 0.1 秒ごとに .aqua だけ書き出す）。
//   エンジンは .aqua の更新を見て読み直すので、エディター・実行中のゲームにそのまま反映される。
//   ライブラリは「RutoEngine に送る」でゲームのプロジェクトの Assets/<フォルダ>/ に書き出す（使っているパーツ・素材も一緒に）。
//   送り先はライブラリごとに覚え、以後は編集するたびに送る。

#include "AquaCommon.h"
#include "AquaKeys.h"

#include <Ruto/Aqua/AquaLink.h>

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace AquaTool {

namespace fs = std::filesystem;

// レイアウトの種類
enum class LayoutKind { Body, Part };
const char* LayoutKindName(LayoutKind kind);   // .aqs に書く名前（"body" / "part"）
const char* LayoutKindLabel(LayoutKind kind);  // 表示名（"Body" / "パーツ"）
const char* LayoutKindIcon(LayoutKind kind);
LayoutKind ParseLayoutKind(std::string_view name, LayoutKind fallback);

// ワークスペースの種類
enum class WorkspaceKind { GameProject, Library };
inline constexpr char kLibraryFile[] = "aqua-library.json"; // ライブラリの目印（フォルダの直下）

// ファイルを含むワークスペースのフォルダ（ゲームのプロジェクトの AquaUI・Assets の中か、aqua-library.json のあるフォルダ。
// どちらでもなければファイルのフォルダ。そのフォルダが Body / Parts ならその親）
fs::path FindWorkspaceFor(const fs::path& file);
// フォルダがゲームのプロジェクト（Assets がある）か
bool IsGameProjectDir(const fs::path& dir);

// レイアウトのウィジェットが使っているアセット（画像・フォント・音・シェーダー・パーツ）のパスに prefix を付ける。
// 付ける前のパスを deps に集める（@ で始まる組み込みの画像は除く）。ライブラリから RutoEngine に送るときに使う
void PrefixAssetPaths(WidgetAsset& asset, const std::string& prefix, std::vector<std::string>* deps = nullptr);

// スクリプト用のヘッダーの中身（assetPath は UIDocument::asset に入れるパス、sourceLabel は元のファイルの説明）
std::string GenerateLayoutHeader(const WidgetAsset& asset, const std::string& title, const std::string& assetPath, const std::string& sourceLabel);
// テキストの {変数} と bind・visibleBind に出てくる変数の名前
std::vector<std::string> CollectLayoutVariables(const WidgetAsset& asset);

// 編集用ファイル（.aqs）を読む。XML（今の形式）と JSON（以前の形式）のどちらも読める
//   wasJson … 以前の JSON の形式だった（次に保存すると XML になる）
std::optional<Json> ReadSourceFile(const fs::path& file, std::string* error = nullptr, bool* wasJson = nullptr);
// 編集用ファイルを XML で書く（内容が同じなら書かない）
bool WriteSourceFile(const fs::path& file, const Json& source);

// ---------------------------------------------------------------------------
// プロジェクト
// ---------------------------------------------------------------------------

// ライブラリの送り先（ユーザーごとに覚える。ライブラリのファイルには書かない）
struct SendTarget {
    fs::path project;        // RutoEngine のゲームのプロジェクトのフォルダ
    std::string folder;      // 送り先（Assets からの相対パス。例: UI/MyLibrary）
    bool live = true;        // 編集するたびに送る
    bool Valid() const { return !project.empty(); }
};

struct ProjectSettingsData {
    std::string outputFolder = "UI";  // 書き出し先（Assets からの相対パス）
    bool generateHeaders = true;      // スクリプト用の名前の定数を Source/AquaGenerated に書き出す
    bool liveExport = true;           // ドラッグ中も書き出してエディター・ゲームに反映する
};

// ワークスペース（ゲームのプロジェクトかライブラリ）
class Project {
public:
    // Assets があればゲームのプロジェクト、なければライブラリとして開く（ライブラリには aqua-library.json を作る）
    bool Open(const fs::path& dir, std::string* error = nullptr);
    // 新しいライブラリ（Body・Parts・Images・Fonts・Shaders のフォルダと aqua-library.json）を作る
    static bool CreateLibrary(const fs::path& dir, std::string* error = nullptr);
    bool IsOpen() const { return !m_Dir.empty(); }

    WorkspaceKind GetKind() const { return m_Kind; }
    bool IsLibrary() const { return m_Kind == WorkspaceKind::Library; }
    const fs::path& GetDir() const { return m_Dir; }
    const Ruto::Aqua::ProjectLayout& GetLayout() const { return m_Layout; } // ゲームのプロジェクトのときだけ使う
    const std::string& GetName() const { return m_Name; }
    Vec2 GetDefaultResolution(LayoutKind kind = LayoutKind::Body) const;
    // アセット（画像・.aqua など）のパスの基準（ゲーム: Assets / ライブラリ: ライブラリのフォルダ）
    fs::path GetAssetRoot() const;
    // 編集用ファイルの置き場所（ゲーム: AquaUI / ライブラリ: ライブラリのフォルダ）
    fs::path GetSourceDir() const;
    // 新しいレイアウトの既定のフォルダ（GetSourceDir からの相対。ライブラリは Body / Parts）
    std::string DefaultFolderFor(LayoutKind kind) const;

    ProjectSettingsData settings;
    void SaveSettings() const;

    // 編集用ファイル（.aqs）の一覧と、その書き出し先・種類
    struct SourceInfo {
        fs::path path;
        std::string output; // アセットのルートからの相対パス
        LayoutKind kind = LayoutKind::Body;
    };
    const std::vector<SourceInfo>& GetSources() const { return m_Sources; }
    const SourceInfo* FindSourceInfo(const std::string& output) const;
    // 書き出し先のレイアウトの種類（編集用ファイルが無ければ .aqua の情報。分からなければ fallback）
    LayoutKind KindOf(const std::string& output, LayoutKind fallback) const;

    // ---- RutoEngine に送る（ライブラリのとき） ----
    SendTarget sendTarget;
    void SaveSendTarget() const;
    // 送り先での Assets からの相対パス
    std::string SendPathFor(const std::string& output) const;
    // 書き出した .aqua を送り先に書く（使っているパーツ・素材も一緒に）。done は同じものを 2 回送らないため
    bool SendLayout(const std::string& output, std::string* error = nullptr, std::set<std::string>* done = nullptr) const;
    // Assets 内のファイルの一覧（拡張子で絞る）。0.5 秒ごとに作り直す
    const std::vector<std::string>& GetAssets(AssetType type);
    const std::vector<std::string>& GetAllAssets();
    void Refresh(bool force = false);

    // 書き出し先 → 編集用ファイル（無ければ空）
    fs::path FindSourceFor(const std::string& output) const;
    // 編集用ファイルの既定の書き出し先 / 書き出し先の既定の編集用ファイル
    std::string DefaultOutputFor(const fs::path& source) const;
    fs::path DefaultSourceFor(const std::string& output) const;
    std::string MakeAssetRelative(const fs::path& absolute) const;

private:
    void LoadSendTarget();

    fs::path m_Dir;
    WorkspaceKind m_Kind = WorkspaceKind::GameProject;
    Ruto::Aqua::ProjectLayout m_Layout;
    std::string m_Name;
    Vec2 m_DefaultResolution{1920.0f, 1080.0f};
    std::vector<SourceInfo> m_Sources;
    std::map<AssetType, std::vector<std::string>> m_AssetLists;
    std::vector<std::string> m_AllAssets;
    double m_RefreshTime = -100.0;
};

// ---------------------------------------------------------------------------
// 編集の状態（文書ごと）
// ---------------------------------------------------------------------------

struct ViewRect {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    bool Valid() const { return w > 1.0f && h > 1.0f; }
};

// ドラッグ中に吸い付いた線（キャンバスに表示する）
struct SnapGuide {
    bool vertical = true;
    float pos = 0.0f;              // 基準解像度の単位
    float from = 0.0f, to = 0.0f;  // 線の範囲
};

struct DesignerState {
    WidgetPath selection;
    bool hasSelection = false;
    // 一緒に選んでいるウィジェットの名前（Shift / Ctrl + クリックで足す。整列パネルで使う）
    // 名前は文書の中で重ならないので、編集で木の形が変わっても指す先がずれない
    std::vector<std::string> extraSelection;
    WidgetPath hover;
    bool hasHover = false;

    float zoom = 0.0f;   // 0 なら全体が見えるように合わせる
    Vec2 pan{0.0f, 0.0f};
    int resolution = 0;
    bool showGrid = true;
    bool snap = true;
    bool preview = false;

    enum class Drag { None, Move, Resize, Pan };
    Drag drag = Drag::None;
    int handle = -1;
    Vec2 dragMouse{0.0f, 0.0f};
    bool dragMoved = false;
    WidgetSlot dragSlot;
    UI::Rect dragRect;
    std::vector<SnapGuide> guides;
    struct GroupDrag {
        std::string name;
        WidgetSlot slot;  // ドラッグを始めたときのスロット
        UI::Rect rect;    // ドラッグを始めたときの矩形
        UI::Rect parent;  // 親の矩形
    };
    std::vector<GroupDrag> dragGroup; // 一緒に選んでいるもの（まとめて動かす）

    WidgetPath renaming;
    bool isRenaming = false;
    std::string renameBuffer;

    // アニメーション
    int animation = -1;
    float animTime = 0.0f;
    bool animPlaying = false;
    bool animReverse = false;   // 逆再生（時刻が戻っていく）
    bool animLoop = true;       // 端まで来たら繰り返す（オフなら止まる）
    bool animRecording = false;
    float animZoom = 200.0f;
    bool keySelected = false;
    int keyTrack = -1;
    float keyTime = 0.0f;
    bool keyDragging = false;
    KeySelection animKeys;      // タイムラインで選んでいるキー（いくつでも）
    std::string renamingAnim;
    bool isRenamingAnim = false;
    // カーブ表示（トラックの値をグラフで見て、キーを上下左右に動かす）
    bool curveView = false;
    int curveTrack = -1;
    int curveKey = -1;      // ドラッグしているキー
    std::string animRenameBuffer;

    // プレビューで起きたイベント
    std::string lastEvent;
    double lastEventTime = -100.0;

    // 描画先（画面上の位置）
    ViewRect canvas;
    ViewRect screen;
    float screenZoom = 1.0f;
    bool focused = false;
};

// ---------------------------------------------------------------------------
// 文書
// ---------------------------------------------------------------------------

class Document {
public:
    Document(Project& project);
    ~Document();

    // 新しいレイアウト（Body は全画面、パーツは小さめの Canvas Panel だけ）
    static std::unique_ptr<Document> CreateNew(Project& project, const fs::path& source, const std::string& output, LayoutKind kind = LayoutKind::Body);
    // .aqs / .aqua / .rui を開く（.aqua・.rui は編集用ファイルを作って開く）
    static std::unique_ptr<Document> Open(Project& project, const fs::path& file, std::string* error = nullptr);

    Project& project;
    fs::path sourcePath;      // 編集用ファイル（.aqs）
    std::string output;       // アセットのルートからの相対パス（.aqua）
    LayoutKind kind = LayoutKind::Body;
    std::shared_ptr<WidgetAsset> asset;
    std::map<std::string, std::string> previewVariables; // プレビュー用の変数の値（.aqs に保存）
    // 編集のときだけの状態（.aqs に保存し、書き出す .aqua・ゲームには入らない）
    std::set<std::string> lockedWidgets;  // ロック: キャンバスで選べない・動かせない
    std::set<std::string> editorHidden;   // 編集中だけ隠す（キャンバスに描かない）
    bool IsLocked(const Widget& w) const;  // 自分か祖先がロックされているか
    DesignerState ui;
    std::string clipboard;

    // プレビュー（キャンバスに描く実物）
    std::unique_ptr<WidgetDocument> view;
    std::unique_ptr<Framebuffer> framebuffer;
    UI::Context* uiContext = nullptr;

    std::string GetTitle() const;
    bool IsPart() const { return kind == LayoutKind::Part; }
    // 種類を変える（保存・書き出しもする）
    void SetKind(LayoutKind k);
    bool IsSaved() const { return !m_Pending && m_SaveError.empty(); }
    const std::string& GetLastError() const { return m_SaveError; }
    double GetLastExportTime() const { return m_LastExport; }

    // 編集した（キャンバスにすぐ反映。マウスを離したら Commit される）
    void Changed();
    bool HasPending() const { return m_Pending; }
    // 元に戻すの単位を確定し、保存・書き出しする
    void Commit();
    void Undo();
    void Redo();
    bool CanUndo() const { return !m_Undo.empty() || m_Pending; }
    bool CanRedo() const { return !m_Redo.empty(); }

    bool Save();
    // 書き出す（ライブラリで送り先があり、ライブ反映なら RutoEngine にも送る）
    bool Export();
    // 毎フレーム: ドラッグ中の書き出し（ライブ反映）
    void Update(double now);

    // テキストの {変数} と bind・visibleBind に出てくる変数の名前
    std::vector<std::string> CollectVariables() const;
    // スクリプト用のヘッダーの中身
    std::string GenerateHeader() const;

private:
    void ResetHistory();
    bool WriteHeader();

    std::vector<std::string> m_Undo;
    std::vector<std::string> m_Redo;
    std::string m_Committed;
    bool m_Pending = false;
    bool m_ExportPending = false;
    double m_LastExport = -100.0;
    std::string m_SaveError;
};

} // namespace AquaTool
