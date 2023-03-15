/*
  ==============================================================================

    Parameters.h
    Created: 15 Mar 2023 3:35:53pm
    Author:  Onez

  ==============================================================================
*/

#pragma once
#include "JuceHeader.h"

const juce::StringArray windowType {
        "Rectangular",
        "Barlett",
        "Hann",
        "Hamming"
};
const juce::StringArray FFtSizes {
        "64",
        "128",
        "256",
        "512",
        "1024",
        "2048",
        "4096",
        "8192",
        "16384"
};
