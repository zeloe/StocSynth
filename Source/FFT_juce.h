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
        for (int index = 0; index < fftSize / 2 + 1; ++index) {
            mX[index]= 20 * log10(abs(frequencyDomainBuffer[index]));
        }
        
            float stocf = fftSize * stocfactor;
                for (int j = 0; j < stocf; j++) {
                    stochEnv[j] = mX[j];
            }
        //interpolate
        for (int i = 0; i < fftSize /2 + 1; i++) {
          double frac = (double) i / fftSize;
          int left_idx = floor(frac * fftSize);
          int right_idx = ceil(frac * fftSize);
            double left_val = stochEnv[left_idx];
            double right_val = stochEnv[right_idx];
            stochEnv[i] = (left_val + (right_val - left_val) * (frac - left_idx / fftSize));
        }
       
             
        for (int index = 0; index < fftSize / 2 + 1; ++index) {
            float  randPhase = fmod(randPhase + (2 * M_PI * rand() / RAND_MAX), 2 * M_PI);
            float amp = std::exp(stochEnv[index] / 20.0);
            frequencyDomainBuffer[index].real(amp * cos(randPhase));
            frequencyDomainBuffer[index].imag(amp * sin(randPhase));
            if (index > 0 && index < fftSize / 2) {
                frequencyDomainBuffer[fftSize - index].real(amp * cos(randPhase));
                frequencyDomainBuffer[fftSize - index].imag(amp * sin(-randPhase));
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
    float mX[16384] = {0};
     //======================================
    int numChannels;
    int numSamples;
    float stocfactor = 0.5;
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
