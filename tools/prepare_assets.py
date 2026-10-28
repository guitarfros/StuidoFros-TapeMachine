#!/usr/bin/env python3
"""
Prepare the panel artwork for "StudioFros - Tape Machine".

Input : the original Meta AI panel picture (1920 x 1280), e.g. assets/source/panel-original.webp
Output: assets/*.png, which CMake embeds into the plug-in (see CMakeLists.txt)

    background.png     the panel with everything that must be dynamic removed or switched off:
                       lit LEDs, the title text, scribbles on the VU faces, lit BYPASS/POWER buttons
    reel_left.png      the two reels, cut out as discs so the plug-in can spin them
    reel_right.png
    button_bypass_on.png / button_power_on.png
                       the lit versions of the two big buttons (with their glow)

Everything is drawn on top of these by the plug-in (knobs, needles, LEDs, text), at the positions
in Source/Layout.h. If you change the picture, re-measure those positions and re-run:

    pip install opencv-python numpy pillow
    python3 tools/prepare_assets.py assets/source/panel-original.webp assets
"""
import sys
from pathlib import Path

import cv2
import numpy as np
from PIL import Image

# ------------------------------------------------------------------------------------------------
# Geometry of the original 1920 x 1280 picture (keep in sync with Source/Layout.h)
# ------------------------------------------------------------------------------------------------
LEDS = [(1631, 219), (1631, 295), (1630, 373), (1632, 453), (1632, 530), (1632, 603), (1633, 660), (1634, 722)]
JUNK_GLYPHS = [(1612, 618, 1745, 644), (1612, 678, 1745, 703), (1612, 735, 1662, 756)]  # AI scribbles
PLATE = (762, 72, 1146, 128)                                          # title plate interior (x0, y0, x1, y1)
VU_SCRIBBLES = [(1270, 238, 1312, 257),      # "INPUT" on the right meter (it is the OUTPUT meter)
                (1503, 238, 1540, 257),      # garbled text, top right of the right meter
                (490, 238, 524, 257)]        # garbled text, top right of the left meter
BUTTONS = {"bypass": (968, 1108), "power": (1165, 1108)}
BTN_FACE_R, BTN_SPRITE_R = 70, 106
REELS = {"left": (744.5, 329.0), "right": (1063.0, 329.0)}
REEL_R = 151


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def radial(shape, cx, cy):
    yy, xx = np.mgrid[0:shape[0], 0:shape[1]].astype(np.float32)
    return np.hypot(xx - cx, yy - cy)


def fill_masked(img, mask, rng, sigma=12.0, grain=3.5):
    """Replace the masked pixels by a smooth continuation of their surroundings plus fine grain.
    (Unlike cv2.inpaint this does not smear the colour of a glow into the result.)"""
    h, w = img.shape[:2]
    m = (mask > 0).astype(np.float32)
    f = img.astype(np.float32)
    num = cv2.GaussianBlur(f * (1 - m)[..., None], (0, 0), sigma)
    den = cv2.GaussianBlur(1 - m, (0, 0), sigma)[..., None]
    est = num / np.maximum(den, 1e-3)
    n = cv2.GaussianBlur(rng.normal(0, 1, (h, w)).astype(np.float32), (0, 0), 1.0) * grain * 2.2
    est = est + n[..., None]
    alpha = np.clip(cv2.GaussianBlur(m, (0, 0), 3.0) * 1.5, 0, 1)[..., None]
    return np.clip(f * (1 - alpha) + est * alpha, 0, 255).astype(np.uint8)


def clean_plate(img, rng):
    """Remove the (truncated) title text: rebuild the plate from its per-row median."""
    x0, y0, x1, y1 = PLATE
    band = img[y0:y1, x0:x1].astype(np.float32)
    prof = np.median(band, axis=1, keepdims=True)
    smooth = cv2.GaussianBlur(img, (0, 0), 28)[y0:y1, x0:x1].astype(np.float32)
    fill = prof + (smooth - smooth.mean(axis=1, keepdims=True)) * 0.6
    n = rng.normal(0, 5.0, (y1 - y0, x1 - x0)).astype(np.float32)
    n = cv2.GaussianBlur(n, (0, 0), sigmaX=6, sigmaY=0.6)[..., None] * 3.0
    fill = fill + n
    a = np.zeros((y1 - y0, x1 - x0), np.float32)
    a[4:-4, 10:-10] = 1
    a = cv2.GaussianBlur(a, (0, 0), 4)[..., None]
    out = img.copy()
    out[y0:y1, x0:x1] = np.clip(band * (1 - a) + fill * a, 0, 255).astype(np.uint8)
    return out


