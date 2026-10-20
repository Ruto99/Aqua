"""Aqua（UI を作るツール）のアイコンを作る（Python の標準ライブラリだけで動く）。

    python make_aqua_icon.py

水色のグラデーションの角丸の四角に、白い水滴と UI の行を描き、
Aqua.png（256px）と Aqua.ico（16〜256px。中身は PNG）をこのフォルダに書き出す。
"""
import math
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))


def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(len(a)))


def rounded_rect_inside(x, y, x0, y0, x1, y1, r):
    cx = min(max(x, x0 + r), x1 - r)
    cy = min(max(y, y0 + r), y1 - r)
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r and x0 <= x <= x1 and y0 <= y <= y1


def droplet_inside(x, y, cx, cy, r, tip_y):
    if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
        return True
    d = cy - tip_y
    if y < tip_y or y > cy:
        return False
    s = r / d                   # 接線の角度の sin
    slope = s / math.sqrt(1 - s * s)
    ty = cy - r * s             # 接点の高さ（ここより上は三角形）
    if y > ty:
        return False
    return abs(x - cx) <= (y - tip_y) * slope


def sample(u, v):
    """0〜1 の座標の色（RGBA、0〜1）"""
    m = 0.06
    if not rounded_rect_inside(u, v, m, m, 1 - m, 1 - m, (1 - 2 * m) * 0.22):
        # 影（下にうっすら）
        if rounded_rect_inside(u, v - 0.02, m, m, 1 - m, 1 - m, (1 - 2 * m) * 0.22):
            return (0, 0, 0, 0.25)
        return (0, 0, 0, 0)
    t = min(max((u + v) / 2, 0), 1)
    col = lerp((0.18, 0.80, 1.00), (0.15, 0.36, 0.92), t)
    # つや
    if (u - 0.5) ** 2 / 0.55 ** 2 + (v + 0.25) ** 2 / 0.6 ** 2 <= 1:
        col = lerp(col, (1, 1, 1), 0.12)
    # 水滴
    cx, cy, r, tip = 0.5, 0.56, 0.21, 0.17
    if droplet_inside(u, v, cx, cy, r, tip):
        shade = min(max((u - cx) * 0.9 + (v - cy) * 1.2 + 0.25, 0), 1)
        c = lerp((1, 1, 1), (0.80, 0.93, 1.0), shade)
        # 中に UI の行（3 本）
        for i, (w, yy) in enumerate(((0.20, 0.52), (0.26, 0.59), (0.14, 0.66))):
            if abs(v - yy) <= 0.018 and cx - w / 2 <= u <= cx + w / 2:
                c = lerp((0.20, 0.62, 1.0), (0.16, 0.42, 0.95), i / 2)
        # 光の点
        if (u - 0.43) ** 2 + (v - 0.47) ** 2 <= 0.028 ** 2:
            c = (1, 1, 1)
        return c + (1.0,)
    return col + (1.0,)


def render(n, ss):
    px = []
    for y in range(n):
        row = []
        for x in range(n):
            acc = [0.0, 0.0, 0.0, 0.0]
            for sy in range(ss):
                for sx in range(ss):
                    c = sample((x + (sx + 0.5) / ss) / n, (y + (sy + 0.5) / ss) / n)
                    a = c[3]
                    acc[0] += c[0] * a
                    acc[1] += c[1] * a
                    acc[2] += c[2] * a
                    acc[3] += a
            a = acc[3] / (ss * ss)
            if a > 0:
                row.append((acc[0] / acc[3], acc[1] / acc[3], acc[2] / acc[3], a))
            else:
                row.append((0, 0, 0, 0))
        px.append(row)
    return px


def png_bytes(px):
    n = len(px)
    raw = bytearray()
    for row in px:
        raw.append(0)
        for r, g, b, a in row:
            raw += bytes((round(r * 255), round(g * 255), round(b * 255), round(a * 255)))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", n, n, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def ico_bytes(images):
    """PNG を入れた ICO（Windows Vista 以降）"""
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries = b""
    data = b""
    for size, png in images:
        entries += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(png), offset + len(data))
        data += png
    return header + entries + data


if __name__ == "__main__":
    sizes = [256, 128, 64, 48, 32, 24, 16]
    # 小さいアイコンほど 1 ピクセルにたくさんサンプルを取る（どの大きさもなめらかに）
    pngs = [(s, png_bytes(render(s, max(3, 768 // s)))) for s in sizes]
    with open(os.path.join(HERE, "Aqua.png"), "wb") as f:
        f.write(pngs[0][1])
    with open(os.path.join(HERE, "Aqua.ico"), "wb") as f:
        f.write(ico_bytes(pngs))
    print("wrote Aqua")
