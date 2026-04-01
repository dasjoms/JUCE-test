#include "PolySynthAudioProcessor.h"
#include "PolySynthAudioProcessorEditor.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
constexpr auto waveformParameterId = "waveform";
constexpr auto maxVoicesParameterId = "maxVoices";
constexpr auto stealPolicyParameterId = "stealPolicy";
constexpr auto attackParameterId = "attack";
constexpr auto decayParameterId = "decay";
constexpr auto sustainParameterId = "sustain";
constexpr auto releaseParameterId = "release";
constexpr auto modulationDepthParameterId = "modDepth";
constexpr auto modulationRateParameterId = "modRate";
constexpr auto velocitySensitivityParameterId = "velocitySensitivity";
constexpr auto modulationDestinationParameterId = "modDestination";
constexpr auto schemaVersionPropertyId = "schemaVersion";
constexpr auto unisonVoicesParameterId = "unisonVoices";
constexpr auto unisonDetuneCentsParameterId = "unisonDetuneCents";
constexpr auto outputStageParameterId = "outputStage";
constexpr auto layersNodeId = "LAYERS";
constexpr auto layerNodeId = "LAYER";
constexpr auto layerOrderNodeId = "LAYER_ORDER";
constexpr auto orderItemNodeId = "ITEM";
constexpr auto layerStateNodeId = "layerState";
constexpr auto selectedLayerIdPropertyId = "selectedLayerId";
constexpr auto orderLayerIdPropertyId = "layerId";
constexpr auto nextLayerIdPropertyId = "nextLayerId";
constexpr auto layerIdPropertyId = "layerId";
constexpr auto rootNoteAbsolutePropertyId = "rootNoteAbsolute";
constexpr auto mutePropertyId = "mute";
constexpr auto soloPropertyId = "solo";
constexpr auto layerVolumePropertyId = "layerVolume";
constexpr int currentStateSchemaVersion = 6;

constexpr std::array<const char*, 11> schemaV2ParameterIds {
    waveformParameterId,
    maxVoicesParameterId,
    stealPolicyParameterId,
    attackParameterId,
    decayParameterId,
    sustainParameterId,
    releaseParameterId,
    modulationDepthParameterId,
    modulationRateParameterId,
    velocitySensitivityParameterId,
    modulationDestinationParameterId
};

constexpr std::array<const char*, 2> schemaV3ParameterIds {
    unisonVoicesParameterId,
    unisonDetuneCentsParameterId
};

constexpr std::array<const char*, 1> schemaV4ParameterIds {
    outputStageParameterId
};

constexpr std::array<const char*, 1> schemaV5ParameterIds {
    modulationDestinationParameterId
};

constexpr std::array<const char*, schemaV2ParameterIds.size() + schemaV3ParameterIds.size() + schemaV4ParameterIds.size()> allKnownParameterIds {
    waveformParameterId,
    maxVoicesParameterId,
    stealPolicyParameterId,
    attackParameterId,
    decayParameterId,
    sustainParameterId,
    releaseParameterId,
    modulationDepthParameterId,
    modulationRateParameterId,
    velocitySensitivityParameterId,
    modulationDestinationParameterId,
    unisonVoicesParameterId,
    unisonDetuneCentsParameterId,
    outputStageParameterId
};
} // namespace

//==============================================================================
PolySynthAudioProcessor::PolySynthAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                      #endif
                       )
     , parameters (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    waveformParameter = parameters.getRawParameterValue (waveformParameterId);
    maxVoicesParameter = parameters.getRawParameterValue (maxVoicesParameterId);
    stealPolicyParameter = parameters.getRawParameterValue (stealPolicyParameterId);
    attackParameter = parameters.getRawParameterValue (attackParameterId);
    decayParameter = parameters.getRawParameterValue (decayParameterId);
    sustainParameter = parameters.getRawParameterValue (sustainParameterId);
    releaseParameter = parameters.getRawParameterValue (releaseParameterId);
    modulationDepthParameter = parameters.getRawParameterValue (modulationDepthParameterId);
    modulationRateParameter = parameters.getRawParameterValue (modulationRateParameterId);
    velocitySensitivityParameter = parameters.getRawParameterValue (velocitySensitivityParameterId);
    modulationDestinationParameter = parameters.getRawParameterValue (modulationDestinationParameterId);
    unisonVoicesParameter = parameters.getRawParameterValue (unisonVoicesParameterId);
    unisonDetuneCentsParameter = parameters.getRawParameterValue (unisonDetuneCentsParameterId);
    outputStageParameter = parameters.getRawParameterValue (outputStageParameterId);
    if (const auto& layerOrder = instrumentState.getLayerOrder(); ! layerOrder.empty())
        selectedLayerId = layerOrder.front();
    updateParameterSnapshotFromAPVTS();
    syncLayerRuntimesFromState();
}

PolySynthAudioProcessor::~PolySynthAudioProcessor()
{
}

int PolySynthAudioProcessor::getLayerCount() const noexcept
{
    return static_cast<int> (instrumentState.getLayerOrder().size());
}

int PolySynthAudioProcessor::clampMidiNote (int midiNote) noexcept
{
    return juce::jlimit (minimumMidiNote, maximumMidiNote, midiNote);
}

int PolySynthAudioProcessor::getLayerRootNoteAbsolute (std::size_t layerVisualIndex) const noexcept
{
    const auto& layerOrder = instrumentState.getLayerOrder();
    if (layerVisualIndex >= layerOrder.size())
        return defaultBaseLayerRootNote;

    if (const auto* layer = instrumentState.findLayerById (layerOrder[layerVisualIndex]))
        return clampMidiNote (layer->rootNoteAbsolute);

    return defaultBaseLayerRootNote;
}

