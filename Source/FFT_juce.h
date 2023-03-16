/*
  ==============================================================================

    FFT_juce.h
    Created: 23 Feb 2023 3:18:57pm
    Author:  Onez

  ==============================================================================
*/

#pragma once
#pragma once

#include "JuceHeader.h"
#include "Wavetabels.h"
//==============================================================================

class STFT
{
public:
    enum windowTypeIndex {
        windowTypeRectangular = 0,
        windowTypeBartlett,
        windowTypeHann,
        windowTypeHamming,
    };

    //======================================

    STFT() : numChannels (1)
    {
    }

    virtual ~STFT()
    {
    }

    //======================================

    void setup (const int numInputChannels)
    {
        numChannels = (numInputChannels > 0) ? numInputChannels : 1;
    }

    void updateParameters (const int newFftSize, const int newOverlap, const int newWindowType)
    {
        updateFftSize (newFftSize);
        updateHopSize (newOverlap);
        updateWindow (newWindowType);
    }

    //======================================

    void processBlock (juce::AudioSampleBuffer& block)
    {
        numSamples = block.getNumSamples();

        for (int channel = 0; channel < numChannels; ++channel) {
            float* channelData = block.getWritePointer (channel);
            currentInputBufferWritePosition = inputBufferWritePosition;
            currentOutputBufferWritePosition = outputBufferWritePosition;
            currentOutputBufferReadPosition = outputBufferReadPosition;
            currentSamplesSinceLastFFT = samplesSinceLastFFT;

            for (int sample = 0; sample < numSamples; ++sample) {
                const float inputSample = channelData[sample];
                inputBuffer.setSample (channel, currentInputBufferWritePosition, inputSample);
                if (++currentInputBufferWritePosition >= inputBufferLength)
                    currentInputBufferWritePosition = 0;

                channelData[sample] = outputBuffer.getSample (channel, currentOutputBufferReadPosition);
                outputBuffer.setSample (channel, currentOutputBufferReadPosition, 0.0f);
                if (++currentOutputBufferReadPosition >= outputBufferLength)
                    currentOutputBufferReadPosition = 0;

                if (++currentSamplesSinceLastFFT >= hopSize) {
                    currentSamplesSinceLastFFT = 0;

                    analysis (channel, inputBuffer,timeDomainBuffer);
                    modification();
                    synthesis (channel);
                }
            }
        }

        inputBufferWritePosition = currentInputBufferWritePosition;
        outputBufferWritePosition = currentOutputBufferWritePosition;
        outputBufferReadPosition = currentOutputBufferReadPosition;
        samplesSinceLastFFT = currentSamplesSinceLastFFT;
    }
    void updateStochfactor(float newValue){
        stocfactor = newValue;
    }
    void updatedecimation(float newValue){
        decimation = newValue * 100;
    }
    
    void updatecutoff(float newValue){
        cutoff = newValue;
    }


private:
    //======================================

    void updateFftSize (const int newFftSize)
    {
        fftSize = newFftSize;
        fft = std::make_unique<juce::dsp::FFT>(log2 (fftSize));

        inputBufferLength = fftSize;
        inputBuffer.clear();
        inputBuffer.setSize (numChannels, inputBufferLength);

        outputBufferLength = fftSize;
        outputBuffer.clear();
        outputBuffer.setSize (numChannels, outputBufferLength);

        fftWindow.realloc (fftSize);
        fftWindow.clear (fftSize);

        timeDomainBuffer.reset(new juce::dsp::Complex<float>[fftSize]);
        frequencyDomainBuffer.reset(new juce::dsp::Complex<float>[fftSize]);
        stochBuffer.reset(new juce::dsp::Complex<float>[fftSize]);
        outbufferBuffer.reset(new juce::dsp::Complex<float>[fftSize]);
        timeoutbufferBuffer.reset(new juce::dsp::Complex<float>[fftSize]);
        
        inputBufferWritePosition = 0;
        outputBufferWritePosition = 0;
        outputBufferReadPosition = 0;
        samplesSinceLastFFT = 0;
    }

    void updateHopSize (const int newOverlap)
    {
        overlap = newOverlap;
        if (overlap != 0) {
            hopSize = fftSize / overlap;
            outputBufferWritePosition = hopSize % outputBufferLength;
        }
    }

