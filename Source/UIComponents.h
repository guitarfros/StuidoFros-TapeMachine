#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>
#include "Layout.h"

// Controls drawn on top of the artwork (assets/background.png): knobs, switches, lit buttons, LEDs,
// VU needles, spinning reels and the preset keys. They are vector-drawn or cut from the artwork, so they
// scale to any window size. Positions come from Source/Layout.h.
namespace ui
{
namespace colours
{
    inline const juce::Colour cream      { 0xffe6dfca };
    inline const juce::Colour printed    { 0xffd9d9d4 };   // colour of the lettering printed on the panel
    inline const juce::Colour creamDim   { 0xff8f8a78 };
    inline const juce::Colour amber      { 0xffe9a53a };
    inline const juce::Colour green      { 0xff5fe06e };
    inline const juce::Colour red        { 0xffff4b3f };
    inline const juce::Colour metalLight { 0xff80858d };
    inline const juce::Colour metalDark  { 0xff17181b };
    inline const juce::Colour patch      { 0xff1b1c1e };   // matches the dark panel; hides printed text under read-outs
}

inline juce::Font makeFont (float height, bool bold = true)
{
    return juce::Font (juce::FontOptions (juce::jmax (6.0f, height), bold ? juce::Font::bold : juce::Font::plain));
}

//==================================================================================================
class TapeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& slider) override
    {
        using namespace colours;
        const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float angle = a0 + pos * (a1 - a0);

        // scale ticks (on the panel, around the socket)
        const int ticks = 11;
        for (int i = 0; i < ticks; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / (float) (ticks - 1);
            const float sx = std::sin (a), sy = -std::cos (a);
            const bool major = (i == 0 || i == ticks - 1 || i == (ticks - 1) / 2);
            g.setColour (printed.withAlpha (major ? 0.85f : 0.5f));
            g.drawLine (c.x + sx * r * 0.80f, c.y + sy * r * 0.80f,
                        c.x + sx * r * 0.98f, c.y + sy * r * 0.98f, juce::jmax (1.0f, r * (major ? 0.030f : 0.020f)));
        }

        // value arc
        if (slider.isEnabled())
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, r * 0.74f, r * 0.74f, 0.0f, a0, angle, true);
            g.setColour (amber.withAlpha (0.9f));
            g.strokePath (arc, juce::PathStrokeType (juce::jmax (1.5f, r * 0.045f),
                                                      juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // knob body: sits in the dark socket of the artwork
        const float br = r * 0.66f;
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (c.x - br, c.y - br + br * 0.07f, br * 2.0f, br * 2.0f);

        g.setGradientFill (juce::ColourGradient (metalLight, c.x - br * 0.6f, c.y - br * 0.9f,
                                                 metalDark, c.x + br * 0.7f, c.y + br * 0.95f, false));
        g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);

        // knurled rim
        g.setColour (juce::Colours::black.withAlpha (0.38f));
        for (int i = 0; i < 44; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 44.0f;
            g.drawLine (c.x + std::sin (a) * br * 0.89f, c.y - std::cos (a) * br * 0.89f,
                        c.x + std::sin (a) * br * 0.99f, c.y - std::cos (a) * br * 0.99f, juce::jmax (1.0f, br * 0.025f));
        }
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.2f);

        const float cr = br * 0.78f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3e44), c.x - cr * 0.5f, c.y - cr * 0.8f,
                                                 juce::Colour (0xff0f1012), c.x + cr * 0.6f, c.y + cr, false));
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);

        // pointer
        juce::Path p;
        p.addRoundedRectangle (-r * 0.040f, -br * 0.90f, r * 0.080f, br * 0.54f, r * 0.035f);
        p.applyTransform (juce::AffineTransform::rotation (angle).translated (c.x, c.y));
        g.setColour (cream);
        g.fillPath (p);
    }
};

