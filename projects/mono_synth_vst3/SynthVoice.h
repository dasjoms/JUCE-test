#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

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

    void prepare (double sampleRate) noexcept;
    void setWaveform (Waveform newWaveform) noexcept;

    void noteOn (int midiNoteNumber) noexcept;
    void noteOff (int midiNoteNumber) noexcept;
    void allNotesOff() noexcept;
    void handleMidiEvent (const juce::MidiMessage& midiMessage) noexcept;

    float renderSample() noexcept;

private:
    enum class NoteTransitionState
    {
        none = 0,
        rampDownForNoteChange,
        rampUpAfterNoteChange
    };

    void updatePhaseIncrement() noexcept;
    static float getOscillatorSample (double phaseInRadians, Waveform waveformType) noexcept;

    static constexpr float outputLevel = 0.12f;
    static constexpr double twoPi = juce::MathConstants<double>::twoPi;
    static constexpr double inverseTwoPi = 1.0 / twoPi;
    static constexpr float attackTimeSeconds = 0.005f;
    static constexpr float releaseTimeSeconds = 0.03f;
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
    float currentAmplitude = 0.0f;
    float attackStep = 0.0f;
    float releaseStep = 0.0f;
    float noteTransitionStep = 0.0f;
};
