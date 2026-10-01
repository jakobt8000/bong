#pragma once

// bong UI building blocks: receipt / pixel style, black ink on paper.

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "BinaryData.h"
#include "PluginProcessor.h"

namespace bong::ui
{
inline const juce::Colour paper (0xfff2f1ec);
inline const juce::Colour ink (0xff1e1e1c);
inline const juce::Colour faint (0xff9a9890);

struct Fonts
{
    juce::Typeface::Ptr regular = juce::Typeface::createSystemTypefaceFor (BinaryData::SilkscreenRegular_ttf, BinaryData::SilkscreenRegular_ttfSize);
    juce::Typeface::Ptr bold = juce::Typeface::createSystemTypefaceFor (BinaryData::SilkscreenBold_ttf, BinaryData::SilkscreenBold_ttfSize);
    static Fonts& get() { static Fonts f; return f; }
};
inline juce::Font font (float h, bool b = false)
{
    return juce::Font (juce::FontOptions (b ? Fonts::get().bold : Fonts::get().regular).withHeight (h));
}

inline void dashedH (juce::Graphics& g, float x1, float x2, float y, float dash = 3.0f, float gap = 3.0f)
{
    g.setColour (ink);
    for (float x = x1; x < x2; x += dash + gap) g.fillRect (x, y, std::min (dash, x2 - x), 1.0f);
}
inline void dashedV (juce::Graphics& g, float x, float y1, float y2, float dash = 3.0f, float gap = 3.0f)
{
    g.setColour (ink);
    for (float y = y1; y < y2; y += dash + gap) g.fillRect (x, y, 1.0f, std::min (dash, y2 - y));
}
inline void drawText (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, float h, bool b,
                      juce::Justification j = juce::Justification::centredLeft, juce::Colour c = ink)
{
    g.setColour (c); g.setFont (font (h, b)); g.drawText (t, r, j, false);
}
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

//==============================================================================
// "bong" drawn from a pixel bitmap
class PixelLogo : public juce::Component
{
public:
    void paint (juce::Graphics& g) override
    {
        static const char* glyphs[4][12] = {
            { "##....", "##....", "##....", "#####.", "##..##", "##..##", "##..##", "##..##", "#####.", "......", "......", "......" },
            { "......", "......", "......", ".####.", "##..##", "##..##", "##..##", "##..##", ".####.", "......", "......", "......" },
            { "......", "......", "......", "#####.", "##..##", "##..##", "##..##", "##..##", "##..##", "......", "......", "......" },
            { "......", "......", "......", ".#####", "##..##", "##..##", "##..##", "##..##", ".#####", "....##", "....##", "#####." },
        };
        const float cell = (float) getHeight() / 12.0f;
        g.setColour (ink);
        for (int l = 0; l < 4; ++l)
            for (int r = 0; r < 12; ++r)
                for (int c = 0; c < 6; ++c)
                    if (glyphs[l][r][c] == '#')
                        g.fillRect ((l * 7 + c) * cell, r * cell, cell, cell);
    }
};

//==============================================================================
// Ring of pixel squares around a value. Drag up/down, double-click resets, wheel nudges.
class PixelKnob : public juce::Component
{
public:
    PixelKnob (juce::RangedAudioParameter& p, juce::String labelText, bool big = false)
        : param (p), label (std::move (labelText)), isBig (big)
    {
        attachment = std::make_unique<juce::ParameterAttachment> (param, [this] (float v) { value = v; repaint(); });
        attachment->sendInitialUpdate();
        setRepaintsOnMouseActivity (false);
    }

    std::function<juce::String (float)> textFn;   // value -> text in the middle
    bool dimmed = false;

