#pragma once
#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Light knobs with a soft shadow and a thin value ring in the preset's accent colours.
class FreezeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override {}

    juce::Colour accent { 0xff4f9dff }, accentLight { 0xffa8e0ff }, accentDeep { 0xff2a55e6 };
};

// The snowflake in the top right: toggles Hold. Grey when off, lit in the preset colour with a glow when on.
class SnowflakeButton : public juce::Button
{
public:
    SnowflakeButton() : juce::Button ("Hold")
    {
        setClickingTogglesState (true);
        setTooltip ("Hold: keep playing the sound frozen right now");
    }

    void paintButton (juce::Graphics&, bool isHighlighted, bool isDown) override;

    juce::Colour accent { 0xff4f9dff }, accentLight { 0xffa8e0ff };
};

// Wide panel: wordmark and preset selector on top, the big Amount knob in the middle over an ice splash
// that shatters as the amount rises, and the small Grain and FX knobs underneath. Colours follow the preset.
class FreezeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FreezeEditor (FreezeProcessor&);
    ~FreezeEditor() override;

    void paint (juce::Graphics&) override;

    // Typefaces for the wordmark (light), preset name (medium) and labels (semibold): embedded Montserrat,
    // replaceable for previews
    struct FontSet
    {
        juce::Typeface::Ptr light, medium, semibold;
        bool capitals = false; // preset name and labels in spaced capitals
    };
    static FontSet& fonts();
    void resized() override;

private:
    struct Shard
    {
        juce::Path shape;
        float reach; // 0..1: how far out the shard sits, so outer shards appear later
        float alpha;
    };

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        juce::String name, paramId;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void timerCallback() override;
    void setupKnob (Knob&, const juce::String& name, const juce::String& paramId);
    void updateLabel (Knob&);
    void stepPreset (int delta);
    void applyPresetColours();
    void drawSplash (juce::Graphics&, juce::Point<float> centre, float amount) const;
    juce::Path splashShape (juce::Point<float> centre, float radius, float rough, int seedOffset, float wobbleScale) const;

    FreezeProcessor& freezeProcessor;
    FreezeLookAndFeel lookAndFeel;
    Knob amount, grain, fx;
    juce::TextButton previous { juce::CharPointer_UTF8 ("\xe2\x80\xb9") }, next { juce::CharPointer_UTF8 ("\xe2\x80\xba") };
    juce::Label presetName;
    SnowflakeButton snowflake;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> holdAttachment;
    std::vector<Shard> shards;
    std::vector<float> edgeNoise; // -1..1 per edge point of the splash
    int shownPreset = -1;
    juce::Image noise;
    juce::Colour accent, accentLight, accentDeep;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreezeEditor)
};
