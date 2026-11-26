#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
// Plages de frequence par bande (Hz) : min, max, valeur par defaut
struct FreqRange { float lo, hi, def; };
const FreqRange freqRanges[kuzko::numBands] = {
    {  20.0f,   300.0f,    60.0f },
    {  80.0f,   800.0f,   250.0f },
    { 200.0f,  2000.0f,   700.0f },
    { 600.0f,  6000.0f,  1800.0f },
    { 2000.0f, 12000.0f, 5000.0f },
    { 3000.0f, 20000.0f, 12000.0f }
};

juce::String freqText (float v, int)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz"
                        : juce::String (juce::roundToInt (v)) + " Hz";
}
juce::String dbText (float v, int) { return juce::String (v, 1) + " dB"; }
juce::String msText (float v, int) { return juce::String (v, 1) + " ms"; }
juce::String qText (float v, int) { return juce::String (v, 2); }
}

//==============================================================================
void KuzkoAudioProcessor::Biquad::makePeak (double fs, double f, double q, double gainDb)
{
    f = juce::jlimit (10.0, fs * 0.45, f);
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = juce::MathConstants<double>::twoPi * f / fs;
    const double cs = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);
    set (1.0 + alpha * A, -2.0 * cs, 1.0 - alpha * A,
         1.0 + alpha / A, -2.0 * cs, 1.0 - alpha / A);
}

void KuzkoAudioProcessor::Biquad::makeLowShelf (double fs, double f, double q, double gainDb)
{
    f = juce::jlimit (10.0, fs * 0.45, f);
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = juce::MathConstants<double>::twoPi * f / fs;
    const double cs = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);
    const double s = 2.0 * std::sqrt (A) * alpha;
    set (A * ((A + 1) - (A - 1) * cs + s),
         2.0 * A * ((A - 1) - (A + 1) * cs),
         A * ((A + 1) - (A - 1) * cs - s),
         (A + 1) + (A - 1) * cs + s,
         -2.0 * ((A - 1) + (A + 1) * cs),
         (A + 1) + (A - 1) * cs - s);
}

void KuzkoAudioProcessor::Biquad::makeHighShelf (double fs, double f, double q, double gainDb)
{
    f = juce::jlimit (10.0, fs * 0.45, f);
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = juce::MathConstants<double>::twoPi * f / fs;
    const double cs = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);
    const double s = 2.0 * std::sqrt (A) * alpha;
    set (A * ((A + 1) + (A - 1) * cs + s),
         -2.0 * A * ((A - 1) + (A + 1) * cs),
         A * ((A + 1) + (A - 1) * cs - s),
         (A + 1) - (A - 1) * cs + s,
         2.0 * ((A - 1) - (A + 1) * cs),
         (A + 1) - (A - 1) * cs - s);
}

void KuzkoAudioProcessor::Biquad::makeBandPass (double fs, double f, double q)
{
    f = juce::jlimit (10.0, fs * 0.45, f);
    const double w0 = juce::MathConstants<double>::twoPi * f / fs;
    const double cs = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);
    set (alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * cs, 1.0 - alpha);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout KuzkoAudioProcessor::createLayout()
{
    using namespace juce;
    using Float = AudioParameterFloat;
    using Attr = AudioParameterFloatAttributes;

    AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < kuzko::numBands; ++i)
    {
        const auto n = String (i + 1);
        const auto& fr = freqRanges[i];

        NormalisableRange<float> freqRange (fr.lo, fr.hi, 0.0f);
        freqRange.setSkewForCentre (std::sqrt (fr.lo * fr.hi));

        layout.add (std::make_unique<Float> (ParameterID { kuzko::pid (i, "freq"), 1 }, "Band " + n + " freq",
                                             freqRange, fr.def, Attr().withStringFromValueFunction (freqText)));

        layout.add (std::make_unique<Float> (ParameterID { kuzko::pid (i, "gain"), 1 }, "Band " + n + " gain",
                                             NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f,
                                             Attr().withStringFromValueFunction (dbText)));

        NormalisableRange<float> qRange (0.3f, 8.0f, 0.0f);
        qRange.setSkewForCentre (1.5f);
        layout.add (std::make_unique<Float> (ParameterID { kuzko::pid (i, "q"), 1 }, "Band " + n + " Q",
                                             qRange, 1.0f, Attr().withStringFromValueFunction (qText)));

        layout.add (std::make_unique<AudioParameterBool> (ParameterID { kuzko::pid (i, "bell"), 1 }, "Band " + n + " bell", false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { kuzko::pid (i, "sc"), 1 }, "Band " + n + " sidechain", false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { kuzko::pid (i, "ext"), 1 }, "Band " + n + " external", false));

        layout.add (std::make_unique<Float> (ParameterID { kuzko::pid (i, "thresh"), 1 }, "Band " + n + " threshold",
                                             NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -30.0f,
                                             Attr().withStringFromValueFunction (dbText)));

        layout.add (std::make_unique<Float> (ParameterID { kuzko::pid (i, "range"), 1 }, "Band " + n + " range",
                                             NormalisableRange<float> (1.0f, 24.0f, 0.1f), 6.0f,
                                             Attr().withStringFromValueFunction (dbText)));
    }

    layout.add (std::make_unique<Float> (ParameterID { "in_gain", 1 }, "Input gain",
                                         NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                         Attr().withStringFromValueFunction (dbText)));
    layout.add (std::make_unique<Float> (ParameterID { "out_gain", 1 }, "Output gain",
                                         NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                         Attr().withStringFromValueFunction (dbText)));

    NormalisableRange<float> atk (0.1f, 100.0f, 0.0f);
    atk.setSkewForCentre (10.0f);
    layout.add (std::make_unique<Float> (ParameterID { "sc_attack", 1 }, "Sidechain attack",
                                         atk, 10.0f, Attr().withStringFromValueFunction (msText)));

    NormalisableRange<float> rel (5.0f, 1000.0f, 0.0f);
    rel.setSkewForCentre (100.0f);
    layout.add (std::make_unique<Float> (ParameterID { "sc_release", 1 }, "Sidechain release",
                                         rel, 100.0f, Attr().withStringFromValueFunction (msText)));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

