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

    void setWaveform (Waveform newWaveform) noexcept;
    Waveform getWaveform() const noexcept;

private:
    void handleMidiEvent (const juce::MidiMessage& midiMessage);
    void updatePhaseIncrement() noexcept;
    float getOscillatorSample (double phaseInRadians, Waveform waveform) const noexcept;

    int currentMidiNote = -1;
    double currentFrequencyHz = 440.0;
    double currentSampleRate = 44100.0;
    double phase = 0.0;
    double phaseIncrement = 0.0;
    bool gateOpen = false;
    std::atomic<Waveform> waveform { Waveform::sine };
    float currentAmplitude = 0.0f;
    float attackStep = 0.0f;
    float releaseStep = 0.0f;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonoSynthAudioProcessor)
};
