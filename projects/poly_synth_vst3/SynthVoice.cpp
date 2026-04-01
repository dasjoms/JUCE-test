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
    voiceGroupId = 0;
    currentFrequencyHz = 440.0;
    pendingFrequencyHz = 440.0;
    currentNoteOnVelocity = 1.0f;
    pendingNoteOnVelocity = 1.0f;
    currentAmplitude = 0.0f;
    envelopeStage = EnvelopeStage::idle;
    noteTransitionState = NoteTransitionState::none;
    shouldResetPhaseOnNoteChange = false;
    startOrder = 0;
    attackStep = 1.0f / static_cast<float> (juce::jmax (1.0, currentSampleRate * attackTimeSeconds));
    decayStep = (1.0f - sustainLevel) / static_cast<float> (juce::jmax (1.0, currentSampleRate * decayTimeSeconds));
    releaseStep = 1.0f / static_cast<float> (juce::jmax (1.0, currentSampleRate * releaseTimeSeconds));
    noteTransitionStep = 1.0f / static_cast<float> (juce::jmax (1.0, currentSampleRate * noteTransitionRampTimeSeconds));
    updatePhaseIncrement();
    updateLfoIncrement();
}

void SynthVoice::setWaveform (Waveform newWaveform) noexcept
{
    waveform = newWaveform;
}

void SynthVoice::setEnvelopeTimes (float attackSeconds, float decaySeconds, float newSustainLevel, float releaseSeconds) noexcept
{
    attackTimeSeconds = juce::jmax (0.001f, attackSeconds);
    decayTimeSeconds = juce::jmax (0.001f, decaySeconds);
    sustainLevel = juce::jlimit (0.0f, 1.0f, newSustainLevel);
    releaseTimeSeconds = juce::jmax (0.001f, releaseSeconds);
    attackStep = 1.0f / static_cast<float> (juce::jmax (1.0, currentSampleRate * attackTimeSeconds));
    decayStep = (1.0f - sustainLevel) / static_cast<float> (juce::jmax (1.0, currentSampleRate * decayTimeSeconds));
    releaseStep = 1.0f / static_cast<float> (juce::jmax (1.0, currentSampleRate * releaseTimeSeconds));
}

void SynthVoice::setModulationParameters (float depth, float rateHz, ModulationDestination destination) noexcept
{
    modulationDepth = juce::jlimit (0.0f, 1.0f, depth);
    modulationRateHz = juce::jlimit (0.0f, 20.0f, rateHz);
    modulationDestination = destination;
    updateLfoIncrement();
}

void SynthVoice::setVelocitySensitivity (float sensitivity) noexcept
{
    velocitySensitivity = juce::jlimit (0.0f, 1.0f, sensitivity);
}

void SynthVoice::setStartOrder (uint64_t newStartOrder) noexcept
{
    startOrder = newStartOrder;
}

void SynthVoice::setVoiceGroupId (int newVoiceGroupId) noexcept
{
    voiceGroupId = juce::jmax (0, newVoiceGroupId);
}

void SynthVoice::setDetuneCents (float cents) noexcept
{
    detuneCents = juce::jlimit (-2400.0f, 2400.0f, cents);
    updatePhaseIncrement();
}

