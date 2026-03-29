#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

//==============================================================================
class MonoSynthAudioProcessor final : public juce::AudioProcessor
{
public:
    enum class Waveform
    {
        sine = 0,
        saw,
        square,
        triangle
    };

    //==============================================================================
    MonoSynthAudioProcessor();
    ~MonoSynthAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }
    Waveform getWaveform() const noexcept;

private:
    enum class NoteTransitionState
    {
        none = 0,
        rampDownForNoteChange,
        rampUpAfterNoteChange
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void handleMidiEvent (const juce::MidiMessage& midiMessage);
    void updatePhaseIncrement() noexcept;
    void updateWaveformFromParameter() noexcept;
    static Waveform waveformFromParameterValue (float parameterValue) noexcept;
    static int waveformToChoiceIndex (Waveform waveformType) noexcept;
    static Waveform waveformFromChoiceIndex (int choiceIndex) noexcept;
    static const juce::StringArray& getWaveformChoices() noexcept;
    float getOscillatorSample (double phaseInRadians, Waveform oscillatorWaveform) const noexcept;

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* waveformParameter = nullptr;

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
    std::atomic<Waveform> waveform { Waveform::sine };
    float currentAmplitude = 0.0f;
    float attackStep = 0.0f;
    float releaseStep = 0.0f;
    float noteTransitionStep = 0.0f;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonoSynthAudioProcessor)
};
