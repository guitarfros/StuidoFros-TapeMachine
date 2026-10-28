#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "Presets.h"
#include "Layout.h"
#include "UIComponents.h"

class TapeMachineEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit TapeMachineEditor (TapeMachineProcessor&);
    ~TapeMachineEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Advance meters/reels by `dt` seconds. Called by the timer; public so it can be driven
    // manually (e.g. for offscreen screenshots).
    void tick (float dt);

private:
    void timerCallback() override { tick (1.0f / 30.0f); }
    juce::Rectangle<int> scaled (const Layout::Rect& r) const;

    // presets
    void showPresetMenu();
    void stepPreset (int delta);
    void saveCurrent();
    void saveAs();
    void deleteCurrent();

    TapeMachineProcessor& processor;
    PresetManager presets;
    ui::TapeLookAndFeel laf;
    juce::Image background;

    std::vector<std::unique_ptr<ui::RackKnob>> knobs;
    std::unique_ptr<ui::SelectorSwitch> selSpeed, selType, selEq;
    std::unique_ptr<ui::LeverSwitch> levHiss, levLink;
    std::unique_ptr<ui::LitButton> btnBypass, btnPower;
    std::array<std::unique_ptr<ui::PresetKey>, 4> keys;      // save, save as, delete, init
    std::array<std::unique_ptr<ui::Led>, 8> leds;            // power, signal, clip, bypass, hiss, 7.5, 15, 30
    std::unique_ptr<ui::VuOverlay> vuIn, vuOut;
    std::unique_ptr<ui::ReelSprite> reelLeft, reelRight;
    std::unique_ptr<ui::PlatePicker> titlePlate, namePlate;
    std::unique_ptr<ui::ArrowKey> btnPrevPreset, btnNextPreset;
    std::unique_ptr<juce::AlertWindow> dialog;               // "save as" name prompt

    juce::String shownPreset;
    float msIn = 0.0f, msOut = 0.0f;   // smoothed mean-square for VU ballistics
    float reelAngle = 0.0f;
    float clipHold = 0.0f;

    std::atomic<float>* rawPower = nullptr;
    std::atomic<float>* rawBypass = nullptr;
    std::atomic<float>* rawHissOn = nullptr;
    std::atomic<float>* rawSpeed = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeMachineEditor)
};
