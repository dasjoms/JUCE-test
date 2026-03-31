#include "SynthVoice.h"

#include <cmath>

void SynthVoice::prepare (double sampleRate) noexcept
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    phase = 0.0;
    gateOpen = false;
    lfoPhase = 0.0;
    currentMidiNote = -1;
    pendingMidiNote = -1;
    currentFrequencyHz = 440.0;
    pendingFrequencyHz = 440.0;
    currentAmplitude = 0.0f;
    noteTransitionState = NoteTransitionState::none;
    shouldResetPhaseOnNoteChange = false;
    startOrder = 0;
    attackStep = 1.0f / static_cast<float> (juce::jmax (1.0, currentSampleRate * attackTimeSeconds));
    releaseStep = 1.0f / static_cast<float> (juce::jmax (1.0, currentSampleRate * releaseTimeSeconds));
    noteTransitionStep = 1.0f / static_cast<float> (juce::jmax (1.0, currentSampleRate * noteTransitionRampTimeSeconds));
    updatePhaseIncrement();
    updateLfoIncrement();
}

void SynthVoice::setWaveform (Waveform newWaveform) noexcept
{
    waveform = newWaveform;
}

void SynthVoice::setEnvelopeTimes (float attackSeconds, float releaseSeconds) noexcept
{
    attackTimeSeconds = juce::jmax (0.001f, attackSeconds);
    releaseTimeSeconds = juce::jmax (0.001f, releaseSeconds);
    attackStep = 1.0f / static_cast<float> (juce::jmax (1.0, currentSampleRate * attackTimeSeconds));
    releaseStep = 1.0f / static_cast<float> (juce::jmax (1.0, currentSampleRate * releaseTimeSeconds));
}

void SynthVoice::setModulationParameters (float depth, float rateHz) noexcept
{
    modulationDepth = juce::jlimit (0.0f, 1.0f, depth);
    modulationRateHz = juce::jlimit (0.0f, 20.0f, rateHz);
    updateLfoIncrement();
}

void SynthVoice::setStartOrder (uint64_t newStartOrder) noexcept
{
    startOrder = newStartOrder;
}

void SynthVoice::noteOn (int midiNoteNumber) noexcept
{
    const auto incomingFrequencyHz = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);

    if (currentAmplitude > minimumEnvelopeValue)
    {
        pendingMidiNote = midiNoteNumber;
        pendingFrequencyHz = incomingFrequencyHz;
        shouldResetPhaseOnNoteChange = true;
        gateOpen = false;
        noteTransitionState = NoteTransitionState::rampDownForNoteChange;
        return;
    }

    const auto shouldResetPhase = (! gateOpen) || (currentAmplitude <= minimumEnvelopeValue);
    currentMidiNote = midiNoteNumber;
    currentFrequencyHz = incomingFrequencyHz;

    if (shouldResetPhase)
        phase = 0.0;

    gateOpen = true;
    lfoPhase = 0.0;
    noteTransitionState = NoteTransitionState::none;
    pendingMidiNote = -1;
    pendingFrequencyHz = currentFrequencyHz;
    shouldResetPhaseOnNoteChange = false;
    updatePhaseIncrement();
}

void SynthVoice::noteOff (int midiNoteNumber) noexcept
{
    if (midiNoteNumber == currentMidiNote)
    {
        gateOpen = false;

        if (noteTransitionState == NoteTransitionState::rampUpAfterNoteChange)
            noteTransitionState = NoteTransitionState::none;
    }

    if (midiNoteNumber == pendingMidiNote)
    {
        pendingMidiNote = -1;
        pendingFrequencyHz = currentFrequencyHz;

        if (noteTransitionState == NoteTransitionState::rampDownForNoteChange)
            noteTransitionState = NoteTransitionState::none;
    }
}

void SynthVoice::allNotesOff() noexcept
{
    gateOpen = false;
    noteTransitionState = NoteTransitionState::none;
}

void SynthVoice::handleMidiEvent (const juce::MidiMessage& midiMessage) noexcept
{
    if (midiMessage.isNoteOn())
    {
        noteOn (midiMessage.getNoteNumber());
        return;
    }

    if (midiMessage.isNoteOff())
    {
        noteOff (midiMessage.getNoteNumber());
        return;
    }

    if (midiMessage.isAllNotesOff() || midiMessage.isAllSoundOff())
        allNotesOff();
}

