/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
StocSynthAudioProcessor::StocSynthAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),treeState(*this, nullptr, "PARAMETERS", createParameterLayout())
#endif
{
    m_StochFactor = treeState.getRawParameterValue("StochFactor");
    m_Decimation = treeState.getRawParameterValue("NoiseLevel");
    m_Amp  = treeState.getRawParameterValue("Amp");
    m_Cutoff  = treeState.getRawParameterValue("LowCutoff");
    gain_BlockL = std::make_unique<Gain_Block>();
    gain_BlockR = std::make_unique<Gain_Block>();
    sTFT = std::make_unique<STFT>();
    
    
    
}

StocSynthAudioProcessor::~StocSynthAudioProcessor()
{
}
juce::AudioProcessorValueTreeState::ParameterLayout
StocSynthAudioProcessor::createParameterLayout()
{
    // create parameters
    // you could also use a array with strings and add them in a for loop
    std::vector <std::unique_ptr<juce::RangedAudioParameter>> params;
    
    auto stochFactor = std::make_unique<juce::AudioParameterFloat>("StochFactor","StochFactor",0.1,1.0,0.5);
    
    auto decimation = std::make_unique<juce::AudioParameterFloat>("NoiseLevel","NoiseLevel",0.0,0.10,0.05);
    
    auto amp = std::make_unique<juce::AudioParameterFloat>("Amp","Amp",0.01,2.0,0.5);
    
    
    auto filter = std::make_unique<juce::AudioParameterFloat>("LowCutoff","LowCutoff",10.0,20000.0,2000);
    params.push_back(std::move(filter));
    params.push_back(std::move(stochFactor));
    params.push_back(std::move(decimation));
    params.push_back(std::move(amp));
    return {params.begin(),params.end()};
}
//==============================================================================
const juce::String StocSynthAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool StocSynthAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool StocSynthAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool StocSynthAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double StocSynthAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int StocSynthAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int StocSynthAudioProcessor::getCurrentProgram()
{
    return 0;
}

void StocSynthAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String StocSynthAudioProcessor::getProgramName (int index)
{
    return {};
}

void StocSynthAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void StocSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sTFT->setup(2);
    sTFT->updateParameters(2048,
                            4,
                           2);
    gain_BlockL->prepare(samplesPerBlock);
    gain_BlockR->prepare(samplesPerBlock);
}

void StocSynthAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool StocSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void StocSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

 
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    sTFT->processBlock(buffer);
    sTFT->updateStochfactor(*m_StochFactor);
    sTFT->updatedecimation(*m_Decimation);
    sTFT->updatecutoff(*m_Cutoff);
    auto left = buffer.getWritePointer(0);
    auto right =  buffer.getWritePointer(1);
    gain_BlockL->setGain(*m_Amp);
    gain_BlockL->process(left);
    gain_BlockR->setGain(*m_Amp);
    gain_BlockR->process(right);
}

//==============================================================================
bool StocSynthAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* StocSynthAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);//new StocSynthAudioProcessorEditor (*this);
}

//==============================================================================
void StocSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos (destData, true);
       treeState.state.writeToStream(mos);
}

void StocSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
                                 if(tree.isValid() )
                                 {
                                     treeState.replaceState(tree);
                                 }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StocSynthAudioProcessor();
}
