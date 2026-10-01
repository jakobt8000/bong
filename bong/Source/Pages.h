#pragma once

#include "UI.h"

namespace bong::ui
{
// Page area: 990 x 450. Middle column x 20..720, right "TOTAL" column x 760..970.
constexpr int kMidX = 20, kMidW = 700, kRightX = 760, kRightW = 210, kCardW = 338, kCardGap = 24;

inline juce::RangedAudioParameter& P (BongProcessor& p, const char* id) { return *p.apvts.getParameter (id); }
inline juce::String pctText (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }

// Draws a card header "01. GATE ........ HAKKER"
inline void cardHeader (juce::Graphics& g, juce::Rectangle<int> r, int num, const juce::String& title, const juce::String& tag,
                        bool showToggle = false, bool on = true)
{
    juce::String t = juce::String (num).paddedLeft ('0', 2) + ". " + title;
    int x = r.getX();
    if (showToggle)
    {
        g.setColour (ink);
        juce::Rectangle<float> box ((float) x, (float) r.getCentreY() - 5.0f, 10.0f, 10.0f);
        if (on) g.fillRect (box); else g.drawRect (box, 1.5f);
        x += 16;
    }
    drawText (g, t, { x, r.getY(), r.getWidth(), r.getHeight() }, 13.0f, true, juce::Justification::centredLeft, on ? ink : faint);
    drawText (g, tag, r, 13.0f, true, juce::Justification::centredRight, on ? ink : faint);
}

//==============================================================================
class RightColumn
{
public:
    static void title (juce::Graphics& g, const juce::String& t)
    {
        drawText (g, t, { kRightX, 22, kRightW, 20 }, 13.0f, true, juce::Justification::centred);
        dashedH (g, (float) kRightX, (float) (kRightX + kRightW), 50.0f);
    }
    static void stars (juce::Graphics& g, int y)
    {
        g.setFont (font (12.0f, true)); g.setColour (ink);
        g.drawText (juce::String::repeatedString ("*", 26), juce::Rectangle<int> (kRightX, y, kRightW, 16), juce::Justification::centredLeft, false);
    }
};

//==============================================================================
class RhythmPage : public juce::Component
{
public:
    explicit RhythmPage (BongProcessor& p) : proc (p)
    {
        for (int f = 0; f < 6; ++f)
        {
            for (int k = 0; k < 3; ++k)
            {
                auto& kd = rhythmFx[f].knobs[k];
                knobs.push_back (std::make_unique<PixelKnob> (P (p, kd.id), u8 (kd.label)));
                addAndMakeVisible (*knobs.back());
            }
            strips.push_back (std::make_unique<StepStrip> (p, f, 15.0f, 3.0f, 9.0f));
            addAndMakeVisible (*strips.back());
        }
        mix = std::make_unique<LeaderLine> ("MIX", &P (p, "rhythm_mix"));
        mix->textFn = pctText;
        swing = std::make_unique<LeaderLine> ("SWING", &P (p, "swing"));
        swing->textFn = pctText;
        sync = std::make_unique<LeaderLine> ("SYNC", &P (p, "sync"));
        big = std::make_unique<PixelKnob> (P (p, "rhythm_mix"), "MIX", true);
        for (auto* c : { (juce::Component*) mix.get(), (juce::Component*) swing.get(), (juce::Component*) sync.get(), (juce::Component*) big.get() })
            addAndMakeVisible (c);
    }

    void refresh() { for (auto& s : strips) s->repaint(); }

    void resized() override
    {
        for (int f = 0; f < 6; ++f)
        {
            const auto card = cardRect (f);
            for (int k = 0; k < 3; ++k)
                knobs[(size_t) (f * 3 + k)]->setBounds (card.getX() + 28 + k * 113 + 56 - 33, card.getY() + 22, 66, 76);
            auto& s = strips[(size_t) f];
            const int w = (int) s->preferredWidth();
            s->setBounds (card.getX() + (kCardW - w) / 2, card.getY() + 100, w, 18);
        }
        mix->setBounds (kRightX, 62, kRightW, 20);
        swing->setBounds (kRightX, 86, kRightW, 20);
        sync->setBounds (kRightX, 110, kRightW, 20);
        big->setBounds (kRightX + 35, 150, 140, 160);
    }

