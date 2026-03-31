#include "MonoSynthAudioProcessor.h"
#include "MonoSynthAudioProcessorEditor.h"

namespace
{
constexpr auto waveformParameterId = "waveform";
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
    updateWaveformFromParameter();
    voice.setWaveform (waveform.load (std::memory_order_relaxed));
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
    juce::ignoreUnused (samplesPerBlock);
    voice.prepare (sampleRate);
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

    updateWaveformFromParameter();
    voice.setWaveform (waveform.load (std::memory_order_relaxed));

    const auto renderRange = [this, &buffer, totalNumOutputChannels] (int startSample, int endSample)
    {
        for (int sample = startSample; sample < endSample; ++sample)
        {
            const auto sampleValue = voice.renderSample();

            for (int channel = 0; channel < totalNumOutputChannels; ++channel)
                buffer.setSample (channel, sample, sampleValue);
        }
    };

    int renderedSamples = 0;

    for (const auto midiMetadata : midiMessages)
    {
        const auto eventOffset = juce::jlimit (0, numSamples, midiMetadata.samplePosition);

        if (eventOffset > renderedSamples)
            renderRange (renderedSamples, eventOffset);

        voice.handleMidiEvent (midiMetadata.getMessage());
        renderedSamples = eventOffset;
    }

    if (renderedSamples < numSamples)
        renderRange (renderedSamples, numSamples);

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
    return { layout.begin(), layout.end() };
}

void MonoSynthAudioProcessor::updateWaveformFromParameter() noexcept
{
    if (waveformParameter == nullptr)
        return;

    waveform.store (waveformFromParameterValue (waveformParameter->load (std::memory_order_relaxed)),
                    std::memory_order_relaxed);
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

const juce::StringArray& MonoSynthAudioProcessor::getWaveformChoices() noexcept
{
    static const juce::StringArray choices { "Sine", "Saw", "Square", "Triangle" };
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
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MonoSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes); xml != nullptr)
    {
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
    }

    updateWaveformFromParameter();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MonoSynthAudioProcessor();
}
