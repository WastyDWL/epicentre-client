"""Imports the CS2-style HUD art of "CS2 New Hud by SaViMoN" (a CS:S HUD) for the Epicentre HUD.

    python tools/import-cs2-hud.py <unpacked "CS2 New Hud by SaViMoN" folder>
    python tools/import-cs2-hud.py --kill-sounds <"Kill Overlay Manager" folder>
    python tools/import-cs2-hud.py --kill-cards <"Kill Overlay Manager" folder>

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


SOUND_OUT = os.path.join(ROOT, 'assets', 'cstrike', 'sound', 'epicentre')


def to_goldsrc_wav(src, dst):
    """16-bit PCM WAV of any channel count at 44.1 kHz -> mono 22.05 kHz, which GoldSrc plays."""
    d = open(src, 'rb').read()
    pos, fmt, data = 12, None, None
    while pos + 8 <= len(d):
        cid, size = d[pos:pos + 4], struct.unpack_from('<I', d, pos + 4)[0]
        body = d[pos + 8:pos + 8 + size]
        if cid == b'fmt ':
            fmt = struct.unpack_from('<HHIIHH', body, 0)
        elif cid == b'data':
            data = body
        pos += 8 + size + (size & 1)
    tag, channels, rate, _, _, bits = fmt
    if tag != 1 or bits != 16:
        raise ValueError(f'{src}: only 16-bit PCM is converted')
    samples = struct.unpack(f'<{len(data) // 2}h', data[:len(data) // 2 * 2])
    mono = [sum(samples[i:i + channels]) // channels for i in range(0, len(samples) - channels + 1, channels)]
    step = max(1, rate // 22050)
    out = [sum(mono[i:i + step]) // step for i in range(0, len(mono) - step + 1, step)]
    pcm = struct.pack(f'<{len(out)}h', *out)
    header = b'RIFF' + struct.pack('<I', 36 + len(pcm)) + b'WAVE' + b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 1, rate // step, rate // step * 2, 2, 16)
    with open(dst, 'wb') as f:
        f.write(header + b'data' + struct.pack('<I', len(pcm)) + pcm)


def import_kill_sounds(kill_overlay_dir):
    """Kill Overlay Manager's sounds -> assets/cstrike/sound/epicentre/{kill,headshot}.wav"""
    os.makedirs(SOUND_OUT, exist_ok=True)
    src = os.path.join(kill_overlay_dir, 'sound', 'cs2_kills_overlays')
    for name, out in (('kill1.wav', 'kill.wav'), ('headshot.wav', 'headshot.wav')):
        to_goldsrc_wav(os.path.join(src, name), os.path.join(SOUND_OUT, out))
        print(f'sound/epicentre/{out}')


CARD_STYLES = {'cs2': 'kill_overlay', 'classic': 'style2'}  # style3 repeats kill_overlay


def import_kill_cards(kill_overlay_dir):
    """Kill Overlay Manager's full-screen overlays -> the divider strip with 0..5 kill cards.

    Each <team>_<n>.vtf is a whole-screen overlay (n = kills + 1) of which only the bottom divider is
    visible; the strip is cut out (one box per style) and cards.txt records, per style, the texture's
    screen stretch (overlays fill the screen, so a 2048x2048 one is squeezed to 16:9) and the row of
    the divider line, so the HUD can line it up with its own.
    """
    out_dir = os.path.join(OUT, 'cards')
    os.makedirs(out_dir, exist_ok=True)
    meta = []
    for style, folder in CARD_STYLES.items():
        frames = {}
        box = None
        for team in ('ct', 'tt'):
            for n in range(1, 7):
                w, h, rgba = read_vtf(os.path.join(kill_overlay_dir, 'materials', 'overlays', 'moder_shop', folder, team, f'{team}_{n}.vtf'))
                frames[(team, n)] = (w, h, rgba)
                for y in range(h):
                    row = y * w * 4
                    for x in range(0, w):
                        if rgba[row + x * 4 + 3] > 8:
                            box = [min(box[0], x), min(box[1], y), max(box[2], x), max(box[3], y)] if box else [x, y, x, y]
        x0, y0, x1, y1 = box[0], box[1], box[2] + 1, box[3] + 1
        cw, ch = x1 - x0, y1 - y0
        # the divider line: the most opaque row near the strip's left end, in the no-kill frame
        w, h, rgba = frames[('ct', 1)]
        line = max(range(ch), key=lambda y: sum(rgba[((y0 + y) * w + x) * 4 + 3] for x in range(x0 + 4, x0 + 40)))
        for (team, n), (w, h, rgba) in frames.items():
            crop = bytearray()
            for y in range(y0, y1):
                crop += rgba[(y * w + x0) * 4:(y * w + x1) * 4]
            write_tga(os.path.join(out_dir, f'{style}_{team}_{n}.tga'), cw, ch, crop)
        meta.append(f'{style} {w} {h} {cw} {ch} {line}')
        print(f'{style}: {cw}x{ch} from {w}x{h}, line at {line}')
    with open(os.path.join(out_dir, 'cards.txt'), 'w') as f:
        f.write('# style texture_w texture_h strip_w strip_h line_row\n' + '\n'.join(meta) + '\n')


def main():
    if len(sys.argv) == 3 and sys.argv[1] == '--kill-sounds':
        return import_kill_sounds(sys.argv[2])
    if len(sys.argv) == 3 and sys.argv[1] == '--kill-cards':
        return import_kill_cards(sys.argv[2])
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