int PolySynthAudioProcessor::getLayerRootNoteRelativeSemitones (std::size_t layerVisualIndex) const noexcept
{
    const auto baseRootNote = getLayerRootNoteAbsolute (0);
    const auto layerRootNote = getLayerRootNoteAbsolute (layerVisualIndex);
    return layerRootNote - baseRootNote;
}

int PolySynthAudioProcessor::setLayerRootNoteAbsolute (std::size_t layerVisualIndex, int absoluteMidiNote) noexcept
{
    const auto clampedMidiNote = clampMidiNote (absoluteMidiNote);
    const auto& layerOrder = instrumentState.getLayerOrder();

    if (layerVisualIndex >= layerOrder.size())
        return clampedMidiNote;

    if (auto* layer = instrumentState.findLayerById (layerOrder[layerVisualIndex]))
        layer->rootNoteAbsolute = clampedMidiNote;

    return clampedMidiNote;
}

int PolySynthAudioProcessor::setLayerRootNoteRelativeSemitones (std::size_t layerVisualIndex, int relativeSemitones) noexcept
{
    const auto baseRootNote = getLayerRootNoteAbsolute (0);
    const auto absoluteMidiNote = baseRootNote + relativeSemitones;
    return setLayerRootNoteAbsolute (layerVisualIndex, absoluteMidiNote);
}

bool PolySynthAudioProcessor::getLayerMute (std::size_t layerVisualIndex) const noexcept
{
    const auto& layerOrder = instrumentState.getLayerOrder();
    if (layerVisualIndex >= layerOrder.size())
        return false;

    if (const auto* layer = instrumentState.findLayerById (layerOrder[layerVisualIndex]))
        return layer->mute;

    return false;
}

bool PolySynthAudioProcessor::getLayerSolo (std::size_t layerVisualIndex) const noexcept
{
    const auto& layerOrder = instrumentState.getLayerOrder();
    if (layerVisualIndex >= layerOrder.size())
        return false;

    if (const auto* layer = instrumentState.findLayerById (layerOrder[layerVisualIndex]))
        return layer->solo;

    return false;
}

float PolySynthAudioProcessor::getLayerVolume (std::size_t layerVisualIndex) const noexcept
{
    const auto& layerOrder = instrumentState.getLayerOrder();
    if (layerVisualIndex >= layerOrder.size())
        return 1.0f;

    if (const auto* layer = instrumentState.findLayerById (layerOrder[layerVisualIndex]))
        return juce::jmax (0.0f, layer->layerVolume);

    return 1.0f;
}

bool PolySynthAudioProcessor::setLayerMute (std::size_t layerVisualIndex, bool shouldMute) noexcept
{
    const auto& layerOrder = instrumentState.getLayerOrder();
    if (layerVisualIndex >= layerOrder.size())
        return false;

    if (auto* layer = instrumentState.findLayerById (layerOrder[layerVisualIndex]))
    {
        layer->mute = shouldMute;
        return true;
    }

    return false;
}

bool PolySynthAudioProcessor::setLayerSolo (std::size_t layerVisualIndex, bool shouldSolo) noexcept
{
    const auto& layerOrder = instrumentState.getLayerOrder();
    if (layerVisualIndex >= layerOrder.size())
        return false;

    if (auto* layer = instrumentState.findLayerById (layerOrder[layerVisualIndex]))
    {
        layer->solo = shouldSolo;
        return true;
    }

    return false;
}

float PolySynthAudioProcessor::setLayerVolume (std::size_t layerVisualIndex, float volume) noexcept
{
    const auto clampedVolume = juce::jmax (0.0f, volume);
    const auto& layerOrder = instrumentState.getLayerOrder();
    if (layerVisualIndex >= layerOrder.size())
        return clampedVolume;

    if (auto* layer = instrumentState.findLayerById (layerOrder[layerVisualIndex]))
        layer->layerVolume = clampedVolume;

    return clampedVolume;
}

bool PolySynthAudioProcessor::addDefaultLayerAndSelect() noexcept
{
    const auto createdLayerId = instrumentState.createDefaultLayer();
    if (! createdLayerId.has_value())
        return false;

    selectedLayerId = *createdLayerId;
    syncLayerRuntimesFromState();
    return true;
}

bool PolySynthAudioProcessor::duplicateLayerAndSelect (std::size_t sourceLayerVisualIndex) noexcept
{
    const auto sourceLayerId = getLayerIdForVisualIndex (sourceLayerVisualIndex);
    if (! sourceLayerId.has_value())
        return false;

    const auto duplicateLayerId = instrumentState.duplicateLayer (*sourceLayerId);
    if (! duplicateLayerId.has_value())
        return false;

    selectedLayerId = *duplicateLayerId;
    syncLayerRuntimesFromState();
    return true;
}

bool PolySynthAudioProcessor::removeLayerWithSelectionFallback (std::size_t layerVisualIndex) noexcept
{
    const auto layerId = getLayerIdForVisualIndex (layerVisualIndex);
    if (! layerId.has_value())
        return false;

    if (! instrumentState.removeLayer (*layerId))
        return false;

    const auto fallbackIndex = InstrumentState::resolveSelectedLayerIndexAfterDelete (layerVisualIndex,
                                                                                      instrumentState.getLayerOrder().size());
    selectLayerByVisualIndex (fallbackIndex);
    syncLayerRuntimesFromState();
    return true;
}

