#include "PluginEditor.h"

namespace
{
const juce::Colour yellow (0xfff2c14e);
const juce::Colour darkCap (0xff24262d);
const juce::Colour bandColours[kuzko::numBands] = {
    juce::Colour (0xff3a3c44), juce::Colour (0xff2f6fb0), juce::Colour (0xff2f6fb0),
    juce::Colour (0xff2f7a4f), juce::Colour (0xff2f7a4f), juce::Colour (0xffc0392b)
};

juce::String freqString (double v)
{
    return v >= 1000.0 ? juce::String (v / 1000.0, 2) + " kHz"
                       : juce::String (juce::roundToInt (v)) + " Hz";
}
}

//==============================================================================
void KuzkoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float startAngle, float endAngle,
                                         juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float radius = size * 0.5f;
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    g.setColour (juce::Colour (0xff9a9ca6));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const float r1 = radius - 2.0f;
        const float r2 = radius;
        g.drawLine (centre.x + std::sin (a) * r1, centre.y - std::cos (a) * r1,
                    centre.x + std::sin (a) * r2, centre.y - std::cos (a) * r2, 1.0f);
    }

    const float capR = radius - 5.0f;
    g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
    g.fillEllipse (centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (juce::Colour (0xff0a0b0e));
    g.drawEllipse (centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f, 1.5f);

    g.setColour (juce::Colours::white);
    const float r0 = capR * 0.15f;
    const float r1 = capR * 0.9f;
    g.drawLine (centre.x + std::sin (angle) * r0, centre.y - std::cos (angle) * r0,
                centre.x + std::sin (angle) * r1, centre.y - std::cos (angle) * r1, 2.2f);
}

void KuzkoLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? yellow : juce::Colour (0xff1f2026));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? yellow : juce::Colour (0xff3a3c44));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (on ? juce::Colour (0xff14151a) : juce::Colour (0xff9a9ca6));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

//==============================================================================
KuzkoAudioProcessorEditor::KuzkoAudioProcessorEditor (KuzkoAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&laf);

#if KUZKO_HAS_BG
    background = juce::ImageCache::getFromMemory (KUZKO_BG_DATA, (int) KUZKO_BG_SIZE);
#endif

    for (int i = 0; i < kuzko::numBands; ++i)
    {
        auto& b = bandUI[i];
        addKnob (b.gain, kuzko::pid (i, "gain"), "Gain", bandColours[i]);
        addKnob (b.freq, kuzko::pid (i, "freq"), "Freq", darkCap);
        addKnob (b.q, kuzko::pid (i, "q"), "Q", darkCap);
        addKnob (b.thresh, kuzko::pid (i, "thresh"), "Thresh", darkCap);
        addKnob (b.range, kuzko::pid (i, "range"), "Range", darkCap);
        addToggle (b.sc, kuzko::pid (i, "sc"), "SC");
        addToggle (b.ext, kuzko::pid (i, "ext"), "Ext");

        if (i == 0 || i == kuzko::numBands - 1)
            addToggle (b.bell, kuzko::pid (i, "bell"), "Bell");

        auto& lbl = freqReadout[i];
        lbl.setJustificationType (juce::Justification::centred);
        lbl.setColour (juce::Label::textColourId, juce::Colour (0xffc9c7bf));
        lbl.setFont (juce::FontOptions (13.0f));
        lbl.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (lbl);

        auto* freqSlider = &b.freq.slider;
        auto* lblPtr = &lbl;
        freqSlider->onValueChange = [freqSlider, lblPtr]
        {
            lblPtr->setText (freqString (freqSlider->getValue()), juce::dontSendNotification);
        };
        lbl.setText (freqString (freqSlider->getValue()), juce::dontSendNotification);
    }

    addKnob (inKnob, "in_gain", "In", darkCap);
    addKnob (outKnob, "out_gain", "Out", darkCap);
    addKnob (atkKnob, "sc_attack", "SC attack", darkCap);
    addKnob (relKnob, "sc_release", "SC release", darkCap);
    addToggle (bypassButton, "bypass", "Bypass");

    setResizable (true, true);
    setResizeLimits (640, 600, 1400, 1200);
    getConstrainer()->setFixedAspectRatio (780.0 / 760.0);
    setSize (780, 760);
}

KuzkoAudioProcessorEditor::~KuzkoAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void KuzkoAudioProcessorEditor::addKnob (Knob& k, const juce::String& paramId,
                                         const juce::String& caption, juce::Colour colour)
{
    k.caption = caption;
    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                  juce::MathConstants<float>::pi * 2.75f, true);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    k.slider.setPopupDisplayEnabled (true, true, this);
    addAndMakeVisible (k.slider);
    k.att = std::make_unique<SliderAtt> (processor.apvts, paramId, k.slider);
}

void KuzkoAudioProcessorEditor::addToggle (Toggle& t, const juce::String& paramId,
                                           const juce::String& text)
{
    t.button.setButtonText (text);
    addAndMakeVisible (t.button);
    t.att = std::make_unique<ButtonAtt> (processor.apvts, paramId, t.button);
}