//==================================================================================================
// A slider that only reacts inside its circle: the knob squares of neighbouring knobs overlap slightly.
class RoundSlider : public juce::Slider
{
public:
    bool hitTest (int x, int y) override
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        return c.getDistanceFrom ({ (float) x, (float) y }) <= (float) juce::jmin (getWidth(), getHeight()) * 0.47f;
    }
};

//==================================================================================================
// Rotary knob with a caption under it. While hovering/dragging the caption turns into the value.
// Knobs whose caption is already printed on the panel get a dark patch behind the read-out.
class RackKnob : public juce::Component
{
public:
    RackKnob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
              const juce::String& caption, bool captionAlreadyPrinted)
        : text (caption), printed (captionAlreadyPrinted), param (apvts.getParameter (paramId)),
          attachment (apvts, paramId, slider)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (180);
        slider.setScrollWheelEnabled (true);
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        slider.onValueChange = [this] { repaint(); };
        slider.addMouseListener (this, false);
        addAndMakeVisible (slider);
    }

    ~RackKnob() override { slider.removeMouseListener (this); }

    void resized() override
    {
        slider.setBounds (0, 0, getWidth(), getWidth());   // knob square; the caption strip is below it
    }

    void paint (juce::Graphics& g) override
    {
        auto strip = getLocalBounds().toFloat().withTrimmedTop ((float) getWidth());
        const bool showValue = slider.isMouseOverOrDragging (true) && param != nullptr;
        if (! showValue && printed)
            return;

        g.setFont (makeFont (strip.getHeight() * 0.62f));
        const auto label = showValue ? param->getCurrentValueAsText().toUpperCase() : text.toUpperCase();
        if (showValue && printed)
        {
            g.setColour (colours::patch);
            g.fillRoundedRectangle (strip.reduced (strip.getWidth() * 0.10f, strip.getHeight() * 0.04f), 4.0f);
        }
        g.setColour (showValue ? colours::amber : colours::printed);
        g.drawFittedText (label, strip.toNearestInt(), juce::Justification::centred, 1, 0.8f);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    RoundSlider slider;

private:
    juce::String text;
    bool printed;
    juce::RangedAudioParameter* param;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

//==================================================================================================
// N-position lever (speed / tape type / EQ). The names are printed on the panel, so this only shows
// the notches, the lever and the current choice as a read-out.
class SelectorSwitch : public juce::Component
{
public:
    SelectorSwitch (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
                    juce::StringArray readoutTexts)
        : readouts (std::move (readoutTexts)), param (apvts.getParameter (paramId))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
        {
            index = juce::jlimit (0, readouts.size() - 1, juce::roundToInt (v));
            repaint();
        });
        attachment->sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace colours;
        const auto r = getLocalBounds().toFloat();
        const float w = r.getWidth(), h = r.getHeight();
        const int n = readouts.size();
        const float slotY = h * 0.38f, slotH = h * 0.15f;

        // notch dots
        const float dr = h * 0.032f;
        for (int i = 0; i < n; ++i)
        {
            g.setColour (i == index ? amber : creamDim.withAlpha (0.6f));
            g.fillEllipse (notchX (i) - dr, slotY - h * 0.25f - dr, dr * 2.0f, dr * 2.0f);
        }

        // slot
        const float x0 = notchX (0) - w * 0.09f, x1 = notchX (n - 1) + w * 0.09f;
        const auto slot = juce::Rectangle<float> (x0, slotY - slotH * 0.5f, x1 - x0, slotH);
        g.setColour (juce::Colours::black.withAlpha (0.9f));
        g.fillRoundedRectangle (slot, slotH * 0.5f);
        g.setColour (printed.withAlpha (0.22f));
        g.drawRoundedRectangle (slot, slotH * 0.5f, 1.0f);

        // lever
        const float lw = w * 0.17f, lh = h * 0.40f;
        const auto lever = juce::Rectangle<float> (lw, lh).withCentre ({ notchX (index), slotY });
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (lever.translated (0.0f, lh * 0.08f), lw * 0.3f);
        g.setGradientFill (juce::ColourGradient (metalLight, lever.getX(), lever.getY(),
                                                 metalDark, lever.getRight(), lever.getBottom(), false));
        g.fillRoundedRectangle (lever, lw * 0.3f);
        g.setColour (cream.withAlpha (0.8f));
        g.drawLine (lever.getX() + lw * 0.2f, slotY, lever.getRight() - lw * 0.2f, slotY, 1.4f);

        // read-out of the current choice
        g.setColour (amber);
        g.setFont (makeFont (h * 0.22f));
        g.drawFittedText (readouts[index].toUpperCase(),
                          juce::Rectangle<float> (0.0f, h * 0.70f, w, h * 0.30f).toNearestInt(),
                          juce::Justification::centred, 1, 0.6f);
    }

    void mouseDown (const juce::MouseEvent& e) override { select (e); }
    void mouseDrag (const juce::MouseEvent& e) override { select (e); }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (std::abs (wheel.deltaY) < 0.01f) return;
        const int next = juce::jlimit (0, readouts.size() - 1, index + (wheel.deltaY > 0.0f ? 1 : -1));
        if (next != index) attachment->setValueAsCompleteGesture ((float) next);
    }