    void paint (juce::Graphics& g) override
    {
        for (int f = 0; f < 6; ++f)
        {
            const auto card = cardRect (f);
            cardHeader (g, card.withHeight (18), f + 1, u8 (rhythmFx[f].title), u8 (rhythmFx[f].tag));
            dashedH (g, (float) card.getX(), (float) card.getRight(), (float) card.getY() + 126.0f);
        }
        RightColumn::title (g, "TOTAL");
        RightColumn::stars (g, 330);
        drawText (g, u8 ("Klik på felterne for at sætte trin."), { kRightX, 360, kRightW, 16 }, 10.0f, false);
    }

private:
    static juce::Rectangle<int> cardRect (int f)
    {
        const int col = f % 2, row = f / 2;
        return { kMidX + col * (kCardW + kCardGap), 22 + row * 138, kCardW, 128 };
    }
    BongProcessor& proc;
    std::vector<std::unique_ptr<PixelKnob>> knobs;
    std::vector<std::unique_ptr<StepStrip>> strips;
    std::unique_ptr<LeaderLine> mix, swing, sync;
    std::unique_ptr<PixelKnob> big;
};

//==============================================================================
class AtmosPage : public juce::Component
{
public:
    explicit AtmosPage (BongProcessor& p) : proc (p)
    {
        for (int f = 0; f < 6; ++f)
        {
            for (int k = 0; k < 3; ++k)
            {
                auto& kd = atmosFx[f].knobs[k];
                knobs.push_back (std::make_unique<PixelKnob> (P (p, kd.id), u8 (kd.label)));
                addAndMakeVisible (*knobs.back());
            }
            onAtt.push_back (std::make_unique<juce::ParameterAttachment> (P (p, atmosOnIds[f]), [this] (float) { updateDim(); repaint(); }));
        }
        mix = std::make_unique<LeaderLine> ("MIX", &P (p, "atmos_mix"));
        mix->textFn = pctText;
        tail = std::make_unique<LeaderLine> ("HALE");
        tail->textFn = [this] (float) { return juce::String (proc.uiTail.load(), 1).replaceCharacter ('.', ',') + " S"; };
        big = std::make_unique<PixelKnob> (P (p, "atmos_mix"), "MIX", true);
        freezeBtn = std::make_unique<BoxButton> ("[ ] FRYS NU");
        freezeBtn->isOn = [this] { return proc.apvts.getRawParameterValue ("freeze")->load() > 0.5f; };
        freezeBtn->onClick = [this]
        {
            auto& fp = P (proc, "freeze");
            const bool on = fp.getValue() > 0.5f;
            fp.beginChangeGesture(); fp.setValueNotifyingHost (on ? 0.0f : 1.0f); fp.endChangeGesture();
            // Freezing needs FRYS switched on
            auto& fo = P (proc, atmosOnIds[1]);
            if (! on && fo.getValue() < 0.5f) { fo.beginChangeGesture(); fo.setValueNotifyingHost (1.0f); fo.endChangeGesture(); }
        };
        for (auto* c : { (juce::Component*) mix.get(), (juce::Component*) tail.get(), (juce::Component*) big.get(), (juce::Component*) freezeBtn.get() })
            addAndMakeVisible (c);
        updateDim();
    }

    void refresh()
    {
        tail->repaint();
        const bool fz = proc.apvts.getRawParameterValue ("freeze")->load() > 0.5f;
        freezeBtn->text = fz ? "[X] FRYS NU" : "[ ] FRYS NU";
        freezeBtn->repaint();
    }

    void resized() override
    {
        for (int f = 0; f < 6; ++f)
        {
            const auto card = cardRect (f);
            for (int k = 0; k < 3; ++k)
                knobs[(size_t) (f * 3 + k)]->setBounds (card.getX() + 28 + k * 113 + 56 - 33, card.getY() + 24, 66, 76);
        }
        mix->setBounds (kRightX, 62, kRightW, 20);
        tail->setBounds (kRightX, 86, kRightW, 20);
        big->setBounds (kRightX + 35, 140, 140, 160);
        freezeBtn->setBounds (kRightX, 318, kRightW, 44);
    }