//==============================================================================
KuzkoAudioProcessor::KuzkoAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "KUZKO", createLayout())
{
    for (int i = 0; i < kuzko::numBands; ++i)
    {
        bp[i].freq = apvts.getRawParameterValue (kuzko::pid (i, "freq"));
        bp[i].gain = apvts.getRawParameterValue (kuzko::pid (i, "gain"));
        bp[i].q = apvts.getRawParameterValue (kuzko::pid (i, "q"));
        bp[i].bell = apvts.getRawParameterValue (kuzko::pid (i, "bell"));
        bp[i].sc = apvts.getRawParameterValue (kuzko::pid (i, "sc"));
        bp[i].ext = apvts.getRawParameterValue (kuzko::pid (i, "ext"));
        bp[i].thresh = apvts.getRawParameterValue (kuzko::pid (i, "thresh"));
        bp[i].range = apvts.getRawParameterValue (kuzko::pid (i, "range"));
    }
    inGain = apvts.getRawParameterValue ("in_gain");
    outGain = apvts.getRawParameterValue ("out_gain");
    attackMs = apvts.getRawParameterValue ("sc_attack");
    releaseMs = apvts.getRawParameterValue ("sc_release");
    bypass = apvts.getRawParameterValue ("bypass");
}

bool KuzkoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    if (mainIn != mainOut)
        return false;

    const auto sc = layouts.getChannelSet (true, 1);
    return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
}

void KuzkoAudioProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    for (auto& b : bands)
    {
        b.eq.reset();
        b.det.reset();
        b.env = 0.0;
    }
}

void KuzkoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto main = getBusBuffer (buffer, true, 0);
    auto sc = getBusBuffer (buffer, true, 1);
    auto* scBus = getBus (true, 1);
    const bool scAvailable = scBus != nullptr && scBus->isEnabled() && sc.getNumChannels() > 0;

    const int numCh = juce::jmin (2, main.getNumChannels());
    const int numSamples = buffer.getNumSamples();

    if (bypass->load() > 0.5f || numCh == 0)
        return;

    main.applyGain (juce::Decibels::decibelsToGain (inGain->load()));

    const double atkCoeff = std::exp (-1.0 / (juce::jmax (0.05f, attackMs->load()) * 0.001 * sampleRate));
    const double relCoeff = std::exp (-1.0 / (juce::jmax (1.0f, releaseMs->load()) * 0.001 * sampleRate));

    constexpr int chunk = 32;
    double effGain[kuzko::numBands];

    for (int start = 0; start < numSamples; start += chunk)
    {
        const int len = juce::jmin (chunk, numSamples - start);

        // 1) detection (sur le signal non egalise du bloc)
        for (int b = 0; b < kuzko::numBands; ++b)
        {
            auto& band = bands[b];
            const double gain = bp[b].gain->load();
            effGain[b] = gain;

            if (bp[b].sc->load() < 0.5f)
            {
                band.env = 0.0;
                continue;
            }

            const double f = bp[b].freq->load();
            const double q = juce::jmax (0.5f, bp[b].q->load());
            band.det.makeBandPass (sampleRate, f, q);

            const bool useExt = bp[b].ext->load() > 0.5f && scAvailable;
            const auto& src = useExt ? sc : main;
            const int srcCh = src.getNumChannels();

            for (int s = 0; s < len; ++s)
            {
                double x = 0.0;
                for (int c = 0; c < srcCh; ++c)
                    x += src.getSample (c, start + s);
                x /= (double) srcCh;

                const double y = std::abs (band.det.process (x, 0));
                const double coeff = y > band.env ? atkCoeff : relCoeff;
                band.env = y + coeff * (band.env - y);
            }

            const float levelDb = juce::Decibels::gainToDecibels ((float) band.env, -100.0f);
            const float thresh = bp[b].thresh->load();
            const float range = juce::jmax (1.0f, bp[b].range->load());
            const float amount = juce::jlimit (0.0f, 1.0f, (levelDb - thresh) / range);
            effGain[b] = gain * (double) amount;
        }

        // 2) egalisation
        for (int b = 0; b < kuzko::numBands; ++b)
        {
            auto& band = bands[b];
            const double f = bp[b].freq->load();
            const double q = bp[b].q->load();
            const bool bell = bp[b].bell->load() > 0.5f;

            if (b == 0 && ! bell)
                band.eq.makeLowShelf (sampleRate, f, q, effGain[b]);
            else if (b == kuzko::numBands - 1 && ! bell)
                band.eq.makeHighShelf (sampleRate, f, q, effGain[b]);
            else
                band.eq.makePeak (sampleRate, f, q, effGain[b]);

            for (int c = 0; c < numCh; ++c)
            {
                auto* data = main.getWritePointer (c, start);
                for (int s = 0; s < len; ++s)
                    data[s] = (float) band.eq.process ((double) data[s], c);
            }
        }
    }

    main.applyGain (juce::Decibels::decibelsToGain (outGain->load()));
}

juce::AudioProcessorEditor* KuzkoAudioProcessor::createEditor()
{
    return new KuzkoAudioProcessorEditor (*this);
}

void KuzkoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void KuzkoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KuzkoAudioProcessor();
}