private:
    float notchX (int i) const
    {
        const float w = (float) getWidth();
        const int n = readouts.size();
        if (n <= 1) return w * 0.5f;
        return w * 0.22f + (w * 0.56f) * (float) i / (float) (n - 1);
    }

    void select (const juce::MouseEvent& e)
    {
        int best = 0;
        float bestDist = 1.0e9f;
        for (int i = 0; i < readouts.size(); ++i)
        {
            const float d = std::abs ((float) e.x - notchX (i));
            if (d < bestDist) { bestDist = d; best = i; }
        }
        if (best != index)
            attachment->setValueAsCompleteGesture ((float) best);
    }

    juce::StringArray readouts;
    juce::RangedAudioParameter* param;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int index = 0;
};

//==================================================================================================
// Two-position lever bound to a bool parameter:  [off label]  ==lever==  [on label]
class LeverSwitch : public juce::Component
{
public:
    LeverSwitch (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
                 const juce::String& offText, const juce::String& onText)
        : off (offText), on (onText), param (apvts.getParameter (paramId))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
        {
            state = v > 0.5f;
            repaint();
        });
        attachment->sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace colours;
        const auto r = getLocalBounds().toFloat();
        const float w = r.getWidth(), h = r.getHeight();

        g.setFont (makeFont (h * 0.34f));
        g.setColour (state ? creamDim : amber);
        g.drawFittedText (off.toUpperCase(), juce::Rectangle<float> (0.0f, 0.0f, w * 0.29f, h).toNearestInt(),
                          juce::Justification::centredRight, 1, 0.7f);
        g.setColour (state ? amber : creamDim);
        g.drawFittedText (on.toUpperCase(), juce::Rectangle<float> (w * 0.71f, 0.0f, w * 0.29f, h).toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.7f);

        const float slotH = h * 0.26f;
        const auto slot = juce::Rectangle<float> (w * 0.33f, h * 0.5f - slotH * 0.5f, w * 0.34f, slotH);
        g.setColour (juce::Colours::black.withAlpha (0.9f));
        g.fillRoundedRectangle (slot, slotH * 0.5f);
        g.setColour (printed.withAlpha (0.22f));
        g.drawRoundedRectangle (slot, slotH * 0.5f, 1.0f);

        const float lw = w * 0.15f, lh = h * 0.64f;
        const float cx = state ? slot.getRight() - lw * 0.5f - slotH * 0.2f : slot.getX() + lw * 0.5f + slotH * 0.2f;
        const auto lever = juce::Rectangle<float> (lw, lh).withCentre ({ cx, h * 0.5f });
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (lever.translated (0.0f, lh * 0.07f), lw * 0.25f);
        g.setGradientFill (juce::ColourGradient (metalLight, lever.getX(), lever.getY(),
                                                 metalDark, lever.getRight(), lever.getBottom(), false));
        g.fillRoundedRectangle (lever, lw * 0.25f);
        g.setColour (cream.withAlpha (0.8f));
        g.drawLine (lever.getCentreX(), lever.getY() + lh * 0.15f, lever.getCentreX(), lever.getBottom() - lh * 0.15f, 1.4f);
    }

    void mouseDown (const juce::MouseEvent&) override { attachment->setValueAsCompleteGesture (state ? 0.0f : 1.0f); }