void SynthVoice::noteOn (int midiNoteNumber, float velocity) noexcept
{
    const auto incomingFrequencyHz = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    const auto normalizedVelocity = juce::jlimit (0.0f, 1.0f, velocity);

    if (currentAmplitude > minimumEnvelopeValue)
    {
        pendingMidiNote = midiNoteNumber;
        pendingFrequencyHz = incomingFrequencyHz;
        pendingNoteOnVelocity = normalizedVelocity;
        shouldResetPhaseOnNoteChange = true;
        gateOpen = false;
        envelopeStage = EnvelopeStage::release;
        noteTransitionState = NoteTransitionState::rampDownForNoteChange;
        return;
    }

    const auto shouldResetPhase = (! gateOpen) || (currentAmplitude <= minimumEnvelopeValue);
    currentMidiNote = midiNoteNumber;
    currentFrequencyHz = incomingFrequencyHz;
    currentNoteOnVelocity = normalizedVelocity;

    if (shouldResetPhase)
        phase = 0.0;

    gateOpen = true;
    envelopeStage = EnvelopeStage::attack;
    lfoPhase = 0.0;
    noteTransitionState = NoteTransitionState::none;
    pendingMidiNote = -1;
    pendingFrequencyHz = currentFrequencyHz;
    pendingNoteOnVelocity = currentNoteOnVelocity;
    shouldResetPhaseOnNoteChange = false;
    updatePhaseIncrement();
}

