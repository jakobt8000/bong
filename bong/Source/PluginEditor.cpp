#include "PluginEditor.h"

using namespace bong::ui;

namespace
{
const char* pageNames[4] = { "HOLD RYTMEN", "ATMOSFÆRE", "KAOS", "SEKVENS" };
}

BongEditor::BongEditor (BongProcessor& p)
    : AudioProcessorEditor (&p), proc (p), rhythmPage (p), atmosPage (p), kaosPage (p), seqPage (p)
{
    addAndMakeVisible (logo);
    for (auto* pg : { (juce::Component*) &rhythmPage, (juce::Component*) &atmosPage, (juce::Component*) &kaosPage, (juce::Component*) &seqPage })
        addChildComponent (pg);

    setSize (1200, 450);
    showPage ((int) proc.apvts.state.getProperty ("page", 0));
    startTimerHz (30);
}

void BongEditor::showPage (int index)
{
    page = juce::jlimit (0, 3, index);
    proc.apvts.state.setProperty ("page", page, nullptr);
    rhythmPage.setVisible (page == 0);
    atmosPage.setVisible (page == 1);
    kaosPage.setVisible (page == 2);
    seqPage.setVisible (page == 3);
    repaint();
}

void BongEditor::timerCallback()
{
    switch (page)
    {
        case 0: rhythmPage.refresh(); break;
        case 1: atmosPage.refresh(); break;
        case 2: kaosPage.refresh(); break;
        case 3: seqPage.refresh(); break;
        default: break;
    }
}

void BongEditor::paint (juce::Graphics& g)
{
    g.fillAll (paper);

    drawText (g, u8 (pageNames[page]), { 26, 112, 180, 18 }, 13.0f, true);

    for (int i = 0; i < 4; ++i)
    {
        auto r = navRow (i);
        const bool active = i == page;
        if (active) { g.setColour (ink); g.fillRect (r); }
        drawText (g, juce::String (i + 1).paddedLeft ('0', 2) + "  " + u8 (pageNames[i]), r.withTrimmedLeft (8), 12.0f, true,
                  juce::Justification::centredLeft, active ? paper : ink);
    }
    dashedH (g, 24.0f, 194.0f, 158.0f);
    dashedH (g, 24.0f, 194.0f, 284.0f);

    drawText (g, "AARHUS * JAKOB2FT", { 24, 404, 180, 14 }, 10.0f, false);
    drawText (g, "V2.0", { 24, 418, 180, 14 }, 10.0f, false);

    dashedV (g, 210.0f, 0.0f, (float) getHeight());
    dashedV (g, 950.0f, 0.0f, (float) getHeight());
}

void BongEditor::resized()
{
    logo.setBounds (26, 28, 162, 72);
    const auto area = juce::Rectangle<int> (210, 0, 990, getHeight());
    for (auto* pg : { (juce::Component*) &rhythmPage, (juce::Component*) &atmosPage, (juce::Component*) &kaosPage, (juce::Component*) &seqPage })
        pg->setBounds (area);
}

void BongEditor::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < 4; ++i)
        if (navRow (i).contains (e.getPosition())) { showPage (i); return; }
}