    void paint (juce::Graphics& g) override
    {
        const float alpha = dimmed ? 0.3f : 1.0f;
        const int labelH = label.isEmpty() ? 0 : (isBig ? 22 : 16);
        auto area = getLocalBounds().toFloat().withTrimmedBottom ((float) labelH);
        const float d = std::min (area.getWidth(), area.getHeight());
        const auto c = area.getCentre();
        const int n = isBig ? 19 : 13;
        const float sq = std::max (3.0f, std::round (d * (isBig ? 0.075f : 0.11f)));
        const float rad = d * 0.5f - sq * 0.6f;
        const float norm = param.convertTo0to1 (value);

        for (int i = 0; i < n; ++i)
        {
            const float frac = (float) i / (float) (n - 1);
            const float a = juce::degreesToRadians (135.0f + frac * 270.0f);
            const float x = std::round (c.x + rad * std::cos (a) - sq * 0.5f);
            const float y = std::round (c.y + rad * std::sin (a) - sq * 0.5f);
            juce::Rectangle<float> r (x, y, sq, sq);
            g.setColour (ink.withAlpha (alpha));
            if (frac <= norm + 0.0001f) g.fillRect (r);
            else g.drawRect (r, 1.0f);
        }

        const juce::String t = textFn ? textFn (value) : juce::String (juce::roundToInt (norm * 100.0f));
        g.setColour (ink.withAlpha (alpha));
        g.setFont (font (isBig ? d * 0.17f : 12.0f, isBig));
        g.drawText (t, area, juce::Justification::centred, false);

        if (labelH > 0)
        {
            g.setFont (font (isBig ? 13.0f : 10.0f, true));
            g.drawText (label, getLocalBounds().removeFromBottom (labelH), juce::Justification::centred, false);
        }
    }

    void mouseDown (const juce::MouseEvent&) override { startNorm = param.convertTo0to1 (value); attachment->beginGesture(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float sens = e.mods.isShiftDown() ? 800.0f : 200.0f;
        const float nn = juce::jlimit (0.0f, 1.0f, startNorm - (float) e.getDistanceFromDragStartY() / sens);
        attachment->setValueAsPartOfGesture (param.convertFrom0to1 (nn));
    }
    void mouseUp (const juce::MouseEvent&) override { attachment->endGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        attachment->setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const float nn = juce::jlimit (0.0f, 1.0f, param.convertTo0to1 (value) + w.deltaY * 0.05f);
        attachment->setValueAsCompleteGesture (param.convertFrom0to1 (nn));
    }

private:
    juce::RangedAudioParameter& param;
    juce::String label;
    bool isBig;
    float value = 0, startNorm = 0;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

//==============================================================================
// 16 steps in four groups. Click to toggle. Shows the playing step.
class StepStrip : public juce::Component
{
public:
    StepStrip (BongProcessor& p, int fxIndex, float cellSize, float gapSize, float groupGapSize)
        : proc (p), fx (fxIndex), cell (cellSize), gap (gapSize), groupGap (groupGapSize) {}

    float preferredWidth() const { return 16 * cell + 12 * gap + 3 * groupGap; }

    float cellX (int i) const { return (float) i * (cell + gap) + (float) (i / 4) * (groupGap - gap); }

    void paint (juce::Graphics& g) override
    {
        const uint16_t pat = proc.getPattern (fx);
        const int len = (int) proc.apvts.getRawParameterValue ("seq_len")->load();
        const int cur = proc.uiStep.load();
        const float y = std::round (((float) getHeight() - cell) * 0.5f);
        for (int i = 0; i < 16; ++i)
        {
            juce::Rectangle<float> r (std::round (cellX (i)), y, cell, cell);
            const bool on = (pat >> i) & 1;
            const float a = i < len ? 1.0f : 0.3f;
            g.setColour (ink.withAlpha (a));
            if (on) g.fillRect (r);
            else g.drawRect (r, 1.5f);
            if (i == cur)
            {
                g.setColour (on ? paper : ink);
                g.fillRect (r.reduced (cell * 0.36f));
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 16; ++i)
            if (e.x >= cellX (i) - gap * 0.5f && e.x < cellX (i) + cell + gap * 0.5f)
            {
                proc.toggleStep (fx, i);
                repaint();
                return;
            }
    }

private:
    BongProcessor& proc;
    int fx;
    float cell, gap, groupGap;
};

//==============================================================================
// "LABEL ........ VALUE" receipt line. Drag to change, click cycles choices.
class LeaderLine : public juce::Component
{
public:
    LeaderLine (juce::String l, juce::RangedAudioParameter* p = nullptr) : label (std::move (l)), param (p)
    {
        if (param != nullptr)
        {
            attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { value = v; repaint(); });
            attachment->sendInitialUpdate();
        }
    }
    std::function<juce::String (float)> textFn;
    std::function<void()> onClick;

    void paint (juce::Graphics& g) override
    {
        const juce::String v = textFn ? textFn (value) : (param ? param->getCurrentValueAsText() : juce::String());
        auto r = getLocalBounds();
        g.setFont (font (12.0f, true));
        const int lw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label);
        const int vw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), v);
        g.setColour (ink);
        g.drawText (label, r, juce::Justification::centredLeft, false);
        g.drawText (v, r, juce::Justification::centredRight, false);
        const float y = (float) r.getCentreY() + 3.0f;
        for (float x = (float) lw + 4.0f; x < (float) (r.getRight() - vw - 4); x += 3.0f)
            g.fillRect (x, y, 1.0f, 1.0f);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (onClick) { onClick(); return; }
        if (param == nullptr) return;
        if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (param))
        {
            attachment->setValueAsCompleteGesture ((float) ((ch->getIndex() + 1) % ch->choices.size()));
            return;
        }
        startNorm = param->convertTo0to1 (value);
        attachment->beginGesture(); dragging = true;
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        const float nn = juce::jlimit (0.0f, 1.0f, startNorm - (float) e.getDistanceFromDragStartY() / 200.0f);
        attachment->setValueAsPartOfGesture (param->convertFrom0to1 (nn));
    }
    void mouseUp (const juce::MouseEvent&) override { if (dragging) attachment->endGesture(); dragging = false; }

