#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

namespace kuzko
{
constexpr int numBands = 6;

inline juce::String pid (int band, const char* name)
{
    return "b" + juce::String (band + 1) + "_" + name;
}
}

class KuzkoAudioProcessor : public juce::AudioProcessor
{
public:
    KuzkoAudioProcessor();
    ~KuzkoAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Kuzko EQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1[2] { 0, 0 }, z2[2] { 0, 0 };

        void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }

        inline double process (double in, int ch)
        {
            const double out = b0 * in + z1[ch];
            z1[ch] = b1 * in - a1 * out + z2[ch];
            z2[ch] = b2 * in - a2 * out;
            return out;
        }

        void set (double nb0, double nb1, double nb2, double a0, double na1, double na2)
        {
            b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0;
            a1 = na1 / a0; a2 = na2 / a0;
        }

        void makePeak (double fs, double f, double q, double gainDb);
        void makeLowShelf (double fs, double f, double q, double gainDb);
        void makeHighShelf (double fs, double f, double q, double gainDb);
        void makeBandPass (double fs, double f, double q);
    };

    struct Band
    {
        Biquad eq;
        Biquad det;
        double env = 0.0;
    };

    struct BandParams
    {
        std::atomic<float>* freq = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* q = nullptr;
        std::atomic<float>* bell = nullptr;
        std::atomic<float>* sc = nullptr;
        std::atomic<float>* ext = nullptr;
        std::atomic<float>* thresh = nullptr;
        std::atomic<float>* range = nullptr;
    };

    Band bands[kuzko::numBands];
    BandParams bp[kuzko::numBands];
    std::atomic<float>* inGain = nullptr;
    std::atomic<float>* outGain = nullptr;
    std::atomic<float>* attackMs = nullptr;
    std::atomic<float>* releaseMs = nullptr;
    std::atomic<float>* bypass = nullptr;
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KuzkoAudioProcessor)
};
