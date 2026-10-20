"""Aqua のサンプルプロジェクト（Samples/AquaSamples）の UI とシェーダーを作り直す。

    python Samples/generate_aqua_samples.py            # UI（AquaUI/*.aqs と Assets/UI/*.aqua）とシェーダーを作り直す
    python Samples/generate_aqua_samples.py --keep     # 既にある .aqs は上書きしない（Aqua で編集した分を残す）

UI はウィジェットのツリー（RutoUI の JSON）をここで組み立て、Aqua のコマンドライン
（Aqua import / Aqua export）で編集用ファイル（.aqs）と書き出し（.aqua）に変える。
bin/Aqua.exe（build.bat でビルド）が要る。
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
PROJECT = ROOT / "AquaSamples"
AQUA_EXE = ROOT.parent / "bin" / "Aqua.exe"
SHADER_DIR = "Shaders/UI"  # Assets からの相対パス

# ---------------------------------------------------------------------------
# 色
# ---------------------------------------------------------------------------

BG = [0.031, 0.063, 0.082, 0.92]
PANEL = [0.055, 0.110, 0.137, 0.95]
PANEL_LIGHT = [0.086, 0.165, 0.200, 1.0]
AQUA = [0.196, 0.851, 0.800, 1.0]
AQUA_DARK = [0.090, 0.420, 0.420, 1.0]
CORAL = [1.0, 0.494, 0.416, 1.0]
INK = [0.016, 0.098, 0.110, 1.0]
TEXT = [0.867, 0.937, 0.945, 1.0]
DIM = [0.55, 0.68, 0.71, 1.0]
GOLD = [1.0, 0.82, 0.35, 1.0]


def rgba(c, a):
    return [c[0], c[1], c[2], a]


# ---------------------------------------------------------------------------
# UI シェーダー（.uishader）
# ---------------------------------------------------------------------------

SHADERS = {
    "Grayscale": """// グレースケール: 色を抜いて白黒にする（使えないボタンなど）
// @param strength float 0 1 1 強さ
vec4 effect(vec4 color) {
    return vec4(mix(color.rgb, vec3(Luma(color.rgb)), strength), color.a);
}
""",
    "Sepia": """// セピア: 古い写真のような茶色にする（回想シーンなど）
// @param strength float 0 1 1 強さ
vec4 effect(vec4 color) {
    float g = Luma(color.rgb);
    vec3 sepia = vec3(g * 1.07, g * 0.74, g * 0.43);
    return vec4(mix(color.rgb, sepia, strength), color.a);
}
""",
    "Flash": """// 白く光らせる: アニメーションで amount を動かすと、ヒット・選択の演出に
// @param amount float 0 1 0.5 光らせる量
// @param flash color 1 1 1 1 色
vec4 effect(vec4 color) {
    return vec4(mix(color.rgb, flash.rgb, amount * flash.a), color.a);
}
""",
    "Shine": """// 光が流れる: 斜めの光が一定の間隔で流れる（レアなボタン・タイトルロゴ）
// @param shine color 1 1 1 0.8 光の色
// @param width float 0.01 0.5 0.12 光の幅
// @param speed float 0.05 3 0.8 速さ
// @param interval float 0 5 1.5 間隔（秒）
vec4 effect(vec4 color) {
    vec2 uv = LocalUV();
    float period = 1.0 / speed + interval;
    float t = mod(uTime, period) * speed;
    float pos = t * 1.7 - 0.35;
    float k = 1.0 - smoothstep(0.0, width, abs(uv.x + uv.y * 0.35 - pos));
    return vec4(mix(color.rgb, shine.rgb, k * shine.a), color.a);
}
""",
    "Dissolve": """// ディゾルブ: まだらに消えていく（progress をアニメーション・スクリプトで動かす）
// @param progress float 0 1 0.4 消えた割合
// @param edge color 1 0.55 0.1 1 縁の色
// @param width float 0 0.3 0.08 縁の太さ
// @param scale float 1 60 14 模様の細かさ
vec4 effect(vec4 color) {
    vec2 uv = LocalUV();
    float n = Noise(uv * scale) * 0.7 + Noise(uv * scale * 2.7) * 0.3;
    float cut = progress * (1.0 + width) - width;
    if (n < cut) return vec4(0.0);
    float e = (1.0 - smoothstep(cut, cut + width, n)) * step(0.001, progress);
    return vec4(mix(color.rgb, edge.rgb, e), color.a);
}
""",
    "Gradient": """// グラデーション: 文字に掛けると金色の見出しなどに
// @param top color 1 0.9 0.4 1 始まりの色
// @param bottom color 1 0.45 0.15 1 終わりの色
// @param angle float 0 1 0 向き（0: 縦 / 1: 横）
vec4 effect(vec4 color) {
    vec2 uv = LocalUV();
    vec4 g = mix(top, bottom, mix(uv.y, uv.x, angle));
    return vec4(color.rgb * g.rgb, color.a * g.a);
}
""",
    "Rainbow": """// 虹色: 虹色が流れる
// @param strength float 0 1 1 強さ
// @param speed float 0 3 0.5 速さ
// @param scale float 0.1 4 1 縞の数
vec4 effect(vec4 color) {
    vec2 uv = LocalUV();
    vec3 rainbow = clamp(HueShift(vec3(1.0, 0.15, 0.15), (uv.x + uv.y * 0.3) * scale - uTime * speed), 0.0, 1.0);
    return vec4(mix(color.rgb, rainbow * (0.45 + 0.55 * Luma(color.rgb)), strength), color.a);
}
""",
    "HueShift": """// 色相を回す: 同じ見た目で色違いを作る
// @param shift float 0 1 0.3 色相のずれ
// @param speed float 0 2 0 回る速さ
vec4 effect(vec4 color) {
    return vec4(clamp(HueShift(color.rgb, shift + uTime * speed), 0.0, 1.0), color.a);
}
""",
    "Blink": """// 点滅: ゆっくり点滅する（「ボタンを押してください」など）
// @param speed float 0 10 1.5 速さ（1 秒に何回）
// @param minAlpha float 0 1 0.2 一番薄いとき
vec4 effect(vec4 color) {
    float k = 0.5 + 0.5 * cos(uTime * speed * 6.28318);
    return vec4(color.rgb, color.a * mix(minAlpha, 1.0, k));
}
""",
    "Scanlines": """// 走査線: ブラウン管のような横線とちらつき（レトロな画面）
// @param strength float 0 1 0.35 線の濃さ
// @param density float 20 800 240 線の数
// @param flicker float 0 0.3 0.04 ちらつき
vec4 effect(vec4 color) {
    float line = 0.5 + 0.5 * sin(LocalUV().y * density * 3.14159);
    float f = 1.0 - flicker * (0.5 + 0.5 * sin(uTime * 60.0));
    return vec4(color.rgb * (1.0 - strength * line) * f, color.a);
}
""",
    "Hologram": """// ホログラム: 青く透けた立体映像の風（SF の画面）
// @param tint color 0.3 0.9 1 1 色
// @param lines float 20 400 120 線の数
// @param speed float 0 5 1.2 流れる速さ
// @param glitch float 0 1 0.3 乱れ
vec4 effect(vec4 color) {
    vec2 uv = LocalUV();
    float scan = 0.65 + 0.35 * sin(uv.y * lines - uTime * speed * 10.0);
    float noise = Hash(vec2(floor(uv.y * 60.0), floor(uTime * 12.0)));
    float a = color.a * scan * (1.0 - glitch * 0.6 * step(0.9, noise));
    return vec4(tint.rgb * (0.4 + Luma(color.rgb)), a * tint.a);
}
""",
    "Vignette": """// ふちを暗く: まわりを暗く（色を付けて光らせることもできる）
// @param strength float 0 1 0.7 濃さ
// @param size float 0 1 0.35 明るい所の大きさ
// @param tint color 0 0 0 1 ふちの色
vec4 effect(vec4 color) {
    float v = smoothstep(size * 0.7, size * 0.7 + 0.5, length(LocalUV() - 0.5));
    return vec4(mix(color.rgb, tint.rgb, v * strength), color.a);
}
""",
    "Caustics": """// 水面の光（このサンプルのオリジナル）: 水の色を重ね、ゆらめく光の網目を流す
