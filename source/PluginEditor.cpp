#include "PluginEditor.h"
#include "BinaryData.h"
#include "dsp/Presets.h"

namespace
{
// Tapered arc: rounded and widest at the start angle, narrowing to a point at the end of the full sweep.
// 'until' (0..1) cuts the shape off part-way, so the travelled part follows the same taper.
juce::Path taperedArc (juce::Point<float> c, float radius, float startAngle, float endAngle, float startWidth, float until)
{
    juce::Path p;
    constexpr int steps = 96;
    const int last = juce::jmax (1, (int) std::ceil (steps * until));
    auto widthAt = [&] (float t) { return startWidth * (1.0f - t); };
    auto angleAt = [&] (float t) { return startAngle + (endAngle - startAngle) * t; };

    for (int i = 0; i <= last; ++i) // outer edge, start to end
    {
        const float t = juce::jmin (until, (float) i / steps);
        const auto pt = c.getPointOnCircumference (radius + widthAt (t) * 0.5f, angleAt (t));
        i == 0 ? p.startNewSubPath (pt) : p.lineTo (pt);
    }
    for (int i = last; i >= 0; --i) // inner edge, back to the start
    {
        const float t = juce::jmin (until, (float) i / steps);
        p.lineTo (c.getPointOnCircumference (radius - widthAt (t) * 0.5f, angleAt (t)));
    }
    p.closeSubPath();
    // Rounded start cap
    const auto start = c.getPointOnCircumference (radius, startAngle);
    p.addEllipse (juce::Rectangle<float> (startWidth, startWidth).withCentre (start));
    return p;
}

// Montserrat: ExtraLight for the wordmark, Light in spaced capitals for the preset name and labels
juce::FontOptions uiFont (float height, const juce::String& style)
{
    const auto& set = FreezeEditor::fonts();
    auto& face = style == "Ultra Light" ? set.light : (style == "Medium" ? set.medium : set.semibold);
    auto options = juce::FontOptions (face).withHeight (height);
    return set.capitals && style != "Ultra Light" ? options.withKerningFactor (0.18f) : options;
}

juce::String labelText (const juce::String& text)
{
    return FreezeEditor::fonts().capitals ? text.toUpperCase() : text;
}

const juce::Colour panelTop (0xfff4f5f7), panelBottom (0xffe2e5ea), ink (0xff3b4250), inkSoft (0xff8a92a0);

constexpr int kWidth = 560, kHeight = 400, kHeaderHeight = 40;
constexpr float kSplashRadius = 108.0f;

} // namespace

void FreezeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    const bool flat = slider.getProperties().getWithDefault ("flat", false); // the big knob is drawn flat

    // Square knob area at the top of the component; the space below holds the shadow
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const juce::Point<float> c (bounds.getCentreX(), bounds.getY() + size * 0.5f);
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // Geometry, outside in: value triangle, white ring, tick marks, knob
    const float triangleSize = juce::jmax (3.5f, size * 0.055f);
    const float ringRadius = size * 0.5f - triangleSize - 1.5f;
    const float tickOuter = ringRadius - juce::jmax (2.0f, size * 0.035f);
    const float tickLength = juce::jmax (2.5f, size * 0.05f);
    const float knobRadius = tickOuter - tickLength - juce::jmax (2.0f, size * (flat ? 0.02f : 0.035f));

    // Measuring ring: a tapered sweep, rounded and wide at 0 %, pointed at 100 %. Dim in full, solid up to the value.
    const float ringStartWidth = juce::jmax (2.5f, size * 0.032f);
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.fillPath (taperedArc (c, ringRadius, startAngle, endAngle, ringStartWidth, 1.0f));
    if (sliderPos > 0.0f)
    {
        g.setColour (juce::Colours::white);
        g.fillPath (taperedArc (c, ringRadius, startAngle, endAngle, ringStartWidth, sliderPos));
    }
    {
        juce::Path tri;
        tri.addTriangle (0.0f, -(ringRadius + 0.5f), -triangleSize * 0.62f, -(ringRadius + triangleSize * 1.15f),
                         triangleSize * 0.62f, -(ringRadius + triangleSize * 1.15f));
        tri.applyTransform (juce::AffineTransform::rotation (angle).translated (c));
        juce::DropShadow (juce::Colour (0x40000000), 3, { 0, 1 }).drawForPath (g, tri);
        g.setColour (juce::Colours::white);
        g.fillPath (tri);
    }

    // Hairline tick marks: solid white at the outer end, fading out towards the knob
    constexpr int ticks = 121;
    const float tickWidth = juce::jmax (0.25f, size * 0.0018f);
    const float tickInner = tickOuter - tickLength;
    for (int i = 0; i < ticks; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (ticks - 1);
        const auto outerPoint = c.getPointOnCircumference (tickOuter, a);
        const auto innerPoint = c.getPointOnCircumference (tickInner, a);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, outerPoint, juce::Colours::white.withAlpha (0.0f), innerPoint, false));
        juce::Path tick;
        tick.startNewSubPath (outerPoint);
        tick.lineTo (innerPoint);
        g.strokePath (tick, juce::PathStrokeType (tickWidth));
    }

    // ---- Knob ----
    const float wall = flat ? 0.0f : juce::jmax (1.5f, knobRadius * 0.09f); // visible side wall below the face
    const auto face = juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (c);
    const auto base = face.translated (0.0f, wall);
    juce::Path body;
    body.addEllipse (base);

    if (flat)
    {
        // Flat knob: just a soft, low shadow
        juce::DropShadow (juce::Colour (0x30101828), (int) (knobRadius * 0.3f), { 0, (int) (knobRadius * 0.06f) }).drawForPath (g, body);
    }
    else
    {
        // Shadows: wide soft ambient shadow, a mid shadow and a tight contact shadow (ambient occlusion)
        juce::DropShadow (juce::Colour (0x40101828), (int) (knobRadius * 0.9f), { (int) (knobRadius * 0.12f), (int) (knobRadius * 0.38f) })
            .drawForPath (g, body);
        juce::DropShadow (juce::Colour (0x50101828), (int) (knobRadius * 0.35f), { (int) (knobRadius * 0.05f), (int) (knobRadius * 0.14f) })
            .drawForPath (g, body);
        juce::DropShadow (juce::Colour (0x80101828), (int) juce::jmax (2.0f, knobRadius * 0.06f), { 0, (int) juce::jmax (1.0f, knobRadius * 0.03f) })
            .drawForPath (g, body);
    }

    // Side wall: satin band with fine vertical grip lines, lit from the left
    if (! flat)
    {
        juce::Path side;
        side.addRectangle (face.getX(), c.y, face.getWidth(), wall);
        side.addEllipse (base);
        juce::ColourGradient wallFill (juce::Colour (0xffb9bdc3), face.getX(), 0.0f, juce::Colour (0xff8e939b), face.getRight(), 0.0f, false);
        wallFill.addColour (0.22, juce::Colour (0xffe9ebee));
        wallFill.addColour (0.5, juce::Colour (0xffc9ccd1));
        g.setGradientFill (wallFill);
        g.fillPath (side);

        juce::Graphics::ScopedSaveState clipToWall (g);
        g.reduceClipRegion (side);
        const int grips = juce::jmax (24, (int) (knobRadius * 1.6f));
        for (int i = 0; i < grips; ++i)
        {
            // Lines spaced evenly around the visible half of the cylinder
            const float t = juce::MathConstants<float>::pi * ((float) i + 0.5f) / (float) grips;
            const float gx = c.x - std::cos (t) * knobRadius;
            g.setColour (juce::Colours::black.withAlpha (0.07f));
            g.drawVerticalLine ((int) gx, c.y, c.y + wall + knobRadius);
        }
    }

    // Face: satin light grey, lit from the top left
    juce::ColourGradient faceFill (juce::Colour (0xfff4f5f7), face.getX() + knobRadius * 0.3f, face.getY(),
                                   juce::Colour (0xffcdd0d5), face.getRight() - knobRadius * 0.3f, face.getBottom(), false);
    faceFill.addColour (0.5, juce::Colour (0xffe2e4e7));
    g.setGradientFill (faceFill);
    g.fillEllipse (face);

    // Brushed spin highlight: two soft bright sectors across the face, like light on turned aluminium
    if (! flat)
    {
        constexpr int sectors = 180;
        for (int i = 0; i < sectors; ++i)
        {
            const float a0 = juce::MathConstants<float>::twoPi * (float) i / sectors;
            const float a1 = juce::MathConstants<float>::twoPi * (float) (i + 1) / sectors;
            const float sheen = std::pow (0.5f + 0.5f * std::cos (2.0f * (0.5f * (a0 + a1) + 0.55f)), 3.0f);
            juce::Path wedge;
            wedge.addPieSegment (face, a0, a1 + 0.004f, 0.0f);
            g.setColour (juce::Colours::white.withAlpha (0.22f * sheen));
            g.fillPath (wedge);
        }
    }

    // Spun finish: very faint concentric rings with slightly varying brightness
    {
        juce::Random grain (7);
        const float step = juce::jmax (0.8f, knobRadius * 0.016f);
        for (float r = knobRadius * 0.08f; r < knobRadius * 0.97f; r += step)
        {
            const float v = grain.nextFloat();
            g.setColour ((v > 0.5f ? juce::Colours::white : juce::Colours::black).withAlpha (0.02f * std::abs (v - 0.5f) * 2.0f));
            g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), step * 0.6f);
        }
    }

    // Slightly concave face: a soft darker band just inside the rim
    if (! flat)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.0f), c.x, c.y,
                                                 juce::Colours::black.withAlpha (0.08f), c.x + knobRadius, c.y, true));
        g.fillEllipse (face);
    }

    // Chamfered edge: crisp highlight on top, darker on the bottom, plus a hairline dark outline
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, c.x, face.getY(),
                                             juce::Colour (0x40000000), c.x, face.getBottom(), false));
    g.drawEllipse (face.reduced (juce::jmax (0.6f, knobRadius * 0.02f)), juce::jmax (1.0f, knobRadius * 0.04f));
    g.setColour (juce::Colour (0x38000000));
    g.drawEllipse (face, 0.6f);

    // Super-thin black pointer line, set into the face (light lower edge)
    juce::Path pointer;
    pointer.startNewSubPath (c.getPointOnCircumference (knobRadius * 0.16f, angle));
    pointer.lineTo (c.getPointOnCircumference (knobRadius * 0.86f, angle));
    const juce::PathStrokeType stroke (juce::jmax (0.8f, knobRadius * 0.022f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    if (! flat)
    {
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.strokePath (pointer, stroke, juce::AffineTransform::translation (0.0f, 0.5f));
    }
    if (flat)
    {
        // Big knob: a light grey needle that tapers to a sharp point at its outer end
        const auto base = c.getPointOnCircumference (knobRadius * 0.16f, angle);
        const auto tip = c.getPointOnCircumference (knobRadius * 0.86f, angle);
        const auto side = juce::Point<float> (std::cos (angle), std::sin (angle)) * (juce::jmax (1.0f, knobRadius * 0.03f));
        juce::Path needle;
        needle.addTriangle (base + side, base - side, tip);
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillPath (needle, juce::AffineTransform::translation (0.0f, 0.5f));
        g.setColour (juce::Colour (0xffa7acb4));
        g.fillPath (needle);
    }
    else
    {
        g.setColour (juce::Colour (0xff111214));
        g.strokePath (pointer, stroke);
    }
}

FreezeEditor::FreezeEditor (FreezeProcessor& p)
    : AudioProcessorEditor (p),
      freezeProcessor (p)
{
    setupKnob (amount, "Amount", "freeze");
    setupKnob (grain, "Grain", "grain");
    setupKnob (fx, "FX", "fx");
    amount.slider.getProperties().set ("flat", true);
    amount.slider.onValueChange = [this] { updateLabel (amount); repaint(); };

    for (auto* button : { &previous, &next })
    {
        button->setLookAndFeel (&lookAndFeel);
        button->setColour (juce::TextButton::textColourOffId, inkSoft);
        button->setColour (juce::TextButton::textColourOnId, ink);
        addAndMakeVisible (*button);
    }
    previous.onClick = [this] { stepPreset (-1); };
    next.onClick = [this] { stepPreset (1); };

    presetName.setJustificationType (juce::Justification::centred);
    presetName.setFont (uiFont (fonts().capitals ? 14.0f : 16.0f, "Medium"));
    presetName.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (presetName);

    holdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (freezeProcessor.apvts, "hold", snowflake);
    addAndMakeVisible (snowflake);
    noiseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (freezeProcessor.apvts, "noise", noiseControl);
    noiseControl.onValueChange = [this] {
        noiseValue.setText (freezeProcessor.apvts.getParameter ("noise")->getCurrentValueAsText(), juce::dontSendNotification);
        noiseControl.repaint();
    };
    noiseControl.onDragStart = [this] { showNoiseValue (true); };
    noiseControl.onDragEnd = [this] { showNoiseValue (false); };
    addAndMakeVisible (noiseControl);

    noiseValue.setFont (uiFont (11.0f, "Demi Bold"));
    noiseValue.setJustificationType (juce::Justification::centredLeft);
    noiseValue.setInterceptsMouseClicks (false, false);
    noiseValue.setText (freezeProcessor.apvts.getParameter ("noise")->getCurrentValueAsText(), juce::dontSendNotification);
    addChildComponent (noiseValue);

    // Shards scattered around the hexagon edge; fixed seed so the splash always looks the same.
    juce::Random rng (2026);
    // Edge roughness: random values smoothed over neighbours, so the edge is chunky rather than hairy
    std::vector<float> raw (160);
    for (auto& v : raw)
        v = rng.nextFloat() * 2.0f - 1.0f;
    edgeNoise.resize (raw.size());
    for (size_t i = 0; i < raw.size(); ++i)
        edgeNoise[i] = 0.6f * raw[i] + 0.2f * raw[(i + 1) % raw.size()] + 0.2f * raw[(i + raw.size() - 1) % raw.size()];
    for (int i = 0; i < 40; ++i)
    {
        const float a = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        const float reach = rng.nextFloat();
        const float dist = kSplashRadius * (1.05f + 0.4f * reach);
        const float size = 1.5f + 4.5f * rng.nextFloat() * (1.0f - 0.5f * reach);
        const auto centre = juce::Point<float>().getPointOnCircumference (dist, a);
        juce::Path tri;
        tri.addTriangle (centre.getPointOnCircumference (size, rng.nextFloat() * 6.28f),
                         centre.getPointOnCircumference (size, rng.nextFloat() * 6.28f),
                         centre.getPointOnCircumference (size * 0.6f, rng.nextFloat() * 6.28f));
        shards.push_back ({ tri, reach, 0.45f + 0.55f * rng.nextFloat() });
    }

    // Fine light grey grain over the panel (the knobs are child components, so it doesn't cover them)
    noise = juce::Image (juce::Image::ARGB, kWidth * 2, kHeight * 2, true);
    for (int py = 0; py < noise.getHeight(); ++py)
        for (int px = 0; px < noise.getWidth(); ++px)
        {
            const float v = rng.nextFloat();
            noise.setPixelAt (px, py, v < 0.5f ? juce::Colour::greyLevel (0.55f).withAlpha (0.10f * (0.5f - v) * 2.0f)
                                               : juce::Colours::white.withAlpha (0.16f * (v - 0.5f) * 2.0f));
        }

    applyPresetColours();
    setSize (kWidth, kHeight);
    startTimerHz (20);
}

FreezeEditor::~FreezeEditor()
{
    for (auto* knob : { &amount, &grain, &fx })
        knob->slider.setLookAndFeel (nullptr);
    previous.setLookAndFeel (nullptr);
    next.setLookAndFeel (nullptr);
}

void FreezeEditor::setupKnob (Knob& knob, const juce::String& name, const juce::String& paramId)
{
    knob.name = name;
    knob.paramId = paramId;
    knob.slider.setLookAndFeel (&lookAndFeel);
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    knob.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    knob.slider.onValueChange = [this, &knob] { updateLabel (knob); };
    knob.slider.onDragStart = knob.slider.onDragEnd = [this, &knob] { updateLabel (knob); };
    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        freezeProcessor.apvts, paramId, knob.slider);
    addAndMakeVisible (knob.slider);

    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setFont (fonts().capitals ? uiFont (11.0f, "Demi Bold") : uiFont (12.5f, "Demi Bold").withKerningFactor (0.08f));
    knob.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (knob.label);
    updateLabel (knob);
}

