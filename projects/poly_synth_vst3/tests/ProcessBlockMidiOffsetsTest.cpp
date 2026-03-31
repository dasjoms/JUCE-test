#include "../PolySynthAudioProcessor.h"

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

std::vector<TimedMidiEvent> createReferenceEvents()
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

juce::AudioBuffer<float> processAsSingleBlock (PolySynthAudioProcessor& processor,
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

juce::AudioBuffer<float> processAsEventSubranges (PolySynthAudioProcessor& processor,
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

bool validateEventSubrangeEquivalence (const std::vector<TimedMidiEvent>& events,
                                       const char* scenario,
                                       double sampleRate,
                                       int blockSize,
                                       int numChannels,
                                       float tolerance)
{
    PolySynthAudioProcessor singleBlockProcessor;
    singleBlockProcessor.prepareToPlay (sampleRate, blockSize);

    PolySynthAudioProcessor segmentedProcessor;
    segmentedProcessor.prepareToPlay (sampleRate, blockSize);

    const auto singleBlockOutput = processAsSingleBlock (singleBlockProcessor, numChannels, blockSize, events);
    const auto segmentedOutput = processAsEventSubranges (segmentedProcessor, numChannels, blockSize, events);

    float maxDifference = 0.0f;

    if (! buffersMatch (singleBlockOutput, segmentedOutput, tolerance, maxDifference))
    {
        std::cerr << scenario << " mismatch between single block and event subrange rendering. Max diff: "
                  << maxDifference << '\n';
        return false;
    }

    return true;
}

bool validateNoLargeRetriggerDiscontinuity (const std::vector<TimedMidiEvent>& events,
                                            int retriggerOffset,
                                            float maxAllowedJump,
                                            const char* scenario,
                                            double sampleRate,
                                            int blockSize,
                                            int numChannels)
{
    PolySynthAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer (numChannels, blockSize);
    buffer.clear();

    auto midi = createMidiBuffer (events);
    processor.processBlock (buffer, midi);

    const auto previousSample = buffer.getSample (0, retriggerOffset - 1);
    const auto retriggerSample = buffer.getSample (0, retriggerOffset);
    const auto jump = std::abs (retriggerSample - previousSample);

    if (jump > maxAllowedJump)
    {
        std::cerr << scenario << " large retrigger discontinuity detected. jump=" << jump
                  << " threshold=" << maxAllowedJump << '\n';
        return false;
    }

    return true;
}

bool validateLatestNoteWinsRetrigger (double sampleRate, int blockSize, int numChannels, float tolerance)
{
    constexpr int overlapOffset = 48;

    const std::vector<TimedMidiEvent> events {
        { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110) },
        { overlapOffset, juce::MidiMessage::noteOn (1, 64, (juce::uint8) 110) },
        { overlapOffset, juce::MidiMessage::noteOn (1, 67, (juce::uint8) 110) },
        { 96, juce::MidiMessage::noteOff (1, 67) }
    };

    if (! validateEventSubrangeEquivalence (events,
                                            "latest note wins",
                                            sampleRate,
                                            blockSize,
                                            numChannels,
                                            tolerance))
        return false;

    return validateNoLargeRetriggerDiscontinuity (events,
                                                  overlapOffset,
                                                  0.05f,
                                                  "latest note wins",
                                                  sampleRate,
                                                  blockSize,
                                                  numChannels);
}

bool validateNoteOffOnlyClosesCurrentGate (double sampleRate, int blockSize, int numChannels, float tolerance)
{
    constexpr int noteSwapOffset = 40;

    const std::vector<TimedMidiEvent> noteOffCurrentFirst {
        { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110) },
        { noteSwapOffset, juce::MidiMessage::noteOff (1, 60) },
        { noteSwapOffset, juce::MidiMessage::noteOn (1, 67, (juce::uint8) 110) },
        { 88, juce::MidiMessage::noteOff (1, 67) },
        { 100, juce::MidiMessage::noteOff (1, 60) }
    };

    const std::vector<TimedMidiEvent> noteOffOldNoteFirst {
        { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110) },
        { noteSwapOffset, juce::MidiMessage::noteOn (1, 67, (juce::uint8) 110) },
        { noteSwapOffset, juce::MidiMessage::noteOff (1, 60) },
        { 88, juce::MidiMessage::noteOff (1, 67) },
        { 100, juce::MidiMessage::noteOff (1, 60) }
    };

    float maxDifference = 0.0f;

    PolySynthAudioProcessor processorA;
    processorA.prepareToPlay (sampleRate, blockSize);

    PolySynthAudioProcessor processorB;
    processorB.prepareToPlay (sampleRate, blockSize);

    const auto outputA = processAsSingleBlock (processorA, numChannels, blockSize, noteOffCurrentFirst);
    const auto outputB = processAsSingleBlock (processorB, numChannels, blockSize, noteOffOldNoteFirst);

    if (! buffersMatch (outputA, outputB, tolerance, maxDifference))
    {
        std::cerr << "note-off gate closure mismatch between same-offset note ordering. Max diff: "
                  << maxDifference << '\n';
        return false;
    }

    if (! validateEventSubrangeEquivalence (noteOffCurrentFirst,
                                            "note-off current note ordering A",
                                            sampleRate,
                                            blockSize,
                                            numChannels,
                                            tolerance))
        return false;

    return validateEventSubrangeEquivalence (noteOffOldNoteFirst,
                                             "note-off current note ordering B",
                                             sampleRate,
                                             blockSize,
                                             numChannels,
                                             tolerance);
}

