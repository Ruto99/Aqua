# AquaSamples

Aqua で作った UI の見本を集めた RutoEngine のプロジェクト。起動するとメニューが出て、カードを選ぶとそれぞれの機能のサンプルが開く（Esc でメニューに戻る）。

| # | サンプル | UI | 見どころ |
|---|---|---|---|
| 01 | 文字アニメーション | `AquaUI/TextAnimation.aqs` | UIText の effect（Wave・Shake・Rainbow・Pulse・Swing・Glitch）と reveal（Typewriter・Fade・Pop・Rise・Drop・Spin）、`<wave>` `<color=#…>` `<pause=秒>` などのタグ |
| 02 | 会話ウィンドウ | `AquaUI/Dialog.aqs` | タイプライター表示の台詞をスクリプトで送る（SetText・SkipText・RestartText）、出し終わったら ▼ を点滅（visibleBind） |
| 03 | UI シェーダー | `AquaUI/Shaders.aqs` | `Assets/Shaders/UI/*.uishader` の 13 種類。ディゾルブ・光らせるはアニメーションで、もう 1 つはスライダー → SetShaderParam で動かす |
| 04 | キャプチャー | `AquaUI/Capture.aqs` | UICapture: 後ろだけをぼかすすりガラス（Behind・EveryFrame）、ポーズ画面（OnShow + Time::SetTimeScale(0)）、スナップショット（Script） |
| 05 | アニメーション | `AquaUI/Animation.aqs` | 登場（Intro）・ずっと繰り返す（Loop）・ボタンで再生（Shake・Fill） |
| 06 | レイアウト | `AquaUI/Layout.aqs` | Horizontal Box の Auto / Fill、Grid、Wrap Box、Scroll Box |
| 07 | 変数と入力 | `AquaUI/Binding.aqs` | スライダー・トグル・入力欄の bind と、文字の `{変数}`・`{変数:2}`、visibleBind |
| 08 | スタイルと部品 | `AquaUI/Styles.aqs`・`AquaUI/StatusCard.aqs` | ボタンのスタイル（Primary・Danger・Ghost）と、UI を部品として入れる UIUserWidget |
| 09 | 変形と頂点カラー | `AquaUI/Transform.aqs` | Null ペインごと回す・時計の針（transformOrigin）・回したボタン、四隅の色、絵の一部だけを当たりにする Bounding ペイン、スクリプトが読むユーザーデータ |
| 10 | パーツの上書き | `AquaUI/Overrides.aqs` | 同じ StatusCard を 3 つ置き、名前・職業・ゲージ・アイコンの色をそれぞれ上書き。スクリプトの SetOverride / ClearOverrides |
| 11 | 区間・グループ・コマ送り | `AquaUI/AnimExtras.aqs` | アニメーション Show の区間 In / Idle / Out を名前で再生、グループ Stars を @Stars のトラックでまとめて動かす・SetGroupVisible で隠す、組み込みの画像でパラパラ漫画 |

背景では 3D の立方体と球が回っている（キャプチャーのサンプルで、撮った画面に写る）。

## 開き方

- **ゲームとして動かす**: このフォルダで `build.bat release run`（または RutoLauncher の「開く」でこのフォルダを選び、エディターの ▶）。
- **UI を編集する**: Aqua で「プロジェクトを開く」からこのフォルダを選ぶ（`..\..\Aqua.bat --project Samples\AquaSamples` でもよい）。編集するとすぐ `Assets/UI/*.aqua` に書き出され、実行中のゲームにも反映される。

RutoEngine は `MyProgram/RutoEngine`（Aqua フォルダの隣）を使う（環境変数 `RUTO_ENGINE_DIR` があればそちら）。キャプチャー・文字アニメーション・UI シェーダーには、Aqua の `ApplyEnginePatches.bat` でパッチを当てた RutoEngine が要る。

## しくみ

UI の切り替えなどは `Source/AquaSampleHub.cpp`（シーンの「UI」オブジェクトに UIDocument と一緒に付いている）がしている。ボタンの「クリック時のメッセージ」（clickMessage）を `OnUIEvent` で受け取る。

| メッセージ | すること |
|---|---|
| `open:UI/名前.aqua` | UIDocument の asset を変えて、その UI に切り替える（Intro・Loop という名前のアニメーションがあれば再生） |
| `back` | メニューに戻る |
| `anim:名前` | アニメーションを再生 |
| `restart-text` | 文字アニメーション（reveal）を最初から |
| `dialog-next` | 会話を送る |
| `pause` / `resume` | ポーズ画面 |
| `snapshot` | UICapture「Snapshot」に今の画面を撮る |
| `score-add` / `score-reset` | 変数 score を変える |
| `bounding-hit` | Bounding ペイン・回したボタンが押された（変数 hits を増やす） |
| `override-rename` / `override-reset` | パーツの上書きをスクリプトで変える（SetOverride）/ 消す（ClearOverrides） |
| `group-hide` / `group-show` | グループ Stars を隠す / 出す（SetGroupVisible） |

UI を切り替えたとき、ウィジェットは次に描くときに作り直される。切り替えた直後に `SetText` などをしても古い UI に入ってしまうので、作り直されてから（ルートのウィジェットが変わってから）初期化している。

## フォルダ

| フォルダ | 内容 |
|---|---|
| `AquaUI/` | Aqua の編集用ファイル（.aqs） |
| `Assets/UI/` | 書き出した UI（.aqua。ゲームが読む） |
| `Assets/Shaders/UI/` | UI のシェーダー（.uishader）。`Caustics.uishader` はこのサンプルで書いたもの |
| `Assets/Scenes/Main.rscene` | カメラ・ライト・回る立体・UI のオブジェクト |
| `Source/` | スクリプト（`AquaSampleHub.cpp`・`Rotator.cpp`）と Aqua が書き出すヘッダー（`AquaGenerated/`） |

## 作り直す

UI とシェーダーは `../generate_aqua_samples.py` で作っている（ウィジェットのツリーを組み立て、`Aqua import` / `Aqua export` で .aqs・.aqua にする）。

```bat
python Samples\generate_aqua_samples.py          :: .aqs を作り直す（Aqua で編集した分は消える）
python Samples\generate_aqua_samples.py --keep   :: 既にある .aqs はそのまま
```