bool PolySynthAudioProcessor::moveLayerUp (std::size_t layerVisualIndex) noexcept
{
    const auto selectedIndexBeforeMove = getVisualIndexForLayerId (selectedLayerId);
    if (! instrumentState.moveLayerUp (layerVisualIndex))
        return false;

    if (selectedIndexBeforeMove == layerVisualIndex)
        selectedLayerId = instrumentState.getLayerOrder()[layerVisualIndex - 1];
    else if (selectedIndexBeforeMove + 1 == layerVisualIndex)
        selectedLayerId = instrumentState.getLayerOrder()[layerVisualIndex];

    syncLayerRuntimesFromState();
    return true;
}

bool PolySynthAudioProcessor::moveLayerDown (std::size_t layerVisualIndex) noexcept
{
    const auto selectedIndexBeforeMove = getVisualIndexForLayerId (selectedLayerId);
    if (! instrumentState.moveLayerDown (layerVisualIndex))
        return false;

    if (selectedIndexBeforeMove == layerVisualIndex)
        selectedLayerId = instrumentState.getLayerOrder()[layerVisualIndex + 1];
    else if (selectedIndexBeforeMove == layerVisualIndex + 1)
        selectedLayerId = instrumentState.getLayerOrder()[layerVisualIndex];

    syncLayerRuntimesFromState();
    return true;
}

bool PolySynthAudioProcessor::selectLayerByVisualIndex (std::size_t layerVisualIndex) noexcept
{
    const auto layerId = getLayerIdForVisualIndex (layerVisualIndex);
    if (! layerId.has_value())
        return false;

    selectedLayerId = *layerId;
    return true;
}

std::size_t PolySynthAudioProcessor::getSelectedLayerVisualIndex() const noexcept
{
    return getVisualIndexForLayerId (selectedLayerId);
}

//==============================================================================
const juce::String PolySynthAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PolySynthAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PolySynthAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PolySynthAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PolySynthAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PolySynthAudioProcessor::getNumPrograms()
{
    return 1;
}

int PolySynthAudioProcessor::getCurrentProgram()
{
    return 0;
}

void PolySynthAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PolySynthAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PolySynthAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PolySynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    preparedSampleRate = sampleRate;
    preparedSamplesPerBlock = samplesPerBlock;
    isPrepared = true;

    syncLayerRuntimesFromState();

    for (std::size_t runtimeIndex = 0; runtimeIndex < activeLayerCount; ++runtimeIndex)
    {
        auto& runtime = layerRuntimes[runtimeIndex];
        runtime.scratchBuffer.setSize (getTotalNumOutputChannels(), samplesPerBlock, false, false, true);
        runtime.scratchBuffer.clear();
        runtime.engine.prepare (sampleRate, samplesPerBlock);
        runtime.prepared = true;
    }
}

void PolySynthAudioProcessor::releaseResources()
{
}

bool PolySynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void PolySynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    updateParameterSnapshotFromAPVTS();
    syncLayerRuntimesFromState();

    const auto numChannels = buffer.getNumChannels();
    bool anySoloed = false;
    for (std::size_t runtimeIndex = 0; runtimeIndex < activeLayerCount; ++runtimeIndex)
    {
        if (layerRuntimes[runtimeIndex].snapshot.solo)
        {
            anySoloed = true;
            break;
        }
    }

    for (std::size_t runtimeIndex = 0; runtimeIndex < activeLayerCount; ++runtimeIndex)
    {
        auto& runtime = layerRuntimes[runtimeIndex];

        if (! runtime.prepared)
        {
            runtime.engine.prepare (preparedSampleRate, preparedSamplesPerBlock);
            runtime.prepared = true;
        }

        if (runtime.scratchBuffer.getNumSamples() < numSamples
            || runtime.scratchBuffer.getNumChannels() < numChannels)
        {
            runtime.scratchBuffer.setSize (numChannels, numSamples, false, false, true);
        }

        runtime.scratchBuffer.clear();
        juce::AudioBuffer<float> layerBlockView (runtime.scratchBuffer.getArrayOfWritePointers(),
                                                 numChannels,
                                                 numSamples);
        runtime.engine.renderBlock (layerBlockView, midiMessages);

        const auto isAudible = anySoloed ? runtime.snapshot.solo : ! runtime.snapshot.mute;
        if (! isAudible)
            continue;

        const auto layerGain = juce::jmax (0.0f, runtime.snapshot.layerVolume);
        if (layerGain <= 0.0f)
            continue;

        for (int channel = 0; channel < numChannels; ++channel)
            buffer.addFrom (channel, 0, runtime.scratchBuffer, channel, 0, numSamples, layerGain);
    }

    midiMessages.clear();
}

PolySynthAudioProcessor::Waveform PolySynthAudioProcessor::getWaveform() const noexcept
{
    return waveform.load (std::memory_order_relaxed);
}