private:
    juce::String label;
    juce::RangedAudioParameter* param;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float value = 0, startNorm = 0;
    bool dragging = false;
};

//==============================================================================
// Simple boxed text button
class BoxButton : public juce::Component
{
public:
    explicit BoxButton (juce::String t) : text (std::move (t)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    std::function<void()> onClick;
    std::function<bool()> isOn;
    juce::String text;
    float textSize = 13.0f;

    void paint (juce::Graphics& g) override
    {
        const bool on = isOn ? isOn() : false;
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (ink);
        if (on) g.fillRect (r); else g.drawRect (r, 2.0f);
        g.setColour (on ? paper : ink);
        g.setFont (font (textSize, true));
        g.drawText (text, getLocalBounds(), juce::Justification::centred, false);
    }
    void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
};

//==============================================================================
// History of a modulator as a strip of bars
class ModGraph : public juce::Component
{
public:
    void push (float v) { hist[(size_t) pos] = v; pos = (pos + 1) % (int) hist.size(); }
    bool bipolar = true;
    void paint (juce::Graphics& g) override
    {
        const int n = (int) hist.size();
        const float w = (float) getWidth() / (float) n;
        const float h = (float) getHeight();
        g.setColour (ink);
        for (int i = 0; i < n; ++i)
        {
            const float v = hist[(size_t) ((pos + i) % n)];
            const float norm = bipolar ? (v + 1.0f) * 0.5f : v;
            const float bh = std::max (2.0f, std::round (juce::jlimit (0.0f, 1.0f, norm) * h / 2.0f) * 2.0f);
            g.fillRect (std::floor (i * w), h - bh, std::ceil (w), bh);
        }
    }
private:
    std::array<float, 56> hist {};
    int pos = 0;
};
} // namespace bong::ui