// 自分でシェーダーを書くときの見本。@param の行がそのまま Aqua の詳細パネルに並ぶ
// @param tint color 0.2 0.85 0.8 1 水の色
// @param strength float 0 1 0.6 強さ
// @param speed float 0 3 1 速さ
// @param scale float 2 40 14 網目の細かさ
vec4 effect(vec4 color) {
    vec2 uv = LocalUV();
    float t = uTime * speed;
    float a = sin(uv.x * scale + t * 2.0 + sin(uv.y * scale * 0.7 + t));
    float b = sin(uv.y * scale * 1.2 - t * 1.6 + sin(uv.x * scale * 0.5 - t));
    float caustic = pow(1.0 - abs(a * b), 6.0);
    vec3 water = mix(color.rgb, tint.rgb * (0.55 + Luma(color.rgb)), strength);
    return vec4(water + caustic * 0.45 * strength, color.a);
}
""",
}


def shader_defaults(name):
    """@param の既定値を宣言の順に（ウィジェットの shaderParam0〜 に入れる）。"""
    params = []
    for line in SHADERS[name].splitlines():
        m = re.match(r"\s*//\s*@param\s+(\w+)\s+(\w+)\s+(.*)", line)
        if not m:
            continue
        pname, ptype, rest = m.group(1), m.group(2), m.group(3).split()
        nums = []
        for tok in rest:
            try:
                nums.append(float(tok))
            except ValueError:
                break
        if ptype == "float":
            value = [nums[2], 0, 0, 0]
        elif ptype == "vec2":
            value = [nums[2], nums[3], 0, 0]
        elif ptype == "color":
            value = nums[0:4]
        else:  # toggle
            value = [nums[0], 0, 0, 0]
        params.append((pname, value))
    return params


def shader_common(name, children=True, **overrides):
    """ウィジェットの common に入れるシェーダーの設定（overrides は @param の名前 → 値）。"""
    common = {"shader": f"{SHADER_DIR}/{name}.uishader", "shaderChildren": children}
    params = shader_defaults(name)
    for pname in overrides:
        if pname not in [p for p, _ in params]:
            raise ValueError(f"{name} に @param {pname} はありません")
    for i, (pname, value) in enumerate(params):
        v = overrides.get(pname, value)
        if isinstance(v, (int, float)):
            v = [v, 0, 0, 0]
        common[f"shaderParam{i}"] = list(v)
    return common


def shader_slot(name, pname):
    """@param の名前 → アニメーションのトラック名（shaderParamN.x）。"""
    for i, (p, _) in enumerate(shader_defaults(name)):
        if p == pname:
            return f"shaderParam{i}.x"
    raise ValueError(pname)


# ---------------------------------------------------------------------------
# ウィジェットを組み立てる道具
# ---------------------------------------------------------------------------


def widget(type_, name, props=None, slot=None, common=None, children=None, style=None):
    w = {"type": type_, "name": name}
    if style:
        w["style"] = style
    w["common"] = common or {}
    w["properties"] = props or {}
    w["slot"] = slot or {}
    w["children"] = children or []
    return w


def at(x, y, w, h, anchor=(0, 0), pivot=None, auto=False, z=0):
    """Canvas Panel の子: anchor（0〜1）を基準に、pivot の位置を (x, y) に置く。"""
    pivot = anchor if pivot is None else pivot
    s = {"anchorMin": list(anchor), "anchorMax": list(anchor), "pivot": list(pivot), "position": [x, y], "size": [w, h]}
    if auto:
        s["autoSize"] = True
    if z:
        s["zOrder"] = z
    return s


def stretch(left=0, top=0, right=0, bottom=0, z=0):
    """Canvas Panel の子: 親いっぱいに広げる（余白つき）。"""
    s = {"anchorMin": [0, 0], "anchorMax": [1, 1], "pivot": [0, 0], "position": [left, top], "size": [-(left + right), -(top + bottom)]}
    if z:
        s["zOrder"] = z
    return s


def box(h="Fill", v="Fill", rule="Auto", weight=1.0, pad=(0, 0, 0, 0)):
    """Box・Overlay・Border の子の置き方。"""
    return {"padding": list(pad), "hAlign": h, "vAlign": v, "sizeRule": rule, "fillWeight": weight}


NBSP = " "


def text(name, s, size=28, color=TEXT, align="Left", valign="Center", wrap=False, slot=None, common=None, **extra):
    if wrap:
        # 日本語の文は、どこでも折り返せるように半角スペースを改行しないスペースにする
        # （エンジンの折り返しは、行の中に半角スペースがあるとそこでしか折らない）
        s = s.replace(" ", NBSP)
    props = {"text": s, "fontSize": size, "color": list(color), "align": align, "verticalAlign": valign, "wrap": wrap}
    props.update(extra)
    return widget("UIText", name, props, slot, common)


def image(name, color, size=(100, 100), texture="", radius=0.0, slot=None, common=None):
    return widget("UIImage", name, {"texture": texture, "color": list(color), "size": list(size), "cornerRadius": radius}, slot, common)


def border(name, child, bg=PANEL, radius=16, pad=(24, 20, 24, 20), slot=None, common=None, shadow=True, border_color=(1, 1, 1, 0.06), style=None):
    props = {
        "background": list(bg),
        "cornerRadius": radius,
        "contentPadding": list(pad),
        "shadow": shadow,
        "borderColor": list(border_color),
        "borderThickness": 1,
    }
    return widget("UIBorder", name, props, slot, common, [child] if child else [], style)


def vbox(name, children, spacing=12, pad=(0, 0, 0, 0), slot=None, common=None):
    return widget("UIVerticalBox", name, {"spacing": spacing, "padding": list(pad)}, slot, common, children)


def hbox(name, children, spacing=12, pad=(0, 0, 0, 0), slot=None, common=None):
    return widget("UIHorizontalBox", name, {"spacing": spacing, "padding": list(pad)}, slot, common, children)


def button(name, label, message="", size=26, color=AQUA_DARK, text_color=TEXT, slot=None, style=None, common=None, child=None, **extra):
    hover = [min(1.0, c * 1.25 + 0.04) for c in color[:3]] + [1.0]
    press = [c * 0.8 for c in color[:3]] + [1.0]
    props = {
        "text": label,
        "fontSize": size,
        "textColor": list(text_color),
        "clickMessage": message,
        "normalColor": list(color),
        "hoveredColor": hover,
        "pressedColor": press,
        "cornerRadius": 12,
    }
    props.update(extra)
    return widget("UIButton", name, props, slot, common, [child] if child else [], style)


def key(t, v, interp=0):
    """アニメーションのキー（interp: 0 なめらか / 1 直線 / 2 一定）。"""
    return [t, v, interp]


def track(widget_name, prop, keys):
    return {"widget": widget_name, "property": prop, "keys": keys}


def anim(name, tracks, sections=None):
    a = {"name": name, "tracks": tracks}
    if sections:
        a["sections"] = [{"name": n, "start": st, "end": en, "loop": lp} for n, st, en, lp in sections]
    return a


def apply_styles(w, styles):
    """スタイルの値をウィジェットに入れる（Aqua でスタイルを選んだときと同じ。ゲームはウィジェットの値で描く）。"""
    if w.get("style"):
        st = next((x for x in styles if x["name"] == w["style"]), None)
        if st is None or st["type"] != w["type"]:
            raise ValueError(f"スタイル {w['style']} が {w['type']} にありません")
        w["properties"] = {**w["properties"], **st["properties"]}
    for c in w["children"]:
        apply_styles(c, styles)


def document(root_children, animations=None, styles=None, kind="body", resolution=(1920, 1080), groups=None):
    """kind: "body"（画面のいちばん親）か "part"（部品として入れるパーツ）。"""
    root = widget("UICanvasPanel", "Root", children=root_children)
    apply_styles(root, styles or [])
    return {
        "format": "RutoUI",
        "version": 1,
        "kind": kind,
        "referenceResolution": list(resolution),
        "root": root,
        "animations": animations or [],
        "styles": styles or [],
        "groups": [{"name": n, "members": m} for n, m in (groups or [])],
    }


def page(title, subtitle, content, background=BG):
    """サンプルの 1 画面: 背景・戻るボタン・見出し + 中身。"""
    header = hbox(
        "Header",
        [
            button("BackButton", "← メニュー", "back", size=24, color=PANEL_LIGHT, slot=box(v="Center"), contentPadding=[20, 10, 20, 10], hoverScale=1.06),
            vbox(
                "Titles",
                [
                    text("Title", title, 52, TEXT, common=shader_common("Gradient", top=[1, 1, 1, 1], bottom=[0.55, 0.95, 0.92, 1])),
                    text("Subtitle", subtitle, 24, DIM),
                ],
                spacing=2,
                slot=box(v="Center"),
            ),
        ],
        spacing=28,
        slot=at(60, 36, 0, 0, auto=True, z=10),
    )
    children = []
    if background:
        children.append(image("Background", background, slot=stretch()))
    children.append(header)
    children.extend(content)
    return children


def card(name, title, body_children, slot=None, width=None, style=None, pad=(24, 18, 24, 22)):
    """見出し付きのパネル。"""
    inner = vbox(
        name + "Body",
        [text(name + "Title", title, 26, AQUA)] + body_children,
        spacing=12,
    )
    if width:
        inner = widget("UISizeBox", name + "Size", {"widthOverride": width}, children=[inner])
    return border(name, inner, PANEL, 18, pad, slot=slot, style=style)


def note(name, s, size=21, slot=None, **extra):
    return text(name, s, size, DIM, wrap=True, slot=slot, **extra)


# ---------------------------------------------------------------------------
# メニュー
# ---------------------------------------------------------------------------

SAMPLES = [
    ("TextAnimation", "文字アニメーション", "波打つ・震える・虹色の文字と、1 文字ずつ出す演出"),
    ("Dialog", "会話ウィンドウ", "タイプライター表示＋スクリプトで台詞を送る"),
    ("Shaders", "UI シェーダー", "グレースケール・ディゾルブ・ホログラムなど 13 種類"),
    ("Capture", "キャプチャー", "ゲーム画面を撮ってぼかすポーズ画面・すりガラス"),
    ("Animation", "アニメーション", "キーで動かす登場・ループ・揺れの演出"),
    ("Layout", "レイアウト", "横・縦・グリッド・折り返し・スクロール"),
    ("Binding", "変数と入力", "スライダー・トグル・入力欄と {変数} の連動"),
    ("Styles", "スタイルと部品", "共通の見た目と、UI を部品として使い回す"),
    ("Transform", "変形と頂点カラー", "回転・四隅の色・Bounding ペイン・ユーザーデータ"),
    ("Overrides", "パーツの上書き", "同じパーツを置いた先ごとに、名前・ゲージ・色を変える"),
    ("AnimExtras", "区間・グループ・コマ送り", "In / Idle / Out の区間、まとめて動かすグループ、パラパラ漫画"),
]


def make_menu():
    cards = []
    for i, (key_, label, desc) in enumerate(SAMPLES):
        content = widget(
            "UISizeBox",
            f"Card{key_}Size",
            {"widthOverride": 326},
            children=[
                vbox(
                    f"Card{key_}Content",
                    [
                        text(f"Card{key_}Number", f"{i + 1:02d}", 22, rgba(AQUA, 0.9)),
                        text(f"Card{key_}Label", label, 32, TEXT),
                        text(f"Card{key_}Desc", desc, 20, DIM, wrap=True),
                    ],
                    spacing=8,
                )
            ],
        )
        cards.append(
            button(
                f"Open{key_}",
                "",
                f"open:UI/{key_}.aqua",
                color=PANEL_LIGHT,
                style="MenuCard",
                child=content,
                slot=box(),
            )
        )
    grid = widget(
        "UIGridPanel",
        "SampleGrid",
        {"columns": 4, "spacing": [20, 18], "cellHeight": 182, "padding": [0, 0, 0, 0]},
        at(0, 120, 1600, 582, anchor=(0.5, 0.5), pivot=(0.5, 0.5)),
        children=cards,
    )
    title = text(
        "MenuTitle",
        "Aqua サンプル集",
        110,
        TEXT,
        align="Center",
        slot=at(0, 70, 1400, 150, anchor=(0.5, 0), pivot=(0.5, 0)),
        common=shader_common("Shine", shine=[0.75, 1, 0.98, 0.9], interval=2.0),
        effect="None",
    )
    sub = text(
        "MenuSubtitle",
        "<wave>Aqua</wave> で作った UI の見本です。カードを選ぶと、それぞれの機能のサンプルが開きます",
        28,
        DIM,
        align="Center",
        slot=at(0, 218, 1600, 50, anchor=(0.5, 0), pivot=(0.5, 0)),
        reveal="Fade",
        revealSpeed=60,
        effectStrength=4,
    )
    hint = text(
        "MenuHint",
        "Esc でいつでもメニューに戻れます　・　各画面の UI は AquaUI フォルダの .aqs を Aqua で開くと編集できます",
        20,
        rgba(DIM, 0.8),
        align="Center",
        slot=at(0, -50, 1600, 40, anchor=(0.5, 1), pivot=(0.5, 1)),
    )
    styles = [
        {
            "name": "MenuCard",
            "type": "UIButton",
            "properties": {
                "normalColor": rgba(PANEL_LIGHT, 0.92),
                "hoveredColor": [0.12, 0.30, 0.33, 1],
                "pressedColor": [0.07, 0.20, 0.22, 1],
                "cornerRadius": 18,
                "borderColor": rgba(AQUA, 0.35),
                "borderThickness": 1.5,
                "contentPadding": [26, 16, 26, 16],
                "hoverScale": 1.04,
            },
        }
    ]
    intro = anim(
        "Intro",
        [
            track("MenuTitle", "opacity", [key(0, 0), key(0.6, 1)]),
            track("MenuTitle", "translation.y", [key(0, -40), key(0.6, 0)]),
            track("SampleGrid", "opacity", [key(0, 0), key(0.25, 0), key(0.8, 1)]),
            track("SampleGrid", "translation.y", [key(0, 40), key(0.25, 40), key(0.8, 0)]),
        ],
    )
    return document(
        [image("Background", [0.02, 0.05, 0.07, 0.6], slot=stretch(), common=shader_common("Vignette", strength=0.85, size=0.45)), title, sub, grid, hint],
        [intro],
        styles,
    )


# ---------------------------------------------------------------------------
# 文字アニメーション
# ---------------------------------------------------------------------------


def make_text_animation():
    effects = [
        ("Wave", "Wave　ゆらゆら波打つ", {}),
        ("Shake", "Shake　ぶるぶる震える", {"effectStrength": 3}),
        ("Rainbow", "Rainbow　虹色が流れる", {}),
        ("Pulse", "Pulse　どくんと脈打つ", {}),
        ("Swing", "Swing　左右に揺れる", {}),
        ("Glitch", "Glitch　ノイズで乱れる", {"effectStrength": 4}),
    ]
    reveals = [
        ("Typewriter", "Typewriter　1 文字ずつ打ち出す", {"revealSpeed": 18}),
        ("Fade", "Fade　1 文字ずつ浮かび上がる", {"revealSpeed": 20, "revealFade": 0.35}),
        ("Pop", "Pop　ぽんっと弾けて出る", {"revealSpeed": 16}),
        ("Rise", "Rise　下からせり上がる", {"revealSpeed": 18}),
        ("Drop", "Drop　上から落ちてくる", {"revealSpeed": 18}),
        ("Spin", "Spin　くるっと回って出る", {"revealSpeed": 14}),
    ]
    left = [text(f"Effect{n}", s, 34, TEXT, effect=n, **ex) for n, s, ex in effects]
    right = [text(f"Reveal{n}", s, 34, TEXT, reveal=n, revealDelay=0.3 + i * 0.25, **ex) for i, (n, s, ex) in enumerate(reveals)]
    effect_card = card("EffectCard", "動き（effect）… ずっと続く", left + [note("EffectNote", "詳細パネルの「アニメーション」で選ぶ。強さ（effectStrength）と速さ（effectSpeed）も変えられる")], width=780)
    reveal_card = card(
        "RevealCard",
        "出し方（reveal）… 表示したときに 1 回",
        right
        + [
            hbox(
                "RevealButtons",
                [
                    button("Replay", "もう一度", "restart-text", size=24, contentPadding=[22, 10, 22, 10]),
                    note("RevealNote", "スクリプトからは ui->RestartText(\"名前\") / SkipText(\"名前\")", slot=box(v="Center")),
                ],
                spacing=16,
            )
        ],
        width=780,
    )
    rich = card(
        "RichCard",
        "リッチテキスト（文字の中にタグを書く）",
        [
            text(
                "RichText",
                "<wave>ゆれる</wave>文字、<shake>震える</shake>文字、<rainbow>虹色</rainbow>の文字。"
                "<color=#ff7e6a>色を変え</color>たり<alpha=0.4>薄く</alpha>したり。<pause=0.6>間を置いて…"
                "<speed=0.25>ゆっくり</speed>出すこともできます。",
                32,
                TEXT,
                wrap=True,
                reveal="Typewriter",
                revealSpeed=16,
                revealDelay=1.2,
            ),
            note(
                "RichNote",
                "使えるタグ: <wave> <shake> <rainbow> <pulse> <swing> <glitch> <color=#rrggbb> <alpha=0.5> <pause=秒> <speed=倍率>（閉じるのは </名前>）",
                richText=False,
            ),
        ],
        width=1600,
    )
    content = [
        hbox("Columns", [effect_card, reveal_card], spacing=40, slot=at(0, 190, 0, 0, anchor=(0.5, 0), pivot=(0.5, 0), auto=True)),
        widget("UIOverlay", "RichHolder", slot=at(0, -50, 0, 0, anchor=(0.5, 1), pivot=(0.5, 1), auto=True), children=[rich]),
    ]
    return document(page("文字アニメーション", "UIText の effect（動き）と reveal（出し方）", content))


# ---------------------------------------------------------------------------
# 会話ウィンドウ
# ---------------------------------------------------------------------------


def make_dialog():
    portrait = widget(
        "UIOverlay",
        "Portrait",
        slot=box(v="Top"),
        children=[
            image("PortraitBg", AQUA, (150, 150), "@Circle", common=shader_common("Caustics", strength=0.7, speed=0.8)),
            text("PortraitFace", "・‿・", 44, INK, align="Center", slot=box("Center", "Center")),
        ],
    )
    body = vbox(
        "DialogBody",
        [
            text("Speaker", "{speaker}", 30, AQUA, common=shader_common("Gradient", top=[0.5, 1, 0.95, 1], bottom=[0.2, 0.75, 0.9, 1])),
            text("DialogText", "……", 36, TEXT, wrap=True, valign="Top", reveal="Typewriter", revealSpeed=24, revealFade=0.08),
        ],
        spacing=10,
        slot=box(rule="Fill"),
    )
    window = border(
        "DialogWindow",
        hbox("DialogRow", [portrait, body], spacing=32),
        [0.03, 0.08, 0.10, 0.94],
        24,
        (36, 30, 40, 30),
        slot=at(0, -60, 1500, 290, anchor=(0.5, 1), pivot=(0.5, 1)),
        border_color=rgba(AQUA, 0.45),
    )
    next_mark = text(
        "NextMark",
        "▼",
        30,
        AQUA,
        slot=at(-250, -84, 40, 40, anchor=(0.5, 1), pivot=(0.5, 1), z=5),
        common=dict(shader_common("Blink", speed=1.2, minAlpha=0.15), visibleBind="!talking"),
        effect="Pulse",
        effectStrength=3,
    )
    next_button = button(
        "NextButton",
        "次へ（Space / クリック）",
        "dialog-next",
        size=24,
        slot=at(-60, -380, 0, 0, anchor=(1, 1), pivot=(1, 1), auto=True),
        contentPadding=[24, 12, 24, 12],
    )
    hint = card(
        "DialogHint",
        "しくみ",
        [
            note(
                "DialogHintText",
                "台詞の表示は UIText の reveal（Typewriter）。台詞の文字・話し手はスクリプト（Source/AquaSampleHub.cpp）が"
                "SetText と SetVariable(\"speaker\") で入れ替えます。出している途中で「次へ」を押すと SkipText で全部出し、"
                "出し終わると ▼ が点滅します（visibleBind に \"!talking\"）。",
            )
        ],
        slot=at(60, 200, 0, 0, auto=True),
        width=760,
    )
    return document(page("会話ウィンドウ", "テキストアニメーション × スクリプト", [hint, window, next_mark, next_button], background=[0.02, 0.05, 0.07, 0.55]))


# ---------------------------------------------------------------------------
# UI シェーダー
# ---------------------------------------------------------------------------


def make_shaders():
    def sample_face(name, shader_name, label_text="Aqua", **params):
        # 色のついた見本（シェーダーの違いが分かるように、グラデーションの帯と文字）
        face = widget(
            "UIOverlay",
            name,
            slot=box(),
            common=shader_common(shader_name, **params),
            children=[
                border(name + "Bg", None, [0.95, 0.45, 0.25, 1], 14, (0, 0, 0, 0), shadow=False, slot=box()),
                hbox(
                    name + "Stripes",
                    [image(f"{name}S{i}", c, (40, 10), radius=6, slot=box(rule="Fill", pad=(0, 0, 0, 0))) for i, c in enumerate([[0.98, 0.78, 0.25, 1], [0.35, 0.85, 0.55, 1], [0.25, 0.65, 0.98, 1], [0.75, 0.40, 0.95, 1]])],
                    spacing=8,
                    pad=(14, 0, 14, 14),
                    slot=box(v="Bottom"),
                ),
                text(name + "Text", label_text, 36, [1, 1, 1, 1], align="Center", slot=box("Center", "Center", pad=(0, 0, 0, 22)), shadow=True),
            ],
        )
        return widget("UISizeBox", name + "Size", {"widthOverride": 270, "heightOverride": 110}, slot=box(), children=[face])

    items = [
        ("Grayscale", "グレースケール", {}),
        ("Sepia", "セピア", {}),
        ("Flash", "白く光らせる", {"amount": 0.0}),
        ("Shine", "光が流れる", {"interval": 0.6}),
        ("Dissolve", "ディゾルブ", {}),
        ("Gradient", "グラデーション", {"top": [0.6, 1, 0.95, 1], "bottom": [0.15, 0.4, 0.9, 1]}),
        ("Rainbow", "虹色", {"strength": 0.8}),
        ("HueShift", "色相を回す", {"speed": 0.15}),
        ("Blink", "点滅", {}),
        ("Scanlines", "走査線", {}),
        ("Hologram", "ホログラム", {}),
        ("Vignette", "ふちを暗く", {"strength": 0.9, "size": 0.2}),
    ]
    cards = []
    for n, label, params in items:
        cards.append(
            border(
                f"{n}Card",
                vbox(f"{n}CardBody", [sample_face(f"{n}Face", n, **params), text(f"{n}Label", label, 24, TEXT), text(f"{n}File", f"{n}.uishader", 17, DIM)], spacing=8),
                PANEL,
                16,
                (16, 16, 16, 14),
                slot=box(),
            )
        )
    grid = widget(
        "UIGridPanel",
        "ShaderGrid",
        {"columns": 4, "spacing": [20, 20], "cellHeight": 0, "padding": [0, 0, 0, 0]},
        at(60, 190, 0, 0, auto=True),
        children=cards,
    )
    custom = card(
        "CustomCard",
        "自作のシェーダー: Caustics",
        [
            widget(
                "UISizeBox",
                "CausticsSize",
                {"widthOverride": 360, "heightOverride": 200},
                children=[image("CausticsFace", [0.10, 0.30, 0.55, 1], (360, 200), radius=16, common=shader_common("Caustics"))],
            ),
            note("CustomNote", "Assets/Shaders/UI/Caustics.uishader を書き換えると、ゲームの実行中でもすぐ反映されます。"),
        ],
        slot=at(-60, 190, 0, 0, anchor=(1, 0), pivot=(1, 0), auto=True),
        width=360,
    )
    control = card(
        "ControlCard",
        "スクリプトで @param を動かす",
        [
            widget(
                "UISizeBox",
                "ControlledSize",
                {"widthOverride": 360, "heightOverride": 110},
                children=[
                    border(
                        "DissolveControlled",
                        text("DissolveControlledText", "ディゾルブ", 40, [1, 1, 1, 1], align="Center", slot=box("Center", "Center")),
                        [0.2, 0.55, 0.95, 1],
                        14,
                        (0, 0, 0, 0),
                        shadow=False,
                        common=shader_common("Dissolve", progress=0.0, edge=[0.3, 1, 0.9, 1]),
                    )
                ],
            ),
            widget("UISlider", "DissolveSlider", {"value": 0, "minValue": 0, "maxValue": 1, "bind": "dissolve", "fillColor": list(AQUA), "size": [360, 32]}),
            note("ControlNote", "スライダー → 変数 dissolve → スクリプトが ui->SetShaderParam(\"DissolveControlled\", \"progress\", 値)"),
        ],
        slot=at(-60, 650, 0, 0, anchor=(1, 0), pivot=(1, 0), auto=True),
        width=360,
    )
    loop = anim(
        "Loop",
        [
            track("DissolveFace", shader_slot("Dissolve", "progress"), [key(0, 0, 1), key(1.6, 1, 1), key(2.2, 1, 1), key(3.8, 0, 1), key(4.4, 0)]),
            track("FlashFace", shader_slot("Flash", "amount"), [key(0, 0), key(0.12, 0.9, 1), key(0.6, 0), key(1.6, 0)]),
        ],
    )
    return document(page("UI シェーダー", "ウィジェットの common に shader（.uishader）を指定。子にも掛かる", [grid, custom, control]), [loop])


# ---------------------------------------------------------------------------
# キャプチャー
# ---------------------------------------------------------------------------


def make_capture():
    glass = widget(
        "UIOverlay",
        "GlassPanel",
        slot=at(0, 40, 760, 420, anchor=(0.5, 0.5), pivot=(0.5, 0.5)),
        children=[
            widget("UICapture", "GlassCapture", {"timing": "EveryFrame", "area": "Behind", "color": [0.85, 1, 1, 1], "blur": 0.55, "blockInput": False, "size": [760, 420]}, slot=box(), common={"clipChildren": True}),
            border(
                "GlassFrame",
                widget("UISizeBox", "GlassBodySize", {"widthOverride": 688}, children=[vbox(
                    "GlassBody",
                    [
                        text("GlassTitle", "すりガラス", 44, TEXT),
                        note("GlassNote", "UICapture（area: Behind、timing: EveryFrame）で、このパネルの後ろだけを毎フレーム撮ってぼかしています。後ろで回っている 3D の立方体が透けて見えます。", size=24),
                        hbox(
                            "GlassButtons",
                            [
                                button("PauseButton", "ポーズ画面を開く", "pause", size=26, color=CORAL, text_color=INK, contentPadding=[26, 12, 26, 12]),
                                button("ShotButton", "スナップショット", "snapshot", size=26, contentPadding=[26, 12, 26, 12]),
                            ],
                            spacing=16,
                        ),
                    ],
                    spacing=16,
                )]),
                [1, 1, 1, 0.08],
                22,
                (36, 30, 36, 30),
                slot=box(),
                border_color=[1, 1, 1, 0.25],
            ),
        ],
    )
    snapshot = card(
        "SnapshotCard",
        "スナップショット（timing: Script）",
        [
            widget(
                "UISizeBox",
                "SnapshotSize",
                {"widthOverride": 384, "heightOverride": 216},
                children=[widget("UICapture", "Snapshot", {"timing": "Script", "area": "Screen", "blur": 0, "blockInput": False, "size": [384, 216]})],
            ),
            note("SnapshotNote", "ボタンでスクリプトが Capture() を呼んだ瞬間の画面。撮った画像はメモリーにあるだけで、ファイルには書きません。"),
        ],
        slot=at(-60, -60, 0, 0, anchor=(1, 1), pivot=(1, 1), auto=True),
        width=384,
    )
    pause_menu = border(
        "PauseMenu",
        vbox(
            "PauseBody",
            [
                text("PauseTitle", "PAUSE", 72, TEXT, align="Center", common=shader_common("Shine", interval=1.0)),
                note("PauseNote", "後ろはポーズした瞬間のゲーム画面（UICapture: OnShow + blur）。Time::SetTimeScale(0) で立方体も止まっています。", size=22),
                button("ResumeButton", "ゲームに戻る", "resume", size=30, color=AQUA, text_color=INK, slot=box(pad=(0, 10, 0, 0))),
                button("PauseBackButton", "メニューへ", "back", size=30, color=PANEL_LIGHT, slot=box()),
            ],
            spacing=16,
        ),
        [0.03, 0.07, 0.09, 0.85],
        24,
        (48, 40, 48, 44),
        slot=box("Center", "Center"),
        border_color=rgba(AQUA, 0.4),
    )
    pause_layer = widget(
        "UIOverlay",
        "PauseLayer",
        slot=stretch(z=50),
        common={"visibility": "Collapsed"},
        children=[
            widget("UICapture", "PauseCapture", {"timing": "OnShow", "area": "Screen", "color": [0.55, 0.62, 0.66, 1], "blur": 0.75, "blockInput": True}, slot=box()),
            widget("UISizeBox", "PauseSize", {"widthOverride": 620}, slot=box("Center", "Center"), children=[pause_menu]),
        ],
    )
    return document(page("キャプチャー", "UICapture: ゲームの画面を撮って UI の中に貼る", [glass, snapshot, pause_layer], background=None))


# ---------------------------------------------------------------------------
# ウィジェットアニメーション
# ---------------------------------------------------------------------------


def make_animation():
    panel = card(
        "SlidePanel",
        "Intro: 登場",
        [note("SlideNote", "translation.x と opacity にキーを打って、左から滑り込ませています。画面を開いたときに自動で再生（スクリプトが Intro という名前のアニメーションを再生）。")],
        slot=at(60, 200, 0, 0, auto=True),
        width=560,
    )
    heart = text("Heart", "♥", 140, CORAL, align="Center", slot=at(-380, 40, 200, 200, anchor=(0.5, 0.5), pivot=(0.5, 0.5)))
    heart_label = note("HeartLabel", "Loop: renderScale を繰り返し（ずっと再生）", slot=at(-380, 190, 360, 60, anchor=(0.5, 0.5), pivot=(0.5, 0.5)))
    bob = image("Bobber", AQUA, (90, 90), "@Circle", slot=at(0, 40, 90, 90, anchor=(0.5, 0.5), pivot=(0.5, 0.5)), common=shader_common("Caustics", strength=0.5))
    bob_label = note("BobLabel", "Loop: translation.y で上下", slot=at(0, 190, 300, 60, anchor=(0.5, 0.5), pivot=(0.5, 0.5)))
    target = border(
        "ShakeTarget",
        text("ShakeText", "揺れる", 40, TEXT, align="Center", slot=box("Center", "Center")),
        PANEL_LIGHT,
        18,
        (40, 26, 40, 26),
        slot=at(380, 40, 0, 0, anchor=(0.5, 0.5), pivot=(0.5, 0.5), auto=True),
        common=shader_common("Flash", amount=0.0, flash=CORAL),
    )
    gauge = widget("UIProgressBar", "Gauge", {"percent": 0, "fillColor": list(AQUA), "size": [700, 30], "cornerRadius": 15, "smoothing": 0}, slot=at(0, 300, 700, 30, anchor=(0.5, 0.5), pivot=(0.5, 0.5)))
    buttons = hbox(
        "AnimButtons",
        [
            button("PlayIntro", "Intro を再生", "anim:Intro", size=26, contentPadding=[24, 12, 24, 12]),
            button("PlayShake", "揺らす（Shake）", "anim:Shake", size=26, color=CORAL, text_color=INK, contentPadding=[24, 12, 24, 12]),
            button("PlayGauge", "ゲージ（Fill）", "anim:Fill", size=26, contentPadding=[24, 12, 24, 12]),
        ],
        spacing=20,
        slot=at(0, -80, 0, 0, anchor=(0.5, 1), pivot=(0.5, 1), auto=True),
    )
    intro = anim(
        "Intro",
        [
            track("SlidePanel", "translation.x", [key(0, -700), key(0.7, 0)]),
            track("SlidePanel", "opacity", [key(0, 0), key(0.5, 1)]),
        ],
    )
    loop = anim(
        "Loop",
        [
            track("Heart", "renderScale.x", [key(0, 1), key(0.15, 1.25), key(0.3, 1), key(0.45, 1.15), key(0.7, 1), key(1.2, 1)]),
            track("Heart", "renderScale.y", [key(0, 1), key(0.15, 1.25), key(0.3, 1), key(0.45, 1.15), key(0.7, 1), key(1.2, 1)]),
            track("Bobber", "translation.y", [key(0, 0), key(0.6, -50), key(1.2, 0)]),
        ],
    )
    shake = anim(
        "Shake",
        [
            track("ShakeTarget", "translation.x", [key(0, 0, 1), key(0.05, -18, 1), key(0.1, 16, 1), key(0.15, -12, 1), key(0.2, 9, 1), key(0.25, -5, 1), key(0.3, 0)]),
            track("ShakeTarget", shader_slot("Flash", "amount"), [key(0, 0.8, 1), key(0.35, 0)]),
        ],
    )
    fill = anim("Fill", [track("Gauge", "percent", [key(0, 0, 1), key(1.5, 1, 1), key(2.0, 1), key(2.4, 0)])])
    return document(
        page("ウィジェットアニメーション", "プロパティにキーを打って動かす（Aqua の下の「アニメーション」パネル）", [panel, heart, heart_label, bob, bob_label, target, gauge, buttons]),
        [intro, loop, shake, fill],
    )


# ---------------------------------------------------------------------------
# レイアウト
# ---------------------------------------------------------------------------


def make_layout():
    def chip(name, label, color):
        return border(name, text(name + "T", label, 22, INK), color, 20, (16, 6, 16, 6), shadow=False, border_color=(0, 0, 0, 0))

    hrow = hbox(
        "HRow",
        [
            border("HAuto", text("HAutoT", "Auto", 24, TEXT, align="Center"), PANEL_LIGHT, 10, (18, 12, 18, 12), shadow=False, slot=box(rule="Auto")),
            border("HFill1", text("HFill1T", "Fill ×1", 24, INK, align="Center"), AQUA, 10, (18, 12, 18, 12), shadow=False, slot=box(rule="Fill", weight=1)),
            border("HFill2", text("HFill2T", "Fill ×2", 24, INK, align="Center"), CORAL, 10, (18, 12, 18, 12), shadow=False, slot=box(rule="Fill", weight=2)),
        ],
        spacing=10,
    )
    hcard = card("HCard", "Horizontal Box（Auto と Fill の割合）", [widget("UISizeBox", "HSize", {"widthOverride": 760}, children=[hrow])], slot=at(60, 190, 0, 0, auto=True))

    tiles = [border(f"Tile{i}", text(f"Tile{i}T", str(i + 1), 30, TEXT, align="Center"), PANEL_LIGHT if i % 3 else rgba(AQUA, 0.35), 10, (0, 18, 0, 18), shadow=False, slot=box()) for i in range(12)]
    gcard = card(
        "GridCard",
        "Grid Panel（4 列）",
        [widget("UIGridPanel", "Grid", {"columns": 4, "spacing": [10, 10], "cellHeight": 0}, children=tiles)],
        slot=at(60, 400, 0, 0, auto=True),
        width=760,
    )

    tag_colors = [AQUA, CORAL, GOLD, [0.6, 0.75, 1, 1]]
    tags = ["RPG", "アクション", "パズル", "シューティング", "ノベル", "レース", "音ゲー", "ローグライク", "シミュレーション", "スポーツ", "ホラー", "サンドボックス"]
    wcard = card(
        "WrapCard",
        "Wrap Box（入りきらなければ折り返す）",
        [widget("UIWrapBox", "Wrap", {"spacing": [10, 10]}, children=[chip(f"Tag{i}", t, tag_colors[i % 4]) for i, t in enumerate(tags)])],
        slot=at(-60, 190, 0, 0, anchor=(1, 0), pivot=(1, 0), auto=True),
        width=820,
    )

    rows = []
    for i in range(24):
        rows.append(
            border(
                f"Row{i}",
                hbox(
                    f"Row{i}Box",
                    [
                        text(f"Row{i}No", f"#{i + 1:02d}", 22, AQUA, slot=box(v="Center")),
                        text(f"Row{i}Name", f"アイテム {i + 1}", 24, TEXT, slot=box(v="Center", rule="Fill")),
                        text(f"Row{i}Count", f"×{(i * 7) % 13 + 1}", 22, DIM, slot=box(v="Center")),
                    ],
                    spacing=16,
                ),
                PANEL_LIGHT if i % 2 else [0.07, 0.13, 0.16, 1],
                8,
                (16, 10, 16, 10),
                shadow=False,
                border_color=(0, 0, 0, 0),
            )
        )
    scroll = widget("UIScrollBox", "Scroll", {"spacing": 6, "padding": [0, 0, 10, 0], "scrollbarColor": rgba(AQUA, 0.6)}, children=rows)
    scard = card(
        "ScrollCard",
        "Scroll Box（ホイール・ドラッグでスクロール）",
        [widget("UISizeBox", "ScrollSize", {"widthOverride": 820, "heightOverride": 380}, children=[scroll])],
        slot=at(-60, 470, 0, 0, anchor=(1, 0), pivot=(1, 0), auto=True),
    )
    return document(page("レイアウト", "Box とスロット（Unreal 式）で並べる。Canvas Panel の子はアンカー（Unity 式）", [hcard, gcard, wcard, scard]))


# ---------------------------------------------------------------------------
# 変数と入力
# ---------------------------------------------------------------------------


def make_binding():
    def row(name, label, control, value_text):
        return hbox(
            name,
            [
                text(name + "Label", label, 26, TEXT, slot=box(v="Center")),
                widget("UISizeBox", name + "Ctl", {"widthOverride": 380}, slot=box(v="Center"), children=[control]),
                text(name + "Value", value_text, 26, AQUA, slot=box(v="Center")),
            ],
            spacing=24,
        )

    volume = widget("UISlider", "VolumeSlider", {"value": 70, "minValue": 0, "maxValue": 100, "step": 1, "bind": "volume", "fillColor": list(AQUA), "size": [380, 32]})
    hp = widget("UISlider", "HpSlider", {"value": 0.65, "minValue": 0, "maxValue": 1, "bind": "hp", "fillColor": list(CORAL), "size": [380, 32]})
    secret = widget("UIToggle", "SecretToggle", {"isOn": False, "text": "ひみつを表示", "bind": "showSecret", "onColor": list(AQUA), "fontSize": 24})
    name_input = widget("UITextInput", "NameInput", {"text": "", "placeholder": "名前を入力", "bind": "playerName", "size": [380, 50]})

    controls = card(
        "Controls",
        "入力（bind に変数名を書くと値が変数に入る）",
        [
            row("VolumeRow", "音量", volume, "{volume}"),
            row("HpRow", "HP", hp, "{hp:2}"),
            row("SecretRow", "表示", secret, ""),
            row("NameRow", "名前", name_input, ""),
        ],
        slot=at(60, 200, 0, 0, auto=True),
    )
    hp_bar = widget("UIProgressBar", "HpBar", {"bind": "hp", "fillColor": list(CORAL), "size": [600, 28], "cornerRadius": 14})
    display = card(
        "Display",
        "表示（{変数} と bind で値を読む）",
        [
            text("Greeting", "ようこそ、{playerName} さん！", 40, TEXT, effect="Wave", effectStrength=3),
            text("VolumeText", "音量は {volume} です", 30, TEXT),
            hbox("HpLine", [text("HpText", "HP", 26, CORAL, slot=box(v="Center")), widget("UISizeBox", "HpBarSize", {"widthOverride": 560}, slot=box(v="Center"), children=[hp_bar])], spacing=16),
            border(
                "Secret",
                text("SecretText", "<rainbow>ひみつのメッセージ</rainbow>：visibleBind が showSecret なので、トグルを入れたときだけ見えます", 26, TEXT, wrap=True),
                rgba(CORAL, 0.18),
                12,
                (18, 12, 18, 12),
                shadow=False,
                common={"visibleBind": "showSecret"},
            ),
            text("ScoreText", "スコア: {score}", 34, GOLD),
            hbox(
                "ScoreButtons",
                [
                    button("AddScore", "+100（スクリプトで加算）", "score-add", size=24, color=GOLD, text_color=INK, contentPadding=[20, 10, 20, 10]),
                    button("ResetScore", "リセット", "score-reset", size=24, color=PANEL_LIGHT, contentPadding=[20, 10, 20, 10]),
                ],
                spacing=12,
            ),
            note("DisplayNote", "スクリプトからは ui->SetVariable(\"score\", 値)。{変数名:2} のように : の後ろで小数の桁数を決められる（HP の 0.65 は hp:2 で表示）"),
        ],
        slot=at(-60, 200, 0, 0, anchor=(1, 0), pivot=(1, 0), auto=True),
        width=820,
    )
    return document(page("変数と入力", "スライダー・トグル・入力欄の bind と、文字の {変数}", [controls, display]))


# ---------------------------------------------------------------------------
# スタイルと部品
# ---------------------------------------------------------------------------


def make_status_card():
    """パーツ（ほかのレイアウトに部品として入れる小さなレイアウト）。"""
    body = vbox(
        "StatusBody",
        [
            hbox(
                "StatusHead",
                [
                    image("StatusIcon", AQUA, (56, 56), "@Circle", slot=box(v="Center"), common=shader_common("Caustics", strength=0.6)),
                    vbox("StatusNames", [text("StatusName", "アクア", 30, TEXT), text("StatusClass", "Lv.12 まほうつかい", 20, DIM)], spacing=0, slot=box(v="Center")),
                ],
                spacing=16,
            ),
            hbox("StatusHpRow", [text("StatusHpLabel", "HP", 20, CORAL, slot=box(v="Center")), widget("UIProgressBar", "StatusHp", {"percent": 0.72, "fillColor": list(CORAL), "size": [260, 16]}, slot=box(v="Center", rule="Fill"))], spacing=12),
            hbox("StatusMpRow", [text("StatusMpLabel", "MP", 20, AQUA, slot=box(v="Center")), widget("UIProgressBar", "StatusMp", {"percent": 0.45, "fillColor": list(AQUA), "size": [260, 16]}, slot=box(v="Center", rule="Fill"))], spacing=12),
        ],
        spacing=12,
    )
    card_ = border("StatusCard", body, PANEL, 18, (22, 18, 22, 20), slot=at(0, 0, 360, 190))
    return document([card_], kind="part", resolution=(360, 190))


def make_styles():
    styles = [
        {
            "name": "Primary",
            "type": "UIButton",
            "properties": {"normalColor": list(AQUA), "hoveredColor": [0.40, 0.92, 0.88, 1], "pressedColor": [0.15, 0.65, 0.62, 1], "textColor": list(INK), "cornerRadius": 30, "fontSize": 28, "contentPadding": [32, 14, 32, 14]},
        },
        {
            "name": "Danger",
            "type": "UIButton",
            "properties": {"normalColor": list(CORAL), "hoveredColor": [1, 0.62, 0.55, 1], "pressedColor": [0.85, 0.38, 0.32, 1], "textColor": list(INK), "cornerRadius": 30, "fontSize": 28, "contentPadding": [32, 14, 32, 14]},
        },
        {
            "name": "Ghost",
            "type": "UIButton",
            "properties": {"normalColor": [1, 1, 1, 0.04], "hoveredColor": [1, 1, 1, 0.12], "pressedColor": [1, 1, 1, 0.02], "textColor": list(TEXT), "borderColor": rgba(AQUA, 0.6), "borderThickness": 2, "cornerRadius": 30, "fontSize": 28, "shadow": False, "contentPadding": [32, 14, 32, 14]},
        },
        {
            "name": "Heading",
            "type": "UIText",
            "properties": {"fontSize": 34, "color": list(GOLD), "shadow": True},
        },
    ]

    def styled_button(name, label, style_name):
        return widget("UIButton", name, {"text": label, "clickMessage": "noop"}, slot=box(), style=style_name)

    buttons = vbox(
        "StyleButtons",
        [
            widget("UIText", "StyleHeading1", {"text": "Primary"}, style="Heading"),
            hbox("PrimaryRow", [styled_button(f"Primary{i}", t, "Primary") for i, t in enumerate(["はじめる", "つづきから", "決定"])], spacing=16),
            widget("UIText", "StyleHeading2", {"text": "Danger"}, style="Heading"),
            hbox("DangerRow", [styled_button(f"Danger{i}", t, "Danger") for i, t in enumerate(["削除", "やめる"])], spacing=16),
            widget("UIText", "StyleHeading3", {"text": "Ghost"}, style="Heading"),
            hbox("GhostRow", [styled_button(f"Ghost{i}", t, "Ghost") for i, t in enumerate(["設定", "クレジット", "戻る"])], spacing=16),
            note("StyleNote", "同じスタイルのボタンは見た目が共通。Aqua の「スタイル」パネルで 1 つ直すと全部変わります。"),
        ],
        spacing=14,
    )
    style_card = card("StyleCard", "スタイル（共通の見た目）", [buttons], slot=at(60, 200, 0, 0, auto=True), width=760)

    parts = hbox(
        "Parts",
        [widget("UIUserWidget", f"Member{i}", {"asset": "UI/StatusCard.aqua"}, slot=box(v="Top")) for i in range(2)],
        spacing=20,
    )
    part_card = card(
        "PartCard",
        "部品（ユーザーウィジェット）",
        [
            widget("UISizeBox", "PartsSize", {"widthOverride": 760, "heightOverride": 200}, children=[parts]),
            note("PartNote", "UI/StatusCard.aqua（AquaUI/StatusCard.aqs）を UIUserWidget で 2 つ置いています。StatusCard を Aqua で直すと、ここも変わります。"),
        ],
        slot=at(-60, 200, 0, 0, anchor=(1, 0), pivot=(1, 0), auto=True),
        width=760,
    )
    return document(page("スタイルと部品", "見た目をスタイルにまとめる・UI を部品として入れる", [style_card, part_card]), styles=styles)


# ---------------------------------------------------------------------------
# 変形と頂点カラー（回転・四隅の色・Bounding ペイン・ユーザーデータ）
# ---------------------------------------------------------------------------


def corners(tl, tr, bl, br):
    """頂点カラー（四隅の色）の common。"""
    return {"vertexColorsEnabled": True, "vertexColorTopLeft": list(tl), "vertexColorTopRight": list(tr),
            "vertexColorBottomLeft": list(bl), "vertexColorBottomRight": list(br)}


def make_transform():
    # 回転: Null ペインを回すと、子がまとめて回る
    dots = []
    for i, (x, y, c) in enumerate([(0, 0, AQUA), (110, 0, CORAL), (0, 110, GOLD), (110, 110, [0.6, 0.75, 1, 1])]):
        dots.append(image(f"Dot{i}", c, (70, 70), radius=14, slot=at(x, y, 70, 70)))
    spinner = widget("UINullPane", "Spinner", slot=at(0, 0, 180, 180, anchor=(0.5, 0.5), pivot=(0.5, 0.5)), children=dots)
    spin_area = widget("UISizeBox", "SpinArea", {"widthOverride": 300, "heightOverride": 260}, children=[widget("UICanvasPanel", "SpinCanvas", children=[spinner])])
    # 時計の針: transformOrigin を根元（下の真ん中）にして回す
    dial = image("Dial", [0.08, 0.16, 0.2, 1], (200, 200), "@Circle", slot=at(0, 0, 200, 200, anchor=(0.5, 0.5), pivot=(0.5, 0.5)))
    hand = image("Hand", CORAL, (10, 90), radius=5, slot=at(0, -45, 10, 90, anchor=(0.5, 0.5), pivot=(0.5, 0.5)),
                 common={"transformOrigin": [0.5, 1.0]})
    clock = widget("UISizeBox", "ClockArea", {"widthOverride": 260, "heightOverride": 260}, children=[widget("UICanvasPanel", "ClockCanvas", children=[dial, hand])])
    rotate_card = card(
        "RotateCard",
        "回転（rotation・transformOrigin）",
        [
            hbox("RotateRow", [spin_area, clock], spacing=20),
            button("TiltedButton", "回したボタン（押すと当たりが増える）", "bounding-hit", size=22, contentPadding=[20, 10, 20, 10],
                   slot=box(h="Center", pad=(0, 10, 0, 10)), common={"rotation": -8}),
            note("RotateNote", "左: Null ペインの rotation を回すと子がまとめて回る。右: 針の transformOrigin を根元（0.5, 1）にして回す。回したボタンもそのまま押せる"),
        ],
        slot=at(60, 190, 0, 0, auto=True),
        width=560,
    )

    # 頂点カラー: 四隅の色を混ぜる（画像・背景・文字）
    grad_box = image("GradBox", [1, 1, 1, 1], (240, 140), radius=18, common=corners(AQUA, [0.6, 0.75, 1, 1], CORAL, GOLD))
    grad_text = text("GradText", "四隅の色", 64, [1, 1, 1, 1], common=corners([1, 0.95, 0.6, 1], [1, 0.95, 0.6, 1], CORAL, CORAL))
    grad_panel = border("GradPanel", text("GradPanelText", "パネルにも", 30, INK, align="Center"), [1, 1, 1, 1], 16, (24, 18, 24, 18),
                        shadow=False, common=corners(AQUA, AQUA, [0.1, 0.4, 0.5, 1], [0.1, 0.4, 0.5, 1]))
    color_card = card(
        "ColorCard",
        "頂点カラー（四隅の色）",
        [grad_box, grad_text, grad_panel,
         note("ColorNote", "詳細の「頂点カラー」で左上・右上・左下・右下の色を決めると、自分の描画（画像・背景・文字）に混ぜて掛かる。色はアニメーションもできる")],
        slot=at(0, 190, 0, 0, anchor=(0.5, 0), pivot=(0.5, 0), auto=True),
        width=440,
    )

    # Bounding ペイン: 絵の一部だけを当たりにする
    target = widget("UICanvasPanel", "TreasureCanvas", children=[
        image("Treasure", [0.35, 0.22, 0.12, 1], (280, 180), radius=20, slot=at(0, 0, 280, 180)),
        image("Lock", GOLD, (60, 60), "@Circle", slot=at(110, 60, 60, 60)),
        text("TreasureLabel", "鍵だけが当たり", 22, TEXT, align="Center", slot=at(0, 190, 280, 30)),
        widget("UIBoundingPane", "LockHit", {"clickMessage": "bounding-hit"}, at(100, 50, 80, 80)),
    ])
    treasure = widget("UISizeBox", "TreasureArea", {"widthOverride": 280, "heightOverride": 230}, children=[target])
    shop = border(
        "ShopItem",
        vbox("ShopItemBody", [text("ShopItemName", "{itemName}", 30, TEXT), text("ShopItemPrice", "{itemPrice} G", 26, GOLD)], spacing=4),
        PANEL_LIGHT, 14, (20, 14, 20, 14), shadow=False,
        common={"userData": "item=ポーション\nprice=120\nrarity=common"},
    )
    hit_card = card(
        "HitCard",
        "Bounding ペイン・ユーザーデータ",
        [
            treasure,
            text("HitCount", "当たった回数: {hits}", 28, CORAL),
            shop,
            note("HitNote", "鍵の上に何も描かない Bounding ペインを置き、押すと Click（bounding-hit）が届く。下の札は userData（item=… / price=…）をスクリプトが GetUserData で読んで表示している"),
        ],
        slot=at(-60, 190, 0, 0, anchor=(1, 0), pivot=(1, 0), auto=True),
        width=520,
    )
    loop = anim("Loop", [
        track("Spinner", "rotation", [key(0, 0, 1), key(4, 360, 1)]),
        track("Hand", "rotation", [key(0, 0, 1), key(6, 360, 1)]),
        track("GradBox", "vertexColorTopLeft.x", [key(0, 0.2), key(1.5, 1.0), key(3, 0.2)]),
    ])
    return document(page("変形と頂点カラー", "LayoutEditor 風の機能 その 1", [rotate_card, color_card, hit_card]), [loop])


# ---------------------------------------------------------------------------
# パーツの上書き
# ---------------------------------------------------------------------------


def make_overrides():
    import json as _json

    members = [
        ("Aqua", {"StatusName": {"text": "アクア"}, "StatusClass": {"text": "Lv.12 まほうつかい"}, "StatusHp": {"percent": 0.72}, "StatusMp": {"percent": 0.45}}),
        ("Ruto", {"StatusName": {"text": "ルト"}, "StatusClass": {"text": "Lv.15 けんし"}, "StatusHp": {"percent": 0.95}, "StatusMp": {"percent": 0.1},
                  "StatusIcon": {"shaderParam0": list(CORAL)}}),
        ("Coral", {"StatusName": {"text": "コーラル"}, "StatusClass": {"text": "Lv.9 そうりょ"}, "StatusHp": {"percent": 0.3}, "StatusMp": {"percent": 0.9},
                   "StatusIcon": {"shaderParam0": list(GOLD)}}),
    ]
    cards = [
        widget("UIUserWidget", f"Member{n}", {"asset": "UI/StatusCard.aqua", "overrides": _json.dumps(ov, ensure_ascii=False)},
               slot=box(v="Top"))
        for n, ov in members
    ]
    party = widget("UISizeBox", "PartySize", {"widthOverride": 1140, "heightOverride": 200},
                   children=[hbox("Party", cards, spacing=30)])
    party_card = card(
        "PartyCard",
        "同じパーツ（StatusCard）を 3 つ置いて、それぞれ上書き",
        [
            party,
            note("PartyNote", "置いたパーツを選ぶと、詳細の下の「パーツの上書き」で中のウィジェット（StatusName・StatusHp・StatusIcon など）を選んで値を変えられる。"
                 "パーツの元の値と違うものだけが保存され、パーツ（StatusCard）を直すと上書きしていない所はすべての置き場所に反映される"),
            hbox("OverrideButtons", [
                button("RenameButton", "スクリプトで名前を変える（SetOverride）", "override-rename", size=24, contentPadding=[20, 10, 20, 10]),
                button("ResetButton", "上書きを消す（ClearOverrides）", "override-reset", size=24, color=PANEL_LIGHT, contentPadding=[20, 10, 20, 10]),
            ], spacing=16),
        ],
        slot=at(0, 200, 0, 0, anchor=(0.5, 0), pivot=(0.5, 0), auto=True),
        width=1180,
    )
    return document(page("パーツの上書き", "LayoutEditor 風の機能 その 2", [party_card]))


# ---------------------------------------------------------------------------
# 区間・グループ・コマ送り
# ---------------------------------------------------------------------------


def make_anim_extras():
    # 区間: 1 本のアニメーション Show を In / Idle / Out に分けて、名前で再生する
    banner = border("Banner", text("BannerText", "セール中！", 48, INK, align="Center"), GOLD, 22, (40, 20, 40, 20), shadow=True,
                    slot=box("Center"), common={"opacity": 0})
    banner_area = widget("UISizeBox", "BannerArea", {"widthOverride": 480, "heightOverride": 150}, children=[
        widget("UIOverlay", "BannerHolder", children=[banner])])
    section_card = card(
        "SectionCard",
        "区間（1 本のタイムラインを名前で分けて再生）",
        [
            banner_area,
            hbox("SectionButtons", [
                button("PlayIn", "In", "anim:In", size=24, contentPadding=[26, 10, 26, 10]),
                button("PlayIdle", "Idle（繰り返し）", "anim:Idle", size=24, contentPadding=[26, 10, 26, 10]),
                button("PlayOut", "Out", "anim:Out", size=24, color=CORAL, text_color=INK, contentPadding=[26, 10, 26, 10]),
            ], spacing=14),
            note("SectionNote", "アニメーション Show（3 秒）に区間 In（0〜0.6 秒）・Idle（0.6〜2.2 秒・繰り返し）・Out（2.2〜2.8 秒）を付け、PlayAnimation(\"In\") のように区間の名前で再生している"),
        ],
        slot=at(60, 190, 0, 0, auto=True),
        width=520,
    )

    # グループ: 階層はばらばらでも、グループ Stars のトラックで全員が同じように動く
    stars = [text(f"Star{i}", "★", 56, [GOLD, AQUA, CORAL][i % 3], align="Center", slot=at(40 + i * 72, 40 + (i % 2) * 50, 64, 64)) for i in range(6)]
    star_area = widget("UISizeBox", "StarArea", {"widthOverride": 520, "heightOverride": 170}, children=[widget("UICanvasPanel", "StarCanvas", children=stars)])
    group_card = card(
        "GroupCard",
        "グループ（まとめて動かす・隠す）",
        [
            star_area,
            hbox("GroupButtons", [
                button("HideStars", "グループを隠す", "group-hide", size=24, color=PANEL_LIGHT, contentPadding=[22, 10, 22, 10]),
                button("ShowStars", "グループを出す", "group-show", size=24, contentPadding=[22, 10, 22, 10]),
            ], spacing=14),
            note("GroupNote", "6 つの星をグループ Stars にまとめ、トラック @Stars（拡大）1 本で全員を瞬かせている。ボタンはスクリプトの SetGroupVisible(\"Stars\", …)"),
        ],
        slot=at(0, 190, 0, 0, anchor=(0.5, 0), pivot=(0.5, 0), auto=True),
        width=520,
    )

    # コマ送り: 画像を 1 行に 1 つ並べ、自動で送る / 今のコマにキーを打つ
    frames = "@Circle\n@SoftCircle\n@White\n@Checker"
    flip_auto = widget("UIImage", "FlipAuto", {"color": list(AQUA), "size": [140, 140], "frames": frames, "framesPerSecond": 3}, slot=box(v="Center"))
    flip_keys = widget("UIImage", "FlipKeys", {"color": list(CORAL), "size": [140, 140], "frames": frames, "frame": 0}, slot=box(v="Center"))
    flip_card = card(
        "FlipCard",
        "コマ送り（パラパラ漫画）",
        [
            hbox("FlipRow", [
                vbox("FlipAutoCol", [flip_auto, text("FlipAutoLabel", "自動（3 コマ/秒）", 22, DIM)], spacing=8),
                vbox("FlipKeysCol", [flip_keys, text("FlipKeysLabel", "キーで送る", 22, DIM)], spacing=8),
            ], spacing=40),
            note("FlipNote", "画像の「コマの画像」に 1 行に 1 つ画像を書くと、そのコマを描く。左は「自動で送る速さ」、右は Loop のアニメーションで「今のコマ」にコンスタントのキーを打っている"),
        ],
        slot=at(-60, 190, 0, 0, anchor=(1, 0), pivot=(1, 0), auto=True),
        width=440,
    )

    show = anim(
        "Show",
        [
            track("Banner", "opacity", [key(0, 0), key(0.6, 1), key(2.2, 1), key(2.8, 0)]),
            track("Banner", "translation.y", [key(0, 60), key(0.6, 0), key(2.2, 0), key(2.8, -40)]),
            track("Banner", "renderScale.x", [key(0.6, 1), key(1.0, 1.08), key(1.4, 1), key(1.8, 1.08), key(2.2, 1)]),
            track("Banner", "renderScale.y", [key(0.6, 1), key(1.0, 1.08), key(1.4, 1), key(1.8, 1.08), key(2.2, 1)]),
        ],
        sections=[("In", 0.0, 0.6, False), ("Idle", 0.6, 2.2, True), ("Out", 2.2, 2.8, False)],
    )
    loop = anim("Loop", [
        track("@Stars", "renderScale.x", [key(0, 1), key(0.4, 1.35), key(0.8, 1), key(1.6, 1)]),
        track("@Stars", "renderScale.y", [key(0, 1), key(0.4, 1.35), key(0.8, 1), key(1.6, 1)]),
        track("FlipKeys", "frame", [key(0, 0, 2), key(0.25, 1, 2), key(0.5, 2, 2), key(0.75, 3, 2), key(1.0, 0, 2), key(1.6, 0, 2)]),
    ])
    return document(
        page("区間・グループ・コマ送り", "LayoutEditor 風の機能 その 3", [section_card, group_card, flip_card]),
        [show, loop],
        groups=[("Stars", [f"Star{i}" for i in range(6)])],
    )


UIS = {
    "Menu": make_menu,
    "TextAnimation": make_text_animation,
    "Dialog": make_dialog,
    "Shaders": make_shaders,
    "Capture": make_capture,
    "Animation": make_animation,
    "Layout": make_layout,
    "Binding": make_binding,
    "StatusCard": make_status_card,
    "Styles": make_styles,
    "Transform": make_transform,
    "Overrides": make_overrides,
    "AnimExtras": make_anim_extras,
}


# ---------------------------------------------------------------------------
# 書き出し
# ---------------------------------------------------------------------------


def write_text(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def run_aqua(*args):
    r = subprocess.run([str(AQUA_EXE), *args], capture_output=True, text=True, encoding="utf-8", errors="replace")
    out = (r.stdout + r.stderr).strip()
    if out:
        print(out)
    if r.returncode != 0:
        sys.exit(f"Aqua {' '.join(args[:1])} に失敗しました（{r.returncode}）")


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--keep", action="store_true", help="既にある .aqs は上書きしない")
    args = parser.parse_args()
    if not AQUA_EXE.exists():
        sys.exit(f"{AQUA_EXE} がありません。先に build.bat で Aqua をビルドしてください")

    for name, source in SHADERS.items():
        write_text(PROJECT / "Assets" / SHADER_DIR / f"{name}.uishader", source)
    print(f"{len(SHADERS)} 個のシェーダーを書きました")

    source_dir = PROJECT / "AquaUI"
    with tempfile.TemporaryDirectory() as tmp:
        for name, make in UIS.items():
            aqs = source_dir / f"{name}.aqs"
            if aqs.exists():
                if args.keep:
                    print(f"そのまま: {aqs.name}")
                    continue
                aqs.unlink()
            rui = Path(tmp) / f"{name}.rui"
            rui.write_text(json.dumps(make(), ensure_ascii=False, indent=2), encoding="utf-8")
            run_aqua("import", str(rui), "--project", str(PROJECT))
    # 全部書き出し直す（部品の UI も含めて .aqua とスクリプト用のヘッダーをそろえる）
    run_aqua("export", str(PROJECT))


if __name__ == "__main__":
    main()
