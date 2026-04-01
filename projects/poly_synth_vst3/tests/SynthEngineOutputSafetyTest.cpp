#include "../SynthEngine.h"

#include <array>
#include <iostream>

namespace
{
float renderAndMeasurePeak (SynthEngine& engine,
                            int numChannels,
                            int numSamples,
                            const juce::MidiBuffer& midiMessages)
{
    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    buffer.clear();
    engine.renderBlock (buffer, midiMessages);

    float peak = 0.0f;
    for (int channel = 0; channel < numChannels; ++channel)
    {
        const auto channelPeak = buffer.getMagnitude (channel, 0, numSamples);
        peak = juce::jmax (peak, channelPeak);
    }

    return peak;
}

bool validateDenseClusterOutputBounded()
{
    SynthEngine engine (32);
    engine.prepare (48000.0, 512);
    engine.setActiveVoiceCount (32);
    engine.setWaveform (SynthVoice::Waveform::square);
    engine.setOutputStage (SynthEngine::OutputStage::softLimit);

    juce::MidiBuffer denseCluster;
    constexpr std::array<int, 16> notes { 36, 40, 43, 47, 50, 52, 55, 59, 60, 64, 67, 71, 74, 76, 79, 83 };

    for (size_t noteIndex = 0; noteIndex < notes.size(); ++noteIndex)
        denseCluster.addEvent (juce::MidiMessage::noteOn (1, notes[noteIndex], (juce::uint8) 127), static_cast<int> (noteIndex % 8));

    auto peak = renderAndMeasurePeak (engine, 2, 512, denseCluster);

    if (peak > 1.02f)
    {
        std::cerr << "dense cluster output exceeded bounded level with soft limit: " << peak << '\n';
        return false;
    }

    juce::MidiBuffer noteOffs;
    for (const auto note : notes)
        noteOffs.addEvent (juce::MidiMessage::noteOff (1, note), 0);

    renderAndMeasurePeak (engine, 2, 512, noteOffs);

    for (int block = 0; block < 32; ++block)
    {
        juce::MidiBuffer midi;
        const auto releasePeak = renderAndMeasurePeak (engine, 2, 512, midi);
        if (releasePeak > 1.02f)
        {
            std::cerr << "release tail exceeded bounded level: " << releasePeak << '\n';
            return false;
        }
    }

    return true;
}

bool validateSilenceAndTailCleanupAcrossSettings()
{
    constexpr std::array<double, 3> sampleRates { 22050.0, 44100.0, 96000.0 };
    constexpr std::array<int, 3> blockSizes { 16, 64, 512 };

    for (const auto sampleRate : sampleRates)
    {
        for (const auto blockSize : blockSizes)
        {
            SynthEngine engine (16);
            engine.prepare (sampleRate, blockSize);
            engine.setActiveVoiceCount (16);
            engine.setOutputStage (SynthEngine::OutputStage::normalizeVoiceSum);

            juce::MidiBuffer noteOn;
            noteOn.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            const auto attackPeak = renderAndMeasurePeak (engine, 2, blockSize, noteOn);
            if (attackPeak <= 0.0f)
            {
                std::cerr << "note-on produced silence unexpectedly at sampleRate/blockSize: "
                          << sampleRate << "/" << blockSize << '\n';
                return false;
            }

            juce::MidiBuffer noteOff;
            noteOff.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            renderAndMeasurePeak (engine, 2, blockSize, noteOff);

            auto silenceSettled = false;
            for (int block = 0; block < 128; ++block)
            {
                juce::MidiBuffer emptyMidi;
                const auto peak = renderAndMeasurePeak (engine, 2, blockSize, emptyMidi);

                if (peak < 1.0e-4f)
                {
                    silenceSettled = true;
                    break;
                }
            }

            if (! silenceSettled)
            {
                std::cerr << "tail failed to settle to silence for sampleRate/blockSize: "
                          << sampleRate << "/" << blockSize << '\n';
                return false;
            }

            for (int voiceIndex = 0; voiceIndex < 16; ++voiceIndex)
            {
                if (engine.getVoiceMetadata (voiceIndex).isActive)
                {
                    std::cerr << "voice remained active after release tail cleanup at sampleRate/blockSize: "
                              << sampleRate << "/" << blockSize << '\n';
                    return false;
                }
            }
        }
    }

    return true;
}
} // namespace

int main()
{
    if (! validateDenseClusterOutputBounded())
        return 1;

    if (! validateSilenceAndTailCleanupAcrossSettings())
        return 1;

    std::cout << "synth engine output safety stress validation passed." << '\n';
    return 0;
}
