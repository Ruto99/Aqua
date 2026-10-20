#include <Ruto/Ruto.h>

#include <string>
#include <string_view>
#include <vector>

using namespace Ruto;

// Aqua のサンプル集をまとめるスクリプト（UI のオブジェクトに UIDocument と一緒に付ける）
//
//   ボタンの「クリック時のメッセージ」（clickMessage）で動く:
//     open:UI/名前.aqua  … その UI に切り替える（Intro / Loop という名前のアニメーションがあれば再生）
//     back              … メニューに戻る（Esc キーでも）
//     anim:名前          … アニメーションを再生
//     restart-text      … 画面の文字アニメーション（reveal）を最初から
//     dialog-next       … 会話を送る（出している途中なら全部出す）
//     pause / resume    … ポーズ画面（キャプチャーのサンプル）
//     snapshot          … UICapture「Snapshot」に今の画面を撮る
//     score-add / score-reset … 変数 score を変える
//     bounding-hit      … Bounding ペインが押された（変数 hits を増やす）
//     override-rename / override-reset … パーツの上書きをスクリプトで変える / 消す
class AquaSampleHub : public Component {
    RUTO_COMPONENT(AquaSampleHub)

    std::string menu = "UI/Menu.aqua"; // 最初に開く UI

    void Reflect(PropertyVisitor& v) override { v.Asset("menu", menu, AssetType::Widget); }

    void OnStart() override { Open(menu); }

    void OnUIEvent(const UIEvent& e) override {
        if (e.type != UIEventType::Click) return;
        const std::string_view msg = e.message;
        if (msg.starts_with("open:")) {
            Open(std::string(msg.substr(5)));
        } else if (msg == "back") {
            Open(menu);
        } else if (msg.starts_with("anim:")) {
            Doc().PlayAnimation(msg.substr(5));
        } else if (msg == "restart-text") {
            if (Widget* root = Doc().GetRoot()) {
                root->ForEach([](Widget& w) {
                    if (auto* t = dynamic_cast<UIText*>(&w); t && t->reveal != TextReveal::None) t->RestartReveal();
                });
            }
        } else if (msg == "dialog-next") {
            NextLine();
        } else if (msg == "pause") {
            SetPaused(true);
        } else if (msg == "resume") {
            SetPaused(false);
        } else if (msg == "snapshot") {
            if (auto* cap = UI()->Find<UICapture>("Snapshot")) cap->Capture();
        } else if (msg == "score-add") {
            m_Score += 100;
            UI()->SetVariable("score", static_cast<double>(m_Score));
        } else if (msg == "score-reset") {
            m_Score = 0;
            UI()->SetVariable("score", 0.0);
        } else if (msg == "bounding-hit") {
            UI()->SetVariable("hits", static_cast<double>(++m_Hits));
        } else if (msg == "override-rename") {
            // 置いたパーツの中の値を、置き場所ごとに変える
            static const char* const kNames[] = {"アクア・改", "ルト・改", "コーラル・改"};
            const char* const members[] = {"MemberAqua", "MemberRuto", "MemberCoral"};
            for (int i = 0; i < 3; ++i) {
                if (auto* part = UI()->Find<UIUserWidget>(members[i])) {
                    part->SetOverride("StatusName", "text", kNames[i]);
                    part->SetOverride("StatusHp", "percent", 1.0);
                }
            }
        } else if (msg == "override-reset") {
            for (const char* m : {"MemberAqua", "MemberRuto", "MemberCoral"}) {
                if (auto* part = UI()->Find<UIUserWidget>(m)) part->ClearOverrides();
            }
        }
    }

