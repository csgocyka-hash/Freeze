#pragma once
#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Light knob with a soft shadow and a thin icy value ring.
class FreezeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// Wide one-knob panel: wordmark on the left, the Freeze knob in the middle over a shattered ice
// splash that grows with the amount, and a snowflake on the right that lights up.
class FreezeEditor : public juce::AudioProcessorEditor
{
public:
    explicit FreezeEditor (FreezeProcessor&);
    ~FreezeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Shard
    {
        juce::Path shape;
        float reach;  // 0..1: how far out the shard sits, so outer shards appear later
        float alpha;
    };

    void drawSplash (juce::Graphics&, juce::Point<float> centre, float amount) const;
    void drawSnowflake (juce::Graphics&, juce::Point<float> centre, float radius, float amount) const;
    void updateLabel();

    FreezeProcessor& freezeProcessor;
    FreezeLookAndFeel lookAndFeel;
    juce::Slider knob;
    juce::Label valueLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    std::vector<Shard> shards;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreezeEditor)
};
