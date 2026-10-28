#include "PluginEditor.h"
#include <BinaryData.h>

using namespace ui;

namespace
{
    juce::Image loadImage (const void* data, int size)
    {
        return juce::ImageCache::getFromMemory (data, size);
    }

    juce::Rectangle<int> toInt (const juce::Rectangle<float>& r, float s)
    {
        return juce::Rectangle<float> (r.getX() * s, r.getY() * s, r.getWidth() * s, r.getHeight() * s).toNearestInt();
    }
}

TapeMachineEditor::TapeMachineEditor (TapeMachineProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p), presets (p.apvts)
{
    setLookAndFeel (&laf);
    auto& apvts = processor.apvts;

    background = loadImage (BinaryData::background_png, BinaryData::background_pngSize);

    rawPower  = apvts.getRawParameterValue (ParamID::power);
    rawBypass = apvts.getRawParameterValue (ParamID::bypass);
    rawHissOn = apvts.getRawParameterValue (ParamID::hissOn);
    rawSpeed  = apvts.getRawParameterValue (ParamID::speed);

    // Reels first, so everything else is drawn above them -------------------------------------
    reelLeft  = std::make_unique<ReelSprite> (loadImage (BinaryData::reel_left_png,  BinaryData::reel_left_pngSize));
    reelRight = std::make_unique<ReelSprite> (loadImage (BinaryData::reel_right_png, BinaryData::reel_right_pngSize));
    addAndMakeVisible (*reelLeft);
    addAndMakeVisible (*reelRight);

    // Knobs: same order as Layout::knobs ----------------------------------------------------------
    struct K { const char* id; const char* caption; };
    static const K knobTable[15] = {
        { ParamID::input,       "Input"   }, { ParamID::drive,       "Drive"  },
        { ParamID::bias,        "Bias"    }, { ParamID::saturation,  "Sat"    }, { ParamID::headBump, "Bump" },
        { ParamID::tone,        "Tone"    }, { ParamID::loss,        "Loss"   }, { ParamID::compression, "Comp" },
        { ParamID::wow,         "Wow"     }, { ParamID::flutter,     "Flutter" }, { ParamID::drift,   "Drift" },
        { ParamID::degrade,     "Degrade" }, { ParamID::hiss,        "Hiss"   },
        { ParamID::output,      "Output"  }, { ParamID::mix,         "Dry/Wet" },
    };
    for (int i = 0; i < 15; ++i)
    {
        knobs.push_back (std::make_unique<RackKnob> (apvts, knobTable[i].id, knobTable[i].caption,
                                                     Layout::knobs[i].captionPrinted));
        addAndMakeVisible (*knobs.back());
    }

    // Switches ----------------------------------------------------------------------------------
    selSpeed = std::make_unique<SelectorSwitch> (apvts, ParamID::speed,    juce::StringArray { "7.5 ips", "15 ips", "30 ips" });
    selType  = std::make_unique<SelectorSwitch> (apvts, ParamID::tapeType, juce::StringArray { "Vintage", "Modern", "High-Output" });
    selEq    = std::make_unique<SelectorSwitch> (apvts, ParamID::eq,       juce::StringArray { "NAB", "IEC" });
    levHiss  = std::make_unique<LeverSwitch> (apvts, ParamID::hissOn, "Off",  "On");
    levLink  = std::make_unique<LeverSwitch> (apvts, ParamID::link,   "Dual", "Link");
    for (auto* c : std::initializer_list<juce::Component*> { selSpeed.get(), selType.get(), selEq.get(), levHiss.get(), levLink.get() })
        addAndMakeVisible (c);

    // Big lit buttons ---------------------------------------------------------------------------
    btnBypass = std::make_unique<LitButton> (apvts, ParamID::bypass, "Bypass",
                                             loadImage (BinaryData::button_bypass_on_png, BinaryData::button_bypass_on_pngSize));
    btnPower  = std::make_unique<LitButton> (apvts, ParamID::power, "Power",
                                             loadImage (BinaryData::button_power_on_png, BinaryData::button_power_on_pngSize));
    addAndMakeVisible (*btnBypass);
    addAndMakeVisible (*btnPower);

    // LEDs --------------------------------------------------------------------------------------
    const juce::Colour ledColours[8] = { colours::green, colours::green, colours::red, colours::amber,
                                         colours::amber, colours::green, colours::green, colours::green };
    for (int i = 0; i < 8; ++i)
    {
        leds[(size_t) i] = std::make_unique<Led> (ledColours[i]);
        addAndMakeVisible (*leds[(size_t) i]);
    }

    // VU needles --------------------------------------------------------------------------------
    vuIn  = std::make_unique<VuOverlay> (Layout::vuWindowIn,  Layout::vuPivotIn,  Layout::vuCapIn,  juce::String());
    vuOut = std::make_unique<VuOverlay> (Layout::vuWindowOut, Layout::vuPivotOut, Layout::vuCapOut, "OUTPUT");
    addAndMakeVisible (*vuIn);
    addAndMakeVisible (*vuOut);

    // Plates and preset keys --------------------------------------------------------------------
    titlePlate = std::make_unique<PlatePicker> (true);
    titlePlate->getText = [] { return juce::String ("StudioFros - Tape Machine"); };
    titlePlate->onClick = [this] { showPresetMenu(); };
    titlePlate->onStep  = [this] (int d) { stepPreset (d); };

    namePlate = std::make_unique<PlatePicker> (false);
    namePlate->getText = [this] { return presets.currentName(); };
    namePlate->onClick = [this] { showPresetMenu(); };
    namePlate->onStep  = [this] (int d) { stepPreset (d); };
    addAndMakeVisible (*titlePlate);
    addAndMakeVisible (*namePlate);

    btnPrevPreset = std::make_unique<ArrowKey> (false);
    btnNextPreset = std::make_unique<ArrowKey> (true);
    btnPrevPreset->onClick = [this] { stepPreset (-1); };
    btnNextPreset->onClick = [this] { stepPreset (+1); };
    addAndMakeVisible (*btnPrevPreset);
    addAndMakeVisible (*btnNextPreset);

    keys[0] = std::make_unique<PresetKey>();  keys[0]->onClick = [this] { saveCurrent(); };
    keys[1] = std::make_unique<PresetKey>();  keys[1]->onClick = [this] { saveAs(); };
    keys[2] = std::make_unique<PresetKey>();  keys[2]->onClick = [this] { deleteCurrent(); };
    keys[3] = std::make_unique<PresetKey>();  keys[3]->onClick = [this] { presets.init(); namePlate->repaint(); };
    for (auto& k : keys) addAndMakeVisible (*k);

    shownPreset = presets.currentName();

    setResizable (true, true);
    setResizeLimits (720, (int) (720.0f * Layout::designHeight / Layout::designWidth),
                     1920, (int) (1920.0f * Layout::designHeight / Layout::designWidth));
    getConstrainer()->setFixedAspectRatio ((double) Layout::designWidth / (double) Layout::designHeight);
    setSize ((int) (Layout::designWidth * 0.5f), (int) (Layout::designHeight * 0.5f));

    startTimerHz (30);
}