    void updateWindow (const int newWindowType)
    {
        switch (newWindowType) {
            case windowTypeRectangular: {
                for (int sample = 0; sample < fftSize; ++sample)
                    fftWindow[sample] = 1.0f;
                break;
            }
            case windowTypeBartlett: {
                for (int sample = 0; sample < fftSize; ++sample)
                    fftWindow[sample] = 1.0f - fabs (2.0f * (float)sample / (float)(fftSize - 1) - 1.0f);
                break;
            }
            case windowTypeHann: {
                for (int sample = 0; sample < fftSize; ++sample)
                    fftWindow[sample] = 0.5f - 0.5f * cosf (2.0f * M_PI * (float)sample / (float)(fftSize - 1));
                break;
            }
            case windowTypeHamming: {
                for (int sample = 0; sample < fftSize; ++sample)
                    fftWindow[sample] = 0.54f - 0.46f * cosf (2.0f * M_PI * (float)sample / (float)(fftSize - 1));
                break;
            }
        }

        float windowSum = 0.0f;
        for (int sample = 0; sample < fftSize; ++sample)
            windowSum += fftWindow[sample];

        windowScaleFactor = 0.0f;
        if (overlap != 0 && windowSum != 0.0f)
            windowScaleFactor = 1.0f / (float)overlap / windowSum * (float)fftSize;
    }

    //======================================

    void analysis (const int channel,juce::AudioSampleBuffer    inputBuffer,std::unique_ptr<juce::dsp::Complex<float>[]>& timeDomainBuffer)
    {
        int inputBufferIndex = currentInputBufferWritePosition;
        for (int index = 0; index < fftSize; ++index) {
            timeDomainBuffer[index].real (fftWindow[index] * inputBuffer.getSample (channel, inputBufferIndex));
            timeDomainBuffer[index].imag (0.0f);

            if (++inputBufferIndex >= inputBufferLength)
                inputBufferIndex = 0;
        }
    }
   
    virtual void modification()
    {
        fft->perform(timeDomainBuffer.get(), frequencyDomainBuffer.get(), false);
        
        
        //calculate magntiude spectrum
        for (int index = 0; index < fftSize / 2 +1; ++index) {
            mX[index]= 20 * log10(abs(frequencyDomainBuffer[index]));
        }
        // apply stochastic function
            float stocf = fftSize / 2 + 1 * stocfactor;
            float decifac = stocfactor * 100;
                for (int j = 0; j < stocf; j++) {
                    stochEnv[j] = fmod(mX[j],decifac);
                    stochphaseEnv[j] = fmod(arg(frequencyDomainBuffer[j]),decifac);
            }
        //it isnt very efficient.... but less artifacts
        for (int i = 0; i <  fftSize / 2 + 1; i++) {
            float v0 = stochEnv[i];
            float v1 = stochEnv[i + 1];
            float v2 = stochEnv[i + 2];
            float v3 = stochEnv[i + 3];
            for (int j = 0; j < 4; ++j) {
                float t = static_cast<float>(j) / 3.0f;
                stochCubicEnv[i + j] = cubicInterpolation(v0, v1, v2, v3, t);
            }
        }
        //still have to find a more efficient way... should pre - compute them in lists
        const float cutoffFrequency = cutoff; // choose a cutoff frequency in Hz
        //this should get in prepare to play
        const float sampleRate = 44100.0f; // set the sample rate of the audio
        const int windowSize = fftSize / 2 - 1; // get the size of the frequency domain buffer
        // create a low-pass filter kernel using Hann window

        for (int i = 0; i < windowSize; ++i) {
            float frequency = (float(i) / (windowSize - 1)) * sampleRate;
            float normalizedFrequency = frequency / cutoffFrequency;
            if (normalizedFrequency > 1.0f) {
                filterKernel[i] = 0.0f;
            } else {
                float hannFactor = 0.5f * (1.0f - std::cos(2.0f * M_PI * normalizedFrequency));
                filterKernel[i] = hannFactor;
            }
        }
        // * 0.1 otherwise it is too loud
        float noiseLevel = decimation * 0.1;
        for(int i = 0; i <  fftSize / 2 + 1; ++i) {
            randPhase = randomTable[i] * noiseLevel;
            float filteredPhase =randPhase* filterKernel[i]  + stochphaseEnv[i];
            filteredphase[i] = filteredPhase;
        }
        //linear interpolation didn't work as expected
        //using cubicinterpolation
        for (int i = 0; i <  fftSize / 2 - 3 ; ++i) {
            float v0 = filteredphase[i];
            float v1 = filteredphase[i + 1];
            float v2 = filteredphase[i + 2];
            float v3 = filteredphase[i + 3];
            for (int j = 0; j < 4; ++j) {
                float t = static_cast<float>(j) / 3.0f;
                cubicfilteredPhase[i + j] = cubicInterpolation(v0, v1, v2, v3, t);
            }
        }
        
        //Not really sure if this is correct but a bit less sample & hold effect
        unwrapPhase(cubicfilteredPhase, fftSize / 2 + 1);
        
        
            
        
        for (int index = 0; index < fftSize / 2 + 1; ++index) {
            randPhase = randomTable[index]  * noiseLevel;
            float amp = std::exp(stochEnv[index] / 20.0);
            float resAmp = amp +randPhase *filterKernel[index]  ;
            
            frequencyDomainBuffer[index].real(resAmp * cosf(cubicfilteredPhase[index]));
            frequencyDomainBuffer[index].imag(resAmp * sinf(cubicfilteredPhase[index]));
            if (index > 0 && index < fftSize / 2) {
                frequencyDomainBuffer[fftSize - index].real(resAmp * cosf(cubicfilteredPhase[index]));
                frequencyDomainBuffer[fftSize - index].imag(resAmp * sinf(-cubicfilteredPhase[index]));
                }
            }
          
            fft->perform(frequencyDomainBuffer.get(), timeoutbufferBuffer.get(), true);
    }

    
    inline float cubicInterpolation(float y0, float y1, float y2, float y3, float x) {
        const float a0 = y3 - y2 - y0 + y1;
        const float a1 = y0 - y1 - a0;
        const float a2 = y2 - y0;
        const float a3 = y1;
        return a0 * x * x * x + a1 * x * x + a2 * x + a3;
    }

