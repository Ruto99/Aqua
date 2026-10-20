#pragma once

// Aqua — RutoEngine の UI を作る別のツール
//
//   左:   プロジェクト（UI の一覧）・アセット（画像・フォント・音・UI）・パレット・階層
//   中央: 文書のタブ + 見たままのキャンバス、下にアニメーションのタイムライン
//   右:   詳細（スロット・プロパティ・スタイル）・スタイル・変数（プレビュー用の値）
//   下:   ログ・状態（書き出し・RutoEditor との連携）
//
// 編集すると Assets/**/*.aqua がすぐ書き出され、RutoEditor・実行中のゲームに反映される。

#include "AquaDocument.h"

#include <Ruto/Aqua/AquaLink.h>

#include <deque>
#include <memory>
#include <string>
#include <vector>

struct ImFont;

namespace AquaTool {

struct StartupOptions {
    fs::path project;
    std::vector<fs::path> open;
};

class App : public Ruto::Application {
public:
    explicit App(StartupOptions options);
    ~App() override;

protected:
    void OnStart() override;
    void OnUpdate(float dt) override;
    void OnUIRender() override;
    void OnShutdown() override;
    bool OnCloseRequested() override;

private:
    // ---- プロジェクト・文書（AquaApp.cpp） ----
    bool OpenProject(const fs::path& dir);
    void CloseProject();
    Document* OpenDocument(const fs::path& file);
    void CloseDocument(size_t index);
    Document* ActiveDocument();
    void NewDocument(const std::string& name);
    void ExportAll();
    void HandleLinkRequests();
    void WriteSession();
    void AddRecentProject(const fs::path& dir);
    std::vector<fs::path> LoadRecentProjects() const;
    void Notify(std::string text, bool error = false);

    // ---- 画面（AquaApp.cpp） ----
    void DrawUI();
    void DrawMenuBar();
    void DrawDockSpace();
    void BuildDefaultLayout(unsigned int dockspace);
    void DrawStatusBar(float height);
    void DrawWelcome();
    void DrawLogPanel();
    void DrawPopups();
    void HandleShortcuts();

    // ---- パネル（AquaPanels.cpp） ----
    void DrawProjectPanel();
    void DrawAssetsPanel();
    void DrawStylesPanel(Document& doc);
    void DrawVariablesPanel(Document& doc);

    // ---- デザイナー（AquaDesigner.cpp） ----
    void DrawDocumentArea();
    void DrawToolbar(Document& doc);
    void DrawPalette(Document* doc);
    void DrawHierarchy(Document& doc);
    void DrawHierarchyNode(Document& doc, Widget& w, WidgetPath& path);
    void DrawCanvas(Document& doc);
    void DrawDetails(Document& doc);
    void DrawSlot(Document& doc, Widget& w);
    void DrawWidgetMenu(Document& doc, const WidgetPath& path);
    void HandleDesignerKeys(Document& doc);
    Widget* AddWidget(Document& doc, const std::string& type, Widget* parent, int index = -1, const Vec2* canvasPos = nullptr);
    void DeleteWidget(Document& doc, const WidgetPath& path);
    void DuplicateWidget(Document& doc, const WidgetPath& path);
    void WrapWidget(Document& doc, const WidgetPath& path, const std::string& type);
    void UnwrapWidget(Document& doc, const WidgetPath& path);
    void PasteWidget(Document& doc);
    void CopyWidget(Document& doc, const WidgetPath& path);
    void RenameWidget(Document& doc, Widget& w, std::string name);
    bool DropAsset(Document& doc, const std::string& asset, Widget* target, const Vec2& localPos);
    void RenderDocument(Document& doc);
    // ドラッグ中の矩形を、親・兄弟の端と中央に吸い付ける（基準解像度の単位）
    UI::Rect Snap(Document& doc, Widget& w, const UI::Rect& rect, int handle);

    // ---- アニメーション（AquaAnimation.cpp） ----
    void DrawAnimations(Document& doc);

    StartupOptions m_Options;
    Project m_Project;
    std::unique_ptr<Ruto::Aqua::LinkHost> m_Link;
    std::vector<std::unique_ptr<Document>> m_Docs;
    int m_Active = -1;
    int m_FocusTab = -1;       // 次のフレームで前に出すタブ
    float m_Scale = 1.0f;
    bool m_LayoutBuilt = false;
    bool m_ResetLayout = false;
    int m_DefaultTabsFrames = 0;
    double m_LastSession = -100.0;
    double m_LastLinkPoll = -100.0;
    bool m_EditorRunning = false;
    double m_LastEditorCheck = -100.0;

    // パネル
    std::string m_PaletteSearch;
    std::string m_AssetSearch;
    std::string m_ProjectSearch;
    int m_AssetFilter = 0;
    bool m_ShowLog = true;

    // ログ
    struct LogLine {
        Log::Level level;
        std::string text;
    };
    std::deque<LogLine> m_Log;
    int m_LogSink = -1;
    bool m_LogScroll = false;

    // ポップアップ
    bool m_OpenNewPopup = false;
    std::string m_NewName;
    std::string m_NewFolder;
    bool m_OpenSettingsPopup = false;
    bool m_OpenAboutPopup = false;
    bool m_OpenCloseConfirm = false;
    std::string m_RenameStyleFrom;
    std::string m_RenameStyleBuffer;
    std::string m_NewVariable;
    fs::path m_DeleteSource; // 削除の確認中の UI

    std::string m_Notice;
    bool m_NoticeError = false;
    double m_NoticeTime = -100.0;
    std::string m_Error; // 起動時のエラー（プロジェクトが開けないなど）
};

} // namespace AquaTool