    void paint (juce::Graphics& g) override
    {
        for (int f = 0; f < 6; ++f)
        {
            const auto card = cardRect (f);
            cardHeader (g, card.withHeight (18), f + 1, u8 (atmosFx[f].title), u8 (atmosFx[f].tag), true, isOn (f));
            dashedH (g, (float) card.getX(), (float) card.getRight(), (float) card.getY() + 118.0f);
        }
        RightColumn::title (g, "VEJRUDSIGT");
        g.setFont (font (12.0f, true)); g.setColour (ink);
        g.drawText (juce::String::repeatedString ("~", 26), juce::Rectangle<int> (kRightX, 112, kRightW, 16), juce::Justification::centredLeft, false);
        RightColumn::stars (g, 374);
        drawText (g, u8 ("Klik på en titel for at tænde/slukke."), { kRightX, 400, kRightW, 16 }, 10.0f, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int f = 0; f < 6; ++f)
            if (cardRect (f).withHeight (20).contains (e.getPosition()))
            {
                auto& prm = P (proc, atmosOnIds[f]);
                prm.beginChangeGesture(); prm.setValueNotifyingHost (isOn (f) ? 0.0f : 1.0f); prm.endChangeGesture();
            }
    }

private:
    bool isOn (int f) const { return proc.apvts.getRawParameterValue (atmosOnIds[f])->load() > 0.5f; }
    void updateDim()
    {
        for (int f = 0; f < 6; ++f)
            for (int k = 0; k < 3; ++k)
                if ((size_t) (f * 3 + k) < knobs.size()) { knobs[(size_t) (f * 3 + k)]->dimmed = ! isOn (f); knobs[(size_t) (f * 3 + k)]->repaint(); }
    }
    static juce::Rectangle<int> cardRect (int f)
    {
        const int col = f % 2, row = f / 2;
        return { kMidX + col * (kCardW + kCardGap), 22 + row * 130, kCardW, 120 };
    }
    BongProcessor& proc;
    std::vector<std::unique_ptr<PixelKnob>> knobs;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> onAtt;
    std::unique_ptr<LeaderLine> mix, tail;
    std::unique_ptr<PixelKnob> big;
    std::unique_ptr<BoxButton> freezeBtn;
};

//==============================================================================
class KaosPage : public juce::Component
{
public:
    explicit KaosPage (BongProcessor& p) : proc (p)
    {
        for (int m = 0; m < 4; ++m)
        {
            for (int k = 0; k < 3; ++k)
            {
                auto& kd = modFx[m].knobs[k];
                knobs.push_back (std::make_unique<PixelKnob> (P (p, kd.id), u8 (kd.label)));
                addAndMakeVisible (*knobs.back());
            }
            graphs[m].bipolar = m != 2;
            addAndMakeVisible (graphs[m]);

            auto& tp = P (p, modTargetIds[m]);
            targetLine[m] = std::make_unique<LeaderLine> (u8 ("MÅL"));
            targetLine[m]->textFn = [&tp] (float) { return juce::String(); };
            addAndMakeVisible (*targetLine[m]);
            prevBtn[m] = std::make_unique<BoxButton> ("<");
            nextBtn[m] = std::make_unique<BoxButton> (">");
            prevBtn[m]->onClick = [this, m] { stepTarget (m, -1); };
            nextBtn[m]->onClick = [this, m] { stepTarget (m, +1); };
            addAndMakeVisible (*prevBtn[m]); addAndMakeVisible (*nextBtn[m]);
            tgtAtt[m] = std::make_unique<juce::ParameterAttachment> (tp, [this] (float) { repaint(); });
        }
        mods = std::make_unique<LeaderLine> ("MODULATORER");
        mods->textFn = [this] (float) { int n = 0; for (auto* id : modTargetIds) if (proc.apvts.getRawParameterValue (id)->load() > 0.5f) ++n; return juce::String (n); };
        kaos = std::make_unique<LeaderLine> ("KAOS", &P (p, "kaos"));
        kaos->textFn = pctText;
        seed = std::make_unique<LeaderLine> (u8 ("FRØ"), &P (p, "seed"));
        seed->textFn = [] (float v) { return "#" + juce::String::toHexString (juce::roundToInt (v)).toUpperCase().paddedLeft ('0', 4); };
        seed->onClick = [this]
        {
            auto& sp = P (proc, "seed");
            sp.beginChangeGesture(); sp.setValueNotifyingHost (juce::Random::getSystemRandom().nextFloat()); sp.endChangeGesture();
        };
        big = std::make_unique<PixelKnob> (P (p, "kaos"), "KAOS", true);
        for (auto* c : { (juce::Component*) mods.get(), (juce::Component*) kaos.get(), (juce::Component*) seed.get(), (juce::Component*) big.get() })
            addAndMakeVisible (c);
    }

    void refresh()
    {
        for (int m = 0; m < 4; ++m) { graphs[m].push (proc.uiMod[m].load()); graphs[m].repaint(); }
        mods->repaint();
    }