juce::AudioProcessorValueTreeState::ParameterLayout PolySynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;
    layout.push_back (std::make_unique<juce::AudioParameterChoice> (waveformParameterId,
                                                                     "Waveform",
                                                                     getWaveformChoices(),
                                                                     waveformToChoiceIndex (Waveform::sine)));
    layout.push_back (std::make_unique<juce::AudioParameterInt> (maxVoicesParameterId,
                                                                  "Max Voices",
                                                                  1,
                                                                  16,
                                                                  1));
    layout.push_back (std::make_unique<juce::AudioParameterChoice> (stealPolicyParameterId,
                                                                     "Steal Policy",
                                                                     getStealPolicyChoices(),
                                                                     stealPolicyToChoiceIndex (SynthEngine::VoiceStealPolicy::releasedFirst)));
    layout.push_back (std::make_unique<juce::AudioParameterFloat> (attackParameterId,
                                                                    "Attack",
                                                                    juce::NormalisableRange<float> (0.001f, 5.0f, 0.001f, 0.35f),
                                                                    0.005f,
                                                                    juce::AudioParameterFloatAttributes().withLabel ("s")));
    layout.push_back (std::make_unique<juce::AudioParameterFloat> (decayParameterId,
                                                                    "Decay",
                                                                    juce::NormalisableRange<float> (0.001f, 5.0f, 0.001f, 0.35f),
                                                                    0.08f,
                                                                    juce::AudioParameterFloatAttributes().withLabel ("s")));
    layout.push_back (std::make_unique<juce::AudioParameterFloat> (sustainParameterId,
                                                                    "Sustain",
                                                                    juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
                                                                    0.8f));
    layout.push_back (std::make_unique<juce::AudioParameterFloat> (releaseParameterId,
                                                                    "Release",
                                                                    juce::NormalisableRange<float> (0.005f, 5.0f, 0.001f, 0.35f),
                                                                    0.03f,
                                                                    juce::AudioParameterFloatAttributes().withLabel ("s")));
    layout.push_back (std::make_unique<juce::AudioParameterFloat> (modulationDepthParameterId,
                                                                    "Mod Depth",
                                                                    juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
                                                                    0.0f));
    layout.push_back (std::make_unique<juce::AudioParameterFloat> (modulationRateParameterId,
                                                                    "Mod Rate",
                                                                    juce::NormalisableRange<float> (0.05f, 20.0f, 0.01f, 0.3f),
                                                                    2.0f,
                                                                    juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.push_back (std::make_unique<juce::AudioParameterFloat> (velocitySensitivityParameterId,
                                                                    "Velocity Sensitivity",
                                                                    juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
                                                                    0.0f));
    layout.push_back (std::make_unique<juce::AudioParameterChoice> (modulationDestinationParameterId,
                                                                     "Mod Destination",
                                                                     getModDestinationChoices(),
                                                                     modDestinationToChoiceIndex (SynthVoice::ModulationDestination::off)));
    layout.push_back (std::make_unique<juce::AudioParameterInt> (unisonVoicesParameterId,
                                                                  "Unison Voices",
                                                                  1,
                                                                  8,
                                                                  1));
    layout.push_back (std::make_unique<juce::AudioParameterFloat> (unisonDetuneCentsParameterId,
                                                                    "Unison Detune",
                                                                    juce::NormalisableRange<float> (0.0f, 50.0f, 0.01f),
                                                                    0.0f,
                                                                    juce::AudioParameterFloatAttributes().withLabel ("cents")));
    layout.push_back (std::make_unique<juce::AudioParameterChoice> (outputStageParameterId,
                                                                     "Output Stage",
                                                                     getOutputStageChoices(),
                                                                     outputStageToChoiceIndex (SynthEngine::OutputStage::normalizeVoiceSum)));
    return { layout.begin(), layout.end() };
}

void PolySynthAudioProcessor::updateParameterSnapshotFromAPVTS() noexcept
{
    if (waveformParameter != nullptr)
    {
        const auto waveformValue = waveformFromParameterValue (waveformParameter->load (std::memory_order_relaxed));
        waveform.store (waveformValue, std::memory_order_relaxed);
        parameterSnapshot.waveform = waveformValue;
    }

    if (maxVoicesParameter != nullptr)
        parameterSnapshot.maxVoices = juce::roundToInt (maxVoicesParameter->load (std::memory_order_relaxed));

    if (stealPolicyParameter != nullptr)
        parameterSnapshot.stealPolicy = stealPolicyFromParameterValue (stealPolicyParameter->load (std::memory_order_relaxed));

    if (attackParameter != nullptr)
        parameterSnapshot.attackSeconds = attackParameter->load (std::memory_order_relaxed);

    if (decayParameter != nullptr)
        parameterSnapshot.decaySeconds = decayParameter->load (std::memory_order_relaxed);

    if (sustainParameter != nullptr)
        parameterSnapshot.sustainLevel = sustainParameter->load (std::memory_order_relaxed);

    if (releaseParameter != nullptr)
        parameterSnapshot.releaseSeconds = releaseParameter->load (std::memory_order_relaxed);

    if (modulationDepthParameter != nullptr)
        parameterSnapshot.modulationDepth = modulationDepthParameter->load (std::memory_order_relaxed);

    if (modulationRateParameter != nullptr)
        parameterSnapshot.modulationRateHz = modulationRateParameter->load (std::memory_order_relaxed);

    if (velocitySensitivityParameter != nullptr)
        parameterSnapshot.velocitySensitivity = velocitySensitivityParameter->load (std::memory_order_relaxed);

    if (modulationDestinationParameter != nullptr)
        parameterSnapshot.modulationDestination = modDestinationFromChoiceIndex (
            juce::roundToInt (modulationDestinationParameter->load (std::memory_order_relaxed)));

    if (unisonVoicesParameter != nullptr)
        parameterSnapshot.unisonVoices = juce::roundToInt (unisonVoicesParameter->load (std::memory_order_relaxed));

    if (unisonDetuneCentsParameter != nullptr)
        parameterSnapshot.unisonDetuneCents = unisonDetuneCentsParameter->load (std::memory_order_relaxed);

    if (outputStageParameter != nullptr)
        parameterSnapshot.outputStage = outputStageFromParameterValue (outputStageParameter->load (std::memory_order_relaxed));
}

void PolySynthAudioProcessor::applyParameterSnapshotToEngine() noexcept
{
    if (auto* baseLayer = instrumentState.findLayerById (instrumentState.getLayerOrder().front()))
    {
        baseLayer->waveform = parameterSnapshot.waveform;
        baseLayer->voiceCount = parameterSnapshot.maxVoices;
        baseLayer->stealPolicy = parameterSnapshot.stealPolicy;
        baseLayer->attackSeconds = parameterSnapshot.attackSeconds;
        baseLayer->decaySeconds = parameterSnapshot.decaySeconds;
        baseLayer->sustainLevel = parameterSnapshot.sustainLevel;
        baseLayer->releaseSeconds = parameterSnapshot.releaseSeconds;
        baseLayer->modulationDepth = parameterSnapshot.modulationDepth;
        baseLayer->modulationRateHz = parameterSnapshot.modulationRateHz;
        baseLayer->modulationDestination = parameterSnapshot.modulationDestination;
        baseLayer->velocitySensitivity = parameterSnapshot.velocitySensitivity;
        baseLayer->unisonVoices = parameterSnapshot.unisonVoices;
        baseLayer->unisonDetuneCents = parameterSnapshot.unisonDetuneCents;
        baseLayer->outputStage = parameterSnapshot.outputStage;
    }
}

void PolySynthAudioProcessor::syncLayerRuntimesFromState() noexcept
{
    ensureSelectedLayerIsValid();
    applyParameterSnapshotToEngine();

    const auto& order = instrumentState.getLayerOrder();
    const auto cappedLayerCount = juce::jmin (order.size(), maxRuntimeLayers);
    activeLayerCount = cappedLayerCount;

    if (order.size() > maxRuntimeLayers)
    {
        // Fallback safety: ignore additional layers beyond the hard runtime cap.
        // Layer management should reject these add attempts before audio rendering reaches this point.
        jassertfalse;
    }

    for (std::size_t runtimeIndex = 0; runtimeIndex < activeLayerCount; ++runtimeIndex)
    {
        const auto layerId = order[runtimeIndex];
        activeLayerOrder[runtimeIndex] = layerId;

        auto& runtime = layerRuntimes[runtimeIndex];
        runtime.layerId = layerId;

        if (const auto* layer = instrumentState.findLayerById (layerId))
        {
            runtime.snapshot = *layer;
            applyLayerStateToEngine (runtime.engine, runtime.snapshot);
        }
    }
}

void PolySynthAudioProcessor::applyLayerStateToEngine (SynthEngine& engine, const LayerState& layerState) noexcept
{
    engine.setActiveVoiceCount (layerState.voiceCount);
    engine.setVoiceStealPolicy (layerState.stealPolicy);
    engine.setWaveform (layerState.waveform);
    engine.setEnvelopeTimes (layerState.attackSeconds,
                             layerState.decaySeconds,
                             layerState.sustainLevel,
                             layerState.releaseSeconds);
    engine.setModulationParameters (layerState.modulationDepth, layerState.modulationRateHz, layerState.modulationDestination);
    engine.setVelocitySensitivity (layerState.velocitySensitivity);
    engine.setUnisonVoices (layerState.unisonVoices);
    engine.setUnisonDetuneCents (layerState.unisonDetuneCents);
    engine.setOutputStage (layerState.outputStage);
}

PolySynthAudioProcessor::Waveform PolySynthAudioProcessor::waveformFromParameterValue (float parameterValue) noexcept
{
    return waveformFromChoiceIndex (juce::roundToInt (parameterValue));
}

int PolySynthAudioProcessor::waveformToChoiceIndex (Waveform waveformType) noexcept
{
    return static_cast<int> (waveformType);
}

PolySynthAudioProcessor::Waveform PolySynthAudioProcessor::waveformFromChoiceIndex (int choiceIndex) noexcept
{
    switch (choiceIndex)
    {
        case 0: return Waveform::sine;
        case 1: return Waveform::saw;
        case 2: return Waveform::square;
        case 3: return Waveform::triangle;
        default: break;
    }

    return Waveform::sine;
}

SynthEngine::VoiceStealPolicy PolySynthAudioProcessor::stealPolicyFromParameterValue (float parameterValue) noexcept
{
    return stealPolicyFromChoiceIndex (juce::roundToInt (parameterValue));
}

int PolySynthAudioProcessor::stealPolicyToChoiceIndex (SynthEngine::VoiceStealPolicy policy) noexcept
{
    switch (policy)
    {
        case SynthEngine::VoiceStealPolicy::releasedFirst: return 0;
        case SynthEngine::VoiceStealPolicy::oldest: return 1;
        case SynthEngine::VoiceStealPolicy::quietest: return 2;
    }

    return 0;
}

SynthEngine::VoiceStealPolicy PolySynthAudioProcessor::stealPolicyFromChoiceIndex (int choiceIndex) noexcept
{
    switch (choiceIndex)
    {
        case static_cast<int> (StealPolicyParameterChoice::releasedFirst): return SynthEngine::VoiceStealPolicy::releasedFirst;
        case static_cast<int> (StealPolicyParameterChoice::oldest): return SynthEngine::VoiceStealPolicy::oldest;
        case static_cast<int> (StealPolicyParameterChoice::quietest): return SynthEngine::VoiceStealPolicy::quietest;
        default: break;
    }

    return SynthEngine::VoiceStealPolicy::releasedFirst;
}

const juce::StringArray& PolySynthAudioProcessor::getWaveformChoices() noexcept
{
    static const juce::StringArray choices { "Sine", "Saw", "Square", "Triangle" };
    return choices;
}

const juce::StringArray& PolySynthAudioProcessor::getStealPolicyChoices() noexcept
{
    static const juce::StringArray choices { "Released First", "Oldest", "Quietest" };
    return choices;
}
int PolySynthAudioProcessor::modDestinationToChoiceIndex (SynthVoice::ModulationDestination destination) noexcept
{
    switch (destination)
    {
        case SynthVoice::ModulationDestination::off: return static_cast<int> (ModDestinationParameterChoice::off);
        case SynthVoice::ModulationDestination::amplitude: return static_cast<int> (ModDestinationParameterChoice::amplitude);
        case SynthVoice::ModulationDestination::pitch: return static_cast<int> (ModDestinationParameterChoice::pitch);
        case SynthVoice::ModulationDestination::pulseWidth: return static_cast<int> (ModDestinationParameterChoice::pulseWidth);
    }

    return static_cast<int> (ModDestinationParameterChoice::off);
}

SynthVoice::ModulationDestination PolySynthAudioProcessor::modDestinationFromChoiceIndex (int choiceIndex) noexcept
{
    switch (choiceIndex)
    {
        case static_cast<int> (ModDestinationParameterChoice::off): return SynthVoice::ModulationDestination::off;
        case static_cast<int> (ModDestinationParameterChoice::amplitude): return SynthVoice::ModulationDestination::amplitude;
        case static_cast<int> (ModDestinationParameterChoice::pitch): return SynthVoice::ModulationDestination::pitch;
        case static_cast<int> (ModDestinationParameterChoice::pulseWidth): return SynthVoice::ModulationDestination::pulseWidth;
        default: break;
    }

    return SynthVoice::ModulationDestination::off;
}

const juce::StringArray& PolySynthAudioProcessor::getModDestinationChoices() noexcept
{
    static const juce::StringArray choices { "Off", "Amplitude", "Pitch", "Pulse Width" };
    return choices;
}

SynthEngine::OutputStage PolySynthAudioProcessor::outputStageFromParameterValue (float parameterValue) noexcept
{
    return outputStageFromChoiceIndex (juce::roundToInt (parameterValue));
}

int PolySynthAudioProcessor::outputStageToChoiceIndex (SynthEngine::OutputStage outputStage) noexcept
{
    switch (outputStage)
    {
        case SynthEngine::OutputStage::none: return 0;
        case SynthEngine::OutputStage::normalizeVoiceSum: return 1;
        case SynthEngine::OutputStage::softLimit: return 2;
    }

    return 1;
}

SynthEngine::OutputStage PolySynthAudioProcessor::outputStageFromChoiceIndex (int choiceIndex) noexcept
{
    switch (choiceIndex)
    {
        case static_cast<int> (OutputStageParameterChoice::none): return SynthEngine::OutputStage::none;
        case static_cast<int> (OutputStageParameterChoice::normalizeVoiceSum): return SynthEngine::OutputStage::normalizeVoiceSum;
        case static_cast<int> (OutputStageParameterChoice::softLimit): return SynthEngine::OutputStage::softLimit;
        default: break;
    }

    return SynthEngine::OutputStage::normalizeVoiceSum;
}

const juce::StringArray& PolySynthAudioProcessor::getOutputStageChoices() noexcept
{
    static const juce::StringArray choices { "None", "Normalize Voice Sum", "Soft Limit" };
    return choices;
}


//==============================================================================
bool PolySynthAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* PolySynthAudioProcessor::createEditor()
{
    return new PolySynthAudioProcessorEditor (*this);
}

//==============================================================================
void PolySynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto stateToSave = parameters.copyState();
    stateToSave.setProperty (schemaVersionPropertyId, currentStateSchemaVersion, nullptr);
    writeLayeredStateToTree (stateToSave);

    if (const auto xml = stateToSave.createXml())
        copyXmlToBinary (*xml, destData);
}

void PolySynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes); xml != nullptr)
    {
        const auto stateTree = juce::ValueTree::fromXml (*xml);

        if (stateTree.isValid())
        {
            const auto schemaVersion = static_cast<int> (stateTree.getProperty (schemaVersionPropertyId, 0));

            if (schemaVersion >= currentStateSchemaVersion
                && stateTree.hasType (parameters.state.getType()))
            {
                parameters.replaceState (stateTree);
                loadLayeredStateFromTree (stateTree);
            }
            else
            {
                restoreLegacyState (stateTree);
            }
        }
    }

    updateParameterSnapshotFromAPVTS();
    applyParameterSnapshotToEngine();
}

void PolySynthAudioProcessor::restoreLegacyState (const juce::ValueTree& legacyState)
{
    for (const auto* parameterId : allKnownParameterIds)
        if (auto* parameter = parameters.getParameter (parameterId))
            parameter->setValueNotifyingHost (parameter->getDefaultValue());

    auto migratedState = parameters.copyState();
    migrateV1ToV2 (migratedState, legacyState);
    migrateV2ToV3 (migratedState, legacyState);
    migrateV3ToV4 (migratedState, legacyState);
    migrateV4ToV5 (migratedState, legacyState);
    migrateV5ToV6 (migratedState, legacyState);

    migratedState.setProperty (schemaVersionPropertyId, currentStateSchemaVersion, nullptr);
    parameters.replaceState (migratedState);
    loadLayeredStateFromTree (migratedState);
}

