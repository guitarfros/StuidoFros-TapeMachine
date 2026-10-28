#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <utility>
#include "Parameters.h"

// Factory + user presets behind the SAVE / SAVE AS / DELETE / INIT keys and the title plate menu.
//
// User presets are small XML files (one <PARAM id=".." value=".."/> per parameter) in
//   Linux:   ~/.config/GuitarFros/StudioFros - Tape Machine/Presets
//   macOS:   ~/Library/GuitarFros/StudioFros - Tape Machine/Presets
//   Windows: %APPDATA%\GuitarFros\StudioFros - Tape Machine\Presets
// The name of the loaded preset is stored in the plug-in state, so it comes back with the project.
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s) {}

    struct Factory
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;   // parameter id -> plain value
    };

    static const std::vector<Factory>& factory()
    {
        // Choice indices: speed 0 = 7.5, 1 = 15, 2 = 30 ips; tapeType 0 = Vintage, 1 = Modern, 2 = High Output.
        static const std::vector<Factory> presets = {
            { "Init", {} },
            { "Warm Glue (15 ips)", {
                { ParamID::speed, 1 }, { ParamID::tapeType, 0 }, { ParamID::drive, 0.45f }, { ParamID::saturation, 0.5f },
                { ParamID::headBump, 0.55f }, { ParamID::loss, 0.3f }, { ParamID::compression, 0.45f },
                { ParamID::wow, 0.08f }, { ParamID::flutter, 0.1f }, { ParamID::drift, 0.05f } } },
            { "Studio Clean (30 ips)", {
                { ParamID::speed, 2 }, { ParamID::tapeType, 1 }, { ParamID::drive, 0.25f }, { ParamID::saturation, 0.3f },
                { ParamID::headBump, 0.3f }, { ParamID::loss, 0.1f }, { ParamID::compression, 0.2f },
                { ParamID::wow, 0.03f }, { ParamID::flutter, 0.03f }, { ParamID::drift, 0.02f }, { ParamID::tone, 0.1f } } },
            { "Drum Bus Crunch", {
                { ParamID::speed, 1 }, { ParamID::tapeType, 2 }, { ParamID::drive, 0.75f }, { ParamID::saturation, 0.7f },
                { ParamID::headBump, 0.6f }, { ParamID::compression, 0.6f }, { ParamID::wow, 0.05f },
                { ParamID::flutter, 0.05f }, { ParamID::drift, 0.03f } } },
            { "Lo-Fi Tape (7.5 ips)", {
                { ParamID::speed, 0 }, { ParamID::tapeType, 0 }, { ParamID::drive, 0.6f }, { ParamID::saturation, 0.7f },
                { ParamID::loss, 0.55f }, { ParamID::compression, 0.5f }, { ParamID::wow, 0.35f }, { ParamID::flutter, 0.3f },
                { ParamID::drift, 0.3f }, { ParamID::degrade, 0.35f }, { ParamID::hiss, 0.45f }, { ParamID::hissOn, 1 } } },
            { "Worn Cassette", {
                { ParamID::speed, 0 }, { ParamID::tapeType, 1 }, { ParamID::drive, 0.4f }, { ParamID::tone, -0.3f },
                { ParamID::loss, 0.6f }, { ParamID::wow, 0.6f }, { ParamID::flutter, 0.5f }, { ParamID::drift, 0.5f },
                { ParamID::degrade, 0.5f }, { ParamID::hiss, 0.6f }, { ParamID::hissOn, 1 } } },
        };
        return presets;
    }

    static juce::File folder()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("GuitarFros").getChildFile ("StudioFros - Tape Machine").getChildFile ("Presets");
    }

    static juce::File fileFor (const juce::String& name)
    {
        return folder().getChildFile (juce::File::createLegalFileName (name.trim()) + ".tapepreset");
    }

    juce::StringArray userPresets() const
    {
        juce::StringArray names;
        for (const auto& f : folder().findChildFiles (juce::File::findFiles, false, "*.tapepreset"))
            names.add (f.getFileNameWithoutExtension());
        names.sortNatural();
        return names;
    }

    juce::String currentName() const
    {
        const auto n = apvts.state.getProperty ("presetName").toString();
        return n.isEmpty() ? juce::String ("Init") : n;
    }

    bool currentIsUser() const { return (bool) apvts.state.getProperty ("presetIsUser", false); }

    //==============================================================================================
    void init()                    { resetAll(); setCurrent ("Init", false); }

    void loadFactory (int index)
    {
        const auto& list = factory();
        if (index < 0 || index >= (int) list.size()) return;
        resetAll();
        for (const auto& kv : list[(size_t) index].values) setParam (kv.first, kv.second);
        setCurrent (list[(size_t) index].name, false);
    }

    bool loadUser (const juce::String& name)
    {
        const auto xml = juce::parseXML (fileFor (name));
        if (xml == nullptr || ! xml->hasTagName ("TapeMachinePreset")) return false;
        resetAll();
        for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
            setParam (e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value"));
        setCurrent (name, true);
        return true;
    }

    bool saveUser (const juce::String& rawName)
    {
        const auto name = rawName.trim();
        if (name.isEmpty()) return false;
        juce::XmlElement xml ("TapeMachinePreset");
        xml.setAttribute ("name", name);
        xml.setAttribute ("version", 1);
        for (const char* id : ParamID::all)
            if (auto* p = apvts.getParameter (id))
            {
                auto* e = xml.createNewChildElement ("PARAM");
                e->setAttribute ("id", id);
                e->setAttribute ("value", (double) p->convertFrom0to1 (p->getValue()));
            }
        folder().createDirectory();
        if (! fileFor (name).replaceWithText (xml.toString())) return false;
        setCurrent (name, true);
        return true;
    }

    bool deleteUser (const juce::String& name)
    {
        const bool ok = fileFor (name).deleteFile();
        if (ok && currentName() == name) setCurrent ("Init", false);
        return ok;
    }

private:
    void setCurrent (const juce::String& name, bool isUser)
    {
        apvts.state.setProperty ("presetName", name, nullptr);
        apvts.state.setProperty ("presetIsUser", isUser, nullptr);
    }

    void setParam (const juce::String& id, float plainValue)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
            p->endChangeGesture();
        }
    }

    void resetAll()
    {
        for (const char* id : ParamID::all)
            if (auto* p = apvts.getParameter (id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->getDefaultValue());
                p->endChangeGesture();
            }
    }

    juce::AudioProcessorValueTreeState& apvts;
};
