#pragma once

#include "Pages.h"

class BongEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BongEditor (BongProcessor&);
    ~BongEditor() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    void showPage (int index);

private:
    void timerCallback() override;
    juce::Rectangle<int> navRow (int i) const { return { 24, 168 + i * 28, 170, 24 }; }

    BongProcessor& proc;
    bong::ui::PixelLogo logo;
    bong::ui::RhythmPage rhythmPage;
    bong::ui::AtmosPage atmosPage;
    bong::ui::KaosPage kaosPage;
    bong::ui::SeqPage seqPage;
    int page = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BongEditor)
};