void PolySynthAudioProcessor::migrateV1ToV2 (juce::ValueTree& migratedState, const juce::ValueTree& sourceState)
{
    for (const auto* parameterId : schemaV2ParameterIds)
    {
        if (const auto value = findLegacyParameterValue (sourceState, parameterId); value.has_value())
        {
            if (auto* parameter = parameters.getParameter (parameterId))
            {
                const auto clampedValue = parameter->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, parameter->convertTo0to1 (*value)));

                for (int i = 0; i < migratedState.getNumChildren(); ++i)
                {
                    auto node = migratedState.getChild (i);
                    if (node.hasType ("PARAM") && node.getProperty ("id").toString() == juce::String (parameterId))
                    {
                        node.setProperty ("value", clampedValue, nullptr);
                        break;
                    }
                }
            }
        }
    }
}

void PolySynthAudioProcessor::migrateV2ToV3 (juce::ValueTree& migratedState, const juce::ValueTree& sourceState)
{
    for (const auto* parameterId : schemaV3ParameterIds)
    {
        if (const auto value = findLegacyParameterValue (sourceState, parameterId); value.has_value())
        {
            if (auto* parameter = parameters.getParameter (parameterId))
            {
                const auto clampedValue = parameter->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, parameter->convertTo0to1 (*value)));

                for (int i = 0; i < migratedState.getNumChildren(); ++i)
                {
                    auto node = migratedState.getChild (i);
                    if (node.hasType ("PARAM") && node.getProperty ("id").toString() == juce::String (parameterId))
                    {
                        node.setProperty ("value", clampedValue, nullptr);
                        break;
                    }
                }
            }
        }
    }
}