void KuzkoAudioProcessorEditor::layoutKnob (Knob& k, juce::Rectangle<int> area)
{
    auto knobArea = area.withTrimmedBottom (14);
    const int s = juce::jmin (knobArea.getWidth(), knobArea.getHeight());
    k.slider.setBounds (knobArea.withSizeKeepingCentre (s, s));
}

void KuzkoAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14151a));

#if KUZKO_HAS_BG
    if (background.isValid())
    {
        g.drawImage (background, getLocalBounds().toFloat(),
                     juce::RectanglePlacement::fillDestination | juce::RectanglePlacement::centred);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillAll();
    }
#endif

    g.setColour (yellow);
    g.setFont (juce::FontOptions ((float) titleArea.getHeight() * 0.62f, juce::Font::bold));
    g.drawText ("K U Z K O", titleArea.reduced (14, 0), juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff8a8fa6));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("EQ 6 bandes  |  sidechain par bande", titleArea.reduced (14, 0),
                juce::Justification::centredRight);

    for (int i = 0; i < kuzko::numBands; ++i)
    {
        auto& b = bandUI[i];
        g.setColour (juce::Colour (0xff1c1d23).withAlpha (0.82f));
        g.fillRoundedRectangle (b.column.toFloat(), 8.0f);
        g.setColour (juce::Colour (0xff2a2c34));
        g.drawRoundedRectangle (b.column.toFloat().reduced (0.5f), 8.0f, 1.0f);

        g.setColour (yellow);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (juce::String (i + 1), b.headArea, juce::Justification::centredTop);

        g.setColour (juce::Colour (0xff2a2c34));
        g.drawHorizontalLine (b.sepArea.getY(), (float) b.sepArea.getX(), (float) b.sepArea.getRight());
        g.setColour (juce::Colour (0xff8a8fa6));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("Sidechain", b.sepArea.withTrimmedTop (2), juce::Justification::centredTop);

        g.setColour (juce::Colour (0xff8a8fa6));
        g.setFont (juce::FontOptions (12.0f));
        for (auto* k : { &b.gain, &b.freq, &b.q, &b.thresh, &b.range })
        {
            auto r = k->slider.getBounds();
            g.drawText (k->caption, r.getX() - 10, r.getBottom(), r.getWidth() + 20, 14,
                        juce::Justification::centred);
        }
    }

    g.setColour (juce::Colour (0xff1c1d23).withAlpha (0.82f));
    g.fillRoundedRectangle (footerArea.toFloat(), 8.0f);
    g.setColour (juce::Colour (0xff8a8fa6));
    g.setFont (juce::FontOptions (12.0f));
    for (auto* k : { &inKnob, &outKnob, &atkKnob, &relKnob })
    {
        auto r = k->slider.getBounds();
        g.drawText (k->caption, r.getX() - 14, r.getBottom(), r.getWidth() + 28, 14,
                    juce::Justification::centred);
    }
}

void KuzkoAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    titleArea = area.removeFromTop (36);
    area.removeFromTop (4);
    footerArea = area.removeFromBottom (86);
    area.removeFromBottom (6);

    const int gap = 6;
    const int colW = (area.getWidth() - gap * (kuzko::numBands - 1)) / kuzko::numBands;

    // hauteurs relatives des elements d'une colonne
    const float hHead = 20, hReadout = 18, hGain = 78, hKnob = 62, hBell = 30,
                hSep = 20, hBtn = 30, hLast = 62;
    const float total = hHead + hReadout + hGain + hKnob * 3 + hBell + hSep + hBtn * 2 + hLast;
    const float scale = (float) area.getHeight() / (total + 16.0f);

    for (int i = 0; i < kuzko::numBands; ++i)
    {
        auto& b = bandUI[i];
        b.column = juce::Rectangle<int> (area.getX() + i * (colW + gap), area.getY(), colW, area.getHeight());
        auto col = b.column.reduced (6, 8);
        auto take = [&] (float h) { return col.removeFromTop (juce::roundToInt (h * scale)); };

        b.headArea = take (hHead);
        freqReadout[i].setBounds (take (hReadout));
        layoutKnob (b.gain, take (hGain));
        layoutKnob (b.freq, take (hKnob));
        layoutKnob (b.q, take (hKnob));

        auto bellArea = take (hBell);
        if (i == 0 || i == kuzko::numBands - 1)
            b.bell.button.setBounds (bellArea.reduced (colW / 8, 4));

        b.sepArea = take (hSep);
        b.sc.button.setBounds (take (hBtn).reduced (colW / 8, 3));
        b.ext.button.setBounds (take (hBtn).reduced (colW / 8, 3));
        layoutKnob (b.thresh, take (hKnob));
        layoutKnob (b.range, col);
    }

    auto foot = footerArea.reduced (10, 6);
    const int cell = foot.getWidth() / 5;
    layoutKnob (inKnob, foot.removeFromLeft (cell));
    layoutKnob (outKnob, foot.removeFromLeft (cell));
    layoutKnob (atkKnob, foot.removeFromLeft (cell));
    layoutKnob (relKnob, foot.removeFromLeft (cell));
    bypassButton.button.setBounds (foot.reduced (10, foot.getHeight() / 3));
}