    void resized() override
    {
        for (int m = 0; m < 4; ++m)
        {
            const auto card = cardRect (m);
            graphs[m].setBounds (card.getX(), card.getY() + 26, kCardW, 16);
            for (int k = 0; k < 3; ++k)
                knobs[(size_t) (m * 3 + k)]->setBounds (card.getX() + 28 + k * 113 + 56 - 33, card.getY() + 48, 66, 76);
            targetLine[m]->setBounds (card.getX(), card.getY() + 140, 150, 26);
            prevBtn[m]->setBounds (card.getX() + 156, card.getY() + 138, 30, 30);
            nextBtn[m]->setBounds (card.getRight() - 30, card.getY() + 138, 30, 30);
        }
        mods->setBounds (kRightX, 62, kRightW, 20);
        kaos->setBounds (kRightX, 86, kRightW, 20);
        seed->setBounds (kRightX, 110, kRightW, 20);
        big->setBounds (kRightX + 35, 140, 140, 160);
    }

    void paint (juce::Graphics& g) override
    {
        for (int m = 0; m < 4; ++m)
        {
            const auto card = cardRect (m);
            cardHeader (g, card.withHeight (18), m + 1, u8 (modFx[m].title), u8 (modFx[m].tag));
            const int t = juce::jlimit (0, numTargets - 1, (int) proc.apvts.getRawParameterValue (modTargetIds[m])->load());
            drawText (g, u8 (targets[t].name), { card.getX() + 190, card.getY() + 138, kCardW - 226, 30 }, 12.0f, true, juce::Justification::centred);
            dashedH (g, (float) card.getX(), (float) card.getRight(), (float) card.getY() + 184.0f);
        }
        RightColumn::title (g, "TOTAL");
        drawText (g, u8 ("KAOS skalerer alle fire"), { kRightX, 318, kRightW, 16 }, 10.0f, false);
        drawText (g, u8 ("modulatorer på én gang."), { kRightX, 334, kRightW, 16 }, 10.0f, false);
        drawText (g, u8 ("Klik FRØ for nyt tilfælde."), { kRightX, 350, kRightW, 16 }, 10.0f, false);
        RightColumn::stars (g, 378);
    }

private:
    void stepTarget (int m, int dir)
    {
        auto* ch = dynamic_cast<juce::AudioParameterChoice*> (&P (proc, modTargetIds[m]));
        const int n = ch->choices.size();
        const int idx = (ch->getIndex() + dir + n) % n;
        ch->beginChangeGesture(); ch->setValueNotifyingHost (ch->convertTo0to1 ((float) idx)); ch->endChangeGesture();
    }
    static juce::Rectangle<int> cardRect (int m)
    {
        const int col = m % 2, row = m / 2;
        return { kMidX + col * (kCardW + kCardGap), 22 + row * 204, kCardW, 190 };
    }
    BongProcessor& proc;
    std::vector<std::unique_ptr<PixelKnob>> knobs;
    ModGraph graphs[4];
    std::unique_ptr<LeaderLine> targetLine[4];
    std::unique_ptr<BoxButton> prevBtn[4], nextBtn[4];
    std::unique_ptr<juce::ParameterAttachment> tgtAtt[4];
    std::unique_ptr<LeaderLine> mods, kaos, seed;
    std::unique_ptr<PixelKnob> big;
};

//==============================================================================
class SeqPage : public juce::Component
{
public:
    explicit SeqPage (BongProcessor& p) : proc (p)
    {
        for (int f = 0; f < 6; ++f)
        {
            strips.push_back (std::make_unique<StepStrip> (p, f, 24.0f, 3.0f, 9.0f));
            addAndMakeVisible (*strips.back());
            amounts.push_back (std::make_unique<PixelKnob> (P (p, amountIds[f]), juce::String()));
            amounts.back()->textFn = [] (float) { return juce::String(); };
            addAndMakeVisible (*amounts.back());
            amtAtt.push_back (std::make_unique<juce::ParameterAttachment> (P (p, amountIds[f]), [this] (float) { repaint(); }));
        }
        mix = std::make_unique<LeaderLine> ("MIX", &P (p, "rhythm_mix"));
        mix->textFn = pctText;
        len = std::make_unique<LeaderLine> (u8 ("LÆNGDE"), &P (p, "seq_len"));
        len->textFn = [] (float v) { return juce::String (juce::roundToInt (v)) + " TRIN"; };
        dir = std::make_unique<LeaderLine> ("RETNING", &P (p, "seq_dir"));
        bigMix = std::make_unique<PixelKnob> (P (p, "rhythm_mix"), "MIX", true);
        bigLen = std::make_unique<PixelKnob> (P (p, "seq_len"), u8 ("LÆNGDE"), true);
        bigLen->textFn = [] (float v) { return juce::String (juce::roundToInt (v)); };
        for (auto* c : { (juce::Component*) mix.get(), (juce::Component*) len.get(), (juce::Component*) dir.get(),
                         (juce::Component*) bigMix.get(), (juce::Component*) bigLen.get() })
            addAndMakeVisible (c);
        lenAtt = std::make_unique<juce::ParameterAttachment> (P (p, "seq_len"), [this] (float) { repaint(); for (auto& s : strips) s->repaint(); });
    }

