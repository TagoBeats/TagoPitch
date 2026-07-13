// Offline render for the delta comparison against tagodsp.pitch.PitchShifter.
// Not shipped with the plugin; built via the TagoPitchRender target.
//
// Two modes:
//   stream  - exact plugin signal path (PitchEngine streaming + ramped gain),
//             latency trimmed afterwards. What a DAW render sounds like.
//   offline - mirrors the python-stretch binding call for call (seek, process,
//             flush, tail trim, scalar mix and gain). Proves engine parity;
//             should be near bit-exact against the Python reference.
//
// Usage: TagoPitchRender <stream|offline> in.wav out.wav pitch formant mix_percent gain_db [formant_base_hz]

#include <cstdlib>
#include <iostream>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "../plugin/PitchEngine.h"

namespace
{
struct Settings
{
    float pitch, formant, mix, gainDb, formantBase;
};

// Exact plugin signal path: PitchEngine streaming in 512er blocks plus the
// processor's ramped output gain, engine latency trimmed from the result.
juce::AudioBuffer<float> renderStream (const juce::AudioBuffer<float>& input, double sr, const Settings& s)
{
    const int channels = input.getNumChannels();
    const int n = input.getNumSamples();
    constexpr int blockSize = 512;

    tagopitch::PitchEngine engine;
    engine.prepare (sr, blockSize, channels);
    engine.setParameters (s.pitch, s.formant, s.formantBase, s.mix);
    const int latency = engine.latencySamples();

    juce::dsp::Gain<float> gain;
    gain.setRampDurationSeconds (0.02);
    gain.setGainDecibels (s.gainDb);
    gain.prepare ({ sr, (juce::uint32) blockSize, (juce::uint32) channels });

    const int total = n + latency;
    juce::AudioBuffer<float> out (channels, total);
    out.clear();

    juce::AudioBuffer<float> block (channels, blockSize);
    for (int pos = 0; pos < total; pos += blockSize)
    {
        const int len = std::min (blockSize, total - pos);
        block.clear();
        for (int c = 0; c < channels; ++c)
            if (pos < n)
                block.copyFrom (c, 0, input, c, pos, std::min (len, n - pos));

        juce::AudioBuffer<float> view (block.getArrayOfWritePointers(), channels, 0, len);
        engine.process (view);
        juce::dsp::AudioBlock<float> ab (view);
        gain.process (juce::dsp::ProcessContextReplacing<float> (ab));

        for (int c = 0; c < channels; ++c)
            out.copyFrom (c, pos, block, c, 0, len);
    }

    juce::AudioBuffer<float> trimmed (channels, n);
    for (int c = 0; c < channels; ++c)
        trimmed.copyFrom (c, 0, out, c, latency, n);

    std::cout << "stream latency=" << latency << " samples\n";
    return trimmed;
}

// Mirror of python-stretch's offline process() (signalsmith-bindings.cpp):
// zero-padded input, seek() pre-roll with the first inputLatency samples,
// process(), flush(), then drop the first outputLatency samples. Mix and gain
// applied as plain scalars like the Python reference.
juce::AudioBuffer<float> renderOffline (const juce::AudioBuffer<float>& input, double sr, const Settings& s)
{
    const int channels = input.getNumChannels();
    const int n = input.getNumSamples();

    signalsmith::stretch::SignalsmithStretch<float> stretch;
    stretch.presetDefault (channels, (float) sr);
    stretch.setTransposeSemitones (s.pitch, tagopitch::PitchEngine::tonalityLimitHz / (float) sr);
    stretch.setFormantSemitones (s.formant, true);
    stretch.setFormantBase (s.formantBase);

    const int inLat = stretch.inputLatency();
    const int outLat = stretch.outputLatency();

    std::vector<std::vector<float>> in ((size_t) channels, std::vector<float> ((size_t) (n + inLat), 0.0f));
    std::vector<std::vector<float>> out ((size_t) channels, std::vector<float> ((size_t) (n + outLat), 0.0f));
    std::vector<float*> inPtr ((size_t) channels), outPtr ((size_t) channels);
    for (int c = 0; c < channels; ++c)
    {
        std::copy_n (input.getReadPointer (c), n, in[(size_t) c].data());
        inPtr[(size_t) c] = in[(size_t) c].data();
        outPtr[(size_t) c] = out[(size_t) c].data();
    }

    stretch.seek (inPtr.data(), inLat, 1.0);

    std::vector<float*> inOff ((size_t) channels), outOff ((size_t) channels);
    for (int c = 0; c < channels; ++c)
        inOff[(size_t) c] = inPtr[(size_t) c] + inLat;
    stretch.process (inOff.data(), n, outPtr.data(), n);

    for (int c = 0; c < channels; ++c)
        outOff[(size_t) c] = outPtr[(size_t) c] + n;
    stretch.flush (outOff.data(), outLat);

    // wet = out[outLat : outLat + n], y = (1 - mix) * dry + mix * wet, then gain
    const float g = juce::Decibels::decibelsToGain (s.gainDb);
    juce::AudioBuffer<float> result (channels, n);
    for (int c = 0; c < channels; ++c)
    {
        const float* dry = input.getReadPointer (c);
        const float* wet = out[(size_t) c].data() + outLat;
        float* y = result.getWritePointer (c);
        for (int i = 0; i < n; ++i)
            y[i] = ((1.0f - s.mix) * dry[i] + s.mix * wet[i]) * g;
    }

    std::cout << "offline latency=" << (inLat + outLat) << " samples\n";
    return result;
}
} // namespace

int main (int argc, char* argv[])
{
    if (argc < 8)
    {
        std::cerr << "usage: TagoPitchRender <stream|offline> in.wav out.wav pitch formant mix gain_db [formant_base_hz]\n";
        return 1;
    }

    const juce::String mode (argv[1]);
    const juce::File inFile (argv[2]);
    const juce::File outFile (argv[3]);
    const Settings s { (float) atof (argv[4]), (float) atof (argv[5]),
                       (float) atof (argv[6]) * 0.01f, (float) atof (argv[7]),
                       argc > 8 ? (float) atof (argv[8]) : 0.0f };

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (inFile));
    if (reader == nullptr)
    {
        std::cerr << "cannot read " << inFile.getFullPathName() << "\n";
        return 1;
    }

    const int channels = (int) reader->numChannels;
    const int n = (int) reader->lengthInSamples;
    const double sr = reader->sampleRate;

    juce::AudioBuffer<float> input (channels, n);
    reader->read (&input, 0, n, 0, true, true);

    const auto result = mode == "offline" ? renderOffline (input, sr, s)
                                          : renderStream (input, sr, s);

    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (new juce::FileOutputStream (outFile));
    auto writer = wav.createWriterFor (stream,
                                       juce::AudioFormatWriterOptions {}
                                           .withSampleRate (sr)
                                           .withNumChannels (channels)
                                           .withBitsPerSample (32));
    if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (result, 0, n))
    {
        std::cerr << "cannot write " << outFile.getFullPathName() << "\n";
        return 1;
    }
    writer->flush();
    return 0;
}
