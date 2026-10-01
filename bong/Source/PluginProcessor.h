#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "Params.h"
#include "Engine.h"

class BongProcessor : public juce::AudioProcessor
{
public:
    BongProcessor();
    ~BongProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "bong"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Step patterns (bit i = step i+1), edited by the UI
    uint16_t getPattern (int fx) const { return pattern[fx].load(); }
    void toggleStep (int fx, int step);

    juce::AudioProcessorValueTreeState apvts;

    // Read by the UI
    std::atomic<int> uiStep { -1 };
    std::atomic<float> uiMod[4] {};
    std::atomic<float> uiTail { 0.0f };
    std::atomic<bool> uiFrozen { false };

private:
    enum FloatIdx
    {
        g_depth, g_rate, g_smooth, r_len, r_count, r_tone, v_len, v_chance, v_fade,
        s_pieces, s_blend, s_chance, f_cut, f_res, f_depth, b_speed, b_curve, b_tone,
        amt0, amt1, amt2, amt3, amt4, amt5, rhythm_mix, swing,
        k_size, k_dens, k_spread, z_hold, z_fade, z_tone, y_size, y_tail, y_color,
        h_oct, h_amt, h_glit, t_drift, t_cut, t_speed, w_wow, w_flut, w_hiss,
        atmos_mix, freeze, output,
        m0_rate, m0_depth, m0_x, m1_rate, m1_depth, m1_x, m2_rate, m2_depth, m2_x, m3_rate, m3_depth, m3_x, kaos,
        numFloat
    };
    static const char* floatIds[numFloat];

    std::atomic<float>* raw[numFloat] {};
    std::atomic<float>* syncP = nullptr; std::atomic<float>* lenP = nullptr; std::atomic<float>* dirP = nullptr;
    std::atomic<float>* seedP = nullptr;
    std::atomic<float>* onP[6] {};
    std::atomic<float>* tgtP[4] {};
    int targetToFloat[bong::numTargets] {};

    std::atomic<uint16_t> pattern[6];

    bong::Rhythm rhythm;
    bong::Atmos atmos;
    bong::Mods mods;
    double sr = 44100, freePpq = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BongProcessor)
};