private:
    juce::String off, on;
    juce::RangedAudioParameter* param;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    bool state = false;
};

//==================================================================================================
// The big round BYPASS / POWER buttons. The unlit button is part of the background; when the
// parameter is on, the lit version (a sprite cut from the artwork, glow included) is drawn over it.
class LitButton : public juce::Button
{
public:
    LitButton (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
               const juce::String& caption, juce::Image litSprite)
        : juce::Button (caption), sprite (std::move (litSprite)),
          attachment (*apvts.getParameter (paramId), *this)
    {
        setClickingTogglesState (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    bool hitTest (int x, int y) override
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        return c.getDistanceFrom ({ (float) x, (float) y }) <= (float) getWidth() * (Layout::buttonRadius / Layout::buttonSprite);
    }

    void paintButton (juce::Graphics& g, bool, bool down) override
    {
        if (! sprite.isValid()) return;
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        if (getToggleState())
        {
            g.setOpacity (down ? 0.88f : 1.0f);
            g.drawImage (sprite, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        }
        else if (down)
        {
            g.setOpacity (0.35f);                   // pre-light while the mouse is held down
            g.drawImage (sprite, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        }
    }

private:
    juce::Image sprite;
    juce::ButtonParameterAttachment attachment;
};

//==================================================================================================
// Square preset key (SAVE / SAVE AS / DELETE / INIT). The key itself is printed on the panel; this
// only adds the pressed feedback and the click.
class PresetKey : public juce::Component
{
public:
    std::function<void()> onClick;

    PresetKey() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced ((float) getWidth() * 0.06f);
        if (down)
        {
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (r, 5.0f);
        }
        else if (over)
        {
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.fillRoundedRectangle (r, 5.0f);
        }
    }

    void mouseEnter (const juce::MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { over = false; down = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override { down = true; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        const bool click = down && contains (e.getPosition());
        down = false;
        repaint();
        if (click && onClick) onClick();
    }

private:
    bool over = false, down = false;
};

//==================================================================================================
// Square key with a triangle (previous / next preset), drawn like the keys printed on the panel.
class ArrowKey : public juce::Component
{
public:
    std::function<void()> onClick;

    explicit ArrowKey (bool pointRight) : right (pointRight)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (1.5f);
        const float rad = r.getHeight() * 0.16f;
        const float dy = down ? r.getHeight() * 0.03f : 0.0f;

        g.setColour (juce::Colours::black.withAlpha (0.85f));                       // bezel
        g.fillRoundedRectangle (r, rad);
        const auto key = r.reduced (r.getHeight() * 0.09f).translated (0.0f, dy);
        g.setGradientFill (juce::ColourGradient (juce::Colour (over ? 0xff4a4d53 : 0xff3a3d42), key.getX(), key.getY(),
                                                 juce::Colour (0xff17181b), key.getX(), key.getBottom(), false));
        g.fillRoundedRectangle (key, rad * 0.8f);
        g.setColour (juce::Colour (0xffb9b6a8).withAlpha (0.25f));
        g.drawRoundedRectangle (key, rad * 0.8f, 1.2f);

        const auto c = key.getCentre();
        const float s = key.getHeight() * 0.26f;
        juce::Path tri;
        if (right) tri.addTriangle (c.x - s * 0.7f, c.y - s, c.x - s * 0.7f, c.y + s, c.x + s * 0.9f, c.y);
        else       tri.addTriangle (c.x + s * 0.7f, c.y - s, c.x + s * 0.7f, c.y + s, c.x - s * 0.9f, c.y);
        g.setColour (colours::amber.withAlpha (down ? 1.0f : 0.9f));
        g.fillPath (tri);
    }

    void mouseEnter (const juce::MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { over = false; down = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override { down = true; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        const bool click = down && contains (e.getPosition());
        down = false;
        repaint();
        if (click && onClick) onClick();
    }

private:
    bool right, over = false, down = false;
};

//==================================================================================================
// A plate that shows a line of text and opens the preset menu when clicked (wheel = next/previous).
class PlatePicker : public juce::Component
{
public:
    std::function<juce::String()> getText;
    std::function<void()> onClick;
    std::function<void (int)> onStep;

    explicit PlatePicker (bool engravedStyle) : engraved (engravedStyle)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        if (! getText) return;
        const auto r = getLocalBounds().toFloat();
        const auto text = getText();

        if (engraved)
        {
            g.setFont (makeFont (r.getHeight() * 0.42f));
            const auto box = r.reduced (r.getWidth() * 0.03f, 0.0f);
            g.setColour (juce::Colour (0xffc4c4c0).withAlpha (0.35f));     // light edge under the letters
            g.drawFittedText (text, box.translated (0.0f, r.getHeight() * 0.03f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
            g.setColour (juce::Colour (0xff2a2a2a).withAlpha (0.92f));
            g.drawFittedText (text, box.toNearestInt(), juce::Justification::centred, 1, 0.6f);
        }
        else
        {
            // preset window: dark inset glass with a thin metal edge
            const auto w = r.reduced (1.5f);
            const float rad = r.getHeight() * 0.14f;
            g.setColour (juce::Colour (0xff050606));
            g.fillRoundedRectangle (w, rad);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1a14), w.getX(), w.getY(),
                                                     juce::Colour (0xff0a0a09), w.getX(), w.getBottom(), false));
            g.fillRoundedRectangle (w.reduced (2.5f), rad * 0.7f);
            g.setColour (juce::Colour (0xffb9b6a8).withAlpha (over ? 0.55f : 0.32f));
            g.drawRoundedRectangle (w, rad, 1.6f);
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.drawRoundedRectangle (w.reduced (1.8f), rad * 0.8f, 1.2f);

            g.setFont (makeFont (r.getHeight() * 0.44f));
            g.setColour (colours::amber.withAlpha (0.97f));
            g.drawFittedText (text, r.reduced (r.getWidth() * 0.06f, r.getHeight() * 0.10f).toNearestInt(),
                              juce::Justification::centred, 1, 0.6f);
        }
    }

    void mouseEnter (const juce::MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { over = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (contains (e.getPosition()) && ! e.mouseWasDraggedSinceMouseDown() && onClick)
            onClick();
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (onStep && std::abs (wheel.deltaY) > 0.01f)
            onStep (wheel.deltaY > 0.0f ? -1 : 1);
    }

private:
    bool engraved;
    bool over = false;
};

//==================================================================================================
// Lit LED. The unlit socket is part of the background, so only the lit state is drawn.
class Led : public juce::Component
{
public:
    explicit Led (juce::Colour colour) : col (colour) { setInterceptsMouseClicks (false, false); }

    void setOn (bool shouldBeOn)
    {
        if (on != shouldBeOn) { on = shouldBeOn; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        if (! on) return;
        const auto r = getLocalBounds().toFloat();
        const auto c = r.getCentre();
        const float R = r.getWidth() * 0.5f;
        const float lr = r.getWidth() * (12.0f / Layout::ledSize);

        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.60f), c.x, c.y, col.withAlpha (0.0f), c.x + R, c.y, true));
        g.fillEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);

        g.setGradientFill (juce::ColourGradient (col.brighter (0.6f), c.x - lr * 0.3f, c.y - lr * 0.4f,
                                                 col.darker (0.2f), c.x + lr, c.y + lr, true));
        g.fillEllipse (c.x - lr, c.y - lr, lr * 2.0f, lr * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.fillEllipse (c.x - lr * 0.5f, c.y - lr * 0.65f, lr * 0.65f, lr * 0.5f);
    }

private:
    juce::Colour col;
    bool on = false;
};

//==================================================================================================
// VU needle. The face is part of the artwork; this draws the needle (pivoting below the window and
// clipped to it) and the dark cap at the bottom of the face on top of it.
class VuOverlay : public juce::Component
{
public:
    VuOverlay (const Layout::Rect& window, Layout::Point pivotPoint, Layout::Point capPoint, juce::String labelText)
        : win (window.x, window.y, window.w, window.h), pivot (pivotPoint), cap (capPoint), label (std::move (labelText))
    {
        setInterceptsMouseClicks (false, false);
    }

    // 0 VU = -18 dBFS RMS sits where the printed scale turns red; scale runs -20 .. +3 VU.
    static float vuToPos (float vu)
    {
        vu = juce::jlimit (-20.0f, 3.0f, vu);
        return vu <= 0.0f ? Layout::vuRedStart * (vu + 20.0f) / 20.0f
                          : Layout::vuRedStart + (1.0f - Layout::vuRedStart) * vu / 3.0f;
    }

    void setPosition (float v)
    {
        v = juce::jlimit (0.0f, 1.0f, v);
        if (std::abs (v - needle) > 0.002f) { needle = v; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        const float s = (float) getWidth() / win.getWidth();
        const juce::Point<float> pv { (pivot.x - win.getX()) * s, (pivot.y - win.getY()) * s };
        const juce::Point<float> cp { (cap.x - win.getX()) * s, (cap.y - win.getY()) * s };
        const float a = (needle - 0.5f) * 2.0f * Layout::vuHalfAngle;
        const float len = Layout::vuNeedleLen * s;
        const juce::Point<float> tip { pv.x + std::sin (a) * len, pv.y - std::cos (a) * len };
        const float thick = juce::jmax (1.2f, 3.2f * s);

        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawLine (pv.x + 2.0f * s, pv.y + 1.5f * s, tip.x + 2.0f * s, tip.y + 1.5f * s, thick);
        g.setColour (juce::Colour (0xff1a1710));
        g.drawLine (pv.x, pv.y, tip.x, tip.y, thick);

        // dark cap over the foot of the needle
        const float cr = Layout::vuCapRadius * s;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff36322b), cp.x - cr * 0.4f, cp.y - cr * 0.9f,
                                                 juce::Colour (0xff0c0b09), cp.x + cr * 0.5f, cp.y + cr * 0.6f, false));
        g.fillEllipse (cp.x - cr, cp.y - cr, cr * 2.0f, cr * 2.0f);

        if (label.isNotEmpty())
        {
            const auto lr = juce::Rectangle<float> ((Layout::vuOutputLabel.x - win.getX()) * s, (Layout::vuOutputLabel.y - win.getY()) * s,
                                                    Layout::vuOutputLabel.w * s, Layout::vuOutputLabel.h * s);
            g.setColour (juce::Colour (0xff3b2e1f));
            g.setFont (makeFont (lr.getHeight() * 1.05f));
            g.drawText (label, lr, juce::Justification::centredLeft, false);
        }
    }

private:
    juce::Rectangle<float> win;
    Layout::Point pivot, cap;
    juce::String label;
    float needle = 0.0f;
};

//==================================================================================================
// Spinning reel: the disc cut from the artwork, rotated around its centre.
class ReelSprite : public juce::Component
{
public:
    explicit ReelSprite (juce::Image img) : sprite (std::move (img)) { setInterceptsMouseClicks (false, false); }

    void setAngle (float a)
    {
        angle = std::fmod (a, juce::MathConstants<float>::twoPi);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        if (! sprite.isValid()) return;
        const auto b = getLocalBounds().toFloat();
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.addTransform (juce::AffineTransform::rotation (angle, b.getCentreX(), b.getCentreY()));
        g.drawImage (sprite, b, juce::RectanglePlacement::stretchToFit);
    }

private:
    juce::Image sprite;
    float angle = 0.0f;
};
} // namespace ui