    void unwrapPhase(float* phase, int size)
    {
        for (int i = 1; i < size; i++) {
            float diff = phase[i] - phase[i-1];
            if (diff > M_PI) {
                phase[i] -= 2.0f * M_PI;
            } else if (diff < -M_PI) {
                phase[i] += 2.0f * M_PI;
            }
        }
    }



    void synthesis (const int channel)
    {
        int outputBufferIndex = currentOutputBufferWritePosition;
        for (int index = 0; index < fftSize; ++index) {
            float outputSample = outputBuffer.getSample (channel, outputBufferIndex);
            outputSample += timeoutbufferBuffer[index].real() * windowScaleFactor;
            outputBuffer.setSample (channel, outputBufferIndex, outputSample);

            if (++outputBufferIndex >= outputBufferLength)
                outputBufferIndex = 0;
        }

        currentOutputBufferWritePosition += hopSize;
        if (currentOutputBufferWritePosition >= outputBufferLength)
            currentOutputBufferWritePosition = 0;
    }
    
  
protected:
    std::unique_ptr<juce::dsp::Complex<float>[]> timeDomainBuffer;
    std::unique_ptr<juce::dsp::Complex<float>[]> frequencyDomainBuffer;
    std::unique_ptr<juce::dsp::Complex<float>[]> stochBuffer;
    
    std::unique_ptr<juce::dsp::Complex<float>[]> outbufferBuffer;
    std::unique_ptr<juce::dsp::Complex<float>[]> timeoutbufferBuffer;
    float stochEnv [16384] = {0};
    float stochphaseEnv [16384] = {0};
    float stochCubicEnv [16384] = {0};
    float stochCubicPhase [16384] = {0};
    float mX[16384] = {0};
    float filterKernel[16384] = {0};
    float filteredphase[16384] = {0};
    float cubicfilteredPhase[16384] = {0};
    float cutoff = 10;
     //======================================
    int numChannels;
    int numSamples;
    float stocfactor = 0.5;
    float randPhase = 0;
    float previousPhase = 0;
    float decimation = 0;
    int fftSize;
    std::unique_ptr<juce::dsp::FFT> fft;

    int inputBufferLength;
    juce::AudioSampleBuffer inputBuffer;
    int outputBufferLength;
    juce::AudioSampleBuffer outputBuffer;
    juce::HeapBlock<float> fftWindow;
    
    int overlap;
    int hopSize;
    float windowScaleFactor;
    int numFrames;
    int frameNumber;
    int inputBufferWritePosition;
    int outputBufferWritePosition;
    int outputBufferReadPosition;
    int samplesSinceLastFFT;
   float mag1 = 0;
   float mag2 = 0;
   float mag3 = 0;
   float mag4 = 0;
    float phase1 = 0;
    float phase2_ = 0;
    float phase3 = 0;
    float phase4 = 0;
    int currentInputBufferWritePosition;
    int currentOutputBufferWritePosition;
    int currentOutputBufferReadPosition;
    int currentSamplesSinceLastFFT;
};
