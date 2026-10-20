# Aqua

RutoEngine のゲームの UI（メニュー・HUD・設定画面など）を見たまま作るツール。
RutoEngine とは別のアプリで、作った UI は `.aqua`（UI のバイナリ）として書き出され、RutoEngine の `UIDocument` が表示する。

![Aqua](docs/aqua.png)

## 起動

**`Aqua.bat` をダブルクリック**すると Aqua が単独で開く（ようこそ画面から最近のプロジェクト・RutoLauncher のプロジェクトを選ぶか、フォルダを開く）。

- まだビルドしていないとき、または Aqua のソース・RutoEngine の SDK が `bin\Aqua.exe` より新しいときは、先に自動でビルドしてから開く。
- ビルドに失敗しても前の `bin\Aqua.exe` があれば、それを開く。
- 引数はそのまま Aqua に渡る（例: `Aqua.bat --project D:\Games\MyGame`、`Aqua.bat D:\Games\MyGame\Assets\UI\Title.aqua`）。

ショートカットを作るなら `Aqua.bat` か、ビルド後の `bin\Aqua.exe` を指せばよい。

## ビルド

RutoEngine の SDK（ビルド済みのエンジン）をリンクして作る。先に RutoEngine の `Setup.bat` で SDK を作っておく。

```bat
build.bat                  :: release でビルドして bin\Aqua.exe に置く（= build.bat release install）
build.bat debug build      :: build\debug\bin\Aqua.exe（デバッグ用）
build.bat release run      :: ビルドして起動
build.bat debug clean
```

RutoEngine の場所は、環境変数 `RUTO_ENGINE_DIR` → このフォルダの隣の `..\RutoEngine` の順に探す。

```
MyProgram/
├─ RutoEngine/   エンジン（SDK/Debug・SDK/Release を Aqua がリンクする）
└─ Aqua/         このフォルダ
   ├─ Aqua.bat         Aqua を開く（必要ならビルドしてから）
   ├─ build.bat        ビルド
   ├─ Source/          Aqua のソース
   ├─ Resources/       アイコン（make_icon.py で作り直せる）
   ├─ build/           ビルドフォルダ（debug / release）
   └─ bin/Aqua.exe     build.bat install で置かれる Aqua
```

## RutoEngine との連携

- RutoEditor の .aqua のダブルクリック・UIDocument の「Aqua で開く」・「作成 > UI（Aqua で作る）」、RutoLauncher の「Aqua で UI を作る」で起動する。
- Aqua.exe の場所は、Aqua が起動するたびにユーザーの設定フォルダ（`%APPDATA%\Ruto\RutoEngine\Aqua\location.json`）に登録するので、どこに置いても見つかる。
  まだ一度も起動していなければ、RutoEngine の隣の `Aqua\bin`・`Aqua\build\release\bin`・`Aqua\build\debug\bin` を探す。環境変数 `AQUA_EXE` で直接指定もできる。
- 同じプロジェクトの Aqua は 1 つだけ起動し、エディターから開くと起動中の Aqua でそのファイルが開く（`build/Aqua` のロックと依頼のファイル）。
- **動的な反映**: 編集のたびに（ドラッグ中も 0.1 秒ごとに）`.aqua` を書き出し、エンジンはファイルの更新を見て読み直す。
  RutoEditor のシーンビュー・ゲームビュー・**プレイ中のゲーム**にそのまま反映され、部品（ユーザーウィジェット）を変えたときは使っている UI も作り直される。

## 画面

| 画面 | できること |
|---|---|
| プロジェクト | UI（`AquaUI/*.aqs`）の一覧・新規作成・複製・削除。以前の RutoEditor の UI デザイナーの `.rui` の取り込み |
| パレット・階層 | ドラッグで配置・並べ替え・親子付け、ラップ / 中身を取り出す、表示 / 非表示の切り替え、名前の変更（F2） |
| キャンバス | 見たままの編集。8 方向のリサイズ、**親・兄弟の端と中央への吸い付き**（Alt で無効、Ctrl で 10 刻み）、アンカー表示、解像度の切り替え、ホイールで拡大、プレビュー（P。実際に操作して確かめる） |
| 詳細・スタイル | スロット（アンカーのプリセット・余白・揃え・Auto / Fill）・プロパティ・スタイルの作成 / 反映 / 名前の変更 / 削除 |
| 変数 | テキストの `{変数}` や「変数と連動」に入る値を入れて見た目を確かめる（スライダーで 0〜1 も試せる） |
| アニメーション | ウィジェットアニメーションのタイムライン（録画・キーの移動・補間） |
| アセット | 画像・フォント・音・UI のサムネイル。キャンバスへドラッグ: 画像 → Image（画像・ボタン・枠に落とすと差し替え）、フォント → テキスト、音 → ボタンのクリック音、UI → 部品（ユーザーウィジェット） |

キー: Ctrl+Z / Ctrl+Y・Ctrl+C / V / X（別の UI・別の Aqua にも貼れる）・Ctrl+D・Delete・F2・矢印キー（1 ずつ、Shift で 10）・F（全体を表示）・G（グリッド）・P（プレビュー）・Ctrl+S（保存して書き出す）・Ctrl+N / Ctrl+O / Ctrl+W・Ctrl+Shift+E（すべて書き出す）。
変更は自動で保存・書き出しされる。

## ゲームのプロジェクトに作られるファイル

| ファイル | 中身 |
|---|---|
| `AquaUI/*.aqs` | Aqua の編集用ファイル（JSON。バージョン管理向け。ゲームには入らない） |
| `Assets/UI/*.aqua` | 書き出した UI のバイナリ（ゲームが読む。文字列表 + 型付きの値 + CRC。JSON と相互に変換でき情報は失われない） |
| `Source/AquaGenerated/**/*.h` | スクリプト用の名前の定数（ウィジェット・アニメーション・変数・ボタンのメッセージ）。書き間違いがコンパイルエラーになる |
| `ProjectSettings/Aqua.json` | Aqua の設定（書き出し先・ヘッダーを書き出すか・ライブで書き出すか） |

```cpp
#include "AquaGenerated/UI/Title.h"   // Aqua が書き出したヘッダー

auto* ui = GetEntity().GetComponent<UIDocument>();
ui->asset = AquaUI::Title::kAsset;                                   // "UI/Title.aqua"
ui->OnClick(AquaUI::Title::Widgets::StartButton, [] { SceneManager::LoadScene("Scenes/Stage1.rscene"); });
ui->SetVariable(AquaUI::Title::Variables::score, 1200);
ui->PlayAnimation(AquaUI::Title::Animations::FadeIn);
```

## コマンドライン

CI・ビルドの前処理向け（ウィンドウは出ない）。

```bat
Aqua.exe export D:\Games\MyGame                   :: AquaUI のすべての UI を書き出す
Aqua.exe export AquaUI\Title.aqs                  :: 1 つだけ書き出す
Aqua.exe import Assets\UI\Menu.rui --project .   :: 以前の .rui を取り込む
Aqua.exe info Assets\UI\Title.aqua               :: バイナリの中身（情報・依存・文書）を JSON で見る
Aqua.exe to-json Assets\UI\Title.aqua out.json
Aqua.exe from-json Menu.json Assets\UI\Menu.aqua
Aqua.exe --project D:\Games\MyGame --open Assets\UI\Title.aqua
```

`.aqua` の形式と読み書きは RutoEngine の `Ruto::Aqua` モジュール（`Ruto/Aqua/AquaFormat.h`）にあり、エンジン・Aqua・ほかの道具で共通。
