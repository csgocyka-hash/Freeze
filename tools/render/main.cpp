// Offline renderer: hear Freeze without a DAW.
//
//   render [--test-pluck | --in file.wav] [--preset Name | --all] [--freeze 0..1 | --sweep]
//          [--grain 0..1] [--fx 0..1] [--hold-at seconds] [--out file.wav]
//
// Writes "00 Dry.wav" next to the output for comparison. --all writes one file per preset.
#include "dsp/FreezeEngine.h"
#include "dsp/Presets.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>
#include <cstring>

namespace
{
constexpr double kTestRate = 48000.0;

// 8 s pluck rhythm: four chords, 2 s each, arpeggiated low-high-middle at 4 plucks per second.
juce::AudioBuffer<float> testPluck()
{
    const double chords[4][3] = { { 220.00, 261.63, 329.63 },   // A3 C4 E4
                                  { 174.61, 220.00, 261.63 },   // F3 A3 C4
                                  { 261.63, 329.63, 392.00 },   // C4 E4 G4
                                  { 196.00, 246.94, 293.66 } }; // G3 B3 D4
    const int order[4] = { 0, 2, 1, 2 };
    juce::AudioBuffer<float> out (2, (int) (8.0 * kTestRate));
    out.clear();
    const double lowpass = 1.0 - std::exp (-juce::MathConstants<double>::twoPi * 3000.0 / kTestRate);

    for (int pluck = 0; pluck < 32; ++pluck)
    {
        const double freq = chords[pluck / 8][order[pluck % 4]];
        const int start = (int) (pluck * 0.25 * kTestRate);
        double phase = 0.0, filtered = 0.0;
        for (int i = 0; i < (int) (0.18 * kTestRate); ++i)
        {
            const double t = i / kTestRate;
            const double saw = 2.0 * phase - 1.0;
            phase = std::fmod (phase + freq / kTestRate, 1.0);
            filtered += (saw - filtered) * lowpass;
            const auto value = (float) (0.3 * std::exp (-t / 0.06) * filtered);
            out.setSample (0, start + i, value);
            out.setSample (1, start + i, value);
        }
    }
    return out;
}

bool readWav (const juce::File& file, juce::AudioBuffer<float>& buffer, double& sampleRate)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
        return false;
    buffer.setSize (2, (int) reader->lengthInSamples);
    reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);
    if (reader->numChannels == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());
    sampleRate = reader->sampleRate;
    return true;
}

bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                   .withSampleRate (sampleRate)
                                                   .withNumChannels (2)
                                                   .withBitsPerSample (24));
    return writer != nullptr && writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

struct Settings
{
    int preset = 0;
    float freeze = 1.0f;
    bool sweep = false;
    float grain = -1.0f, fx = -1.0f; // -1: use the preset's starting value
    double holdAt = -1.0;           // seconds; switch Hold on from this point (-1: never)
};

juce::AudioBuffer<float> render (const juce::AudioBuffer<float>& dry, double sampleRate, const Settings& s)
{
    const auto& preset = gf::preset (s.preset);
    gf::FreezeEngine engine;
    engine.prepare (sampleRate, 512, 2);
    engine.setSeed (1);
    engine.setDensity (preset.density);
    engine.setFxCharacter (preset.character);
    engine.setGrainSeconds (s.grain >= 0.0f ? gf::grainKnobToSeconds (s.grain) : preset.grainSeconds);
    const float fx = s.fx >= 0.0f ? s.fx : preset.fx;

    auto out = dry;
    const int length = out.getNumSamples();
    std::vector<float> amount (512), fxAmount (512, fx);
    for (int pos = 0; pos < length; pos += 512)
    {
        const int n = std::min (512, length - pos);
        for (int i = 0; i < n; ++i)
            amount[(size_t) i] = s.sweep ? (float) (pos + i) / (float) length : s.freeze;
        engine.setHold (s.holdAt >= 0.0 && pos >= (int) (s.holdAt * sampleRate));
        float* channels[2] = { out.getWritePointer (0, pos), out.getWritePointer (1, pos) };
        engine.process (channels, 2, n, amount.data(), fxAmount.data());
    }
    return out;
}

