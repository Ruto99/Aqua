#pragma once

// Aqua — UI のレイアウトを作るツール（RutoEngine のゲームのプロジェクトでも、単体のライブラリでも使える）
//
//   左:   レイアウト（Body・パーツの一覧）・パレット（パーツも）・階層
//   中央: 文書のタブ + 見たままのキャンバス、下にアニメーションのタイムライン
//   右:   詳細（スロット・プロパティ・スタイル）・スタイル・変数（プレビュー用の値）・整列
//   下:   アニメーション（文字アニメーションも）・アセット・シェーダー（UI のシェーダーを書く）・ログ
//
// 編集すると .aqua がすぐ書き出され、ゲームのプロジェクト（か、ライブラリの送り先）の RutoEditor・実行中のゲームに反映される。

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
    bool OpenProject(const fs::path& dir);  // ゲームのプロジェクトかライブラリ（Assets があるかで決まる）
    void CloseProject();
    Document* OpenDocument(const fs::path& file);
    // どこにあるファイルでも開く（含むワークスペースを探して開き直す）
    Document* OpenAnyFile(const fs::path& file);
    void ShowOpenFileDialog();
    void ShowOpenFolderDialog();
    void CreateLibraryAt(const fs::path& dir);
    // ライブラリのレイアウトを送り先にすべて送る
    void SendAll();
    void CloseDocument(size_t index);
    Document* ActiveDocument();
    void NewDocument(const std::string& name, LayoutKind kind);
    void ExportAll();
    void HandleLinkRequests();
    void WriteSession();
    void AddRecentProject(const fs::path& dir);
    std::vector<fs::path> LoadRecentProjects() const;
    void Notify(std::string text, bool error = false);

    // ---- 画面（AquaApp.cpp） ----
    void DrawUI();
    void DrawMenuBar();
    void DrawMenuBrand();
    void DrawDockSpace();
    void BuildDefaultLayout(unsigned int dockspace);
    void DrawStatusBar(float height);
    void DrawWelcome();
    void DrawLogPanel();
    void DrawPopups();
    void DrawSendPopup();
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
    // パーツ（UIUserWidget）の上書き: 中のウィジェットを選んで、置いた先だけ値を変える
    void DrawPartOverrides(Document& doc, UIUserWidget& uw);
    void DrawWidgetMenu(Document& doc, const WidgetPath& path);
    // timeline: アニメーションのパネルから（キーのコピー・削除などはパネルが扱うので、ウィジェットには効かせない）
    void HandleDesignerKeys(Document& doc, bool timeline = false);
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

    // ---- 文字アニメーション（AquaTextAnim.cpp）: テキストの「自作」の出し方・動きをキーフレームで作る ----
    //   アニメーションのパネルの右側に描く（一覧で「文字アニメーション」を選んだとき）
    void DrawTextAnimPanel(Document& doc);
    void OpenTextAnimEditor(int slot); // 0: 出し方 / 1: 動き（アニメーションのパネルを前に出す）

    // ---- シェーダー（AquaShaderPanel.cpp）: .uishader を書く・見本から作る・ウィジェットに掛ける ----
    void DrawShaderPanel();
    void OpenShaderEditor(const std::string& assetPath); // 空なら新しく作る
    void LoadShaderForEdit(const std::string& assetPath);
    bool SaveShader();
    std::string CreateShader(const std::string& name, const std::string& source);
    void ApplyShaderToSelection(const std::string& assetPath);

    // ---- グループ（AquaArrange.cpp）: 階層とは別にウィジェットを名前でまとめる（アニメーション・スクリプト用） ----
    void DrawGroupsPanel(Document& doc);

    // ---- 整列（AquaArrange.cpp）: 選んだウィジェットを揃える・等間隔にする・格子に並べる ----
    void DrawArrangePanel(Document& doc);
    std::vector<Widget*> ArrangeTargets(Document& doc, bool* children = nullptr);
    void ToggleExtraSelection(Document& doc, const WidgetPath& path);

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

    // シェーダーの編集
    std::string m_ShaderPath;                  // Assets からの相対パス
    std::string m_ShaderText;                  // 編集中の中身
    std::string m_ShaderSavedText;             // ファイルの中身
    std::shared_ptr<UIShader> m_ShaderCheck;   // 編集中の中身をコンパイルしたもの（エラーの表示）
    std::vector<UIShaderParam> m_ShaderSavedParams; // 保存したときの @param（値を名前で引き継ぐ）
    double m_ShaderEditTime = -1.0;            // 最後に書き換えた時刻（少し待ってからコンパイルする）
    bool m_ShaderLive = true;                  // コンパイルできたらすぐ保存してキャンバスに反映する
    bool m_ShaderHelp = false;
    bool m_FocusShaderPanel = false;
    bool m_OpenNewShader = false;
    std::string m_NewShaderName;
    int m_NewShaderPreset = -1;                // -1: 空のひな形
    bool m_NewShaderApply = true;
    int m_ShaderJumpLine = -1;                 // エラーの行へカーソルを動かす
    int m_ShaderCursorLine = 1;

    // コピーしたキー（ウィジェットのアニメーション・文字アニメーション）
    KeyClipboard m_KeyClipboard;
    // 文字アニメーション
    int m_TextAnimSlot = 0;            // 0: 出し方 / 1: 動き
    float m_TextAnimTime = 0.0f;       // 見本の時刻
    bool m_TextAnimPlaying = true;
    float m_TextAnimDir = 1.0f;        // 1: 再生 / -1: 逆再生
    bool m_TextAnimLoop = true;
    float m_TextAnimZoom = 400.0f;     // 1 秒の幅（ピクセル）
    KeySelection m_TextKeySel;         // 選んでいるキー（行はチャンネルの番号）
    std::string m_TextKeyOwner;        // 選んだキーのテキストと出し方 / 動き（変わったら選択を外す）
    std::string m_TextAnimSample;      // 見本の文字（空ならテキストの文字）
    std::string m_TextAnimSaveName;
    bool m_AnimTextMode = false;       // アニメーションのパネルで文字アニメーションを編集している
    bool m_FocusAnimPanel = false;

    // パーツの上書きで編集している中のウィジェット（名前）
    std::string m_OverrideTarget;
    // グループ
    std::string m_GroupRename;   // 名前を変えているグループ
    std::string m_GroupRenameBuffer;

    // 整列
    int m_ArrangeRelative = 0;         // 0: 選んだものの範囲 / 1: 親 / 2: 最初に選んだもの
    int m_ArrangeColumns = 3;
    float m_ArrangeGap[2] = {16.0f, 16.0f};
    float m_ArrangeSpacing = 16.0f;
    float m_ArrangeMargin = 0.0f;
    int m_ArrangeOrder = 0;            // 0: 今の位置の順 / 1: 階層の順
    int m_ArrangeCellSize = 0;         // 0: そのまま / 1: 一番大きいものに揃える
    int m_ArrangeOrigin = 0;           // 0: 今の範囲の左上 / 1: 親の左上（余白付き）/ 2: 親の中央

    // ポップアップ
    bool m_OpenNewPopup = false;
    std::string m_NewName;
    std::string m_NewFolder;
    LayoutKind m_NewKind = LayoutKind::Body;
    bool m_OpenSendPopup = false;
    std::string m_SendProjectInput; // 送り先のプロジェクト（入力中）
    std::string m_SendFolderInput;
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
