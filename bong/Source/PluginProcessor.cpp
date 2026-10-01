#include "PluginProcessor.h"
#include "PluginEditor.h"

const char* BongProcessor::floatIds[numFloat] = {
    "g_depth", "g_rate", "g_smooth", "r_len", "r_count", "r_tone", "v_len", "v_chance", "v_fade",
    "s_pieces", "s_blend", "s_chance", "f_cut", "f_res", "f_depth", "b_speed", "b_curve", "b_tone",
    "amt0", "amt1", "amt2", "amt3", "amt4", "amt5", "rhythm_mix", "swing",
    "k_size", "k_dens", "k_spread", "z_hold", "z_fade", "z_tone", "y_size", "y_tail", "y_color",
    "h_oct", "h_amt", "h_glit", "t_drift", "t_cut", "t_speed", "w_wow", "w_flut", "w_hiss",
    "atmos_mix", "freeze", "output",
    "m0_rate", "m0_depth", "m0_x", "m1_rate", "m1_depth", "m1_x", "m2_rate", "m2_depth", "m2_x", "m3_rate", "m3_depth", "m3_x", "kaos"
};

BongProcessor::BongProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BONG", bong::createLayout())
{
    for (int i = 0; i < numFloat; ++i)
    {
        raw[i] = apvts.getRawParameterValue (floatIds[i]);
        jassert (raw[i] != nullptr);
    }
    syncP = apvts.getRawParameterValue ("sync");
    lenP = apvts.getRawParameterValue ("seq_len");
    dirP = apvts.getRawParameterValue ("seq_dir");
    seedP = apvts.getRawParameterValue ("seed");
    for (int i = 0; i < 6; ++i) onP[i] = apvts.getRawParameterValue (bong::atmosOnIds[i]);
    for (int i = 0; i < 4; ++i) tgtP[i] = apvts.getRawParameterValue (bong::modTargetIds[i]);

    for (int t = 0; t < bong::numTargets; ++t)
    {
        targetToFloat[t] = -1;
        for (int i = 0; i < numFloat; ++i)
            if (juce::String (floatIds[i]) == bong::targets[t].paramId)
                targetToFloat[t] = i;
    }
    for (int f = 0; f < 6; ++f) pattern[f].store (bong::defaultPatterns[f]);
}

bool BongProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void BongProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    rhythm.prepare (sampleRate);
    atmos.prepare (sampleRate);
    mods.prepare (sampleRate);
    freePpq = 0;
}

void BongProcessor::toggleStep (int fx, int step)
{
    if (! juce::isPositiveAndBelow (fx, 6) || ! juce::isPositiveAndBelow (step, 16)) return;
    uint16_t v = pattern[fx].load();
    pattern[fx].store ((uint16_t) (v ^ (1u << step)));
}

void BongProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    for (auto i = numIn; i < getTotalNumOutputChannels(); ++i) buffer.clear (i, 0, buffer.getNumSamples());

    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int total = buffer.getNumSamples();
    if (numCh == 0 || total == 0) return;

    // Transport
    double bpm = 120.0, ppq = freePpq;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 400.0, *b);
            playing = pos->getIsPlaying();
            if (playing)
                if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    const double ppqPerSample = bpm / 60.0 / sr;

    const uint32_t seed = (uint32_t) seedP->load();
    float knobs[4][3];
    static const int depthIdx[4] = { 1, 1, 2, 1 };

    constexpr int sub = 32;
    for (int start = 0; start < total; start += sub)
    {
        const int n = juce::jmin (sub, total - start);
        const double ppqHere = ppq + ppqPerSample * start;

        // input level for FØLGER
        float peak = 0;
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < n; ++i) peak = juce::jmax (peak, std::abs (buffer.getSample (c, start + i)));

        float v[numFloat];
        for (int i = 0; i < numFloat; ++i) v[i] = raw[i]->load();

        for (int m = 0; m < 4; ++m)
            for (int k = 0; k < 3; ++k) knobs[m][k] = v[m0_rate + m * 3 + k];
        mods.advance (n, knobs, peak, ppqHere, seed);

        // KAOS: fold modulator outputs into their targets
        for (int m = 0; m < 4; ++m)
        {
            const int t = juce::jlimit (0, bong::numTargets - 1, (int) tgtP[m]->load());
            const float val = mods.value (m);
            uiMod[m].store (val);
            const int fi = targetToFloat[t];
            if (fi < 0) continue;
            const float depth = knobs[m][depthIdx[m]] * v[kaos];
            v[fi] = bong::clamp01 (v[fi] + val * depth);
        }

        bong::Params p;
        p.g_depth = v[g_depth]; p.g_rate = v[g_rate]; p.g_smooth = v[g_smooth];
        p.r_len = v[r_len]; p.r_count = v[r_count]; p.r_tone = v[r_tone];
        p.v_len = v[v_len]; p.v_chance = v[v_chance]; p.v_fade = v[v_fade];
        p.s_pieces = v[s_pieces]; p.s_blend = v[s_blend]; p.s_chance = v[s_chance];
        p.f_cut = v[f_cut]; p.f_res = v[f_res]; p.f_depth = v[f_depth];
        p.b_speed = v[b_speed]; p.b_curve = v[b_curve]; p.b_tone = v[b_tone];
        for (int f = 0; f < 6; ++f) { p.amt[f] = v[amt0 + f]; p.pattern[f] = pattern[f].load(); }
        p.rhythmMix = v[rhythm_mix]; p.swing = v[swing];
        p.sync = (int) syncP->load(); p.seqLen = (int) lenP->load(); p.seqDir = (int) dirP->load();
        p.k_size = v[k_size]; p.k_dens = v[k_dens]; p.k_spread = v[k_spread];
        p.z_hold = v[z_hold]; p.z_fade = v[z_fade]; p.z_tone = v[z_tone];
        p.y_size = v[y_size]; p.y_tail = v[y_tail]; p.y_color = v[y_color];
        p.h_oct = v[h_oct]; p.h_amt = v[h_amt]; p.h_glit = v[h_glit];
        p.t_drift = v[t_drift]; p.t_cut = v[t_cut]; p.t_speed = v[t_speed];
        p.w_wow = v[w_wow]; p.w_flut = v[w_flut]; p.w_hiss = v[w_hiss];
        for (int f = 0; f < 6; ++f) p.atmosOn[f] = onP[f]->load() > 0.5f;
        p.atmosMix = v[atmos_mix];
        p.freeze = v[freeze] > 0.5f;
        p.outputGain = juce::Decibels::decibelsToGain ((v[output] - 0.75f) * 48.0f);
        p.seed = seed;

        bong::Clock clk { ppqHere, ppqPerSample, bpm };
        float* ptrs[2] = { buffer.getWritePointer (0, start), numCh > 1 ? buffer.getWritePointer (1, start) : nullptr };
        rhythm.process (ptrs, numCh, n, p, clk);
        atmos.process (ptrs, numCh, n, p);

        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < n; ++i)
            {
                float y = ptrs[c][i] * p.outputGain;
                if (! std::isfinite (y)) y = 0.0f;
                const float a = std::abs (y);
                if (a > 0.891f) y = std::copysign (0.891f + 0.109f * std::tanh ((a - 0.891f) / 0.109f), y);
                ptrs[c][i] = y;
            }
    }

    freePpq = playing ? ppq + ppqPerSample * total : freePpq + ppqPerSample * total;
    uiStep.store (rhythm.currentSeqStep());
    uiTail.store (atmos.tailSeconds());
    uiFrozen.store (atmos.isFrozen());
}

void BongProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::String pat;
    for (int f = 0; f < 6; ++f) pat << juce::String::toHexString ((int) pattern[f].load()) << (f < 5 ? "," : "");
    state.setProperty ("patterns", pat, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void BongProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            auto parts = juce::StringArray::fromTokens (state.getProperty ("patterns").toString(), ",", "");
            for (int f = 0; f < 6 && f < parts.size(); ++f)
                pattern[f].store ((uint16_t) parts[f].getHexValue32());
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* BongProcessor::createEditor() { return new BongEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BongProcessor(); }
