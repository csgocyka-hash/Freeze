// Renders the plugin window to a PNG, no DAW or audio device needed.
//   snapshot out.png [amount 0..1] [preset 0..4] [light.ttf medium.ttf semibold.ttf [caps]]
//   Add "hold" (Freeze on), "noise=0.4", "detune=0.4" and/or "closeup" (enlarged bottom corners)
//   anywhere after out.png.
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <cstdio>

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf (stderr, "Usage: snapshot out.png [amount 0..1] [preset 0..4]\n");
        return 1;
    }

    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 6 && juce::String (argv[4]) != "hold" && ! juce::String (argv[4]).startsWith ("splash=") && juce::String (argv[4]) != "detunecorner" && ! juce::String (argv[4]).startsWith ("warp=") && juce::String (argv[4]) != "window" && ! juce::String (argv[4]).startsWith ("noise=") && ! juce::String (argv[4]).startsWith ("detune=") && juce::String (argv[4]) != "closeup")
    {
        auto load = [] (const char* path) {
            juce::MemoryBlock data;
            juce::File::getCurrentWorkingDirectory().getChildFile (path).loadFileAsData (data);
            return juce::Typeface::createSystemTypefaceFor (data.getData(), data.getSize());
        };
        FreezeEditor::fonts() = { load (argv[4]), load (argv[5]), load (argv[6]), argc > 7 && juce::String (argv[7]) == "caps" };
    }
    FreezeProcessor processor;
    if (argc > 3 && juce::String (argv[3]) != "hold" && ! juce::String (argv[3]).startsWith ("splash=") && juce::String (argv[3]) != "detunecorner" && ! juce::String (argv[3]).startsWith ("warp=") && juce::String (argv[3]) != "window" && ! juce::String (argv[3]).startsWith ("noise=") && ! juce::String (argv[3]).startsWith ("detune=") && juce::String (argv[3]) != "closeup")
    {
        processor.setCurrentProgram (std::atoi (argv[3]));
        processor.handleUpdateNowIfNeeded();
    }
    auto* amount = processor.apvts.getParameter ("freeze");
    amount->setValueNotifyingHost (argc > 2 && juce::String (argv[2]) != "hold" && ! juce::String (argv[2]).startsWith ("splash=") && juce::String (argv[2]) != "detunecorner" && ! juce::String (argv[2]).startsWith ("warp=") && juce::String (argv[2]) != "window" && ! juce::String (argv[2]).startsWith ("noise=") && ! juce::String (argv[2]).startsWith ("detune=") ? (float) std::atof (argv[2]) : 0.6f);

    for (int i = 2; i < argc; ++i)
        if (juce::String (argv[i]) == "hold")
            processor.apvts.getParameter ("hold")->setValueNotifyingHost (1.0f);
        else if (juce::String (argv[i]).startsWith ("detune="))
            processor.apvts.getParameter ("detune")->setValueNotifyingHost (juce::String (argv[i]).fromFirstOccurrenceOf ("=", false, false).getFloatValue());
        else if (juce::String (argv[i]).startsWith ("noise="))
            processor.apvts.getParameter ("noise")->setValueNotifyingHost (juce::String (argv[i]).fromFirstOccurrenceOf ("=", false, false).getFloatValue());

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    // "closeup" renders just the bottom-left corner, enlarged
    bool closeup = false;
    for (int i = 2; i < argc; ++i)
        closeup = closeup || juce::String (argv[i]) == "closeup";

    // Render the panel at its design size (the window itself opens scaled down), or with "window" the
    // window exactly as it opens (at 2x for a Retina screen)
    bool window = false;
    for (int i = 2; i < argc; ++i)
        window = window || juce::String (argv[i]) == "window";
    auto& panel = *editor->getChildComponent (0);
    bool detuneOnly = false;
    for (int i = 2; i < argc; ++i)
        detuneOnly = detuneOnly || juce::String (argv[i]) == "detunecorner";
    auto area = closeup ? juce::Rectangle<int> (0, panel.getHeight() - 60, panel.getWidth(), 60) : panel.getLocalBounds();
    if (detuneOnly)
        area = juce::Rectangle<int> (panel.getWidth() - 60, panel.getHeight() - 60, 60, 60);
    const auto image = detuneOnly ? panel.createComponentSnapshot (area, true, 6.0f) : window ? editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f)
                              : panel.createComponentSnapshot (area, true, closeup ? 3.0f : 2.0f);

    const juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::FileOutputStream stream (file);
    if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, stream))
        return 1;
    std::printf ("%s\n", file.getFullPathName().toRawUTF8());
    return 0;
}
