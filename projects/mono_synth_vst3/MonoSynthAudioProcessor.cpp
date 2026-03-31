#include "MonoSynthAudioProcessor.h"
#include "MonoSynthAudioProcessorEditor.h"
#include <cmath>

namespace
{
constexpr auto waveformParameterId = "waveform";
constexpr auto maxVoicesParameterId = "maxVoices";
constexpr auto stealPolicyParameterId = "stealPolicy";
constexpr auto attackParameterId = "attack";
constexpr auto releaseParameterId = "release";
constexpr auto modulationDepthParameterId = "modDepth";
constexpr auto modulationRateParameterId = "modRate";
constexpr auto schemaVersionPropertyId = "schemaVersion";
constexpr int currentStateSchemaVersion = 1;
} // namespace

//==============================================================================
MonoSynthAudioProcessor::MonoSynthAudioProcessor()
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
    releaseParameter = parameters.getRawParameterValue (releaseParameterId);
    modulationDepthParameter = parameters.getRawParameterValue (modulationDepthParameterId);
    modulationRateParameter = parameters.getRawParameterValue (modulationRateParameterId);
    updateParameterSnapshotFromAPVTS();
    applyParameterSnapshotToEngine();
}

MonoSynthAudioProcessor::~MonoSynthAudioProcessor()
{
}

//==============================================================================
const juce::String MonoSynthAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MonoSynthAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool MonoSynthAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool MonoSynthAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double MonoSynthAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MonoSynthAudioProcessor::getNumPrograms()
{
    return 1;
}

int MonoSynthAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MonoSynthAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String MonoSynthAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void MonoSynthAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void MonoSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synthEngine.prepare (sampleRate, samplesPerBlock);
}

void MonoSynthAudioProcessor::releaseResources()
{
}

bool MonoSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void MonoSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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

MonoSynthAudioProcessor::Waveform MonoSynthAudioProcessor::getWaveform() const noexcept
{
    return waveform.load (std::memory_order_relaxed);
}

juce::AudioProcessorValueTreeState::ParameterLayout MonoSynthAudioProcessor::createParameterLayout()
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
    return { layout.begin(), layout.end() };
}

void MonoSynthAudioProcessor::updateParameterSnapshotFromAPVTS() noexcept
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

    if (releaseParameter != nullptr)
        parameterSnapshot.releaseSeconds = releaseParameter->load (std::memory_order_relaxed);

    if (modulationDepthParameter != nullptr)
        parameterSnapshot.modulationDepth = modulationDepthParameter->load (std::memory_order_relaxed);

    if (modulationRateParameter != nullptr)
        parameterSnapshot.modulationRateHz = modulationRateParameter->load (std::memory_order_relaxed);
}

void MonoSynthAudioProcessor::applyParameterSnapshotToEngine() noexcept
{
    synthEngine.setActiveVoiceCount (parameterSnapshot.maxVoices);
    synthEngine.setVoiceStealPolicy (parameterSnapshot.stealPolicy);
    synthEngine.setWaveform (parameterSnapshot.waveform);
    synthEngine.setEnvelopeTimes (parameterSnapshot.attackSeconds, parameterSnapshot.releaseSeconds);
    synthEngine.setModulationParameters (parameterSnapshot.modulationDepth, parameterSnapshot.modulationRateHz);
}

MonoSynthAudioProcessor::Waveform MonoSynthAudioProcessor::waveformFromParameterValue (float parameterValue) noexcept
{
    return waveformFromChoiceIndex (juce::roundToInt (parameterValue));
}

int MonoSynthAudioProcessor::waveformToChoiceIndex (Waveform waveformType) noexcept
{
    return static_cast<int> (waveformType);
}

MonoSynthAudioProcessor::Waveform MonoSynthAudioProcessor::waveformFromChoiceIndex (int choiceIndex) noexcept
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

SynthEngine::VoiceStealPolicy MonoSynthAudioProcessor::stealPolicyFromParameterValue (float parameterValue) noexcept
{
    return stealPolicyFromChoiceIndex (juce::roundToInt (parameterValue));
}

int MonoSynthAudioProcessor::stealPolicyToChoiceIndex (SynthEngine::VoiceStealPolicy policy) noexcept
{
    switch (policy)
    {
        case SynthEngine::VoiceStealPolicy::releasedFirst: return 0;
        case SynthEngine::VoiceStealPolicy::oldest: return 1;
        case SynthEngine::VoiceStealPolicy::quietest: return 2;
    }

    return 0;
}

SynthEngine::VoiceStealPolicy MonoSynthAudioProcessor::stealPolicyFromChoiceIndex (int choiceIndex) noexcept
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

const juce::StringArray& MonoSynthAudioProcessor::getWaveformChoices() noexcept
{
    static const juce::StringArray choices { "Sine", "Saw", "Square", "Triangle" };
    return choices;
}

const juce::StringArray& MonoSynthAudioProcessor::getStealPolicyChoices() noexcept
{
    static const juce::StringArray choices { "Released First", "Oldest", "Quietest" };
    return choices;
}

//==============================================================================
bool MonoSynthAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* MonoSynthAudioProcessor::createEditor()
{
    return new MonoSynthAudioProcessorEditor (*this);
}

//==============================================================================
void MonoSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto stateToSave = parameters.copyState();
    stateToSave.setProperty (schemaVersionPropertyId, currentStateSchemaVersion, nullptr);

    if (const auto xml = stateToSave.createXml())
        copyXmlToBinary (*xml, destData);
}

void MonoSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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

void MonoSynthAudioProcessor::restoreLegacyState (const juce::ValueTree& legacyState)
{
    auto migratedState = parameters.copyState();

    for (const auto* parameterId : { waveformParameterId,
                                     maxVoicesParameterId,
                                     stealPolicyParameterId,
                                     attackParameterId,
                                     releaseParameterId,
                                     modulationDepthParameterId,
                                     modulationRateParameterId })
    {
        if (auto* parameter = parameters.getParameter (parameterId))
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
    }

    migratedState = parameters.copyState();

    for (const auto* parameterId : { waveformParameterId,
                                     maxVoicesParameterId,
                                     stealPolicyParameterId,
                                     attackParameterId,
                                     releaseParameterId,
                                     modulationDepthParameterId,
                                     modulationRateParameterId })
    {
        if (const auto value = findLegacyParameterValue (legacyState, parameterId); value.has_value())
        {
            int childIndex = -1;
            for (int i = 0; i < migratedState.getNumChildren(); ++i)
            {
                const auto node = migratedState.getChild (i);
                if (node.hasType ("PARAM") && node.getProperty ("id").toString() == juce::String (parameterId))
                {
                    childIndex = i;
                    break;
                }
            }

            if (childIndex >= 0)
                migratedState.getChild (childIndex).setProperty ("value", *value, nullptr);
        }
    }

    migratedState.setProperty (schemaVersionPropertyId, currentStateSchemaVersion, nullptr);
    parameters.replaceState (migratedState);
}

std::optional<float> MonoSynthAudioProcessor::findLegacyParameterValue (const juce::ValueTree& tree,
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

    return std::nullopt;
}

std::optional<float> MonoSynthAudioProcessor::parseWaveformLegacyValue (const juce::var& value)
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
    return new MonoSynthAudioProcessor();
}
