# Artwork

`background.png`, `reel_left.png`, `reel_right.png`, `button_bypass_on.png` and `button_power_on.png`
are embedded in the plug-in by CMake. They are generated from `source/panel-original.webp`
(panel artwork generated with Meta AI) by `../tools/prepare_assets.py`:

    pip install opencv-python numpy pillow
    python3 tools/prepare_assets.py assets/source/panel-original.webp assets

Where each control sits on the picture is defined in `../Source/Layout.h`.
