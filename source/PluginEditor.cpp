#include "PluginEditor.h"
#include "dsp/Presets.h"

namespace
{
const juce::Colour panelTop (0xfff4f5f7), panelBottom (0xffe2e5ea), ink (0xff3b4250), inkSoft (0xff8a92a0);

constexpr int kWidth = 560, kHeight = 330;
constexpr float kSplashRadius = 74.0f;

} // namespace

void FreezeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float startAngle, float endAngle, juce::Slider&)
{
    // Square knob area at the top of the component; the space below holds the shadow
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const juce::Point<float> c (bounds.getCentreX(), bounds.getY() + size * 0.5f);
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // Geometry, outside in: value triangle, white ring, tick marks, cap
    const float triangleSize = juce::jmax (3.5f, size * 0.055f);
    const float ringRadius = size * 0.5f - triangleSize - 1.5f;
    const float tickOuter = ringRadius - juce::jmax (2.0f, size * 0.035f);
    const float tickLength = juce::jmax (2.5f, size * (variant == 2 ? 0.045f : 0.055f));
    const float capRadius = tickOuter - tickLength - juce::jmax (2.0f, size * 0.03f);

    // White measuring ring (Raum-style): full ring dim, travelled part solid in variant 2
    const float ringWidth = juce::jmax (1.2f, size * 0.012f);
    juce::Path ring;
    ring.addCentredArc (c.x, c.y, ringRadius, ringRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colours::white.withAlpha (variant == 1 ? 0.45f : 0.9f));
    g.strokePath (ring, juce::PathStrokeType (ringWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (variant == 1 && sliderPos > 0.0f)
    {
        juce::Path travelled;
        travelled.addCentredArc (c.x, c.y, ringRadius, ringRadius, 0.0f, startAngle, angle, true);
        g.setColour (juce::Colours::white);
        g.strokePath (travelled, juce::PathStrokeType (ringWidth * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Triangle marker riding on the ring: outside pointing in (variants 0 and 1), inside pointing out (variant 2)
    {
        juce::Path tri;
        const bool outside = variant != 2;
        const float tip = outside ? ringRadius + 0.5f : ringRadius - 0.5f;
        const float base = outside ? ringRadius + triangleSize * 1.15f : ringRadius - triangleSize * 1.15f;
        tri.addTriangle (0.0f, -tip, -triangleSize * 0.62f, -base, triangleSize * 0.62f, -base);
        tri.applyTransform (juce::AffineTransform::rotation (angle).translated (c));
        juce::DropShadow (juce::Colour (0x40000000), 3, { 0, 1 }).drawForPath (g, tri);
        g.setColour (juce::Colours::white);
        g.fillPath (tri);
    }

    // Tick marks: opaque white, close together
    {
        const int ticks = variant == 0 ? 25 : (variant == 1 ? 41 : 31);
        const float tickWidth = juce::jmax (1.0f, size * (variant == 1 ? 0.010f : 0.013f));
        for (int i = 0; i < ticks; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (ticks - 1);
            const bool major = variant == 2 && i % 5 == 0;
            const float inner = tickOuter - tickLength * (major ? 1.0f : (variant == 2 ? 0.55f : 1.0f));
            g.setColour (juce::Colours::white);
            g.drawLine ({ c.getPointOnCircumference (inner, a), c.getPointOnCircumference (tickOuter, a) }, tickWidth);
        }
    }

    // Cap: light grey, shadow falling down-right, light top-left edge
    const auto capBounds = juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (c);
    juce::Path cap;
    cap.addEllipse (capBounds);
    juce::DropShadow (juce::Colour (0x66101828), (int) (capRadius * 0.55f), { (int) (capRadius * 0.12f), (int) (capRadius * 0.18f) })
        .drawForPath (g, cap);
    juce::DropShadow (juce::Colour (0x40101828), (int) (capRadius * 0.12f), { 0, (int) juce::jmax (1.0f, capRadius * 0.05f) })
        .drawForPath (g, cap);

    if (variant == 1)
    {
        // Thin white bevel ring around the cap
        const auto bevel = capBounds.expanded (juce::jmax (1.5f, capRadius * 0.08f));
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, bevel.getX(), bevel.getY(),
                                                 juce::Colour (0xffbfc3c8), bevel.getRight(), bevel.getBottom(), false));
        g.fillEllipse (bevel);
    }

    const auto capLight = variant == 2 ? juce::Colour (0xffe6e7e9) : juce::Colour (0xffdedede);
    const auto capDark = variant == 2 ? juce::Colour (0xffcfd1d4) : juce::Colour (0xffc6c6c6);
    if (variant == 0)
        g.setGradientFill (juce::ColourGradient (capLight, capBounds.getX(), capBounds.getY(), capDark,
                                                 capBounds.getRight(), capBounds.getBottom(), false));
    else
        g.setGradientFill (juce::ColourGradient (capLight.brighter (0.15f), c.x - capRadius * 0.3f, c.y - capRadius * 0.4f, capDark,
                                                 c.x + capRadius * 0.8f, c.y + capRadius, true));
    g.fillPath (cap);

    if (variant == 2)
    {
        // Soft gloss on the upper half
        auto shine = capBounds.reduced (capRadius * 0.2f).withTrimmedBottom (capRadius * 0.9f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.6f), c.x, shine.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), c.x, shine.getBottom(), false));
        g.fillEllipse (shine);
    }

    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.9f), capBounds.getX(), capBounds.getY(),
                                             juce::Colour (0x30000000), capBounds.getRight(), capBounds.getBottom(), false));
    g.strokePath (cap, juce::PathStrokeType (juce::jmax (0.8f, capRadius * 0.03f)));

    // Black pointer line
    juce::Path pointer;
    pointer.startNewSubPath (c.getPointOnCircumference (capRadius * 0.12f, angle));
    pointer.lineTo (c.getPointOnCircumference (capRadius * 0.86f, angle));
    g.setColour (juce::Colours::black);
    g.strokePath (pointer, juce::PathStrokeType (juce::jmax (1.6f, capRadius * 0.075f),
                                                 juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

FreezeEditor::FreezeEditor (FreezeProcessor& p)
    : AudioProcessorEditor (p),
      freezeProcessor (p)
{
    setupKnob (amount, "Amount", "freeze");
    setupKnob (grain, "Grain", "grain");
    setupKnob (fx, "FX", "fx");
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
    presetName.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    presetName.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (presetName);

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
    knob.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    knob.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (knob.label);
    updateLabel (knob);
}

void FreezeEditor::updateLabel (Knob& knob)
{
    // Shows the knob's name, or its value while it is being dragged.
    const bool showValue = knob.slider.isMouseButtonDown();
    knob.label.setText (showValue ? freezeProcessor.apvts.getParameter (knob.paramId)->getCurrentValueAsText() : knob.name,
                        juce::dontSendNotification);
}

void FreezeEditor::setDesignVariant (int variant)
{
    designVariant = variant;
    lookAndFeel.variant = variant;
    for (auto* knob : { &amount, &grain, &fx })
        knob->slider.repaint();
    repaint();
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

    presetName.setText (preset.name, juce::dontSendNotification);
    presetName.setColour (juce::Label::textColourId, ink);
    for (auto* knob : { &amount, &grain, &fx })
        knob->label.setColour (juce::Label::textColourId, accentDeep);
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
    const float radius = kSplashRadius * (0.86f + 0.14f * level);
    const float rough = (designVariant == 2 ? 0.05f : 0.03f) + (designVariant == 1 ? 0.03f : 0.06f) * level;
    const float wobble = designVariant == 1 ? 0.6f : 1.0f;
    const float spread[3] = { designVariant == 2 ? 1.85f : 1.7f, designVariant == 2 ? 1.4f : 1.32f, 1.0f };
    const juce::Colour pale = accentLight.interpolatedWith (juce::Colours::white, 0.55f);

    for (int layer = 0; layer < 3; ++layer)
    {
        auto blob = splashShape (c, radius * spread[layer], rough * (layer == 2 ? 1.0f : 1.3f), layer, wobble);
        if (layer == 0)
        {
            g.setColour (pale.withAlpha (designVariant == 1 ? 0.45f : 0.55f));
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

    // Icy specks breaking off the edge as the amount rises
    for (const auto& s : shards)
    {
        const float visible = juce::jlimit (0.0f, 1.0f, (level * 1.3f - s.reach * 0.9f) * 2.0f);
        if (visible <= 0.0f)
            continue;
        auto shape = s.shape;
        shape.applyTransform (juce::AffineTransform::scale (1.6f + 0.1f * level * s.reach).translated (c));
        g.setColour (accent.interpolatedWith (accentLight, s.reach).withAlpha (s.alpha * visible * 0.6f));
        g.fillPath (shape);
    }
}

void FreezeEditor::drawSnowflake (juce::Graphics& g, juce::Point<float> c, float r, float level) const
{
    g.setColour (inkSoft.interpolatedWith (accent, level).withAlpha (0.45f + 0.55f * level));
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
    g.drawHorizontalLine (40, 12.0f, bounds.getRight() - 12.0f);

    const auto amountArea = amount.slider.getBounds().toFloat();
    drawSplash (g, { amountArea.getCentreX(), amountArea.getY() + amountArea.getWidth() * 0.5f }, level);

    // Letter-spaced wordmark
    juce::GlyphArrangement word;
    word.addLineOfText (juce::FontOptions (20.0f), "FREEZE", 18.0f, 27.0f);
    for (int i = 0; i < word.getNumGlyphs(); ++i)
        word.moveRangeOfGlyphs (i, 1, 3.0f * (float) i, 0.0f);
    g.setColour (ink);
    word.draw (g);

    drawSnowflake (g, { bounds.getRight() - 28.0f, 20.0f }, 10.0f, level);

    g.drawImage (noise, bounds);
}

void FreezeEditor::resized()
{
    const auto centreX = getWidth() / 2;
    presetName.setBounds (juce::Rectangle<int> (140, 24).withCentre ({ centreX, 20 }));
    previous.setBounds (presetName.getX() - 28, 8, 26, 24);
    next.setBounds (presetName.getRight() + 2, 8, 26, 24);

    // Big Amount knob in the middle, Grain and FX side by side underneath it
    // Width sets the knob size; the extra height is room for the shadow
    amount.slider.setBounds (centreX - 70, 62, 140, 150);
    grain.slider.setBounds (centreX - 64 - 22, 246, 44, 50);
    fx.slider.setBounds (centreX + 64 - 22, 246, 44, 50);

    for (auto* knob : { &amount, &grain, &fx })
        knob->label.setBounds (juce::Rectangle<int> (110, 20).withCentre ({ knob->slider.getBounds().getCentreX(),
                                                                           knob->slider.getBottom() + 2 }));
}
