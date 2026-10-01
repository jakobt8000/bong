#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// All parameter IDs, labels and the layout live here so the processor and the UI agree.
namespace bong
{
struct KnobDef { const char* id; const char* label; float def; };

struct FxDef
{
    const char* title;   // "GATE"
    const char* tag;     // "HAKKER"
    KnobDef knobs[3];
};

// ---- Page 1: HOLD RYTMEN (step effects) ----
inline const FxDef rhythmFx[6] = {
    { "GATE",       "HAKKER",    { { "g_depth", "DYBDE", 0.70f }, { "g_rate", "RATE", 0.37f },   { "g_smooth", "GLAT", 0.74f } } },
    { "GENTAG",     "STUTTER",   { { "r_len", "LÆNGDE", 0.11f },  { "r_count", "ANTAL", 0.48f }, { "r_tone", "TONE", 0.85f } } },
    { "BAGLÆNS",    "VEND",      { { "v_len", "LÆNGDE", 0.22f },  { "v_chance", "CHANCE", 0.59f }, { "v_fade", "FADE", 0.96f } } },
    { "SKÆR",       "SLICE",     { { "s_pieces", "STYKKER", 0.33f }, { "s_blend", "BLAND", 0.70f }, { "s_chance", "CHANCE", 0.07f } } },
    { "TRINFILTER", "FILTER",    { { "f_cut", "CUTOFF", 0.44f },  { "f_res", "RES", 0.81f },     { "f_depth", "DYBDE", 0.18f } } },
    { "BREMS",      "HALV FART", { { "b_speed", "FART", 0.55f },  { "b_curve", "KURVE", 0.92f }, { "b_tone", "TONE", 0.29f } } },
};
inline const char* amountIds[6] = { "amt0", "amt1", "amt2", "amt3", "amt4", "amt5" };
inline const float amountDefaults[6] = { 0.80f, 0.50f, 0.60f, 0.90f, 0.40f, 0.55f };
inline const uint16_t defaultPatterns[6] = {
    0b0101010101010101, // gate: 1,3,5,7...
    0b1100000011000000, // gentag: 7,8,15,16
    0b0001000000010000, // baglæns: 5,13
    0b1000000000000000, // skær: 16
    0b0000110000001100, // filter: 3,4,11,12
    0b0000000100000001, // brems: 1,9
};

// ---- Page 2: ATMOSFÆRE ----
inline const FxDef atmosFx[6] = {
    { "KORN",    "GRANULÆR",    { { "k_size", "STØRR.", 0.10f }, { "k_dens", "TÆTHED", 0.51f }, { "k_spread", "SPRED", 0.92f } } },
    { "FRYS",    "HOLD",        { { "z_hold", "HOLD", 0.33f },   { "z_fade", "FADE", 0.74f },   { "z_tone", "TONE", 0.15f } } },
    { "SKY",     "RUMKLANG",    { { "y_size", "STØRR.", 0.56f }, { "y_tail", "HALE", 0.97f },   { "y_color", "FARVE", 0.38f } } },
    { "SHIMMER", "OKTAV",       { { "h_oct", "OKTAV", 0.79f },   { "h_amt", "MÆNGDE", 0.20f },  { "h_glit", "GLITTER", 0.61f } } },
    { "TÅGE",    "FILTERDRIFT", { { "t_drift", "DRIFT", 0.02f }, { "t_cut", "CUTOFF", 0.43f },  { "t_speed", "FART", 0.84f } } },
    { "BÅND",    "SLID",        { { "w_wow", "WOW", 0.25f },     { "w_flut", "FLUTTER", 0.66f },{ "w_hiss", "SUS", 0.07f } } },
};
inline const char* atmosOnIds[6] = { "a_on0", "a_on1", "a_on2", "a_on3", "a_on4", "a_on5" };
inline const bool atmosOnDefaults[6] = { false, false, true, false, false, false };

// ---- Page 3: KAOS (modulators) ----
inline const FxDef modFx[4] = {
    { "TERNING", "TILFÆLDIG", { { "m0_rate", "RATE", 0.20f },   { "m0_depth", "DYBDE", 0.49f }, { "m0_x", "GLAT", 0.78f } } },
    { "LFO",     "BØLGE",     { { "m1_rate", "RATE", 0.07f },   { "m1_depth", "DYBDE", 0.36f }, { "m1_x", "FORM", 0.65f } } },
    { "FØLGER",  "ENVELOPE",  { { "m2_rate", "ANSLAG", 0.94f }, { "m2_depth", "SLIP", 0.23f },  { "m2_x", "DYBDE", 0.52f } } },
    { "TRINMOD", "SEKVENS",   { { "m3_rate", "RATE", 0.81f },   { "m3_depth", "DYBDE", 0.10f }, { "m3_x", "TRIN", 0.39f } } },
};
inline const char* modTargetIds[4] = { "m0_tgt", "m1_tgt", "m2_tgt", "m3_tgt" };

// Modulation targets: display name + parameter id ("" = none)
struct Target { const char* name; const char* paramId; };
inline const Target targets[] = {
    { "INGEN", "" },
    { "GATE.DYBDE", "g_depth" },   { "GATE.RATE", "g_rate" },
    { "GENTAG.ANTAL", "r_count" }, { "GENTAG.LÆNGDE", "r_len" },
    { "BAGLÆNS.CHANCE", "v_chance" },
    { "SKÆR.BLAND", "s_blend" },   { "SKÆR.CHANCE", "s_chance" },
    { "FILTER.CUTOFF", "f_cut" },  { "BREMS.FART", "b_speed" },
    { "KORN.TÆTHED", "k_dens" },   { "KORN.STØRR", "k_size" },
    { "RUM.FRYS", "freeze" },      { "SKY.HALE", "y_tail" },
    { "SHIMMER.MÆNGDE", "h_amt" }, { "TÅGE.CUTOFF", "t_cut" },
    { "BÅND.WOW", "w_wow" },       { "RYTME.MIX", "rhythm_mix" },
    { "ATMOS.MIX", "atmos_mix" },
};
inline constexpr int numTargets = (int) (sizeof (targets) / sizeof (targets[0]));
inline const int modTargetDefaults[4] = { 10, 12, 1, 3 };

inline juce::StringArray targetNames()
{
    juce::StringArray s;
    for (auto& t : targets) s.add (juce::String::fromUTF8 (t.name));
    return s;
}

inline juce::StringArray syncNames() { return { "1/8", "1/16", "1/32" }; }
inline juce::StringArray dirNames()  { return { ">", "<", "<>", "?" }; }

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v * 100.0f)); });

    auto knob = [&] (const char* id, const char* label, float def, const char* group)
    {
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, String::fromUTF8 (group) + " " + String::fromUTF8 (label),
            NormalisableRange<float> (0.0f, 1.0f), def, pct));
    };
    auto onOff = [&] (const char* id, const String& name, bool def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, StringArray { "Fra", "Til" }, def ? 1 : 0));
    };

    for (int i = 0; i < 6; ++i)
    {
        for (auto& k : rhythmFx[i].knobs) knob (k.id, k.label, k.def, rhythmFx[i].title);
        knob (amountIds[i], "MÆNGDE", amountDefaults[i], rhythmFx[i].title);
    }
    knob ("rhythm_mix", "MIX", 0.64f, "RYTME");
    knob ("swing", "SWING", 0.20f, "RYTME");
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "sync", 1 }, "Rytme Sync", syncNames(), 1));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "seq_len", 1 }, "Sekvens Længde", 1, 16, 16));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "seq_dir", 1 }, "Sekvens Retning", dirNames(), 0));

    for (int i = 0; i < 6; ++i)
    {
        for (auto& k : atmosFx[i].knobs) knob (k.id, k.label, k.def, atmosFx[i].title);
        onOff (atmosOnIds[i], String::fromUTF8 (atmosFx[i].title) + " Til", atmosOnDefaults[i]);
    }
    knob ("atmos_mix", "MIX", 0.55f, "ATMOS");
    onOff ("freeze", "Frys Nu", false);

    for (int i = 0; i < 4; ++i)
    {
        for (auto& k : modFx[i].knobs) knob (k.id, k.label, k.def, modFx[i].title);
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { modTargetIds[i], 1 },
                                                            String::fromUTF8 (modFx[i].title) + " Mål", targetNames(), modTargetDefaults[i]));
    }
    knob ("kaos", "KAOS", 0.45f, "KAOS");
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "seed", 1 }, "Frø", 0, 65535, 0x4F2A));

    knob ("output", "OUTPUT", 0.75f, "UD");
    return layout;
}
} // namespace bong