void SynthVoice::noteOff (int midiNoteNumber) noexcept
{
    if (midiNoteNumber == currentMidiNote)
    {
        gateOpen = false;
        if (noteTransitionState == NoteTransitionState::none)
            envelopeStage = EnvelopeStage::release;
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
    envelopeStage = EnvelopeStage::release;
    voiceGroupId = 0;
}

void SynthVoice::handleMidiEvent (const juce::MidiMessage& midiMessage) noexcept
{
    if (midiMessage.isNoteOn())
    {
        noteOn (midiMessage.getNoteNumber(), midiMessage.getFloatVelocity());
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
    if (noteTransitionState == NoteTransitionState::rampDownForNoteChange)
    {
        currentAmplitude = juce::jmax (0.0f, currentAmplitude - noteTransitionStep);
    }
    else
    {
        switch (envelopeStage)
        {
            case EnvelopeStage::idle:
                currentAmplitude = 0.0f;
                break;

            case EnvelopeStage::attack:
                currentAmplitude = juce::jmin (1.0f, currentAmplitude + attackStep);

                if (currentAmplitude >= (1.0f - minimumEnvelopeValue))
                {
                    currentAmplitude = 1.0f;
                    envelopeStage = sustainLevel < (1.0f - minimumEnvelopeValue) ? EnvelopeStage::decay
                                                                                  : EnvelopeStage::sustain;
                }

                break;

            case EnvelopeStage::decay:
                currentAmplitude = juce::jmax (sustainLevel, currentAmplitude - decayStep);

                if (currentAmplitude <= (sustainLevel + minimumEnvelopeValue))
                {
                    currentAmplitude = sustainLevel;
                    envelopeStage = EnvelopeStage::sustain;
                }

                break;

            case EnvelopeStage::sustain:
                currentAmplitude = sustainLevel;
                break;

            case EnvelopeStage::release:
                currentAmplitude = juce::jmax (0.0f, currentAmplitude - releaseStep);

                if (currentAmplitude <= minimumEnvelopeValue)
                {
                    currentAmplitude = 0.0f;
                    envelopeStage = EnvelopeStage::idle;
                }

                break;
        }
    }

    if (noteTransitionState == NoteTransitionState::rampDownForNoteChange
        && currentAmplitude <= minimumEnvelopeValue)
    {
        currentAmplitude = 0.0f;
        currentMidiNote = pendingMidiNote;
        currentFrequencyHz = pendingFrequencyHz;
        currentNoteOnVelocity = pendingNoteOnVelocity;

        if (shouldResetPhaseOnNoteChange)
            phase = 0.0;

        updatePhaseIncrement();
        lfoPhase = 0.0;
        gateOpen = true;
        noteTransitionState = NoteTransitionState::none;
        envelopeStage = EnvelopeStage::attack;
        pendingMidiNote = -1;
        pendingFrequencyHz = currentFrequencyHz;
        pendingNoteOnVelocity = currentNoteOnVelocity;
    }

    if (gateOpen || currentAmplitude > minimumEnvelopeValue)
    {
        const auto lfoSample = static_cast<float> (std::sin (lfoPhase));
        const auto unipolarLfo = 0.5f * (1.0f + lfoSample);

        auto oscillatorPhaseIncrement = phaseIncrement;
        auto modulationGain = 1.0f;
        auto pulseWidth = 0.5f;

        switch (modulationDestination)
        {
            case ModulationDestination::amplitude:
                modulationGain = (1.0f - modulationDepth) + (modulationDepth * unipolarLfo);
                break;

            case ModulationDestination::pitch:
            {
                constexpr auto pitchDepthInSemitones = 12.0f;
                const auto semitoneOffset = pitchDepthInSemitones * modulationDepth * lfoSample;
                const auto pitchRatio = std::pow (2.0, static_cast<double> (semitoneOffset) / 12.0);
                oscillatorPhaseIncrement = phaseIncrement * pitchRatio;
                break;
            }

            case ModulationDestination::pulseWidth:
                pulseWidth = juce::jlimit (0.05f, 0.95f, 0.5f + (0.45f * modulationDepth * lfoSample));
                break;
        }

        const auto velocityGain = juce::jmap (velocitySensitivity, 1.0f, currentNoteOnVelocity);
        const auto sampleValue = outputLevel * velocityGain * currentAmplitude * modulationGain * getOscillatorSample (phase, waveform, pulseWidth);

        phase += oscillatorPhaseIncrement;
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
    voiceGroupId = 0;
    currentNoteOnVelocity = 1.0f;
    pendingNoteOnVelocity = 1.0f;
    shouldResetPhaseOnNoteChange = false;
    noteTransitionState = NoteTransitionState::none;
    envelopeStage = EnvelopeStage::idle;
    return 0.0f;
}

SynthVoice::RuntimeMetadata SynthVoice::getRuntimeMetadata() const noexcept
{
    RuntimeMetadata metadata;
    metadata.isActive = envelopeStage != EnvelopeStage::idle || noteTransitionState != NoteTransitionState::none;
    metadata.isReleasing = metadata.isActive && (envelopeStage == EnvelopeStage::release);
    metadata.startOrder = startOrder;
    metadata.amplitudeEstimate = currentAmplitude;
    metadata.noteOnVelocity = currentNoteOnVelocity;
    metadata.midiNote = currentMidiNote;
    metadata.voiceGroupId = voiceGroupId;
    metadata.detuneCents = detuneCents;
    return metadata;
}

void SynthVoice::updatePhaseIncrement() noexcept
{
    phaseIncrement = twoPi * getDetunedFrequencyHz (currentFrequencyHz) / currentSampleRate;
}

void SynthVoice::updateLfoIncrement() noexcept
{
    lfoPhaseIncrement = twoPi * modulationRateHz / currentSampleRate;
}

float SynthVoice::getOscillatorSample (double phaseInRadians, Waveform waveformType, float pulseWidth) noexcept
{
    switch (waveformType)
    {
        case Waveform::sine:
            return std::sin (phaseInRadians);
        case Waveform::saw:
            return static_cast<float> ((phaseInRadians * inverseTwoPi * 2.0) - 1.0);
        case Waveform::square:
        {
            const auto wrappedPulseWidth = juce::jlimit (0.01f, 0.99f, pulseWidth);
            const auto normalizedPhase = phaseInRadians * inverseTwoPi;
            return normalizedPhase < wrappedPulseWidth ? 1.0f : -1.0f;
        }
        case Waveform::triangle:
        {
            const auto normalizedPhase = phaseInRadians * inverseTwoPi;
            return static_cast<float> (1.0 - 4.0 * std::abs (normalizedPhase - 0.5));
        }
    }

    jassertfalse;
    return 0.0f;
}


double SynthVoice::getDetunedFrequencyHz (double baseFrequencyHz) const noexcept
{
    const auto detuneRatio = std::pow (2.0, static_cast<double> (detuneCents) / 1200.0);
    return baseFrequencyHz * detuneRatio;
}
