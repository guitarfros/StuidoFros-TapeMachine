#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// All automatable parameters of StudioFros - Tape Machine.
// IDs are part of the saved-state format: do not rename them once released.
namespace ParamID
{
    inline constexpr const char* power       = "power";
    inline constexpr const char* bypass      = "bypass";
    inline constexpr const char* input       = "input";
    inline constexpr const char* drive       = "drive";
    inline constexpr const char* speed       = "speed";
    inline constexpr const char* tapeType    = "tapeType";
    inline constexpr const char* eq          = "eq";
    inline constexpr const char* bias        = "bias";
    inline constexpr const char* saturation  = "saturation";
    inline constexpr const char* headBump    = "headBump";
    inline constexpr const char* tone        = "tone";
    inline constexpr const char* loss        = "loss";
    inline constexpr const char* compression = "compression";
    inline constexpr const char* wow         = "wow";
    inline constexpr const char* flutter     = "flutter";
    inline constexpr const char* drift       = "drift";
    inline constexpr const char* degrade     = "degrade";
    inline constexpr const char* hiss        = "hiss";
    inline constexpr const char* hissOn      = "hissOn";
    inline constexpr const char* link        = "link";
    inline constexpr const char* output      = "output";
    inline constexpr const char* mix         = "mix";

    // Every parameter, for presets
    inline constexpr const char* all[] = { power, bypass, input, drive, speed, tapeType, eq, bias, saturation,
                                           headBump, tone, loss, compression, wow, flutter, drift, degrade,
                                           hiss, hissOn, link, output, mix };
}

namespace Parameters
{
    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;

        auto percent = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
        auto bipolar = [] (float v, int)
        {
            const int p = roundToInt (v * 100.0f);
            return (p > 0 ? "+" : "") + String (p) + " %";
        };
        auto decibels = [] (float v, int)
        {
            return (v > 0.04f ? "+" : "") + String (v, 1) + " dB";
        };

        auto addPercent = [&] (const char* id, const char* name, float def)
        {
            layout.add (std::make_unique<AudioParameterFloat> (
                ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f, 0.001f), def,
                AudioParameterFloatAttributes().withStringFromValueFunction (percent)));
        };

        auto addDb = [&] (const char* id, const char* name)
        {
            layout.add (std::make_unique<AudioParameterFloat> (
                ParameterID { id, 1 }, name, NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                AudioParameterFloatAttributes().withStringFromValueFunction (decibels)));
        };

        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::power, 1 },  "Power",  true));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::bypass, 1 }, "Bypass", false));

        // Input stage
        addDb (ParamID::input, "Input");
        addPercent (ParamID::drive, "Drive", 0.35f);
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::speed, 1 }, "Tape Speed",
                                                            StringArray { "7.5 ips", "15 ips", "30 ips" }, 1));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::tapeType, 1 }, "Tape Type",
                                                            StringArray { "Vintage", "Modern", "High Output" }, 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::eq, 1 }, "EQ Standard",
                                                            StringArray { "NAB", "IEC" }, 0));

        // Tape character
        addPercent (ParamID::bias,        "Bias",        0.5f);
        addPercent (ParamID::saturation,  "Saturation",  0.5f);
        addPercent (ParamID::headBump,    "Head Bump",   0.4f);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::tone, 1 }, "Tone", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction (bipolar)));
        addPercent (ParamID::loss,        "Loss",        0.3f);
        addPercent (ParamID::compression, "Compression", 0.3f);

        // Instability
        addPercent (ParamID::wow,     "Wow",     0.12f);
        addPercent (ParamID::flutter, "Flutter", 0.15f);
        addPercent (ParamID::drift,   "Drift",   0.1f);
        addPercent (ParamID::degrade, "Degrade", 0.0f);
        addPercent (ParamID::hiss,    "Hiss",    0.2f);
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::hissOn, 1 }, "Hiss On", false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::link, 1 },   "Stereo Link", true));

        // Output
        addDb (ParamID::output, "Output");
        addPercent (ParamID::mix, "Dry/Wet", 1.0f);

        return layout;
    }
}
