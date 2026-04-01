#include "PolySynthAudioProcessor.h"
#include "PolySynthAudioProcessorEditor.h"
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
constexpr int currentStateSchemaVersion = 3;

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

constexpr std::array<const char*, schemaV2ParameterIds.size() + schemaV3ParameterIds.size()> allKnownParameterIds {
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
    unisonDetuneCentsParameterId
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
    updateParameterSnapshotFromAPVTS();
    applyParameterSnapshotToEngine();
}

PolySynthAudioProcessor::~PolySynthAudioProcessor()
{
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
    synthEngine.prepare (sampleRate, samplesPerBlock);
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
    applyParameterSnapshotToEngine();
    synthEngine.renderBlock (buffer, midiMessages);
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
                                                                     modDestinationToChoiceIndex (SynthVoice::ModulationDestination::amplitude)));
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
}

void PolySynthAudioProcessor::applyParameterSnapshotToEngine() noexcept
{
    synthEngine.setActiveVoiceCount (parameterSnapshot.maxVoices);
    synthEngine.setVoiceStealPolicy (parameterSnapshot.stealPolicy);
    synthEngine.setWaveform (parameterSnapshot.waveform);
    synthEngine.setEnvelopeTimes (parameterSnapshot.attackSeconds,
                                  parameterSnapshot.decaySeconds,
                                  parameterSnapshot.sustainLevel,
                                  parameterSnapshot.releaseSeconds);
    synthEngine.setModulationParameters (parameterSnapshot.modulationDepth, parameterSnapshot.modulationRateHz, parameterSnapshot.modulationDestination);
    synthEngine.setVelocitySensitivity (parameterSnapshot.velocitySensitivity);
    synthEngine.setUnisonVoices (parameterSnapshot.unisonVoices);
    synthEngine.setUnisonDetuneCents (parameterSnapshot.unisonDetuneCents);
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
    return static_cast<int> (destination);
}

SynthVoice::ModulationDestination PolySynthAudioProcessor::modDestinationFromChoiceIndex (int choiceIndex) noexcept
{
    switch (choiceIndex)
    {
        case static_cast<int> (ModDestinationParameterChoice::amplitude): return SynthVoice::ModulationDestination::amplitude;
        case static_cast<int> (ModDestinationParameterChoice::pitch): return SynthVoice::ModulationDestination::pitch;
        case static_cast<int> (ModDestinationParameterChoice::pulseWidth): return SynthVoice::ModulationDestination::pulseWidth;
        default: break;
    }

    return SynthVoice::ModulationDestination::amplitude;
}

const juce::StringArray& PolySynthAudioProcessor::getModDestinationChoices() noexcept
{
    static const juce::StringArray choices { "Amplitude", "Pitch", "Pulse Width" };
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
    const auto schemaVersion = static_cast<int> (legacyState.getProperty (schemaVersionPropertyId, 1));
    migrateV1ToV2 (migratedState, legacyState);

    if (schemaVersion >= 2)
        migrateV2ToV3 (migratedState, legacyState);

    migratedState.setProperty (schemaVersionPropertyId, currentStateSchemaVersion, nullptr);
    parameters.replaceState (migratedState);
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