bool validateAllNotesAndSoundOffCloseGate (double sampleRate, int blockSize, int numChannels, float tolerance)
{
    const std::vector<TimedMidiEvent> allNotesOffEvents {
        { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) },
        { 36, juce::MidiMessage::allNotesOff (1) }
    };

    const std::vector<TimedMidiEvent> allSoundOffEvents {
        { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) },
        { 36, juce::MidiMessage::allSoundOff (1) }
    };

    PolySynthAudioProcessor processorNotesOff;
    processorNotesOff.prepareToPlay (sampleRate, blockSize);

    PolySynthAudioProcessor processorSoundOff;
    processorSoundOff.prepareToPlay (sampleRate, blockSize);

    const auto notesOffOutput = processAsSingleBlock (processorNotesOff, numChannels, blockSize, allNotesOffEvents);
    const auto soundOffOutput = processAsSingleBlock (processorSoundOff, numChannels, blockSize, allSoundOffEvents);

    float maxDifference = 0.0f;

    if (! buffersMatch (notesOffOutput, soundOffOutput, tolerance, maxDifference))
    {
        std::cerr << "all-notes-off/all-sound-off behavior mismatch. Max diff: "
                  << maxDifference << '\n';
        return false;
    }

    if (! validateEventSubrangeEquivalence (allNotesOffEvents,
                                            "all notes off",
                                            sampleRate,
                                            blockSize,
                                            numChannels,
                                            tolerance))
        return false;

    return validateEventSubrangeEquivalence (allSoundOffEvents,
                                             "all sound off",
                                             sampleRate,
                                             blockSize,
                                             numChannels,
                                             tolerance);
}

bool validateNoLargeDiscontinuityAcrossNoteTransitions (double sampleRate,
                                                        int blockSize,
                                                        int numChannels,
                                                        float tolerance)
{
    constexpr int retriggerOffset = 32;

    const std::vector<TimedMidiEvent> events {
        { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) },
        { retriggerOffset, juce::MidiMessage::noteOff (1, 60) },
        { retriggerOffset, juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100) },
        { 86, juce::MidiMessage::noteOff (1, 67) }
    };

    if (! validateEventSubrangeEquivalence (events,
                                            "retrigger discontinuity reference",
                                            sampleRate,
                                            blockSize,
                                            numChannels,
                                            tolerance))
        return false;

    return validateNoLargeRetriggerDiscontinuity (events,
                                                  retriggerOffset,
                                                  0.05f,
                                                  "retrigger discontinuity reference",
                                                  sampleRate,
                                                  blockSize,
                                                  numChannels);
}

} // namespace

int main()
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr int numChannels = 2;
    constexpr float tolerance = 1.0e-6f;

    if (! validateEventSubrangeEquivalence (createReferenceEvents(),
                                            "reference scenario",
                                            sampleRate,
                                            blockSize,
                                            numChannels,
                                            tolerance))
        return 1;

    if (! validateLatestNoteWinsRetrigger (sampleRate, blockSize, numChannels, tolerance))
        return 1;

    if (! validateNoteOffOnlyClosesCurrentGate (sampleRate, blockSize, numChannels, tolerance))
        return 1;

    if (! validateAllNotesAndSoundOffCloseGate (sampleRate, blockSize, numChannels, tolerance))
        return 1;

    if (! validateNoLargeDiscontinuityAcrossNoteTransitions (sampleRate, blockSize, numChannels, tolerance))
        return 1;

    std::cout << "processBlock MIDI offset and transition validation passed." << '\n';
    return 0;
}
