#include "../Source/PluginEditor.h"
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    BongProcessor proc;
    proc.prepareToPlay (48000, 512);
    // run some audio so meters/graphs have data
    juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
    std::unique_ptr<BongEditor> ed (dynamic_cast<BongEditor*> (proc.createEditor()));
    ed->showPage (juce::String (argv[2]).getIntValue());
    for (int b = 0; b < 200; ++b)
    {
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) buf.setSample (c, i, 0.3f * std::sin ((b * 512 + i) * 0.03f));
        proc.processBlock (buf, midi);
        if (b % 4 == 0) { auto* kp = ed->getChildComponent (3); (void) kp; }
    }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    juce::File out (argv[1]); out.deleteFile();
    juce::FileOutputStream os (out); juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