void PolySynthAudioProcessor::migrateV3ToV4 (juce::ValueTree& migratedState, const juce::ValueTree& sourceState)
{
    for (const auto* parameterId : schemaV4ParameterIds)
    {
        if (const auto value = findLegacyParameterValue (sourceState, parameterId); value.has_value())
        {
            if (auto* parameter = parameters.getParameter (parameterId))
            {
                const auto clampedValue = parameter->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, parameter->convertTo0to1 (*value)));

                for (int i = 0; i < migratedState.getNumChildren(); ++i)
                {
                    auto node = migratedState.getChild (i);
                    if (node.hasType ("PARAM") && node.getProperty ("id").toString() == juce::String (parameterId))
                    {
                        node.setProperty ("value", clampedValue, nullptr);
                        break;
                    }
                }
            }
        }
    }
}

void PolySynthAudioProcessor::migrateV4ToV5 (juce::ValueTree& migratedState, const juce::ValueTree& sourceState)
{
    juce::ignoreUnused (sourceState);
    writeLayeredStateToTree (migratedState);
}

void PolySynthAudioProcessor::migrateV5ToV6 (juce::ValueTree& migratedState, const juce::ValueTree& sourceState)
{
    const auto sourceSchemaVersion = static_cast<int> (sourceState.getProperty (schemaVersionPropertyId, 0));

    for (const auto* parameterId : schemaV5ParameterIds)
    {
        if (juce::StringRef (parameterId) != juce::StringRef (modulationDestinationParameterId))
            continue;

        const auto legacyValue = findLegacyParameterValue (sourceState, parameterId);
        if (! legacyValue.has_value())
            continue;

        // Schema v5 and earlier encoded: 0=Amplitude, 1=Pitch, 2=Pulse Width.
        // Schema v6 inserts Off at index 0, so explicit legacy values are shifted by +1 to preserve sound.
        auto mappedChoice = juce::roundToInt (*legacyValue);
        if (sourceSchemaVersion <= 5)
            mappedChoice = juce::jlimit (0, 2, mappedChoice) + 1;

        if (auto* parameter = parameters.getParameter (parameterId))
        {
            const auto clampedValue = parameter->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, parameter->convertTo0to1 (static_cast<float> (mappedChoice))));

            for (int i = 0; i < migratedState.getNumChildren(); ++i)
            {
                auto node = migratedState.getChild (i);
                if (node.hasType ("PARAM") && node.getProperty ("id").toString() == juce::String (parameterId))
                {
                    node.setProperty ("value", clampedValue, nullptr);
                    break;
                }
            }
        }
    }
}

void PolySynthAudioProcessor::writeLayeredStateToTree (juce::ValueTree& stateTree) const
{
    stateTree.removeChild (stateTree.getChildWithName (layersNodeId), nullptr);

    juce::ValueTree layersNode (layersNodeId);
    layersNode.setProperty (nextLayerIdPropertyId, static_cast<juce::int64> (instrumentState.getLayers().size() + 1), nullptr);
    layersNode.setProperty (selectedLayerIdPropertyId, static_cast<juce::int64> (selectedLayerId), nullptr);

    for (const auto& layer : instrumentState.getLayers())
    {
        juce::ValueTree layerNode (layerNodeId);
        layerNode.setProperty (layerIdPropertyId, static_cast<juce::int64> (layer.layerId), nullptr);

        juce::ValueTree layerStateNode (layerStateNodeId);
        layerStateNode.setProperty (waveformParameterId, static_cast<int> (layer.waveform), nullptr);
        layerStateNode.setProperty (maxVoicesParameterId, layer.voiceCount, nullptr);
        layerStateNode.setProperty (stealPolicyParameterId, stealPolicyToChoiceIndex (layer.stealPolicy), nullptr);
        layerStateNode.setProperty (attackParameterId, layer.attackSeconds, nullptr);
        layerStateNode.setProperty (decayParameterId, layer.decaySeconds, nullptr);
        layerStateNode.setProperty (sustainParameterId, layer.sustainLevel, nullptr);
        layerStateNode.setProperty (releaseParameterId, layer.releaseSeconds, nullptr);
        layerStateNode.setProperty (modulationDepthParameterId, layer.modulationDepth, nullptr);
        layerStateNode.setProperty (modulationRateParameterId, layer.modulationRateHz, nullptr);
        layerStateNode.setProperty (velocitySensitivityParameterId, layer.velocitySensitivity, nullptr);
        layerStateNode.setProperty (modulationDestinationParameterId, modDestinationToChoiceIndex (layer.modulationDestination), nullptr);
        layerStateNode.setProperty (unisonVoicesParameterId, layer.unisonVoices, nullptr);
        layerStateNode.setProperty (unisonDetuneCentsParameterId, layer.unisonDetuneCents, nullptr);
        layerStateNode.setProperty (outputStageParameterId, outputStageToChoiceIndex (layer.outputStage), nullptr);
        layerStateNode.setProperty (rootNoteAbsolutePropertyId, clampMidiNote (layer.rootNoteAbsolute), nullptr);
        layerStateNode.setProperty (mutePropertyId, layer.mute, nullptr);
        layerStateNode.setProperty (soloPropertyId, layer.solo, nullptr);
        layerStateNode.setProperty (layerVolumePropertyId, juce::jmax (0.0f, layer.layerVolume), nullptr);
        layerNode.addChild (layerStateNode, -1, nullptr);
        layersNode.addChild (layerNode, -1, nullptr);
    }

    juce::ValueTree layerOrderNode (layerOrderNodeId);
    for (const auto layerId : instrumentState.getLayerOrder())
    {
        juce::ValueTree itemNode (orderItemNodeId);
        itemNode.setProperty (orderLayerIdPropertyId, static_cast<juce::int64> (layerId), nullptr);
        layerOrderNode.addChild (itemNode, -1, nullptr);
    }
    layersNode.addChild (layerOrderNode, -1, nullptr);

    stateTree.addChild (layersNode, -1, nullptr);
}