def clean_led_column(img):
    """Switch every LED off and remove the glow, the scribbles under the speed LEDs and the green
    tint of the "15" / "30 ips" labels. The plug-in draws the (dynamic) LEDs itself.

    Glow only ever *adds* light, so a morphological opening per colour channel gives the plate
    underneath; the fine grain is taken from the original, the printed labels are kept as they are."""
    h, w = img.shape[:2]
    x0, x1, y0, y1 = 1609, 1762, 170, 772   # starts at the LED plate, not on the neighbouring panel
    f = img.astype(np.float32)
    k = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (111, 111))
    floor = np.dstack([cv2.GaussianBlur(cv2.morphologyEx(f[..., c], cv2.MORPH_OPEN, k), (0, 0), 9) for c in range(3)])
    # the opening is the *minimum* envelope (too dark): let the plate rise a little above it
    base = cv2.GaussianBlur(np.minimum(f, floor + 11), (0, 0), 6)
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY).astype(np.float32)
    grain = np.clip(gray - cv2.GaussianBlur(gray, (0, 0), 2.5), -10, 10)
    for (a, b, c, d) in JUNK_GLYPHS:
        grain[b - 6:d + 6, a - 6:c + 6] = 0
    plate = base + grain[..., None]

    # printed text to keep: bright pixels right of the LEDs, minus the scribbles
    keep = np.zeros((h, w), np.float32)
    keep[y0:y1, 1645:x1] = np.clip((gray[y0:y1, 1645:x1] - 110) / 70.0, 0, 1)
    for (a, b, c, d) in JUNK_GLYPHS:
        keep[b:d, a:c] = 0
    keep = cv2.GaussianBlur(keep, (0, 0), 0.5)[..., None]
    white = np.repeat(f.max(axis=2, keepdims=True), 3, axis=2)         # strips the green tint of "15"/"30 ips"
    white = np.minimum(white, 240)
    result = plate * (1 - keep) + white * keep

    zone = np.zeros((h, w), np.float32)
    zone[y0:y1, x0:x1] = 1
    zone = cv2.GaussianBlur(zone, (0, 0), sigmaX=3.0, sigmaY=14.0)[..., None]   # soft top/bottom, crisp left edge
    return np.clip(f * (1 - zone) + result * zone, 0, 255).astype(np.uint8)


def remove_glow_spill(img):
    """Green / red light of the LEDs leaks a little over the rim onto the neighbouring panel:
    take the colour out of that strip (only strongly green or red pixels, so rust is kept)."""
    x0, x1, y0, y1 = 1570, 1612, 150, 780
    f = img.astype(np.float32)
    reg = f[y0:y1, x0:x1]
    b, g, r = reg[..., 0], reg[..., 1], reg[..., 2]
    green = np.clip((g - np.maximum(r, b) - 4.0) / 14.0, 0, 1)
    red = np.clip((r - np.maximum(g, b) - 24.0) / 20.0, 0, 1)
    k = np.maximum(green, red)[..., None]
    grey = reg.mean(axis=2, keepdims=True) * np.ones((1, 1, 3), np.float32)
    fade = np.ones(x1 - x0, np.float32)
    fade[-6:] = np.linspace(1, 0, 6)                     # do not leave a seam at the LED plate
    k = k * fade[None, :, None]
    f[y0:y1, x0:x1] = reg * (1 - k) + grey * k
    return np.clip(f, 0, 255).astype(np.uint8)