void FreezeEditor::updateLabel (Knob& knob)
{
    // Shows the knob's name, or its value while it is being dragged.
    const bool showValue = knob.slider.isMouseButtonDown();
    knob.label.setText (showValue ? freezeProcessor.apvts.getParameter (knob.paramId)->getCurrentValueAsText() : labelText (knob.name),
                        juce::dontSendNotification);
}

FreezeEditor::FontSet& FreezeEditor::fonts()
{
    static FontSet set = [] {
        auto load = [] (const char* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); };
        auto light = load (BinaryData::MontserratLight_ttf, BinaryData::MontserratLight_ttfSize);
        return FontSet { load (BinaryData::MontserratExtraLight_ttf, BinaryData::MontserratExtraLight_ttfSize),
                         light, light, true };
    }();
    return set;
}

void FreezeEditor::showNoiseValue (bool shouldShow)
{
    noiseValue.setVisible (shouldShow);
}

void FreezeEditor::stepPreset (int delta)
{
    const int index = (freezeProcessor.currentPreset() + delta + gf::kNumPresets) % gf::kNumPresets;
    freezeProcessor.setCurrentProgram (index);
}

void FreezeEditor::timerCallback()
{
    if (freezeProcessor.currentPreset() != shownPreset)
        applyPresetColours();
}

