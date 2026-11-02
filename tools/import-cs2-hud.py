"""Imports the CS2-style HUD art of "CS2 New Hud by SaViMoN" (a CS:S HUD) for the Epicentre HUD.

    python tools/import-cs2-hud.py <unpacked "CS2 New Hud by SaViMoN" folder>

Converts the HUD's DXT5 VTF textures to 32-bit TGA and copies its fonts into
assets/cstrike/resource/epicentre_hud, where client_mini looks for them. Only the standard library.
"""
import os
import shutil
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'assets', 'cstrike', 'resource', 'epicentre_hud')

TEXTURES = {
    'materials/vgui/gfx/cs2vgui/mainhud': [
        'hud_bot', 'hud_bot_tr', 'ico_armor', 'ico_armorhelm', 'ico_bullet', 'ico_buyzone',
        'ico_dolar', 'ico_health', 'topscoreboard',
    ],
    'materials/vgui/gfx/cs2vgui/tabscore': ['logo_ct', 'logo_tr', 'name_ct_team', 'name_tr_team', 'tabscore_bg'],
}
FONTS = ['StratumNo2.ttf', 'CS2weapons.ttf']

FMT_DXT1, FMT_DXT5 = 13, 15


def rgb565(c):
    return ((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31


def color_block(block, out, w, x0, y0, opaque):
    c0, c1, bits = struct.unpack_from('<HHI', block, 0)
    p0, p1 = rgb565(c0), rgb565(c1)
    if c0 > c1 or not opaque:
        p2 = tuple((2 * a + b) // 3 for a, b in zip(p0, p1))
        p3 = tuple((a + 2 * b) // 3 for a, b in zip(p0, p1))
    else:
        p2 = tuple((a + b) // 2 for a, b in zip(p0, p1))
        p3 = (0, 0, 0)
    pal = (p0, p1, p2, p3)
    for i in range(16):
        r, g, b = pal[(bits >> (2 * i)) & 3]
        o = ((y0 + i // 4) * w + x0 + i % 4) * 4
        out[o:o + 3] = bytes((r, g, b))


def alpha_block(block, out, w, x0, y0):
    a0, a1 = block[0], block[1]
    bits = int.from_bytes(block[2:8], 'little')
    if a0 > a1:
        pal = [a0, a1] + [((6 - i) * a0 + (i + 1) * a1) // 7 for i in range(6)]
    else:
        pal = [a0, a1] + [((4 - i) * a0 + (i + 1) * a1) // 5 for i in range(4)] + [0, 255]
    for i in range(16):
        out[((y0 + i // 4) * w + x0 + i % 4) * 4 + 3] = pal[(bits >> (3 * i)) & 7]


def decode(data, w, h, fmt):
    out = bytearray(b'\xff' * (w * h * 4))
    step = 16 if fmt == FMT_DXT5 else 8
    pos = 0
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            block = data[pos:pos + step]
            pos += step
            if fmt == FMT_DXT5:
                alpha_block(block[:8], out, w, bx, by)
                color_block(block[8:], out, w, bx, by, True)
            else:
                color_block(block, out, w, bx, by, False)
    return out


def read_vtf(path):
    d = open(path, 'rb').read()
    if d[:4] != b'VTF\0':
        raise ValueError(f'{path}: not a VTF')
    w, h = struct.unpack_from('<HH', d, 16)
    fmt = struct.unpack_from('<i', d, 52)[0]
    if fmt not in (FMT_DXT1, FMT_DXT5):
        raise ValueError(f'{path}: format {fmt} is not DXT1/DXT5')
    size = max(1, (w + 3) // 4) * max(1, (h + 3) // 4) * (16 if fmt == FMT_DXT5 else 8)
    # mipmaps are stored smallest first, so the full-size image is the file's tail
    return w, h, decode(d[len(d) - size:], w, h, fmt)


def write_tga(path, w, h, rgba):
    header = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, w, h, 32, 0x28)  # top-left origin, 8 alpha bits
    bgra = bytearray(len(rgba))
    bgra[0::4], bgra[1::4], bgra[2::4], bgra[3::4] = rgba[2::4], rgba[1::4], rgba[0::4], rgba[3::4]
    with open(path, 'wb') as f:
        f.write(header + bytes(bgra))


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    src = sys.argv[1]
    os.makedirs(OUT, exist_ok=True)
    for folder, names in TEXTURES.items():
        for name in names:
            w, h, rgba = read_vtf(os.path.join(src, folder, name + '.vtf'))
            write_tga(os.path.join(OUT, name + '.tga'), w, h, rgba)
            print(f'{name}.tga {w}x{h}')
    for font in FONTS:
        shutil.copyfile(os.path.join(src, 'resource', 'fonts', font), os.path.join(OUT, font))
        print(font)


if __name__ == '__main__':
    main()