def clean_vu(img, rng):
    h, w = img.shape[:2]
    m = np.zeros((h, w), np.uint8)
    for (a, b, c, d) in VU_SCRIBBLES:
        cv2.rectangle(m, (a, b), (c, d), 255, -1)
    return fill_masked(img, m, rng, sigma=7.0, grain=2.0)


def button_off(img, cx, cy):
    """Same button, unlit: dark lens, dim legend, no glow."""
    h, w = img.shape[:2]
    d = radial((h, w), cx, cy)
    hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV).astype(np.float32)
    v = hsv[..., 2]
    face_v = np.median(v[d < 48])

    # dark lens
    dim = hsv.copy()
    dim[..., 1] *= 0.70
    dim[..., 2] *= 0.20
    dim_bgr = cv2.cvtColor(dim.astype(np.uint8), cv2.COLOR_HSV2BGR).astype(np.float32)

    # legend: dark letters on the lit lens -> pale, dim letters on the dark lens
    letters = ((v < 0.62 * face_v) & (d < BTN_FACE_R - 6)).astype(np.float32)
    letters = cv2.GaussianBlur(letters, (0, 0), 0.9)[..., None]
    legend = np.array([140, 158, 175], np.float32)          # warm grey (BGR)
    face = dim_bgr * (1 - letters) + legend * letters * 0.55 + dim_bgr * letters * 0.45
    face_a = (1 - smoothstep(BTN_FACE_R - 5, BTN_FACE_R + 1, d))[..., None]

    # glow ring outside the lens: remove the colour cast and a little brightness
    grey = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY).astype(np.float32)[..., None]
    halo_a = (smoothstep(BTN_FACE_R - 4, BTN_FACE_R + 4, d) * (1 - smoothstep(BTN_SPRITE_R - 14, BTN_SPRITE_R, d)))[..., None]
    halo = np.repeat(grey, 3, axis=2) * 0.80
    base = img.astype(np.float32)
    out = base * (1 - halo_a) + halo * halo_a
    out = out * (1 - face_a) + face * face_a
    return np.clip(out, 0, 255).astype(np.uint8)


def disc_sprite(img, cx, cy, radius, feather):
    """Cut a disc (centre may be fractional) out of the picture as a BGRA image."""
    size = int(2 * (radius + 2))
    x0 = int(round(cx - size / 2))
    y0 = int(round(cy - size / 2))
    crop = img[y0:y0 + size, x0:x0 + size]
    d = radial(crop.shape, cx - x0, cy - y0)
    alpha = 1 - smoothstep(radius - feather, radius, d)
    bgra = np.dstack([crop, (alpha * 255).astype(np.uint8)])
    return bgra, (x0, y0)


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    src, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)

    rgb = np.array(Image.open(src).convert("RGB"))
    img = cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR)
    assert img.shape[:2] == (1280, 1920), f"expected 1920x1280, got {img.shape[1]}x{img.shape[0]}"
    rng = np.random.default_rng(5)

    bg = clean_plate(img, rng)
    bg = clean_led_column(bg)
    bg = remove_glow_spill(bg)
    bg = clean_vu(bg, rng)

    # lit buttons -> sprites, unlit versions -> background
    for name, (cx, cy) in BUTTONS.items():
        sprite, pos = disc_sprite(bg, cx, cy, BTN_SPRITE_R, 12)
        cv2.imwrite(str(out_dir / f"button_{name}_on.png"), sprite)
        print(f"button_{name}_on.png  top-left in panel: {pos}")
        bg = button_off(bg, cx, cy)

    # reels -> sprites (taken from the cleaned picture; the static reel stays in the background)
    for name, (cx, cy) in REELS.items():
        sprite, pos = disc_sprite(bg, cx, cy, REEL_R, 1.5)
        cv2.imwrite(str(out_dir / f"reel_{name}.png"), sprite)
        print(f"reel_{name}.png  top-left in panel: {pos}, size {sprite.shape[1]}x{sprite.shape[0]}")

    cv2.imwrite(str(out_dir / "background.png"), bg, [cv2.IMWRITE_PNG_COMPRESSION, 9])
    print("background.png written")


if __name__ == "__main__":
    main()
