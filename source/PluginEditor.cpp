#include "PluginEditor.h"
#include "dsp/Presets.h"

namespace
{
const juce::Colour panelTop (0xfff4f5f7), panelBottom (0xffe2e5ea), ink (0xff3b4250), inkSoft (0xff8a92a0);

constexpr int kWidth = 560, kHeight = 330;
constexpr float kSplashRadius = 86.0f;

} // namespace

void FreezeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    const bool onColour = slider.getProperties().getWithDefault ("onColour", false);
    // Square knob area at the top of the component; the space below holds the body and its shadow
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const juce::Point<float> c (bounds.getCentreX(), bounds.getY() + size * 0.5f);
    const float ringRadius = size * 0.5f - 3.0f;          // value ring
    const float skirtRadius = ringRadius - size * 0.075f; // knurled metal edge
    const float capRadius = skirtRadius * 0.80f;          // domed top
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float ringWidth = juce::jmax (2.5f, size * 0.025f);

    // Value ring: recessed track, then the lit arc in the preset colour
    juce::Path track, value;
    track.addCentredArc (c.x, c.y, ringRadius, ringRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (onColour ? juce::Colour (0x40000000) : juce::Colour (0xffc3c8d0));
    g.strokePath (track, juce::PathStrokeType (ringWidth + 1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (onColour ? juce::Colours::white.withAlpha (0.3f) : juce::Colour (0xffe9ecf0));
    g.strokePath (track, juce::PathStrokeType (ringWidth - 0.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (sliderPos > 0.0f)
    {
        value.addCentredArc (c.x, c.y, ringRadius, ringRadius, 0.0f, startAngle, angle, true);
        juce::DropShadow (accent.withAlpha (0.55f), (int) (ringWidth * 3.0f), {}).drawForPath (g, value);
        if (onColour)
            g.setColour (juce::Colours::white); // the big knob's ring sits on the coloured splash
        else
            g.setGradientFill (juce::ColourGradient (accent, c.x - ringRadius, c.y + ringRadius, accentLight,
                                                     c.x + ringRadius, c.y - ringRadius, false));
        g.strokePath (value, juce::PathStrokeType (ringWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // The knob below is "maschine-white2096x96.knob" from KnobGallery (CC0), redrawn from its layer
    // definitions so it stays sharp at any size. Sizes are fractions of the KnobMan canvas, which is
    // chosen so the knob's rim (layer 1, 62 % of the canvas) fills our skirt radius.
    const float canvas = skirtRadius * 2.0f / 0.62f;
    auto disc = [&] (float zoom) { return juce::Rectangle<float> (canvas * zoom, canvas * zoom).withCentre (c); };
    const auto lightOffset = juce::Point<float> (0.7071f, 0.7071f); // light from the top left, shadows fall down-right

    // Body height for the taller small knobs: a white cylinder wall under the rim
    const float bodyHeight = size * (float) slider.getProperties().getWithDefault ("height", 0.06);
    const auto rimBounds = disc (0.62f);
    juce::Path footprint;
    footprint.addEllipse (rimBounds.translated (0.0f, bodyHeight));
    juce::DropShadow (juce::Colour (0x70101828), (int) (canvas * 0.12f),
                      (lightOffset * canvas * 0.05f).roundToInt()).drawForPath (g, footprint);
    if (bodyHeight > 0.5f)
    {
        juce::Path wall;
        wall.addRectangle (rimBounds.getX(), c.y, rimBounds.getWidth(), bodyHeight);
        wall.addEllipse (rimBounds.translated (0.0f, bodyHeight));
        juce::ColourGradient side (juce::Colour (0xff9a9fa7), rimBounds.getX(), 0.0f, juce::Colour (0xff8c919a), rimBounds.getRight(), 0.0f, false);
        side.addColour (0.25, juce::Colour (0xffe6e8eb));
        side.addColour (0.45, juce::Colour (0xffffffff));
        side.addColour (0.8, juce::Colour (0xffbfc3c9));
        g.setGradientFill (side);
        g.fillPath (wall);
    }

    // Layer 1: white disc with a strong, soft inner shadow (density 100 %, diffuse 64 %) -> polished rim
    g.setColour (juce::Colours::white);
    g.fillEllipse (rimBounds);
    {
        juce::ColourGradient inner (juce::Colours::black.withAlpha (0.0f), c.x, c.y,
                                    juce::Colours::black.withAlpha (0.38f), c.x + rimBounds.getWidth() * 0.5f, c.y, true);
        inner.addColour (0.6, juce::Colours::black.withAlpha (0.0f));
        inner.addColour (0.88, juce::Colours::black.withAlpha (0.12f));
        g.setGradientFill (inner);
        g.fillEllipse (rimBounds);
        // Light falls on the upper left of the rim and leaves the lower right darker
        juce::ColourGradient light (juce::Colours::white.withAlpha (0.8f), c.x, rimBounds.getY(),
                                    juce::Colours::black.withAlpha (0.42f), c.x, rimBounds.getBottom(), false);
        light.addColour (0.45, juce::Colours::white.withAlpha (0.1f));
        g.setGradientFill (light);
        g.fillEllipse (rimBounds);
    }

    // Layer 8: seven white tick marks on the rim (every 45.7 degrees within -140..140), slightly glossy
    {
        const float outerR = canvas * 0.57f * 0.5f, innerR = outerR * (1.0f - 0.12f * 2.0f);
        const float tickWidth = juce::jmax (1.0f, canvas * 0.57f * 0.05f * 0.5f);
        for (int i = -3; i <= 3; ++i)
        {
            const float a = juce::degreesToRadians (45.7f * (float) i);
            g.setColour (juce::Colours::black.withAlpha (0.25f));
            g.drawLine ({ c.getPointOnCircumference (innerR, a).translated (0.4f, 0.6f),
                          c.getPointOnCircumference (outerR, a).translated (0.4f, 0.6f) }, tickWidth);
            g.setColour (juce::Colours::white);
            g.drawLine ({ c.getPointOnCircumference (innerR, a), c.getPointOnCircumference (outerR, a) }, tickWidth);
        }
    }

    // Layer 3: white disc with soft diffuse shading (27 %)
    const auto midBounds = disc (0.49f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, midBounds.getX(), midBounds.getY(),
                                             juce::Colour (0xffbdbdbd), midBounds.getRight(), midBounds.getBottom(), false));
    g.fillEllipse (midBounds);

    // Layer 4: light grey cap (#D6D6D6) casting a shadow down-right (offset 12 %, density 80 %, diffuse 40 %),
    // with a light top-left edge (highlight 26 %)
    const auto capBounds = disc (0.43f);
    {
        juce::Path cap;
        cap.addEllipse (capBounds);
        juce::DropShadow (juce::Colours::black.withAlpha (0.8f), (int) (canvas * 0.43f * 0.4f * 0.5f),
                          (lightOffset * canvas * 0.43f * 0.12f * 0.5f).roundToInt()).drawForPath (g, cap);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe0e0e0), capBounds.getX(), capBounds.getY(),
                                                 juce::Colour (0xffcbcbcb), capBounds.getRight(), capBounds.getBottom(), false));
        g.fillPath (cap);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.75f), capBounds.getX(), capBounds.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), capBounds.getCentreX(), capBounds.getCentreY(), false));
        g.strokePath (cap, juce::PathStrokeType (juce::jmax (1.0f, canvas * 0.012f)));
    }

    // Layer 5: black pointer line turning from -140 to +140 degrees
    {
        const float lineCentre = canvas * 0.20f * 0.80f, halfLength = canvas * 0.26f * 0.80f * 0.5f;
        const auto inner = c.getPointOnCircumference (juce::jmax (0.0f, lineCentre - halfLength), angle);
        const auto outer = c.getPointOnCircumference (juce::jmin (capBounds.getWidth() * 0.47f, lineCentre + halfLength), angle);
        juce::Path pointer;
        pointer.startNewSubPath (inner);
        pointer.lineTo (outer);
        g.setColour (juce::Colours::black);
        g.strokePath (pointer, juce::PathStrokeType (juce::jmax (1.8f, canvas * 0.08f * 0.80f * 0.4f),
                                                     juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }
}

FreezeEditor::FreezeEditor (FreezeProcessor& p)
    : AudioProcessorEditor (p),
      freezeProcessor (p)
{
    setupKnob (amount, "Amount", "freeze");
    setupKnob (grain, "Grain", "grain");
    setupKnob (fx, "FX", "fx");
    amount.slider.onValueChange = [this] { updateLabel (amount); repaint(); };
    amount.slider.getProperties().set ("onColour", true);
    amount.slider.getProperties().set ("height", 0.05);
    grain.slider.getProperties().set ("height", 0.20);
    fx.slider.getProperties().set ("height", 0.20);

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

void FreezeEditor::drawSplash (juce::Graphics& g, juce::Point<float> c, float level) const
{
    // Organic disc: a slow wobble plus fine ragged edges that get rougher as the amount rises.
    const float radius = kSplashRadius * (0.86f + 0.14f * level);
    const float rough = 0.03f + 0.06f * level;
    juce::Path blob;
    const auto n = edgeNoise.size();
    for (size_t i = 0; i <= n; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) (i % n) / (float) n;
        const float wobble = 0.045f * std::sin (3.0f * a + 0.7f) + 0.03f * std::sin (5.0f * a + 2.1f)
                             + 0.02f * std::sin (9.0f * a + 4.0f);
        const float r = radius * (1.0f + wobble + rough * edgeNoise[i % n]);
        const auto pt = c.getPointOnCircumference (r, a);
        i == 0 ? blob.startNewSubPath (pt) : blob.lineTo (pt);
    }
    blob.closeSubPath();

    // Soft coloured glow under the disc
    juce::DropShadow (accent.withAlpha (0.35f + 0.3f * level), (int) (radius * 0.35f), { 0, 4 }).drawForPath (g, blob);

    // Diagonal colour gradient: light at the top left, deep at the bottom right
    juce::ColourGradient fill (accentLight, c.x - radius, c.y - radius, accentDeep, c.x + radius, c.y + radius, false);
    fill.addColour (0.5, accent);
    g.setGradientFill (fill);
    g.fillPath (blob);

    // Frosted centre light so the knob sits in a glow
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.45f), c.x, c.y - radius * 0.15f,
                                             juce::Colours::white.withAlpha (0.0f), c.x, c.y + radius, true));
    g.fillPath (blob);

    // Icy specks breaking off the edge as the amount rises
    for (const auto& s : shards)
    {
        const float visible = juce::jlimit (0.0f, 1.0f, (level * 1.3f - s.reach * 0.9f) * 2.0f);
        if (visible <= 0.0f)
            continue;
        auto shape = s.shape;
        shape.applyTransform (juce::AffineTransform::scale (1.0f + 0.1f * level * s.reach).translated (c));
        g.setColour (accent.interpolatedWith (accentLight, s.reach).withAlpha (s.alpha * visible * 0.8f));
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
}

void FreezeEditor::resized()
{
    const auto centreX = getWidth() / 2;
    presetName.setBounds (juce::Rectangle<int> (140, 24).withCentre ({ centreX, 20 }));
    previous.setBounds (presetName.getX() - 28, 8, 26, 24);
    next.setBounds (presetName.getRight() + 2, 8, 26, 24);

    // Big Amount knob in the middle, Grain and FX side by side underneath it
    // Width sets the knob size; the extra height is room for the knob body and shadow
    amount.slider.setBounds (centreX - 68, 64, 136, 152);
    grain.slider.setBounds (centreX - 70 - 27, 236, 54, 72);
    fx.slider.setBounds (centreX + 70 - 27, 236, 54, 72);

    for (auto* knob : { &amount, &grain, &fx })
        knob->label.setBounds (juce::Rectangle<int> (110, 20).withCentre ({ knob->slider.getBounds().getCentreX(),
                                                                           knob->slider.getBottom() + 2 }));
}