    void refresh()
    {
        for (auto& s : strips) s->repaint();
        const int cur = proc.uiStep.load();
        if (cur != lastStep) { lastStep = cur; repaint (juce::Rectangle<int> (kMidX, 40, kMidW, 22)); }
    }

    void resized() override
    {
        for (int f = 0; f < 6; ++f)
        {
            const int y = rowY (f);
            strips[(size_t) f]->setBounds (kMidX + gridX, y, (int) strips[(size_t) f]->preferredWidth(), 36);
            amounts[(size_t) f]->setBounds (kMidX + 610, y - 2, 40, 40);
        }
        mix->setBounds (kRightX, 62, kRightW, 20);
        len->setBounds (kRightX, 86, kRightW, 20);
        dir->setBounds (kRightX, 110, kRightW, 20);
        bigMix->setBounds (kRightX, 150, 100, 120);
        bigLen->setBounds (kRightX + 110, 150, 100, 120);
    }

    void paint (juce::Graphics& g) override
    {
        drawText (g, "EFFEKT PR. TRIN", { kMidX, 18, 300, 18 }, 13.0f, true);
        const int L = (int) proc.apvts.getRawParameterValue ("seq_len")->load();
        drawText (g, u8 ("1 TAKT · ") + juce::String (L) + " TRIN", { kMidX, 18, kMidW, 18 }, 13.0f, true, juce::Justification::centredRight);
        dashedH (g, (float) kMidX, (float) (kMidX + kMidW), 40.0f);

        auto* s0 = strips.empty() ? nullptr : strips[0].get();
        const int cur = proc.uiStep.load();
        for (int i = 0; i < 16 && s0 != nullptr; ++i)
        {
            juce::Rectangle<int> r (kMidX + gridX + (int) s0->cellX (i), 46, 24, 14);
            if (i == cur) { g.setColour (ink); g.fillRect (r); }
            drawText (g, juce::String (i + 1), r, 10.0f, true, juce::Justification::centred, i == cur ? paper : (i < L ? ink : faint));
        }
        drawText (g, u8 ("MÆNGDE"), { kMidX + 596, 46, 104, 14 }, 10.0f, true, juce::Justification::centredLeft);

        for (int f = 0; f < 6; ++f)
        {
            const int y = rowY (f);
            drawText (g, juce::String (f + 1).paddedLeft ('0', 2) + ". " + u8 (rhythmFx[f].title), { kMidX, y + 10, gridX, 16 }, 11.0f, true);
            const float amt = proc.apvts.getRawParameterValue (amountIds[f])->load();
            drawText (g, juce::String (juce::roundToInt (amt * 100.0f)), { kMidX + 654, y + 8, 40, 18 }, 12.0f, true);
            dashedH (g, (float) kMidX, (float) (kMidX + kMidW), (float) y + 44.0f);
        }
        RightColumn::title (g, "TOTAL");
        RightColumn::stars (g, 292);
        drawText (g, u8 ("Retning: > frem, < bagud,"), { kRightX, 320, kRightW, 16 }, 10.0f, false);
        drawText (g, u8 ("<> frem og tilbage, ? tilfældig."), { kRightX, 336, kRightW, 16 }, 10.0f, false);
    }

private:
    static constexpr int gridX = 140;
    static int rowY (int f) { return 66 + f * 60; }
    BongProcessor& proc;
    std::vector<std::unique_ptr<StepStrip>> strips;
    std::vector<std::unique_ptr<PixelKnob>> amounts;
    std::unique_ptr<LeaderLine> mix, len, dir;
    std::unique_ptr<PixelKnob> bigMix, bigLen;
    std::unique_ptr<juce::ParameterAttachment> lenAtt;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> amtAtt;
    int lastStep = -2;
};
} // namespace bong::ui