TapeMachineEditor::~TapeMachineEditor()
{
    stopTimer();
    dialog.reset();
    setLookAndFeel (nullptr);
}

juce::Rectangle<int> TapeMachineEditor::scaled (const Layout::Rect& r) const
{
    return toInt ({ r.x, r.y, r.w, r.h }, (float) getWidth() / Layout::designWidth);
}

void TapeMachineEditor::resized()
{
    for (int i = 0; i < 15; ++i)
    {
        const auto& k = Layout::knobs[i];
        knobs[(size_t) i]->setBounds (scaled ({ k.x - Layout::knobSize * 0.5f, k.y - Layout::knobSize * 0.5f,
                                                Layout::knobSize, Layout::knobSize + Layout::knobCaptionH }));
    }

    selSpeed->setBounds (scaled (Layout::selectorSpeed));
    selType->setBounds (scaled (Layout::selectorType));
    selEq->setBounds (scaled (Layout::selectorEq));
    levHiss->setBounds (scaled (Layout::leverHiss));
    levLink->setBounds (scaled (Layout::leverLink));

    btnBypass->setBounds (scaled (Layout::centred (Layout::bypassCentre.x, Layout::bypassCentre.y, Layout::buttonSprite, Layout::buttonSprite)));
    btnPower->setBounds (scaled (Layout::centred (Layout::powerCentre.x, Layout::powerCentre.y, Layout::buttonSprite, Layout::buttonSprite)));

    for (int i = 0; i < 8; ++i)
        leds[(size_t) i]->setBounds (scaled (Layout::centred (Layout::leds[i].x, Layout::leds[i].y, Layout::ledSize, Layout::ledSize)));

    vuIn->setBounds (scaled (Layout::vuWindowIn));
    vuOut->setBounds (scaled (Layout::vuWindowOut));
    reelLeft->setBounds (scaled ({ Layout::reelLeftSpriteTopLeft.x, Layout::reelLeftSpriteTopLeft.y, Layout::reelSprite, Layout::reelSprite }));
    reelRight->setBounds (scaled ({ Layout::reelRightSpriteTopLeft.x, Layout::reelRightSpriteTopLeft.y, Layout::reelSprite, Layout::reelSprite }));

    titlePlate->setBounds (scaled (Layout::titlePlate));
    namePlate->setBounds (scaled (Layout::presetWindow));
    btnPrevPreset->setBounds (scaled (Layout::presetPrev));
    btnNextPreset->setBounds (scaled (Layout::presetNext));

    keys[0]->setBounds (scaled (Layout::keySave));
    keys[1]->setBounds (scaled (Layout::keySaveAs));
    keys[2]->setBounds (scaled (Layout::keyDelete));
    keys[3]->setBounds (scaled (Layout::keyInit));
}

