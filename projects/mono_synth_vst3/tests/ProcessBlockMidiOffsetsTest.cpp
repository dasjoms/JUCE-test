#include "../MonoSynthAudioProcessor.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
struct TimedMidiEvent
{
    int offset = 0;
    juce::MidiMessage message;
};

std::vector<TimedMidiEvent> createTestEvents()
{
    return {
        { 0,  juce::MidiMessage::noteOn  (1, 60, (juce::uint8) 100) },
        { 24, juce::MidiMessage::noteOff (1, 60) },
        { 24, juce::MidiMessage::noteOn  (1, 67, (juce::uint8) 110) },
        { 48, juce::MidiMessage::noteOff (1, 67) },
        { 64, juce::MidiMessage::noteOn  (1, 64, (juce::uint8) 100) },
        { 92, juce::MidiMessage::noteOff (1, 64) }
    };
}

juce::MidiBuffer createMidiBuffer (const std::vector<TimedMidiEvent>& events)
{
    juce::MidiBuffer midi;

    for (const auto& event : events)
        midi.addEvent (event.message, event.offset);

    return midi;
}

juce::AudioBuffer<float> processAsSingleBlock (MonoSynthAudioProcessor& processor,
                                               int numChannels,
                                               int numSamples,
                                               const std::vector<TimedMidiEvent>& events)
{
    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    buffer.clear();
    auto midi = createMidiBuffer (events);
    processor.processBlock (buffer, midi);
    return buffer;
}

juce::AudioBuffer<float> processAsEventSubranges (MonoSynthAudioProcessor& processor,
                                                  int numChannels,
                                                  int numSamples,
                                                  const std::vector<TimedMidiEvent>& events)
{
    juce::AudioBuffer<float> fullOutput (numChannels, numSamples);
    fullOutput.clear();

    std::vector<int> boundaries { 0, numSamples };
    boundaries.reserve (events.size() + 2);

    for (const auto& event : events)
        boundaries.push_back (juce::jlimit (0, numSamples, event.offset));

    std::sort (boundaries.begin(), boundaries.end());
    boundaries.erase (std::unique (boundaries.begin(), boundaries.end()), boundaries.end());

    for (size_t boundaryIndex = 0; boundaryIndex + 1 < boundaries.size(); ++boundaryIndex)
    {
        const auto start = boundaries[boundaryIndex];
        const auto end = boundaries[boundaryIndex + 1];
        const auto segmentSamples = end - start;

        if (segmentSamples <= 0)
            continue;

        juce::MidiBuffer segmentMidi;

        for (const auto& event : events)
            if (juce::jlimit (0, numSamples, event.offset) == start)
                segmentMidi.addEvent (event.message, 0);

        juce::AudioBuffer<float> segmentBuffer (numChannels, segmentSamples);
        segmentBuffer.clear();
        processor.processBlock (segmentBuffer, segmentMidi);

        for (int channel = 0; channel < numChannels; ++channel)
            fullOutput.copyFrom (channel, start, segmentBuffer, channel, 0, segmentSamples);
    }

    return fullOutput;
}

bool buffersMatch (const juce::AudioBuffer<float>& actual,
                   const juce::AudioBuffer<float>& expected,
                   float tolerance,
                   float& maxDifference)
{
    if (actual.getNumChannels() != expected.getNumChannels()
        || actual.getNumSamples() != expected.getNumSamples())
        return false;

    maxDifference = 0.0f;

    for (int channel = 0; channel < actual.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < actual.getNumSamples(); ++sample)
        {
            const auto difference = std::abs (actual.getSample (channel, sample)
                                              - expected.getSample (channel, sample));
            maxDifference = std::max (maxDifference, difference);

            if (difference > tolerance)
                return false;
        }
    }

    return true;
}

bool validateNoLargeRetriggerDiscontinuity()
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr int numChannels = 2;
    constexpr int retriggerOffset = 32;
    constexpr float maxAllowedJump = 0.05f;

    MonoSynthAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), retriggerOffset);
    midi.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), retriggerOffset);

    juce::AudioBuffer<float> buffer (numChannels, blockSize);
    buffer.clear();
    processor.processBlock (buffer, midi);

    const auto previousSample = buffer.getSample (0, retriggerOffset - 1);
    const auto retriggerSample = buffer.getSample (0, retriggerOffset);
    const auto jump = std::abs (retriggerSample - previousSample);

    if (jump > maxAllowedJump)
    {
        std::cerr << "Large retrigger discontinuity detected. jump=" << jump
                  << " threshold=" << maxAllowedJump << '\n';
        return false;
    }

    return true;
}
} // namespace

int main()
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr int numChannels = 2;
    constexpr float tolerance = 1.0e-6f;

    auto events = createTestEvents();

    MonoSynthAudioProcessor singleBlockProcessor;
    singleBlockProcessor.prepareToPlay (sampleRate, blockSize);

    MonoSynthAudioProcessor segmentedProcessor;
    segmentedProcessor.prepareToPlay (sampleRate, blockSize);

    const auto singleBlockOutput = processAsSingleBlock (singleBlockProcessor, numChannels, blockSize, events);
    const auto segmentedOutput = processAsEventSubranges (segmentedProcessor, numChannels, blockSize, events);

    float maxDifference = 0.0f;

    if (! buffersMatch (singleBlockOutput, segmentedOutput, tolerance, maxDifference))
    {
        std::cerr << "processBlock mismatch for multi-event offsets. Max diff: "
                  << maxDifference << '\n';
        return 1;
    }

    if (! validateNoLargeRetriggerDiscontinuity())
        return 1;

    std::cout << "processBlock MIDI offset validation passed. Max diff: "
              << maxDifference << '\n';
    return 0;
}