int presetIndex (const char* name)
{
    for (int i = 0; i < gf::kNumPresets; ++i)
        if (juce::String (gf::preset (i).name).equalsIgnoreCase (name))
            return i;
    return -1;
}

void report (const juce::File& file, const juce::AudioBuffer<float>& buffer)
{
    const float peak = std::max (buffer.getMagnitude (0, 0, buffer.getNumSamples()),
                                 buffer.getMagnitude (1, 0, buffer.getNumSamples()));
    std::printf ("%s  (peak %.2f dBFS)\n", file.getFullPathName().toRawUTF8(), juce::Decibels::gainToDecibels (peak));
}
} // namespace

int main (int argc, char** argv)
{
    Settings settings;
    juce::String inPath, outPath = "renders/Freeze.wav";
    bool all = false;

    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg (argv[i]);
        const bool hasValue = i + 1 < argc;
        if (arg == "--in" && hasValue) inPath = argv[++i];
        else if (arg == "--out" && hasValue) outPath = argv[++i];
        else if (arg == "--freeze" && hasValue) settings.freeze = (float) std::atof (argv[++i]);
        else if (arg == "--grain" && hasValue) settings.grain = (float) std::atof (argv[++i]);
        else if (arg == "--fx" && hasValue) settings.fx = (float) std::atof (argv[++i]);
        else if (arg == "--sweep") settings.sweep = true;
        else if (arg == "--hold-at" && hasValue) settings.holdAt = std::atof (argv[++i]);
        else if (arg == "--all") all = true;
        else if (arg == "--test-pluck") inPath = {};
        else if (arg == "--preset" && hasValue)
        {
            settings.preset = presetIndex (argv[++i]);
            if (settings.preset < 0)
            {
                std::fprintf (stderr, "Unknown preset. Choose Ice, Glass, Haze, Ember or Abyss.\n");
                return 1;
            }
        }
        else
        {
            std::fprintf (stderr, "Usage: render [--test-pluck | --in file.wav] [--preset Name | --all] "
                                  "[--freeze 0..1 | --sweep] [--grain 0..1] [--fx 0..1] [--hold-at seconds] [--out file.wav]\n");
            return 1;
        }
    }

    juce::AudioBuffer<float> dry;
    double sampleRate = kTestRate;
    if (inPath.isEmpty())
        dry = testPluck();
    else if (! readWav (juce::File::getCurrentWorkingDirectory().getChildFile (inPath), dry, sampleRate))
    {
        std::fprintf (stderr, "Could not read %s\n", inPath.toRawUTF8());
        return 1;
    }

    // 2 s of silence at the end, so you can hear the freeze hold after the input stops
    dry.setSize (2, dry.getNumSamples() + (int) (2.0 * sampleRate), true, true);

    const auto outFile = juce::File::getCurrentWorkingDirectory().getChildFile (outPath);
    const auto dryFile = outFile.getSiblingFile ("00 Dry.wav");
    if (! writeWav (dryFile, dry, sampleRate))
        return 1;
    report (dryFile, dry);

    for (int p = all ? 0 : settings.preset; p < (all ? gf::kNumPresets : settings.preset + 1); ++p)
    {
        auto s = settings;
        s.preset = p;
        const auto file = all ? outFile.getSiblingFile (juce::String::formatted ("%02d ", p + 1) + gf::preset (p).name
                                                        + (s.sweep ? " sweep" : "") + ".wav")
                              : outFile;
        const auto wet = render (dry, sampleRate, s);
        if (! writeWav (file, wet, sampleRate))
            return 1;
        report (file, wet);
    }
    return 0;
}
