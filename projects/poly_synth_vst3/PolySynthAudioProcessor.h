#pragma once

#include "LayeredInstrumentState.h"
#include "SynthEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <optional>

//==============================================================================
class PolySynthAudioProcessor final : public juce::AudioProcessor
{
public:
    using Waveform = SynthVoice::Waveform;

    //==============================================================================
    PolySynthAudioProcessor();
    ~PolySynthAudioProcessor() override;

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
    enum class StealPolicyParameterChoice
    {
        releasedFirst = 0,
        oldest,
        quietest
    };

    enum class ModDestinationParameterChoice
    {
        amplitude = 0,
        pitch,
        pulseWidth
    };

    enum class OutputStageParameterChoice
    {
        none = 0,
        normalizeVoiceSum,
        softLimit
    };

    struct EngineParameterSnapshot
    {
        Waveform waveform = Waveform::sine;
        int maxVoices = 1;
        SynthEngine::VoiceStealPolicy stealPolicy = SynthEngine::VoiceStealPolicy::releasedFirst;
        float attackSeconds = 0.005f;
        float decaySeconds = 0.08f;
        float sustainLevel = 0.8f;
        float releaseSeconds = 0.03f;
        float modulationDepth = 0.0f;
        float modulationRateHz = 0.0f;
        float velocitySensitivity = 0.0f;
        SynthVoice::ModulationDestination modulationDestination = SynthVoice::ModulationDestination::amplitude;
        int unisonVoices = 1;
        float unisonDetuneCents = 0.0f;
        SynthEngine::OutputStage outputStage = SynthEngine::OutputStage::normalizeVoiceSum;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateParameterSnapshotFromAPVTS() noexcept;
    void applyParameterSnapshotToEngine() noexcept;
    static Waveform waveformFromParameterValue (float parameterValue) noexcept;
    static int waveformToChoiceIndex (Waveform waveformType) noexcept;
    static Waveform waveformFromChoiceIndex (int choiceIndex) noexcept;
    static SynthEngine::VoiceStealPolicy stealPolicyFromParameterValue (float parameterValue) noexcept;
    static int stealPolicyToChoiceIndex (SynthEngine::VoiceStealPolicy policy) noexcept;
    static SynthEngine::VoiceStealPolicy stealPolicyFromChoiceIndex (int choiceIndex) noexcept;
    void migrateV1ToV2 (juce::ValueTree& migratedState, const juce::ValueTree& sourceState);
    void migrateV2ToV3 (juce::ValueTree& migratedState, const juce::ValueTree& sourceState);
    void migrateV3ToV4 (juce::ValueTree& migratedState, const juce::ValueTree& sourceState);
    void restoreLegacyState (const juce::ValueTree& legacyState);
    static std::optional<float> findLegacyParameterValue (const juce::ValueTree& tree, juce::StringRef parameterId);
    static std::optional<float> parseWaveformLegacyValue (const juce::var& value);
    static const juce::StringArray& getWaveformChoices() noexcept;
    static const juce::StringArray& getStealPolicyChoices() noexcept;
    static int modDestinationToChoiceIndex (SynthVoice::ModulationDestination destination) noexcept;
    static SynthVoice::ModulationDestination modDestinationFromChoiceIndex (int choiceIndex) noexcept;
    static const juce::StringArray& getModDestinationChoices() noexcept;
    static SynthEngine::OutputStage outputStageFromParameterValue (float parameterValue) noexcept;
    static int outputStageToChoiceIndex (SynthEngine::OutputStage outputStage) noexcept;
    static SynthEngine::OutputStage outputStageFromChoiceIndex (int choiceIndex) noexcept;
    static const juce::StringArray& getOutputStageChoices() noexcept;
    void syncLayerRuntimesFromState() noexcept;
    static void applyLayerStateToEngine (SynthEngine& engine, const LayerState& layerState) noexcept;

    struct LayerRuntime
    {
        uint64_t layerId = 0;
        LayerState snapshot;
        SynthEngine engine;
        juce::AudioBuffer<float> scratchBuffer;
        bool prepared = false;
    };

    static constexpr std::size_t maxRuntimeLayers = InstrumentState::maxLayerCount;

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* waveformParameter = nullptr;
    std::atomic<float>* maxVoicesParameter = nullptr;
    std::atomic<float>* stealPolicyParameter = nullptr;
    std::atomic<float>* attackParameter = nullptr;
    std::atomic<float>* decayParameter = nullptr;
    std::atomic<float>* sustainParameter = nullptr;
    std::atomic<float>* releaseParameter = nullptr;
    std::atomic<float>* modulationDepthParameter = nullptr;
    std::atomic<float>* modulationRateParameter = nullptr;
    std::atomic<float>* velocitySensitivityParameter = nullptr;
    std::atomic<float>* modulationDestinationParameter = nullptr;
    std::atomic<float>* unisonVoicesParameter = nullptr;
    std::atomic<float>* unisonDetuneCentsParameter = nullptr;
    std::atomic<float>* outputStageParameter = nullptr;
    std::atomic<Waveform> waveform { Waveform::sine };
    EngineParameterSnapshot parameterSnapshot;
    InstrumentState instrumentState;
    std::array<LayerRuntime, maxRuntimeLayers> layerRuntimes;
    std::array<uint64_t, maxRuntimeLayers> activeLayerOrder {};
    std::size_t activeLayerCount = 0;
    bool isPrepared = false;
    double preparedSampleRate = 44100.0;
    int preparedSamplesPerBlock = 0;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolySynthAudioProcessor)
};
