"""Launcher icon generator (JKEngine) — docs/106 §3 후속 / 2026-11-12.

기존 128px 아트(테트리스·지뢰찾기·브라우저)와 같은 시각 문법:
  - 투명 모서리 + 둥근 카드(세로 그라데이션 #2B3040→#1F232C)
  - 중앙 글리프 = 앱 의미(단색/2색)
동일한 카드 위에 앱별 글리프만 바꿔 "같은 아이콘 여러 개" 문제를 없앤다.
실행: python tools/icons/gen_launcher_icons.py  (engine/ 루트에서)
산출: assets/icons/launcher_<name>@{1x,2x}.png
"""
import math
import os
from PIL import Image, ImageDraw

OUT = os.path.join("assets", "icons")
SS = 4          # 슈퍼샘플링 배율
RADIUS = 0.20   # 카드 모서리(변 대비)
CARD_INSET = 0.03


def card(size):
    s = size * SS
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    inset = int(s * CARD_INSET)
    r = int(s * RADIUS)
    # 세로 그라데이션 카드
    top = (43, 48, 64)
    bot = (31, 35, 46)
    inner = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    di = ImageDraw.Draw(inner)
    di.rounded_rectangle([inset, inset, s - inset - 1, s - inset - 1],
                         radius=r, fill=top + (255,))
    grad = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    gd = ImageDraw.Draw(grad)
    for y in range(s):
        t = y / max(1, s - 1)
        col = tuple(int(top[i] + (bot[i] - top[i]) * t) for i in range(3))
        gd.line([(0, y), (s, y)], fill=col + (255,))
    mask = Image.new("L", (s, s), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        [inset, inset, s - inset - 1, s - inset - 1], radius=r, fill=255)
    img.paste(grad, (0, 0), mask)
    # 상단 하이라이트(1px 느낌)
    hl = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    ImageDraw.Draw(hl).rounded_rectangle(
        [inset, inset, s - inset - 1, s - inset - 1], radius=r,
        outline=(255, 255, 255, 26), width=max(1, s // 96))
    img = Image.alpha_composite(img, hl)
    return img, d


def glyph(d, s, name):
    """d = supersampled draw, s = supersampled size."""
    u = s / 128.0            # 128-grid → 실제 px
    def P(x):
        return x * u
    def box(x0, y0, x1, y1, color, r=0):
        if r:
            d.rounded_rectangle([P(x0), P(y0), P(x1), P(y1)],
                                radius=P(r), fill=color)
        else:
            d.rectangle([P(x0), P(y0), P(x1), P(y1)], fill=color)

    if name == "files":
        amber = (242, 181, 68, 255)
        box(24, 38, 66, 52, amber, 6)
        box(24, 46, 104, 96, amber, 8)
        box(24, 46, 104, 58, (255, 205, 112, 255), 6)
        box(34, 66, 94, 74, (120, 78, 18, 255), 4)
    elif name == "notes":
        c = (242, 214, 92, 255)
        box(32, 24, 96, 104, (245, 245, 245, 255), 8)
        box(32, 24, 96, 40, c, 8)
        for y in (54, 68, 82):
            box(42, y, 86, y + 6, (150, 150, 160, 255), 3)
        d.polygon([(P(96), P(24)), (P(110), P(38)), (P(96), P(38))],
                  fill=(210, 210, 218, 255))
    elif name == "settings":
        c = (127, 180, 255, 255)
        cx = cy = 64
        for i in range(8):
            a = math.radians(i * 45)
            x0 = cx + math.cos(a) * 30 - 7
            y0 = cy + math.sin(a) * 30 - 7
            box(x0, y0, x0 + 14, y0 + 14, c, 3)
        d.ellipse([P(30), P(30), P(98), P(98)], fill=c)
        d.ellipse([P(50), P(50), P(78), P(78)], fill=(31, 35, 46, 255))
    elif name == "snap":
        c = (79, 209, 197, 255)
        box(22, 42, 106, 96, c, 10)
        box(46, 30, 82, 46, c, 6)
        d.ellipse([P(44), P(52), P(84), P(92)], fill=(24, 28, 36, 255))
        d.ellipse([P(52), P(60), P(76), P(84)], fill=c)
        box(84, 50, 96, 58, (24, 28, 36, 255), 3)
    elif name == "lf":
        c = (58, 210, 159, 255)
        box(20, 28, 62, 100, c, 8)
        box(66, 28, 108, 100, (44, 52, 66, 255), 8)
        d.rectangle([P(66), P(28), P(108), P(100)], outline=c, width=int(P(5)))
        for y in (40, 50, 60):
            box(27, y, 55, y + 5, (20, 60, 45, 255), 2)
        box(74, 44, 100, 50, c, 2)
        box(74, 56, 94, 62, c, 2)
    elif name == "helix":
        c = (176, 122, 255, 255)
        # 연필(대각) — 편집기 상징
        d.polygon([(P(32), P(94)), (P(40), P(64)), (P(88), P(26)),
                   (P(102), P(42)), (P(64), P(92))], fill=c)
        d.polygon([(P(32), P(94)), (P(40), P(64)), (P(56), P(80))],
                  fill=(255, 255, 255, 255))
        box(30, 96, 58, 102, (44, 52, 66, 255), 3)
    elif name == "sampletodo":
        c = (107, 224, 139, 255)
        box(26, 24, 102, 104, (245, 245, 245, 255), 10)
        for i, y in enumerate((40, 60, 80)):
            d.rectangle([P(36), P(y), P(50), P(y + 14)],
                        outline=(120, 120, 130, 255), width=int(P(3)))
            box(58, y + 4, 92, y + 10, (150, 150, 160, 255), 3)
        # 첫 항목 체크
        d.line([(P(37), P(47)), (P(42), P(53)), (P(50), P(41))],
               fill=c, width=int(P(5)))
    elif name == "chat":
        c = (90, 162, 255, 255)
        d.rounded_rectangle([P(20), P(26), P(108), P(86)], radius=P(20),
                            fill=c)
        d.polygon([(P(40), P(84)), (P(36), P(106)), (P(66), P(84))], fill=c)
        for x in (42, 62, 82):
            d.ellipse([P(x), P(50), P(x + 12), P(62)], fill=(18, 30, 52, 255))
    elif name == "music":
        c = (255, 111, 181, 255)
        box(78, 26, 86, 78, c, 3)
        d.ellipse([P(56), P(70), P(86), P(92)], fill=c)
        d.polygon([(P(78), P(26)), (P(104), P(32)), (P(104), P(44)),
                   (P(78), P(38))], fill=c)
        box(96, 32, 104, 44, c, 2)
    elif name == "library":
        c = (255, 164, 91, 255)
        box(26, 30, 48, 100, (224, 106, 72, 255), 5)
        box(52, 26, 74, 100, c, 5)
        box(78, 34, 100, 100, (255, 205, 122, 255), 5)
        box(30, 40, 44, 48, (255, 255, 255, 90), 2)
        box(56, 36, 70, 44, (255, 255, 255, 90), 2)
        box(82, 44, 96, 52, (255, 255, 255, 90), 2)
    elif name == "palette":
        cols = [(255, 95, 95, 255), (255, 196, 87, 255),
                (107, 224, 139, 255), (90, 162, 255, 255)]
        pos = [(30, 30), (66, 30), (30, 66), (66, 66)]
        for (x, y), c in zip(pos, cols):
            d.ellipse([P(x), P(y), P(x + 32), P(y + 32)], fill=c)
        d.ellipse([P(48), P(48), P(80), P(80)], fill=(31, 35, 46, 255))
        d.ellipse([P(50), P(50), P(78), P(78)], fill=(255, 255, 255, 60))
    elif name == "script":
        c = (124, 227, 139, 255)
        box(20, 30, 108, 98, (44, 52, 66, 255), 10)
        d.rectangle([P(20), P(30), P(108), P(98)], outline=c, width=int(P(4)))
        box(28, 40, 46, 44, (90, 162, 255, 255), 2)
        d.line([(P(36), P(56)), (P(52), P(70))], fill=c, width=int(P(6)))
        d.line([(P(52), P(56)), (P(36), P(70))], fill=c, width=int(P(6)))
        box(60, 66, 92, 72, c, 2)
    else:
        raise SystemExit("unknown glyph: " + name)


def render(name, size):
    img, _ = card(size)
    # 주의: card() 내부에서 alpha_composite로 새 이미지를 만들기 때문에 초기
    # ImageDraw는 최종 이미지에 붙지 않는다(글리프 소실 함정) — 최종 이미지
    # 위에 다시 Draw를 만들어 그린다.
    d = ImageDraw.Draw(img)
    glyph(d, size * SS, name)
    return img.resize((size, size), Image.LANCZOS)


ICONS = ["files", "notes", "settings", "snap", "lf", "helix",
         "sampletodo", "chat", "music", "library", "palette", "script"]


def main():
    os.makedirs(OUT, exist_ok=True)
    for name in ICONS:
        for suffix, size in (("1x", 64), ("2x", 128)):
            path = os.path.join(OUT, f"launcher_{name}@{suffix}.png")
            render(name, size).save(path)
            print("wrote", path)


if __name__ == "__main__":
    main()