float SynthVoice::renderSample() noexcept
{
    const auto shouldRunTransitionRamp = (noteTransitionState == NoteTransitionState::rampDownForNoteChange)
                                      || (noteTransitionState == NoteTransitionState::rampUpAfterNoteChange);
    const auto targetAmplitude = shouldRunTransitionRamp ? (noteTransitionState == NoteTransitionState::rampDownForNoteChange ? 0.0f : 1.0f)
                                                         : (gateOpen ? 1.0f : 0.0f);
    const auto envelopeStep = shouldRunTransitionRamp ? noteTransitionStep
                                                      : (gateOpen ? attackStep : releaseStep);

    if (currentAmplitude < targetAmplitude)
        currentAmplitude = juce::jmin (targetAmplitude, currentAmplitude + envelopeStep);
    else if (currentAmplitude > targetAmplitude)
        currentAmplitude = juce::jmax (targetAmplitude, currentAmplitude - envelopeStep);

    if (noteTransitionState == NoteTransitionState::rampDownForNoteChange
        && currentAmplitude <= minimumEnvelopeValue)
    {
        currentAmplitude = 0.0f;
        currentMidiNote = pendingMidiNote;
        currentFrequencyHz = pendingFrequencyHz;

        if (shouldResetPhaseOnNoteChange)
            phase = 0.0;

        updatePhaseIncrement();
        lfoPhase = 0.0;
        gateOpen = true;
        noteTransitionState = NoteTransitionState::rampUpAfterNoteChange;
    }
    else if (noteTransitionState == NoteTransitionState::rampUpAfterNoteChange
             && currentAmplitude >= (1.0f - minimumEnvelopeValue))
    {
        noteTransitionState = NoteTransitionState::none;
    }

    if (gateOpen || currentAmplitude > minimumEnvelopeValue)
    {
        const auto lfoSample = std::sin (lfoPhase);
        const auto unipolarLfo = 0.5f * (1.0f + static_cast<float> (lfoSample));
        const auto tremoloGain = (1.0f - modulationDepth) + (modulationDepth * unipolarLfo);
        const auto sampleValue = outputLevel * currentAmplitude * tremoloGain * getOscillatorSample (phase, waveform);

        phase += phaseIncrement;
        if (phase >= twoPi)
            phase -= twoPi;

        lfoPhase += lfoPhaseIncrement;
        if (lfoPhase >= twoPi)
            lfoPhase -= twoPi;

        return sampleValue;
    }

    currentAmplitude = 0.0f;
    currentMidiNote = -1;
    pendingMidiNote = -1;
    shouldResetPhaseOnNoteChange = false;
    noteTransitionState = NoteTransitionState::none;
    return 0.0f;
}

SynthVoice::RuntimeMetadata SynthVoice::getRuntimeMetadata() const noexcept
{
    RuntimeMetadata metadata;
    metadata.isActive = gateOpen || currentAmplitude > minimumEnvelopeValue || noteTransitionState != NoteTransitionState::none;
    metadata.isReleasing = metadata.isActive && ! gateOpen;
    metadata.startOrder = startOrder;
    metadata.amplitudeEstimate = currentAmplitude;
    metadata.midiNote = currentMidiNote;
    return metadata;
}

void SynthVoice::updatePhaseIncrement() noexcept
{
    phaseIncrement = twoPi * currentFrequencyHz / currentSampleRate;
}

void SynthVoice::updateLfoIncrement() noexcept
{
    lfoPhaseIncrement = twoPi * modulationRateHz / currentSampleRate;
}

float SynthVoice::getOscillatorSample (double phaseInRadians, Waveform waveformType) noexcept
{
    switch (waveformType)
    {
        case Waveform::sine:
            return std::sin (phaseInRadians);
        case Waveform::saw:
            return static_cast<float> ((phaseInRadians * inverseTwoPi * 2.0) - 1.0);
        case Waveform::square:
            return phaseInRadians < juce::MathConstants<double>::pi ? 1.0f : -1.0f;
        case Waveform::triangle:
        {
            const auto normalizedPhase = phaseInRadians * inverseTwoPi;
            return static_cast<float> (1.0 - 4.0 * std::abs (normalizedPhase - 0.5));
        }
    }

    jassertfalse;
    return 0.0f;
}