void FreezeEditor::applyPresetColours()
{
    shownPreset = freezeProcessor.currentPreset();
    const auto& preset = gf::preset (shownPreset);
    accent = juce::Colour (preset.accent);
    accentLight = juce::Colour (preset.accentLight);
    accentDeep = juce::Colour (preset.accentDeep);
    lookAndFeel.accent = accent;
    lookAndFeel.accentLight = accentLight;
    lookAndFeel.accentDeep = accentDeep;
    snowflake.accent = accent;
    snowflake.accentLight = accentLight;
    snowflake.repaint();
    noiseControl.accent = accent;
    noiseControl.accentLight = accentLight;
    noiseControl.repaint();
    noiseValue.setColour (juce::Label::textColourId, accentDeep);

    presetName.setText (labelText (preset.name), juce::dontSendNotification);
    presetName.setColour (juce::Label::textColourId, ink);
    for (auto* knob : { &grain, &fx })
        knob->label.setColour (juce::Label::textColourId, accentDeep);
    amount.label.setVisible (false); // the big knob needs no caption
    repaint();
}

juce::Path FreezeEditor::splashShape (juce::Point<float> c, float radius, float rough, int seedOffset, float wobbleScale) const
{
    juce::Path blob;
    const auto n = edgeNoise.size();
    const float phase = 1.3f * (float) seedOffset;
    for (size_t i = 0; i <= n; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) (i % n) / (float) n;
        const float wobble = wobbleScale * (0.045f * std::sin (3.0f * a + 0.7f + phase) + 0.03f * std::sin (5.0f * a + 2.1f + phase)
                                            + 0.02f * std::sin (9.0f * a + 4.0f + phase));
        const float r = radius * (1.0f + wobble + rough * edgeNoise[(i + (size_t) seedOffset * 37) % n]);
        const auto pt = c.getPointOnCircumference (r, a);
        i == 0 ? blob.startNewSubPath (pt) : blob.lineTo (pt);
    }
    blob.closeSubPath();
    return blob;
}

