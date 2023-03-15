/*
  ==============================================================================

    string_to_fftsize.h
    Created: 15 Mar 2023 3:58:34pm
    Author:  Onez

  ==============================================================================
*/

#pragma once
#include "JuceHeader.h"
int string_to_fftsize(int index){
    int fftsize;
    switch (index) {
        case 0:
            fftsize = 64;
            break;
        case 1:
            fftsize = 128;
            break;
        case 2:
            fftsize = 256;
            break;
        case 3:
            fftsize = 512;
            break;
        case 4:
            fftsize = 1024;
            break;
        case 5:
            fftsize = 2048;
            break;
        case 6:
            fftsize = 4096;
            break;
        case 7:
            fftsize = 8192;
            break;
        case 8:
            fftsize = 16384;
        default:
            fftsize = 2048;
            break;
    }
    
    
    
    
    
    return fftsize;
}