void PolySynthAudioProcessor::loadLayeredStateFromTree (const juce::ValueTree& stateTree)
{
    if (const auto layersNode = stateTree.getChildWithName (layersNodeId); layersNode.isValid())
    {
        const auto restoredSelectedId = static_cast<uint64_t> (static_cast<juce::int64> (layersNode.getProperty (selectedLayerIdPropertyId, juce::var (0))));
        if (restoredSelectedId != 0 && instrumentState.findLayerById (restoredSelectedId) != nullptr)
        {
            selectedLayerId = restoredSelectedId;
            return;
        }
    }

    if (const auto& order = instrumentState.getLayerOrder(); ! order.empty())
        selectedLayerId = order.front();
}

std::optional<uint64_t> PolySynthAudioProcessor::getLayerIdForVisualIndex (std::size_t visualIndex) const noexcept
{
    const auto& layerOrder = instrumentState.getLayerOrder();
    if (visualIndex >= layerOrder.size())
        return std::nullopt;

    return layerOrder[visualIndex];
}

std::size_t PolySynthAudioProcessor::getVisualIndexForLayerId (uint64_t layerId) const noexcept
{
    const auto& layerOrder = instrumentState.getLayerOrder();
    const auto it = std::find (layerOrder.begin(), layerOrder.end(), layerId);
    if (it == layerOrder.end())
        return 0;

    return static_cast<std::size_t> (std::distance (layerOrder.begin(), it));
}

void PolySynthAudioProcessor::ensureSelectedLayerIsValid() noexcept
{
    if (instrumentState.findLayerById (selectedLayerId) != nullptr)
        return;

    if (const auto& layerOrder = instrumentState.getLayerOrder(); ! layerOrder.empty())
        selectedLayerId = layerOrder.front();
    else
        selectedLayerId = 0;
}

std::optional<float> PolySynthAudioProcessor::findLegacyParameterValue (const juce::ValueTree& tree,
                                                                        juce::StringRef parameterId)
{
    if (! tree.isValid())
        return std::nullopt;

    const juce::Identifier propertyId { juce::String (parameterId) };

    if (tree.hasProperty (propertyId))
    {
        const auto propertyValue = tree.getProperty (propertyId);
        if (parameterId == juce::StringRef (waveformParameterId))
            return parseWaveformLegacyValue (propertyValue);

        if (propertyValue.isDouble() || propertyValue.isInt() || propertyValue.isInt64() || propertyValue.isBool())
            return static_cast<float> (propertyValue);

        const auto numeric = propertyValue.toString().getFloatValue();
        if (! std::isnan (numeric))
            return numeric;
    }

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto node = tree.getChild (i);
        if (! node.hasType ("PARAM") || ! node.hasProperty ("id")
            || node.getProperty ("id").toString() != juce::String (parameterId))
            continue;

        const auto parameterValue = node.getProperty ("value");

        if (parameterId == juce::StringRef (waveformParameterId))
            return parseWaveformLegacyValue (parameterValue);

        if (parameterValue.isDouble() || parameterValue.isInt() || parameterValue.isInt64() || parameterValue.isBool())
            return static_cast<float> (parameterValue);

        const auto numeric = parameterValue.toString().getFloatValue();
        if (! std::isnan (numeric))
            return numeric;
    }

    if (const auto legacyMonoNode = tree.getChildWithName ("LegacyMonoState"); legacyMonoNode.isValid())
        return findLegacyParameterValue (legacyMonoNode, parameterId);

    if (parameterId == juce::StringRef (modulationDepthParameterId))
        return findLegacyParameterValue (tree, "lfoDepth");

    return std::nullopt;
}

std::optional<float> PolySynthAudioProcessor::parseWaveformLegacyValue (const juce::var& value)
{
    if (value.isInt() || value.isInt64() || value.isDouble() || value.isBool())
        return static_cast<float> (value);

    const auto normalized = value.toString().trim().toLowerCase();

    if (normalized.containsOnly ("0123456789.-"))
        return normalized.getFloatValue();

    if (normalized == "sine")
        return static_cast<float> (waveformToChoiceIndex (Waveform::sine));
    if (normalized == "saw")
        return static_cast<float> (waveformToChoiceIndex (Waveform::saw));
    if (normalized == "square")
        return static_cast<float> (waveformToChoiceIndex (Waveform::square));
    if (normalized == "triangle")
        return static_cast<float> (waveformToChoiceIndex (Waveform::triangle));

    return std::nullopt;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PolySynthAudioProcessor();
}
