#pragma once

// Aqua のプロジェクトと文書
//
//   プロジェクト = RutoEngine のゲームプロジェクトのフォルダ（Assets・ProjectSettings がある所）
//   文書         = 1 つの UI。編集用の AquaUI/*.aqs（XML）と、書き出し先の Assets/**/*.aqua の組
//
//   編集するたびに .aqs を保存して .aqua を書き出す（ドラッグ中も 0.1 秒ごとに .aqua だけ書き出す）。
//   エンジンは .aqua の更新を見て読み直すので、エディター・実行中のゲームにそのまま反映される。

#include "AquaCommon.h"

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

// 編集用ファイル（AquaUI/*.aqs）を読む。XML（今の形式）と JSON（以前の形式）のどちらも読める
//   wasJson … 以前の JSON の形式だった（次に保存すると XML になる）
std::optional<Json> ReadSourceFile(const fs::path& file, std::string* error = nullptr, bool* wasJson = nullptr);
// 編集用ファイルを XML で書く（内容が同じなら書かない）
bool WriteSourceFile(const fs::path& file, const Json& source);

// ---------------------------------------------------------------------------
// プロジェクト
// ---------------------------------------------------------------------------

struct ProjectSettingsData {
    std::string outputFolder = "UI";  // 書き出し先（Assets からの相対パス）
    bool generateHeaders = true;      // スクリプト用の名前の定数を Source/AquaGenerated に書き出す
    bool liveExport = true;           // ドラッグ中も書き出してエディター・ゲームに反映する
};

class Project {
public:
    bool Open(const fs::path& dir, std::string* error = nullptr);
    bool IsOpen() const { return !m_Dir.empty(); }

    const fs::path& GetDir() const { return m_Dir; }
    const Ruto::Aqua::ProjectLayout& GetLayout() const { return m_Layout; }
    const std::string& GetName() const { return m_Name; }
    Vec2 GetDefaultResolution() const { return m_DefaultResolution; }

    ProjectSettingsData settings;
    void SaveSettings() const;

    // 編集用ファイル（AquaUI/*.aqs）の一覧と、その書き出し先
    struct SourceInfo {
        fs::path path;
        std::string output; // Assets からの相対パス
    };
    const std::vector<SourceInfo>& GetSources() const { return m_Sources; }
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
    fs::path m_Dir;
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

    WidgetPath renaming;
    bool isRenaming = false;
    std::string renameBuffer;

    // アニメーション
    int animation = -1;
    float animTime = 0.0f;
    bool animPlaying = false;
    bool animRecording = false;
    float animZoom = 200.0f;
    bool keySelected = false;
    int keyTrack = -1;
    float keyTime = 0.0f;
    bool keyDragging = false;
    std::string renamingAnim;
    bool isRenamingAnim = false;
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

    // 新しい UI（全画面の Canvas Panel だけ）
    static std::unique_ptr<Document> CreateNew(Project& project, const fs::path& source, const std::string& output);
    // .aqs / .aqua / .rui を開く（.aqua・.rui は編集用ファイルを作って開く）
    static std::unique_ptr<Document> Open(Project& project, const fs::path& file, std::string* error = nullptr);

    Project& project;
    fs::path sourcePath;      // AquaUI/*.aqs
    std::string output;       // Assets からの相対パス（.aqua）
    std::shared_ptr<WidgetAsset> asset;
    std::map<std::string, std::string> previewVariables; // プレビュー用の変数の値（.aqs に保存）
    DesignerState ui;
    std::string clipboard;

    // プレビュー（キャンバスに描く実物）
    std::unique_ptr<WidgetDocument> view;
    std::unique_ptr<Framebuffer> framebuffer;
    UI::Context* uiContext = nullptr;

    std::string GetTitle() const;
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