void FreezeEditor::drawSplash (juce::Graphics& g, juce::Point<float> c, float level) const
{
    // Three nested splashes: a big pale one, a lighter middle one and the coloured core.
    // Each layer grows with the amount; the outer layers grow the most, so turning up spreads the splash out.
    const float radius = kSplashRadius * (0.8f + 0.2f * level);
    const float rough = 0.03f + 0.03f * level;
    const float wobble = 0.6f;
    const float spread[3] = { 1.08f + 0.72f * level, 1.04f + 0.36f * level, 1.0f };
    const juce::Colour pale = accentLight.interpolatedWith (juce::Colours::white, 0.55f);

    for (int layer = 0; layer < 3; ++layer)
    {
        auto blob = splashShape (c, radius * spread[layer], rough * (layer == 2 ? 1.0f : 1.3f), layer, wobble);
        if (layer == 0)
        {
            g.setColour (pale.withAlpha (0.45f));
            g.fillPath (blob);
        }
        else if (layer == 1)
        {
            g.setGradientFill (juce::ColourGradient (accentLight.withAlpha (0.75f), c.x - radius, c.y - radius,
                                                     accent.withAlpha (0.6f), c.x + radius * 1.3f, c.y + radius * 1.3f, false));
            g.fillPath (blob);
        }
        else
        {
            juce::DropShadow (accentDeep.withAlpha (0.35f), (int) (radius * 0.25f), { 0, 3 }).drawForPath (g, blob);
            juce::ColourGradient fill (accentLight, c.x - radius, c.y - radius, accentDeep, c.x + radius, c.y + radius, false);
            fill.addColour (0.5, accent);
            g.setGradientFill (fill);
            g.fillPath (blob);
        }
    }
}