    void OnUpdate(float /*dt*/) override {
        // UI を切り替えると、ウィジェットは次に描くときに作り直される。作り直されてから初期化する
        if (m_NeedSetup && Doc().GetRoot() && Doc().GetRoot() != m_OldRoot) {
            m_NeedSetup = false;
            Setup();
        }
        if (Input::IsKeyPressed(Key::Escape) && m_Current != menu) Open(menu);
        // 会話: Space でも送る。出している間は ▼ を隠す（visibleBind = "!talking"）
        if (m_Current == "UI/Dialog.aqua") {
            if (Input::IsKeyPressed(Key::Space)) NextLine();
            UI()->SetVariable("talking", UI()->IsTextRevealing("DialogText"));
        }
        // シェーダー: スライダーの変数 dissolve → ディゾルブの progress
        if (m_Current == "UI/Shaders.aqua") UI()->SetShaderParam("DissolveControlled", "progress", Doc().GetFloat("dissolve"));
    }

private:
    struct Line {
        const char* speaker;
        const char* text;
    };
    // 台詞（<wave> などのタグ・<pause=秒> も使える）
    inline static const std::vector<Line> kLines = {
        {"アクア", "こんにちは！ これは Aqua で作った<wave>会話ウィンドウ</wave>のサンプルです。"},
        {"アクア", "文字は UIText の reveal（Typewriter）で 1 文字ずつ出しています。<pause=0.5>途中で止めたり<speed=0.3>ゆっくり</speed>出したりもできます。"},
        {"アクア", "出している途中で「次へ」を押すと、<shake>すぐに全部</shake>出ます。"},
        {"ルト", "<color=#ffd15a>色</color>や<rainbow>虹色</rainbow>も、タグを書くだけ。台詞はスクリプトの SetText で入れ替えています。"},
        {"アクア", "最後まで読んでくれてありがとう！ 最初の台詞に戻ります。"},
    };

    UIDocument* UI() { return GetEntity().GetComponent<UIDocument>(); }
    WidgetDocument& Doc() { return UI()->GetDocument(); }

    void Open(const std::string& asset) {
        SetPaused(false);
        m_OldRoot = Doc().GetRoot();
        UI()->asset = asset;
        m_Current = asset;
        Doc(); // ここで UI が読み込まれる（ウィジェットは次に描くときに作られる）
        m_NeedSetup = true;
    }

    // 開いた UI のウィジェットができてから呼ばれる
    void Setup() {
        WidgetDocument& doc = Doc();
        if (const auto& a = doc.GetAsset()) {
            if (a->FindAnimation("Intro")) doc.PlayAnimation("Intro");
            if (a->FindAnimation("Loop")) doc.PlayAnimation("Loop", true);
        }
        // 変数の最初の値（変数は UI を切り替えても残る）
        auto setDefault = [&](const char* name, auto value) {
            if (!doc.GetVariables().contains(name)) doc.SetVariable(name, value);
        };
        setDefault("score", 0.0);
        setDefault("volume", 70.0);
        setDefault("hp", 0.65);
        setDefault("showSecret", false);
        setDefault("playerName", std::string("Aqua"));
        setDefault("dissolve", 0.0);
        if (m_Current == "UI/Dialog.aqua") ShowLine(0);
        setDefault("hits", 0.0);
        // ユーザーデータ: ウィジェットに書いた「キー=値」をスクリプトで読む
        if (Widget* item = UI()->Find("ShopItem")) {
            doc.SetVariable("itemName", item->GetUserData("item", "?"));
            doc.SetVariable("itemPrice", static_cast<double>(item->GetUserDataFloat("price")));
        }
        if (auto* cap = UI()->Find<UICapture>("Snapshot")) cap->Capture();
    }

    // 出している途中なら全部出す。出し終わっていれば次の台詞
    void NextLine() {
        if (UI()->IsTextRevealing("DialogText")) {
            UI()->SkipText("DialogText");
            return;
        }
        ShowLine((m_Line + 1) % static_cast<int>(kLines.size()));
    }

    void ShowLine(int index) {
        m_Line = index;
        UI()->SetVariable("speaker", kLines[m_Line].speaker);
        UI()->SetText("DialogText", NoBreakSpaces(kLines[m_Line].text));
        UI()->RestartText("DialogText");
    }

    // 折り返しは半角スペースがあるとそこでしか折らないので、日本語の文では改行しないスペース（U+00A0）にする
    static std::string NoBreakSpaces(std::string_view s) {
        std::string out;
        for (const char c : s) {
            if (c == ' ') out += "\xC2\xA0";
            else out += c;
        }
        return out;
    }

    void SetPaused(bool paused) {
        Time::SetTimeScale(paused ? 0.0f : 1.0f);
        if (UI()->Find("PauseLayer")) UI()->SetVisible("PauseLayer", paused);
    }

    std::string m_Current;
    Widget* m_OldRoot = nullptr; // 切り替える前の UI（作り直されたかを見る。触らない）
    bool m_NeedSetup = false;
    int m_Line = -1;
    int m_Score = 0;
    int m_Hits = 0;
};
RUTO_REGISTER_COMPONENT(AquaSampleHub);
