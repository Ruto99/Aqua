#include "AquaCommon.h"

#include <Ruto/EditorUI/Widgets.h>

#include <IconsFontAwesome6.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>

namespace AquaTool {

using namespace Ruto::EditorUI;

const char* WidgetIcon(std::string_view type) {
    if (type == "UICanvasPanel") return ICON_FA_VECTOR_SQUARE;
    if (type == "UINullPane") return ICON_FA_CROSSHAIRS;
    if (type == "UIBoundingPane") return ICON_FA_BULLSEYE;
    if (type == "UIVerticalBox") return ICON_FA_BARS;
    if (type == "UIHorizontalBox") return ICON_FA_GRIP_LINES_VERTICAL;
    if (type == "UIOverlay") return ICON_FA_LAYER_GROUP;
    if (type == "UIGridPanel") return ICON_FA_TABLE_CELLS;
    if (type == "UIWrapBox") return ICON_FA_TABLE_CELLS_LARGE;
    if (type == "UIScrollBox") return ICON_FA_SCROLL;
    if (type == "UISizeBox") return ICON_FA_EXPAND;
    if (type == "UIBorder") return ICON_FA_SQUARE;
    if (type == "UIText") return ICON_FA_FONT;
    if (type == "UIImage") return ICON_FA_IMAGE;
    if (type == "UICapture") return ICON_FA_CAMERA;
    if (type == "UIButton") return ICON_FA_HAND_POINTER;
    if (type == "UIProgressBar") return ICON_FA_BATTERY_HALF;
    if (type == "UISpacer") return ICON_FA_ARROWS_LEFT_RIGHT;
    if (type == "UISlider") return ICON_FA_SLIDERS;
    if (type == "UIToggle") return ICON_FA_TOGGLE_ON;
    if (type == "UITextInput") return ICON_FA_KEYBOARD;
    if (type == "UIUserWidget") return ICON_FA_PUZZLE_PIECE;
    return ICON_FA_SQUARE;
}

ImVec4 WidgetIconColor(std::string_view type) {
    const WidgetTypeInfo* info = WidgetRegistry::Find(type);
    if (!info) return Theme::kTextDim;
    if (type == "UINullPane") return Theme::kTextDim; // 何も描かない
    if (type == "UIBoundingPane") return Theme::kCoral;  // 当たりだけ
    if (info->category == "パネル") return {0.55f, 0.75f, 1.0f, 1.0f};
    if (info->category == "入力") return {0.55f, 0.9f, 0.6f, 1.0f};
    if (info->category == "部品") return {0.95f, 0.65f, 0.95f, 1.0f};
    return {1.0f, 0.82f, 0.45f, 1.0f};
}

const char* TypeLabel(std::string_view type) {
    const WidgetTypeInfo* info = WidgetRegistry::Find(type);
    return info ? info->label.c_str() : "ウィジェット";
}

namespace {
// "fontSize" → "Font Size"
std::string PrettyName(std::string_view name) {
    std::string out;
    for (size_t i = 0; i < name.size(); ++i) {
        const char c = name[i];
        if (i == 0) {
            out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            continue;
        }
        if (std::isupper(static_cast<unsigned char>(c)) && !std::isupper(static_cast<unsigned char>(name[i - 1]))) out += ' ';
        out += c;
    }
    return out;
}
} // namespace

std::string PropertyLabelJa(std::string_view n) {
    static const std::pair<const char*, const char*> kNames[] = {
        {"visibility", "表示状態"}, {"enabled", "有効"}, {"opacity", "不透明度"}, {"translation", "描画のずれ"},
        {"renderScale", "描画の拡大"}, {"clipChildren", "子を切り取る"}, {"tooltip", "ツールチップ"}, {"visibleBind", "表示の条件（変数）"},
        {"text", "テキスト"}, {"font", "フォント"}, {"fontSize", "文字の大きさ"}, {"color", "色"}, {"align", "横の揃え"},
        {"verticalAlign", "縦の揃え"}, {"wrap", "折り返す"}, {"shadow", "影"}, {"shadowColor", "影の色"}, {"shadowOffset", "影のずれ"},
        {"shadowSize", "影の大きさ"}, {"texture", "画像"}, {"size", "大きさ"}, {"preserveAspect", "縦横比を保つ"},
        {"sliceBorder", "9 スライス（左上右下）"}, {"cornerRadius", "角丸"}, {"background", "背景色"}, {"borderColor", "枠線の色"},
        {"borderThickness", "枠線の太さ"}, {"contentPadding", "内側の余白"}, {"textColor", "文字の色"}, {"normalColor", "通常の色"},
        {"hoveredColor", "ホバーの色"}, {"pressedColor", "押したときの色"}, {"disabledColor", "無効の色"}, {"hoverScale", "ホバーで拡大"},
        {"clickMessage", "クリック時のメッセージ"}, {"clickSound", "クリックの音"}, {"percent", "割合"}, {"bind", "変数と連動"},
        {"direction", "方向"}, {"fillColor", "塗りの色"}, {"backgroundColor", "背景色"}, {"smoothing", "なめらかさ"},
        {"value", "値"}, {"minValue", "最小"}, {"maxValue", "最大"}, {"step", "刻み"}, {"trackColor", "溝の色"},
        {"handleColor", "つまみの色"}, {"isOn", "オン"}, {"toggleStyle", "見た目"}, {"onColor", "オンの色"}, {"offColor", "オフの色"},
        {"placeholder", "空のときの文字"}, {"spacing", "間隔"}, {"padding", "余白（左上右下）"}, {"columns", "列の数"},
        {"cellHeight", "マスの高さ（0 で自動）"}, {"showScrollbar", "スクロールバー"}, {"scrollbarColor", "スクロールバーの色"},
        {"widthOverride", "幅（0 で自動）"}, {"heightOverride", "高さ（0 で自動）"}, {"minSize", "最小の大きさ"}, {"maxSize", "最大の大きさ"},
        {"asset", "UI アセット"}, {"scrollOffset", "スクロール位置"},
        {"timing", "撮るとき"}, {"area", "撮る範囲"}, {"blur", "ぼかし"}, {"blockInput", "マウスを受け止める"},
        {"previewTexture", "デザイン中の見本（ゲームでは使わない）"},
        {"shader", "シェーダー"}, {"shaderChildren", "子にも掛ける"},
        {"rotation", "回転（度・時計回り）"}, {"transformOrigin", "拡大・回転の中心"},
        {"vertexColorsEnabled", "頂点カラーを使う"}, {"vertexColorTopLeft", "左上の色"}, {"vertexColorTopRight", "右上の色"},
        {"vertexColorBottomLeft", "左下の色"}, {"vertexColorBottomRight", "右下の色"}, {"userData", "ユーザーデータ"}, {"overrides", "パーツの上書き"},
        {"letterSpacing", "文字間（足す幅）"}, {"lineSpacing", "行間（足す高さ）"}, {"shrinkToFit", "枠に合わせて縮小"},
        {"gradient", "行ごとのグラデーション"}, {"gradientTop", "上の色"}, {"gradientBottom", "下の色"},
        {"blackWhiteEnabled", "白黒カラーを使う"}, {"blackColor", "黒の色"}, {"whiteColor", "白の色"},
        {"frames", "コマの画像（1 行に 1 つ）"}, {"frame", "今のコマ"}, {"framesPerSecond", "自動で送る速さ（コマ/秒）"},
        {"effect", "動き"}, {"effectStrength", "動きの大きさ"}, {"effectSpeed", "動きの速さ"},
        {"reveal", "出し方"}, {"revealSpeed", "出す速さ"}, {"revealDelay", "待ち時間"},
        {"revealFade", "1 文字の時間"}, {"revealAuto", "自動で出す"}, {"revealProgress", "出した割合"},
        {"revealOnShow", "毎回出し直す"}, {"revealSound", "文字の音"}, {"revealSoundEvery", "音の間隔"},
        {"richText", "タグを使う"},
    };
    for (const auto& [k, v] : kNames) {
        if (n == k) return v;
    }
    return PrettyName(n);
}

std::string EnumOptionLabelJa(std::string_view property, std::string_view option) {
    struct Entry {
        const char* property;
        const char* option;
        const char* label;
    };
    static const Entry kLabels[] = {
        // キャプチャー
        {"timing", "OnShow", "表示したときに撮る（止めた画面）"},
        {"timing", "EveryFrame", "毎フレーム撮る（動く画面）"},
        {"timing", "Script", "スクリプトの Capture() で撮る"},
        {"area", "Screen", "画面全体"},
        {"area", "Behind", "このウィジェットの後ろだけ"},
        // 共通の選択肢
        {"visibility", "Visible", "表示する"},
        {"visibility", "Collapsed", "隠す（場所も取らない）"},
        {"visibility", "Hidden", "隠す（場所は取る）"},
        {"visibility", "HitTestInvisible", "表示する（自分も子もクリックを通す）"},
        {"visibility", "SelfHitTestInvisible", "表示する（自分だけクリックを通す）"},
        {"align", "Left", "左"}, {"align", "Center", "中央"}, {"align", "Right", "右"},
        {"verticalAlign", "Top", "上"}, {"verticalAlign", "Center", "中央"}, {"verticalAlign", "Bottom", "下"},
        {"direction", "LeftToRight", "左から右"}, {"direction", "RightToLeft", "右から左"},
        {"direction", "BottomToTop", "下から上"}, {"direction", "TopToBottom", "上から下"},
        {"toggleStyle", "Switch", "スイッチ"}, {"toggleStyle", "Checkbox", "チェックボックス"},
        // テキストの動き
        {"effect", "None", "なし"},
        {"effect", "Wave", "波"},
        {"effect", "Shake", "震え"},
        {"effect", "Rainbow", "虹色"},
        {"effect", "Pulse", "脈打つ"},
        {"effect", "Swing", "揺れる"},
        {"effect", "Glitch", "グリッチ"},
        // テキストの出し方
        {"reveal", "None", "すぐ全部出す"},
        {"reveal", "Typewriter", "1 文字ずつ（タイプライター）"},
        {"reveal", "Fade", "1 文字ずつふわっと"},
        {"reveal", "Pop", "1 文字ずつポップ"},
        {"reveal", "Rise", "下から浮かぶ"},
        {"reveal", "Drop", "上から落ちて弾む"},
        {"reveal", "Spin", "回りながら出る"},
    };
    for (const Entry& e : kLabels) {
        if (property == e.property && option == e.option) return e.label;
    }
    return std::string(option);
}

int ShaderParamSlot(std::string_view property) {
    constexpr std::string_view prefix = "shaderParam";
    if (!property.starts_with(prefix) || property.size() != prefix.size() + 1) return -1;
    const char c = property.back();
    return c >= '0' && c < '0' + Widget::kShaderParamCount ? c - '0' : -1;
}

std::string AnimPropertyLabel(Widget& w, const std::string& property, const std::string& channel) {
    const int slot = ShaderParamSlot(property);
    if (slot < 0) return channel.empty() ? PropertyLabelJa(property) : PropertyLabelJa(property) + "." + channel;
    auto sh = w.shader.empty() ? nullptr : UIShader::Get(w.shader);
    if (!sh) return {};
    for (const UIShaderParam& p : sh->GetParams()) {
        if (p.slot != slot) continue;
        // 使う成分だけ（float・トグルは x、vec2 は x / y、色は r g b a）
        static const char* const kColor[] = {"赤", "緑", "青", "不透明度"};
        const int ch = channel.empty() ? 0 : channel == "x" ? 0 : channel == "y" ? 1 : channel == "z" ? 2 : 3;
        const bool single = p.type == UIShaderParam::Type::Float || p.type == UIShaderParam::Type::Toggle;
        if (single && ch > 0) return {};
        if (p.type == UIShaderParam::Type::Vec2 && ch > 1) return {};
        const std::string base = "シェーダー: " + p.label;
        if (single) return base;
        if (p.type == UIShaderParam::Type::Color) return base + "（" + kColor[ch] + "）";
        return base + "." + channel;
    }
    return {};
}

Widget* Resolve(Widget* root, const WidgetPath& path) {
    Widget* w = root;
    for (int i : path) {
        if (!w || i < 0 || i >= static_cast<int>(w->children.size())) return nullptr;
        w = w->children[static_cast<size_t>(i)].get();
    }
    return w;
}

WidgetPath PathOf(const Widget* w) {
    WidgetPath p;
    for (; w && w->GetParent(); w = w->GetParent()) p.insert(p.begin(), w->GetParent()->IndexOf(w));
    return p;
}

WidgetPath DesignPathOf(Widget* w) {
    Widget* target = w;
    for (Widget* p = w; p; p = p->GetParent()) {
        if (p->generated && p->GetParent()) target = p->GetParent();
    }
    return PathOf(target);
}

std::string PathKey(const WidgetPath& p) {
    std::string s;
    for (int i : p) s += std::to_string(i) + "/";
    return s;
}

WidgetPath PathFromKey(const std::string& s) {
    WidgetPath p;
    size_t start = 0;
    while (start < s.size()) {
        const size_t slash = s.find('/', start);
        if (slash == std::string::npos) break;
        p.push_back(std::atoi(s.c_str() + start));
        start = slash + 1;
    }
    return p;
}

bool IsCanvasChild(const Widget& w) { return w.GetParent() && dynamic_cast<const UICanvasPanel*>(w.GetParent()); }

void SetSlotRect(WidgetSlot& s, const UI::Rect& parent, const UI::Rect& rect) {
    const float ax0 = parent.x + s.anchorMin.x * parent.w, ay0 = parent.y + s.anchorMin.y * parent.h;
    const float ax1 = parent.x + s.anchorMax.x * parent.w, ay1 = parent.y + s.anchorMax.y * parent.h;
    s.size = {rect.w - (ax1 - ax0), rect.h - (ay1 - ay0)};
    const float refX = ax0 + (ax1 - ax0) * s.pivot.x, refY = ay0 + (ay1 - ay0) * s.pivot.y;
    s.position = {std::round(rect.x + rect.w * s.pivot.x - refX), std::round(rect.y + rect.h * s.pivot.y - refY)};
    s.size = {std::round(s.size.x), std::round(s.size.y)};
}

UI::Rect SlotRect(const WidgetSlot& s, const UI::Rect& p, const Vec2& desired) {
    const float ax0 = p.x + s.anchorMin.x * p.w, ay0 = p.y + s.anchorMin.y * p.h;
    const float ax1 = p.x + s.anchorMax.x * p.w, ay1 = p.y + s.anchorMax.y * p.h;
    Vec2 size{(ax1 - ax0) + s.size.x, (ay1 - ay0) + s.size.y};
    if (s.autoSize) size = desired;
    const float px = ax0 + (ax1 - ax0) * s.pivot.x + s.position.x, py = ay0 + (ay1 - ay0) * s.pivot.y + s.position.y;
    return {px - size.x * s.pivot.x, py - size.y * s.pivot.y, size.x, size.y};
}

Vec2 DefaultCanvasSize(std::string_view type) {
    if (type == "UIButton") return {320, 80};
    if (type == "UIText") return {360, 60};
    if (type == "UIImage") return {200, 200};
    if (type == "UIBorder") return {640, 420};
    if (type == "UIProgressBar") return {420, 28};
    if (type == "UISlider") return {420, 40};
    if (type == "UIToggle") return {260, 40};
    if (type == "UITextInput") return {480, 60};
    if (type == "UIVerticalBox") return {420, 360};
    if (type == "UIHorizontalBox") return {640, 100};
    if (type == "UIGridPanel" || type == "UIWrapBox") return {640, 420};
    if (type == "UIScrollBox") return {520, 420};
    if (type == "UICanvasPanel" || type == "UIOverlay") return {640, 420};
    if (type == "UINullPane") return {200, 200};
    if (type == "UIBoundingPane") return {240, 120};
    return {200, 200};
}

std::string UniqueWidgetName(Widget* root, const std::string& base) {
    if (!root || !root->Find(base)) return base;
    for (int n = 1;; ++n) {
        std::string candidate = std::format("{}{}", base, n);
        if (!root->Find(candidate)) return candidate;
    }
}

std::span<const ResolutionPreset> ResolutionPresets() {
    static const ResolutionPreset kResolutions[] = {
        {"基準解像度", 0, 0},
        {ICON_FA_DESKTOP "  1920 × 1080（フル HD）", 1920, 1080},
        {ICON_FA_DESKTOP "  1280 × 720", 1280, 720},
        {ICON_FA_DESKTOP "  2560 × 1440", 2560, 1440},
        {ICON_FA_DESKTOP "  3840 × 2160（4K）", 3840, 2160},
        {ICON_FA_DESKTOP "  2560 × 1080（ウルトラワイド）", 2560, 1080},
        {ICON_FA_DESKTOP "  1024 × 768（4:3）", 1024, 768},
        {ICON_FA_MOBILE_SCREEN "  1080 × 1920（縦）", 1080, 1920},
        {ICON_FA_MOBILE_SCREEN "  1170 × 2532（スマホ縦）", 1170, 2532},
        {ICON_FA_MOBILE_SCREEN "  2532 × 1170（スマホ横）", 2532, 1170},
    };
    return kResolutions;
}

float CanvasScale(const WidgetAsset& asset, const Vec2& res) {
    const float lw = std::log2(res.x / asset.referenceResolution.x), lh = std::log2(res.y / asset.referenceResolution.y);
    return std::exp2(lw + (lh - lw) * 0.5f);
}

int FramebufferSize(float screenSize) {
    return std::clamp(static_cast<int>(screenSize), 4, 4096);
}

std::string ToIdentifier(std::string_view name) {
    // ASCII の英数字と _、ひらがな・カタカナ・漢字はそのまま（C++20 の識別子に使える）。それ以外は _
    auto usable = [](uint32_t cp) {
        if (cp < 0x80) return std::isalnum(static_cast<int>(cp)) || cp == '_';
        return (cp >= 0x3041 && cp <= 0x3096) || (cp >= 0x30A1 && cp <= 0x30FA) || cp == 0x30FC || (cp >= 0x3400 && cp <= 0x4DBF) ||
               (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0xFF66 && cp <= 0xFF9F);
    };
    std::string out;
    for (size_t i = 0; i < name.size();) {
        const uint32_t cp = Utf8::Next(name, i);
        if (usable(cp)) Utf8::Append(out, cp);
        else out += '_';
    }
    if (out.empty() || std::isdigit(static_cast<unsigned char>(out[0]))) out.insert(out.begin(), '_');
    return out;
}

} // namespace AquaTool
