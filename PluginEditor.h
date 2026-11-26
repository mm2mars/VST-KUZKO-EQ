#pragma once
#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

class KuzkoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class KuzkoAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit KuzkoAudioProcessorEditor (KuzkoAudioProcessor&);
    ~KuzkoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::String caption;
        std::unique_ptr<SliderAtt> att;
    };

    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<ButtonAtt> att;
    };

    struct BandUI
    {
        Knob gain, freq, q, thresh, range;
        Toggle bell, sc, ext;
        juce::Rectangle<int> column;
        juce::Rectangle<int> sepArea;
        juce::Rectangle<int> headArea;
    };

    void addKnob (Knob& k, const juce::String& paramId, const juce::String& caption,
                  juce::Colour colour);
    void addToggle (Toggle& t, const juce::String& paramId, const juce::String& text);
    void layoutKnob (Knob& k, juce::Rectangle<int> area);

    KuzkoAudioProcessor& processor;
    KuzkoLookAndFeel laf;
    BandUI bandUI[kuzko::numBands];
    Knob inKnob, outKnob, atkKnob, relKnob;
    Toggle bypassButton;
    juce::Rectangle<int> titleArea, footerArea;
    juce::Label freqReadout[kuzko::numBands];

#if KUZKO_HAS_BG
    juce::Image background;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KuzkoAudioProcessorEditor)
};
