#include "PluginProcessor.h"
#include "PluginEditor.h"

FreezeProcessor::FreezeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    freezeParam = apvts.getRawParameterValue ("freeze");
}

juce::AudioProcessorValueTreeState::ParameterLayout FreezeProcessor::createLayout()
{
    auto percent = [] (float value, int) { return juce::String (juce::roundToInt (value * 100.0f)) + " %"; };
    return { std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "freeze", 1 }, "Freeze", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (percent)) };
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
    amountBuffer.assign ((size_t) preparedBlockSize, 0.0f);
    freezeSmoothed.reset (sampleRate, 0.05);
    freezeSmoothed.setCurrentAndTargetValue (freezeParam->load());
}

void FreezeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    freezeSmoothed.setTargetValue (freezeParam->load());
    const int numChannels = buffer.getNumChannels();

    // Hosts may send blocks larger than announced, so work in chunks of the prepared size.
    for (int pos = 0; pos < buffer.getNumSamples(); pos += preparedBlockSize)
    {
        const int n = juce::jmin (preparedBlockSize, buffer.getNumSamples() - pos);
        for (int i = 0; i < n; ++i)
            amountBuffer[(size_t) i] = freezeSmoothed.getNextValue();

        float* channels[2] {};
        for (int ch = 0; ch < juce::jmin (numChannels, 2); ++ch)
            channels[ch] = buffer.getWritePointer (ch, pos);
        engine.process (channels, juce::jmin (numChannels, 2), n, amountBuffer.data());
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
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FreezeProcessor();
}
