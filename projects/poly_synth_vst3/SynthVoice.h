#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cstdint>

class SynthVoice final
{
public:
    enum class Waveform
    {
        sine = 0,
        saw,
        square,
        triangle
    };

    enum class ModulationDestination
    {
        amplitude = 0,
        pitch,
        pulseWidth
    };

    struct RuntimeMetadata
    {
        bool isActive = false;
        bool isReleasing = false;
        uint64_t startOrder = 0;
        float amplitudeEstimate = 0.0f;
        float noteOnVelocity = 1.0f;
        int midiNote = -1;
        int voiceGroupId = 0;
        float detuneCents = 0.0f;
    };

    void prepare (double sampleRate) noexcept;
    void setWaveform (Waveform newWaveform) noexcept;
    void setEnvelopeTimes (float attackSeconds, float decaySeconds, float sustainLevel, float releaseSeconds) noexcept;
    void setModulationParameters (float depth, float rateHz, ModulationDestination destination) noexcept;
    void setVelocitySensitivity (float sensitivity) noexcept;
    void setStartOrder (uint64_t newStartOrder) noexcept;
    void setVoiceGroupId (int newVoiceGroupId) noexcept;
    void setDetuneCents (float cents) noexcept;

    void noteOn (int midiNoteNumber, float velocity) noexcept;
    void noteOff (int midiNoteNumber) noexcept;
    void allNotesOff() noexcept;
    void handleMidiEvent (const juce::MidiMessage& midiMessage) noexcept;

    float renderSample() noexcept;
    RuntimeMetadata getRuntimeMetadata() const noexcept;

private:
    enum class NoteTransitionState
    {
        none = 0,
        rampDownForNoteChange
    };

    enum class EnvelopeStage
    {
        idle = 0,
        attack,
        decay,
        sustain,
        release
    };

    void updatePhaseIncrement() noexcept;
    double getDetunedFrequencyHz (double baseFrequencyHz) const noexcept;
    void updateLfoIncrement() noexcept;
    static float getOscillatorSample (double phaseInRadians, Waveform waveformType, float pulseWidth) noexcept;

    static constexpr float outputLevel = 0.12f;
    static constexpr double twoPi = juce::MathConstants<double>::twoPi;
    static constexpr double inverseTwoPi = 1.0 / twoPi;
    static constexpr float noteTransitionRampTimeSeconds = 0.002f;
    static constexpr float minimumEnvelopeValue = 1.0e-5f;

    int currentMidiNote = -1;
    int pendingMidiNote = -1;
    double currentFrequencyHz = 440.0;
    double pendingFrequencyHz = 440.0;
    double currentSampleRate = 44100.0;
    double phase = 0.0;
    double phaseIncrement = 0.0;
    bool gateOpen = false;
    bool shouldResetPhaseOnNoteChange = false;
    NoteTransitionState noteTransitionState = NoteTransitionState::none;
    Waveform waveform = Waveform::sine;
    float attackTimeSeconds = 0.005f;
    float decayTimeSeconds = 0.08f;
    float sustainLevel = 0.8f;
    float releaseTimeSeconds = 0.03f;
    float modulationDepth = 0.0f;
    float modulationRateHz = 0.0f;
    float velocitySensitivity = 0.0f;
    float currentNoteOnVelocity = 1.0f;
    float pendingNoteOnVelocity = 1.0f;
    ModulationDestination modulationDestination = ModulationDestination::amplitude;
    double lfoPhase = 0.0;
    double lfoPhaseIncrement = 0.0;
    float currentAmplitude = 0.0f;
    EnvelopeStage envelopeStage = EnvelopeStage::idle;
    float attackStep = 0.0f;
    float decayStep = 0.0f;
    float releaseStep = 0.0f;
    float noteTransitionStep = 0.0f;
    uint64_t startOrder = 0;
    int voiceGroupId = 0;
    float detuneCents = 0.0f;
};