void TapeMachineEditor::tick (float dt)
{
    const bool powered = rawPower->load() > 0.5f;

    // VU ballistics: ~300 ms integration of the mean square
    const float a = 1.0f - std::exp (-dt / 0.30f);
    const float inRms = processor.inRms.load(), outRms = processor.outRms.load();
    msIn  += a * ((powered ? inRms  * inRms  : 0.0f) - msIn);
    msOut += a * ((powered ? outRms * outRms : 0.0f) - msOut);

    auto toNeedle = [] (float ms)
    {
        const float vu = 10.0f * std::log10 (ms + 1.0e-9f) + 18.0f;   // 0 VU = -18 dBFS RMS
        return VuOverlay::vuToPos (vu);
    };
    vuIn->setPosition (toNeedle (msIn));
    vuOut->setPosition (toNeedle (msOut));

    // LEDs
    const float peak = processor.outPeak.exchange (0.0f);
    processor.inPeak.store (0.0f);
    if (peak >= 0.99f) clipHold = 0.6f;
    clipHold = juce::jmax (0.0f, clipHold - dt);

    const int speed = juce::jlimit (0, 2, (int) rawSpeed->load());
    leds[0]->setOn (powered);
    leds[1]->setOn (powered && inRms > 0.0007f);
    leds[2]->setOn (powered && clipHold > 0.0f);
    leds[3]->setOn (rawBypass->load() > 0.5f);
    leds[4]->setOn (powered && rawHissOn->load() > 0.5f);
    for (int i = 0; i < 3; ++i)
        leds[(size_t) (5 + i)]->setOn (powered && i == speed);

    // Reels spin while powered; faster at higher tape speeds
    if (powered)
    {
        static constexpr float omega[3] = { 2.0f, 4.0f, 8.0f };   // rad/s
        reelAngle += omega[speed] * dt;
        reelLeft->setAngle (reelAngle);
        reelRight->setAngle (reelAngle * 1.07f);
    }

    // the preset name changes from a click, a host recall or a menu pick
    const auto name = presets.currentName();
    if (name != shownPreset)
    {
        shownPreset = name;
        namePlate->repaint();
    }
}