NoiseControl::NoiseControl() : juce::Slider ("Noise")
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag); // drag up/down; the dots show the value
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setDoubleClickReturnValue (true, 0.0);
    setTooltip ("Noise: drag up or down for clean, airy noise that sweeps with the big knob");

    // Halftone fade: a round dot grid whose dots shrink towards the edge, like dithered noise
    for (int row = -6; row <= 6; ++row)
        for (int col = -6; col <= 6; ++col)
        {
            const float x = 0.075f * (float) col, y = 0.075f * (float) row;
            const float r = std::sqrt (x * x + y * y) / 0.47f;
            if (r > 1.0f)
                continue;
            dots.push_back ({ 0.5f + x, 0.5f + y, 0.029f * (1.0f - 0.7f * r) });
        }
}

void NoiseControl::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (1.0f);
    const auto level = (float) getValue();

    if (level > 0.0f)
    {
        const auto glow = getLocalBounds().toFloat();
        const auto c = glow.getCentre();
        g.setGradientFill (juce::ColourGradient (accentLight.withAlpha (0.7f * level), c.x, c.y,
                                                 accentLight.withAlpha (0.0f), c.x + glow.getWidth() * 0.5f, c.y, true));
        g.fillEllipse (glow);
    }

    // Fill line: from the bottom of the dot pattern (0 %) to its top (100 %)
    const float fillFrom = 0.5f + 0.47f - level * 0.94f;
    const bool hot = isMouseOverOrDragging();
    for (const auto& d : dots)
    {
        const bool lit = level > 0.0f && d.y >= fillFrom;
        auto colour = lit ? accent : inkSoft;
        if (hot)
            colour = colour.interpolatedWith (lit ? accent.darker (0.3f) : ink, 0.35f);
        const float r = d.radius * area.getWidth();
        g.setColour (colour);
        g.fillEllipse (area.getX() + d.x * area.getWidth() - r, area.getY() + d.y * area.getHeight() - r, 2.0f * r, 2.0f * r);
    }
}

