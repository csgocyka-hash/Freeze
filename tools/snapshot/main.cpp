// Renders the plugin window to a PNG, no DAW or audio device needed.
//   snapshot out.png [amount 0..1] [preset 0..4] [light.ttf medium.ttf semibold.ttf [caps]]
//   Add "hold" (Freeze on), "noise=0.4" (Noise level) and/or "closeup" (enlarged bottom-left corner)
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
    if (argc > 6 && juce::String (argv[4]) != "hold" && ! juce::String (argv[4]).startsWith ("noise=") && juce::String (argv[4]) != "closeup")
    {
        auto load = [] (const char* path) {
            juce::MemoryBlock data;
            juce::File::getCurrentWorkingDirectory().getChildFile (path).loadFileAsData (data);
            return juce::Typeface::createSystemTypefaceFor (data.getData(), data.getSize());
        };
        FreezeEditor::fonts() = { load (argv[4]), load (argv[5]), load (argv[6]), argc > 7 && juce::String (argv[7]) == "caps" };
    }
    FreezeProcessor processor;
    if (argc > 3 && juce::String (argv[3]) != "hold" && ! juce::String (argv[3]).startsWith ("noise=") && juce::String (argv[3]) != "closeup")
    {
        processor.setCurrentProgram (std::atoi (argv[3]));
        processor.handleUpdateNowIfNeeded();
    }
    auto* amount = processor.apvts.getParameter ("freeze");
    amount->setValueNotifyingHost (argc > 2 && juce::String (argv[2]) != "hold" && ! juce::String (argv[2]).startsWith ("noise=") ? (float) std::atof (argv[2]) : 0.6f);

    for (int i = 2; i < argc; ++i)
        if (juce::String (argv[i]) == "hold")
            processor.apvts.getParameter ("hold")->setValueNotifyingHost (1.0f);
        else if (juce::String (argv[i]).startsWith ("noise="))
            processor.apvts.getParameter ("noise")->setValueNotifyingHost (juce::String (argv[i]).fromFirstOccurrenceOf ("=", false, false).getFloatValue());

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    // "closeup" renders just the bottom-left corner, enlarged
    bool closeup = false;
    for (int i = 2; i < argc; ++i)
        closeup = closeup || juce::String (argv[i]) == "closeup";
    const auto area = closeup ? juce::Rectangle<int> (0, editor->getHeight() - 56, 112, 56) : editor->getLocalBounds();
    const auto image = editor->createComponentSnapshot (area, true, closeup ? 5.0f : 2.0f);

    const juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::FileOutputStream stream (file);
    if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, stream))
        return 1;
    std::printf ("%s\n", file.getFullPathName().toRawUTF8());
    return 0;
}