//==================================================================================================
// Presets
//==================================================================================================
void TapeMachineEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto& factory = PresetManager::factory();
    const bool user = presets.currentIsUser();
    const auto current = presets.currentName();

    menu.addSectionHeader ("Factory presets");
    for (size_t i = 0; i < factory.size(); ++i)
        menu.addItem (1000 + (int) i, factory[i].name, true, ! user && current == factory[i].name);

    const auto users = presets.userPresets();
    if (users.size() > 0)
    {
        menu.addSeparator();
        menu.addSectionHeader ("User presets");
        for (int j = 0; j < users.size(); ++j)
            menu.addItem (2000 + j, users[j], true, user && current == users[j]);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (namePlate.get()),
                        [safe = juce::Component::SafePointer<TapeMachineEditor> (this), users] (int result)
                        {
                            if (safe == nullptr || result == 0) return;
                            if (result >= 2000)      safe->presets.loadUser (users[result - 2000]);
                            else if (result >= 1000) safe->presets.loadFactory (result - 1000);
                            safe->namePlate->repaint();
                        });
}

void TapeMachineEditor::stepPreset (int delta)
{
    const int nFactory = (int) PresetManager::factory().size();
    const auto users = presets.userPresets();
    const int total = nFactory + users.size();
    if (total <= 0) return;

    int index = 0;
    const auto current = presets.currentName();
    if (presets.currentIsUser())
    {
        const int j = users.indexOf (current);
        index = j >= 0 ? nFactory + j : 0;
    }
    else
    {
        for (int i = 0; i < nFactory; ++i)
            if (current == PresetManager::factory()[(size_t) i].name) index = i;
    }

    index = ((index + delta) % total + total) % total;
    if (index < nFactory) presets.loadFactory (index);
    else                  presets.loadUser (users[index - nFactory]);
    namePlate->repaint();
}

void TapeMachineEditor::saveCurrent()
{
    if (presets.currentIsUser())
    {
        if (! presets.saveUser (presets.currentName()))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save preset",
                                                    "The preset could not be written to disk.", "OK", this);
        namePlate->repaint();
    }
    else
    {
        saveAs();    // factory presets and "Init" are never overwritten
    }
}

void TapeMachineEditor::saveAs()
{
    dialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name of the new preset:",
                                                  juce::MessageBoxIconType::NoIcon, this);
    dialog->addTextEditor ("name", presets.currentIsUser() ? presets.currentName() : juce::String ("My Preset"));
    dialog->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    dialog->enterModalState (true,
        juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<TapeMachineEditor> (this)] (int result)
        {
            if (safe == nullptr || safe->dialog == nullptr) return;
            const auto name = safe->dialog->getTextEditorContents ("name");

            // the window is still on the modal stack here: delete it a moment later
            juce::MessageManager::callAsync ([safe, result, name]
            {
                if (safe == nullptr) return;
                safe->dialog.reset();
                if (result != 1 || name.trim().isEmpty()) return;
                if (! safe->presets.saveUser (name))
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save preset",
                                                            "The preset could not be written to disk.", "OK", safe.getComponent());
                safe->namePlate->repaint();
            });
        }),
        false);
}

void TapeMachineEditor::deleteCurrent()
{
    if (! presets.currentIsUser())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Delete preset",
                                                "Only your own presets can be deleted.", "OK", this);
        return;
    }

    const auto name = presets.currentName();
    juce::AlertWindow::showAsync (
        juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::QuestionIcon, "Delete preset",
                                                      "Delete the preset \"" + name + "\"?", "Delete", "Cancel", this),
        [safe = juce::Component::SafePointer<TapeMachineEditor> (this), name] (int result)
        {
            if (safe == nullptr || result != 1) return;
            safe->presets.deleteUser (name);
            safe->namePlate->repaint();
        });
}

//==================================================================================================
void TapeMachineEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
}
