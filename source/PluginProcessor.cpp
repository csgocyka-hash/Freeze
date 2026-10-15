#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/Presets.h"

FreezeProcessor::FreezeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    freezeParam = apvts.getRawParameterValue ("freeze");
    grainParam = apvts.getRawParameterValue ("grain");
    fxParam = apvts.getRawParameterValue ("fx");
    presetParam = apvts.getRawParameterValue ("preset");
    holdParam = apvts.getRawParameterValue ("hold");
    snowParam = apvts.getRawParameterValue ("noise");
    detuneParam = apvts.getRawParameterValue ("detune");
    divisionParam = apvts.getRawParameterValue ("division");
    unlinkParam = apvts.getRawParameterValue ("unlink");
    timeParam = apvts.getRawParameterValue ("time");
    invertParam = apvts.getRawParameterValue ("invert");
    apvts.addParameterListener ("preset", this);
}

FreezeProcessor::~FreezeProcessor()
{
    apvts.removeParameterListener ("preset", this);
    cancelPendingUpdate();
}

int FreezeProcessor::currentPreset() const
{
    return juce::jlimit (0, gf::kNumPresets - 1, (int) presetParam->load());
}

void FreezeProcessor::parameterChanged (const juce::String&, float newValue)
{
    if (restoringState)
        return;
    pendingPreset = juce::jlimit (0, gf::kNumPresets - 1, juce::roundToInt (newValue));
    triggerAsyncUpdate();
}

void FreezeProcessor::handleAsyncUpdate()
{
    const int index = pendingPreset.exchange (-1);
    if (index < 0)
        return;
    const auto& preset = gf::preset (index);
    setParameterValue ("grain", gf::grainSecondsToKnob (preset.grainSeconds));
    setParameterValue ("fx", preset.fx);
    setParameterValue ("noise", preset.noise);
}

juce::String FreezeProcessor::pulseTimeText (float time)
{
    // A note value: 4 beats = a whole note, so 2 beats = 1/2 and 1/16 beat = 1/64
    return "1/" + juce::String (juce::roundToInt (4.0 / gf::FreezeEngine::pulseBeats (time)));
}

void FreezeProcessor::setParameterValue (const char* id, float value)
{
    auto* param = apvts.getParameter (id);
    param->beginChangeGesture();
    param->setValueNotifyingHost (param->convertTo0to1 (value));
    param->endChangeGesture();
}

int FreezeProcessor::getNumPrograms()
{
    return gf::kNumPresets;
}

int FreezeProcessor::getCurrentProgram()
{
    return currentPreset();
}

void FreezeProcessor::setCurrentProgram (int index)
{
    setParameterValue ("preset", (float) juce::jlimit (0, gf::kNumPresets - 1, index));
}

const juce::String FreezeProcessor::getProgramName (int index)
{
    return gf::preset (index).name;
}

juce::AudioProcessorValueTreeState::ParameterLayout FreezeProcessor::createLayout()
{
    auto percent = [] (float value, int) { return juce::String (juce::roundToInt (value * 100.0f)) + " %"; };
    auto milliseconds = [] (float value, int) {
        return juce::String (juce::roundToInt (gf::grainKnobToSeconds (value) * 1000.0)) + " ms";
    };
    juce::StringArray presetNames;
    for (int i = 0; i < gf::kNumPresets; ++i)
        presetNames.add (gf::preset (i).name);

    const auto& first = gf::preset (0);
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "freeze", 1 }, "Amount", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (percent)));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "grain", 1 }, "Grain", juce::NormalisableRange<float> (0.0f, 1.0f),
        gf::grainSecondsToKnob (first.grainSeconds),
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (milliseconds)));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx", 1 }, "FX", juce::NormalisableRange<float> (0.0f, 1.0f), first.fx,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (percent)));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "preset", 1 }, "Preset", presetNames, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "hold", 1 }, "Freeze", false)); // the snowflake
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "noise", 1 }, "Noise", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (percent))); // the dot control
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "detune", 1 }, "Detune", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (percent))); // the lines control: +-50 cents at 100 %
    // Preset-specific controls, shown top left only with their preset
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "division", 1 }, "Division", divisionNames(), 2)); // Rhythm
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "unlink", 1 }, "Unlink", false)); // Pulse
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "time", 1 }, "Time", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return pulseTimeText (v); }))); // Pulse, unlinked
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "invert", 1 }, "Invert", false)); // Follow
    return layout;
}

