#include "../PolySynthAudioProcessor.h"

#include <cmath>
#include <iostream>

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::cerr << message << '\n';

    return condition;
}

bool validateAbsoluteAndRelativeStaySynchronized()
{
    PolySynthAudioProcessor processor;
    processor.addDefaultLayerAndSelect();

    const auto absolute = processor.setLayerRootNoteAbsolute (1, 67);
    const auto relative = processor.getLayerRootNoteRelativeSemitones (1);

    const auto absoluteViaRelative = processor.setLayerRootNoteRelativeSemitones (1, -5);

    return expect (absolute == 67, "absolute root-note setter should return assigned value")
        && expect (relative == 7, "relative representation should reflect absolute assignment")
        && expect (absoluteViaRelative == 55, "relative setter should map back to absolute root note")
        && expect (processor.getLayerRootNoteRelativeSemitones (1) == -5,
                   "absolute and relative root-note representations should remain synchronized");
}

bool validateBaseLayerChangesRetargetRelativeOffsets()
{
    PolySynthAudioProcessor processor;
    processor.addDefaultLayerAndSelect();
    processor.addDefaultLayerAndSelect();

    processor.setLayerRootNoteAbsolute (0, 60);
    processor.setLayerRootNoteRelativeSemitones (1, 7);
    processor.setLayerRootNoteRelativeSemitones (2, -12);

    processor.setLayerRootNoteAbsolute (0, 65);

    return expect (processor.getLayerRootNoteAbsolute (1) == 67, "layer absolute notes should remain unchanged when base moves")
        && expect (processor.getLayerRootNoteAbsolute (2) == 48, "layer absolute notes should remain unchanged when base moves")
        && expect (processor.getLayerRootNoteRelativeSemitones (1) == 2,
                   "base-root changes should update derived relative value for upper layer")
        && expect (processor.getLayerRootNoteRelativeSemitones (2) == -17,
                   "base-root changes should update derived relative value for lower layer");
}

float estimateFundamentalFrequency (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const auto* data = buffer.getReadPointer (0);
    const auto startSample = buffer.getNumSamples() / 4;
    const auto endSample = buffer.getNumSamples() - 1;
    int zeroCrossings = 0;

    for (int sample = startSample + 1; sample < endSample; ++sample)
    {
        if (data[sample - 1] <= 0.0f && data[sample] > 0.0f)
            ++zeroCrossings;
    }

    const auto measuredSamples = juce::jmax (1, endSample - startSample);
    const auto measuredSeconds = static_cast<double> (measuredSamples) / sampleRate;
    if (measuredSeconds <= 0.0)
        return 0.0f;

    return static_cast<float> (static_cast<double> (zeroCrossings) / measuredSeconds);
}

float renderAndEstimateSustainedFrequency (PolySynthAudioProcessor& processor,
                                           int midiNote,
                                           int noteOnVelocity,
                                           int numSamples,
                                           double sampleRate)
{
    processor.prepareToPlay (sampleRate, numSamples);

    juce::AudioBuffer<float> firstBlock (2, numSamples);
    firstBlock.clear();
    juce::MidiBuffer noteOnMidi;
    noteOnMidi.addEvent (juce::MidiMessage::noteOn (1, midiNote, static_cast<juce::uint8> (noteOnVelocity)), 0);
    processor.processBlock (firstBlock, noteOnMidi);

    juce::AudioBuffer<float> sustainedBlock (2, numSamples);
    sustainedBlock.clear();
    juce::MidiBuffer noMidi;
    processor.processBlock (sustainedBlock, noMidi);
    return estimateFundamentalFrequency (sustainedBlock, sampleRate);
}

bool validateLayerRootChangesRenderedPitch()
{
    constexpr auto sampleRate = 48000.0;
    constexpr auto numSamples = 4096;
    constexpr auto sourceMidiNote = 60;
    constexpr auto noteOnVelocity = 120;

    PolySynthAudioProcessor processor;
    processor.addDefaultLayerAndSelect();
    processor.setLayerMute (0, true);
    processor.setLayerWaveformByVisualIndex (1, PolySynthAudioProcessor::Waveform::sine);

    processor.setLayerRootNoteAbsolute (0, 60);
    processor.setLayerRootNoteAbsolute (1, 60);
    const auto unshiftedFrequency = renderAndEstimateSustainedFrequency (processor,
                                                                         sourceMidiNote,
                                                                         noteOnVelocity,
                                                                         numSamples,
                                                                         sampleRate);

    processor.setLayerRootNoteAbsolute (1, 72);
    const auto shiftedFrequency = renderAndEstimateSustainedFrequency (processor,
                                                                       sourceMidiNote,
                                                                       noteOnVelocity,
                                                                       numSamples,
                                                                       sampleRate);

    const auto measuredRatio = shiftedFrequency / juce::jmax (1.0f, unshiftedFrequency);
    const auto expectedRatio = std::pow (2.0f, 12.0f / 12.0f);

    return expect (unshiftedFrequency > 200.0f, "unshifted layer should render audible pitch near C4")
        && expect (shiftedFrequency > unshiftedFrequency * 1.8f, "raising layer root by 12 semitones should raise rendered pitch")
        && expect (std::abs (measuredRatio - expectedRatio) < 0.25f,
                   "rendered pitch ratio should track semitone mapping from root-note offset");
}

bool validateRootChangeDoesNotBreakNoteOffMatching()
{
    constexpr auto sampleRate = 48000.0;
    constexpr auto blockSize = 256;

    PolySynthAudioProcessor processor;
    processor.addDefaultLayerAndSelect();
    processor.setLayerMute (0, true);
    processor.setLayerRootNoteAbsolute (0, 60);
    processor.setLayerRootNoteAbsolute (1, 67);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> noteOnBlock (2, blockSize);
    noteOnBlock.clear();
    juce::MidiBuffer noteOnMidi;
    noteOnMidi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 127), 0);
    processor.processBlock (noteOnBlock, noteOnMidi);

    processor.setLayerRootNoteAbsolute (1, 72);

    juce::AudioBuffer<float> noteOffBlock (2, blockSize);
    noteOffBlock.clear();
    juce::MidiBuffer noteOffMidi;
    noteOffMidi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    processor.processBlock (noteOffBlock, noteOffMidi);

    juce::AudioBuffer<float> tailBlock (2, blockSize);
    juce::MidiBuffer noMidi;
    for (int i = 0; i < 20; ++i)
    {
        tailBlock.clear();
        processor.processBlock (tailBlock, noMidi);
    }

    const auto residualLevel = juce::jmax (tailBlock.getMagnitude (0, 0, blockSize),
                                           tailBlock.getMagnitude (1, 0, blockSize));
    return expect (residualLevel < 0.001f, "note-off should release active voice even after root transpose changes");
}

} // namespace

int main()
{
    if (! validateAbsoluteAndRelativeStaySynchronized())
        return 1;

    if (! validateBaseLayerChangesRetargetRelativeOffsets())
        return 1;

    if (! validateLayerRootChangesRenderedPitch())
        return 1;

    if (! validateRootChangeDoesNotBreakNoteOffMatching())
        return 1;

    std::cout << "root-note synchronization validation passed." << '\n';
    return 0;
}
