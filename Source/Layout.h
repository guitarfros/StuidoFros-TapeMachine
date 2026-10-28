#pragma once

// ------------------------------------------------------------------------------------------------
// Panel layout, in pixels of the 1920 x 1280 artwork (assets/background.png).
//
// Everything the plug-in draws or lets you click sits at a position measured on that picture.
// The editor scales it all to the real window size. If you change the artwork, this file and
// tools/prepare_assets.py are the only places that need new numbers.
// ------------------------------------------------------------------------------------------------
namespace Layout
{
    constexpr float designWidth  = 1920.0f;
    constexpr float designHeight = 1280.0f;

    struct Point { float x, y; };
    struct Rect  { float x, y, w, h; };

    constexpr Rect centred (float cx, float cy, float w, float h) { return { cx - w * 0.5f, cy - h * 0.5f, w, h }; }

    // ---- Knobs -------------------------------------------------------------------------------
    // Centres of the 15 dark sockets in the picture, in the order Input, Drive | Bias, Sat, Bump,
    // Tone, Loss, Comp | Wow, Flutter, Drift, Degrade, Hiss | Output, Dry/Wet.
    // The last two have their caption printed on the panel already.
    struct KnobSlot { float x, y; bool captionPrinted; };
    constexpr KnobSlot knobs[15] = {
        {  250.0f,  620.0f, false }, {  411.0f,  620.0f, false },
        {  610.0f,  622.0f, false }, {  768.0f,  622.0f, false }, {  930.0f,  622.0f, false },
        { 1097.0f,  622.0f, false }, { 1259.0f,  622.0f, false }, { 1427.0f,  622.0f, false },
        {  614.0f,  858.0f, false }, {  783.0f,  858.0f, false }, {  952.0f,  858.0f, false },
        { 1123.0f,  858.0f, false }, { 1297.0f,  858.0f, false },
        {  611.0f, 1090.0f, true  }, {  777.0f, 1090.0f, true  },
    };
    constexpr float knobSize     = 164.0f;   // knob square; its cap (0.66 of the radius) covers the socket
    constexpr float knobCaptionH = 36.0f;    // caption strip under the knob

    // ---- Selector levers (left panel; their names are printed to the right of each) ----------
    constexpr float selectorW = 130.0f, selectorH = 104.0f;
    constexpr Rect selectorSpeed = centred (245.0f,  808.0f, selectorW, selectorH);
    constexpr Rect selectorType  = centred (245.0f,  945.0f, selectorW, selectorH);
    constexpr Rect selectorEq    = centred (245.0f, 1072.0f, selectorW, selectorH);

    // ---- Lever switches (right of the third knob row) ----------------------------------------
    constexpr Rect leverHiss = centred (1625.0f, 815.0f, 190.0f, 64.0f);
    constexpr Rect leverLink = centred (1625.0f, 905.0f, 190.0f, 64.0f);

    // ---- Big lit buttons (sprites cut from the artwork) --------------------------------------
    constexpr Point bypassCentre { 968.0f, 1108.0f };
    constexpr Point powerCentre  { 1165.0f, 1108.0f };
    constexpr float buttonSprite = 216.0f;       // size of button_*_on.png
    constexpr float buttonRadius = 82.0f;        // clickable radius

    // ---- Preset keys under the right-hand side ------------------------------------------------
    constexpr float keySize = 72.0f;
    constexpr Rect keySave   = centred ( 1361.0f, 1090.0f, keySize, keySize);
    constexpr Rect keySaveAs = centred ( 1464.0f, 1090.0f, keySize, keySize);
    constexpr Rect keyDelete = centred ( 1567.0f, 1090.0f, keySize, keySize);
    constexpr Rect keyInit   = centred ( 1672.0f, 1090.0f, keySize, keySize);

    // ---- LEDs --------------------------------------------------------------------------------
    // POWER, SIGNAL, PEAK/CLIP, BYPASS, HISS, 7.5, 15, 30
    constexpr Point leds[8] = {
        { 1631.0f, 219.0f }, { 1631.0f, 295.0f }, { 1630.0f, 373.0f }, { 1632.0f, 453.0f },
        { 1632.0f, 530.0f }, { 1632.0f, 603.0f }, { 1633.0f, 660.0f }, { 1634.0f, 722.0f },
    };
    constexpr float ledSize = 64.0f;

    // ---- VU meters -----------------------------------------------------------------------------
    // The "window" is the cream face; the needle pivots below it (behind the dark cap) and is
    // clipped to the window. Scale geometry measured on the printed arc.
    constexpr Rect vuWindowIn  { 243.0f,  217.0f, 294.0f, 161.0f };
    constexpr Rect vuWindowOut { 1263.0f, 217.0f, 290.0f, 161.0f };
    constexpr Point vuPivotIn  { 388.0f,  436.0f };
    constexpr Point vuPivotOut { 1407.0f, 436.0f };
    constexpr Point vuCapIn    { 388.0f,  410.0f };    // dark half-disc at the bottom of the face
    constexpr Point vuCapOut   { 1408.0f, 410.0f };
    constexpr float vuCapRadius   = 53.0f;
    constexpr float vuNeedleLen   = 172.0f;
    constexpr float vuHalfAngle   = 0.707f;            // radians either side of vertical (+-40.5 deg)
    constexpr float vuRedStart    = 0.64f;             // where the printed red zone starts (0..1 of the arc)
    constexpr Rect  vuOutputLabel { 1277.0f, 241.0f, 60.0f, 13.0f };   // replaces the printed "INPUT"

    // ---- Reels (sprites cut from the artwork; pivot = centre of the disc) ----------------------
    constexpr Point reelLeftCentre  { 744.5f, 329.0f };
    constexpr Point reelRightCentre { 1063.0f, 329.0f };
    constexpr Point reelLeftSpriteTopLeft  { 592.0f, 176.0f };
    constexpr Point reelRightSpriteTopLeft { 910.0f, 176.0f };
    constexpr float reelSprite = 306.0f;

    // ---- Title plate (click it to open the preset menu) -----------------------------------------------
    constexpr Rect titlePlate { 740.0f, 64.0f, 420.0f, 72.0f };

    // ---- Preset window with its arrows, above the SAVE / SAVE AS / DELETE / INIT keys ---------
    constexpr Rect presetPrev   { 1325.0f, 1008.0f,  52.0f, 48.0f };
    constexpr Rect presetWindow { 1385.0f, 1008.0f, 272.0f, 48.0f };
    constexpr Rect presetNext   { 1665.0f, 1008.0f,  52.0f, 48.0f };
}