bool FreezeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void FreezeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    preparedBlockSize = juce::jmax (1, samplesPerBlock);
    engine.prepare (sampleRate, preparedBlockSize, juce::jmax (1, getTotalNumOutputChannels()));
    snowfall.prepare (sampleRate, juce::jmax (1, getTotalNumOutputChannels()));
    noiseBuffer.assign ((size_t) preparedBlockSize, 0.0f);
    noiseSmoothed.reset (sampleRate, 0.05);
    noiseSmoothed.setCurrentAndTargetValue (snowParam->load());
    amountBuffer.assign ((size_t) preparedBlockSize, 0.0f);
    fxBuffer.assign ((size_t) preparedBlockSize, 0.0f);
    freezeSmoothed.reset (sampleRate, 0.05);
    freezeSmoothed.setCurrentAndTargetValue (freezeParam->load());
    fxSmoothed.reset (sampleRate, 0.05);
    fxSmoothed.setCurrentAndTargetValue (fxParam->load());
    appliedPreset = -1;
}

void FreezeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    freezeSmoothed.setTargetValue (freezeParam->load());
    fxSmoothed.setTargetValue (fxParam->load());
    engine.setGrainSeconds (gf::grainKnobToSeconds (grainParam->load()));
    engine.setHold (holdParam->load() > 0.5f);
    engine.setDetune (detuneParam->load());
    noiseSmoothed.setTargetValue (snowParam->load());
    if (const int index = currentPreset(); index != appliedPreset)
    {
        engine.setDensity (gf::preset (index).density);
        engine.setSmoothOverlap (gf::preset (index).smoothOverlap);
        engine.setFxCharacter (gf::preset (index).character);
        engine.setTone (gf::preset (index).tone);
        engine.setMode (gf::preset (index).mode);
        engine.setOctaveShare (gf::preset (index).octaveShare);
        appliedPreset = index;
    }
    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (auto hostBpm = position->getBpm(); hostBpm.hasValue() && *hostBpm > 0.0)
                bpm = *hostBpm;
    engine.setTempo (bpm);
    engine.setRhythmBeats (kDivisionBeats[juce::jlimit (0, 4, (int) divisionParam->load())]);
    engine.setPulse (unlinkParam->load() > 0.5f, timeParam->load());
    engine.setFollowInvert (invertParam->load() > 0.5f);
    const int numChannels = buffer.getNumChannels();

    // Hosts may send blocks larger than announced, so work in chunks of the prepared size.
    for (int pos = 0; pos < buffer.getNumSamples(); pos += preparedBlockSize)
    {
        const int n = juce::jmin (preparedBlockSize, buffer.getNumSamples() - pos);
        for (int i = 0; i < n; ++i)
        {
            amountBuffer[(size_t) i] = freezeSmoothed.getNextValue();
            fxBuffer[(size_t) i] = fxSmoothed.getNextValue();
            noiseBuffer[(size_t) i] = noiseSmoothed.getNextValue();
        }

        float* channels[2] {};
        for (int ch = 0; ch < juce::jmin (numChannels, 2); ++ch)
            channels[ch] = buffer.getWritePointer (ch, pos);
        engine.process (channels, juce::jmin (numChannels, 2), n, amountBuffer.data(), fxBuffer.data());
        snowfall.process (channels, juce::jmin (numChannels, 2), n, amountBuffer.data(), noiseBuffer.data()); // clean noise on top
    }
}

juce::AudioProcessorEditor* FreezeProcessor::createEditor()
{
    return new FreezeEditor (*this);
}

void FreezeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FreezeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            // The saved Grain and FX values win over the restored preset's starting values.
            const juce::ScopedValueSetter<bool> restoring (restoringState, true);
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FreezeProcessor();
}