void SnowflakeButton::paintButton (juce::Graphics& g, bool isHighlighted, bool isDown)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float r = (juce::jmin (getWidth(), getHeight()) * 0.5f - 4.0f) * 0.75f;
    const bool on = getToggleState();

    if (on)
    {
        // Soft glow behind the lit snowflake
        g.setGradientFill (juce::ColourGradient (accentLight.withAlpha (0.7f), c.x, c.y,
                                                 accentLight.withAlpha (0.0f), c.x + r * 1.5f, c.y, true));
        g.fillEllipse (juce::Rectangle<float> (r * 3.0f, r * 3.0f).withCentre (c));
    }

    auto colour = on ? accent : inkSoft;
    if (isHighlighted || isDown)
        colour = colour.interpolatedWith (on ? accent.darker (0.3f) : ink, 0.5f);
    g.setColour (colour);
    for (int i = 0; i < 6; ++i)
    {
        const float a = juce::MathConstants<float>::pi / 3.0f * (float) i;
        g.drawLine ({ c, c.getPointOnCircumference (r, a) }, 1.4f);
        const auto mid = c.getPointOnCircumference (r * 0.6f, a);
        g.drawLine ({ mid, mid.getPointOnCircumference (r * 0.3f, a - 0.8f) }, 1.2f);
        g.drawLine ({ mid, mid.getPointOnCircumference (r * 0.3f, a + 0.8f) }, 1.2f);
    }
}

void FreezeEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float level = (float) amount.slider.getValue();

    g.setGradientFill (juce::ColourGradient (panelTop, 0.0f, 0.0f, panelBottom, 0.0f, bounds.getHeight(), false));
    g.fillAll();
    g.setColour (juce::Colour (0xffc8ccd3));
    g.drawRect (bounds, 1.0f);

    // Top strip
    g.setColour (juce::Colour (0xffd5d9df));
    g.drawHorizontalLine (kHeaderHeight, 12.0f, bounds.getRight() - 12.0f);

    // The splash stays below the header line
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (getLocalBounds().withTrimmedTop (kHeaderHeight + 1));
        const auto amountArea = amount.slider.getBounds().toFloat();
        drawSplash (g, { amountArea.getCentreX(), amountArea.getY() + amountArea.getWidth() * 0.5f }, level);
    }

    // Letter-spaced wordmark
    juce::GlyphArrangement word;
    word.addLineOfText (uiFont (19.0f, "Ultra Light"), "FREEZE", 18.0f, 27.0f);
    for (int i = 0; i < word.getNumGlyphs(); ++i)
        word.moveRangeOfGlyphs (i, 1, 5.0f * (float) i, 0.0f);
    g.setColour (ink);
    word.draw (g);


    g.drawImage (noise, bounds);
}

void FreezeEditor::resized()
{
    const auto centreX = getWidth() / 2;
    presetName.setBounds (juce::Rectangle<int> (140, 24).withCentre ({ centreX, 20 }));
    previous.setBounds (presetName.getX() - 28, 8, 26, 24);
    next.setBounds (presetName.getRight() + 2, 8, 26, 24);
    snowflake.setBounds (getWidth() - 48, 0, 40, 40);
    noiseControl.setBounds (8, getHeight() - 54, 46, 46); // bottom-left corner, opposite the snowflake
    noiseValue.setBounds (noiseControl.getRight() + 2, noiseControl.getY() + 10, 60, 20);

    // Big Amount knob in the middle, Grain and FX side by side underneath it
    // Width sets the knob size; the extra height is room for the shadow
    const int middleY = (kHeaderHeight + getHeight()) / 2 - 10;
    amount.slider.setBounds (centreX - 86, middleY - 86, 172, 180);
    grain.slider.setBounds (centreX - 64 - 22, getHeight() - 92, 44, 50);
    fx.slider.setBounds (centreX + 64 - 22, getHeight() - 92, 44, 50);

    for (auto* knob : { &amount, &grain, &fx })
        knob->label.setBounds (juce::Rectangle<int> (110, 20).withCentre ({ knob->slider.getBounds().getCentreX(),
                                                                           knob->slider.getBottom() + 2 }));
}
