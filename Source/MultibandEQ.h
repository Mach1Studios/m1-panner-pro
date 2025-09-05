#pragma once

#include <JuceHeader.h>

/**
 * Single-header multiband EQ processor with 6 configurable bands
 * Each band supports frequency, gain, Q, and filter type settings
 */
class MultibandEQ
{
public:
    enum FilterType
    {
        Bypass = 0,
        HighPass,
        LowShelf,
        Peak,
        HighShelf,
        LowPass
    };
    
    struct Band
    {
        float frequency = 1000.0f;  // Hz
        float gain = 0.0f;          // dB
        float q = 0.707f;           // Q factor
        FilterType type = Peak;
        bool enabled = false;
        
        juce::dsp::IIR::Filter<float> filter;
        juce::dsp::IIR::Coefficients<float>::Ptr coefficients;
    };
    
    MultibandEQ()
    {
        // Initialize default band frequencies
        bands[0] = {80.0f, 0.0f, 0.707f, HighPass, false};    // HPF
        bands[1] = {200.0f, 0.0f, 0.707f, LowShelf, false};   // Low shelf
        bands[2] = {800.0f, 0.0f, 1.0f, Peak, false};         // Low mid
        bands[3] = {3200.0f, 0.0f, 1.0f, Peak, false};        // High mid
        bands[4] = {8000.0f, 0.0f, 0.707f, HighShelf, false}; // High shelf
        bands[5] = {12000.0f, 0.0f, 0.707f, LowPass, false};  // LPF
        
        for (int i = 0; i < 6; ++i)
        {
            updateBandCoefficients(i);
        }
    }
    
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        
        for (auto& band : bands)
        {
            band.filter.prepare(spec);
        }
        
        // Update all coefficients after sample rate is set
        for (int i = 0; i < 6; ++i)
        {
            updateBandCoefficients(i);
        }
    }
    
    void reset()
    {
        for (auto& band : bands)
        {
            band.filter.reset();
        }
    }
    
    void setBandFrequency(int bandIndex, float frequency)
    {
        if (bandIndex >= 0 && bandIndex < 6)
        {
            bands[bandIndex].frequency = juce::jlimit(20.0f, 20000.0f, frequency);
            updateBandCoefficients(bandIndex);
        }
    }
    
    void setBandGain(int bandIndex, float gainDb)
    {
        if (bandIndex >= 0 && bandIndex < 6)
        {
            bands[bandIndex].gain = juce::jlimit(-24.0f, 24.0f, gainDb);
            updateBandCoefficients(bandIndex);
        }
    }
    
    void setBandQ(int bandIndex, float q)
    {
        if (bandIndex >= 0 && bandIndex < 6)
        {
            bands[bandIndex].q = juce::jlimit(0.1f, 10.0f, q);
            updateBandCoefficients(bandIndex);
        }
    }
    
    void setBandType(int bandIndex, FilterType type)
    {
        if (bandIndex >= 0 && bandIndex < 6)
        {
            bands[bandIndex].type = type;
            updateBandCoefficients(bandIndex);
        }
    }
    
    void setBandEnabled(int bandIndex, bool enabled)
    {
        if (bandIndex >= 0 && bandIndex < 6)
        {
            bands[bandIndex].enabled = enabled;
        }
    }
    
    float processSample(float sample)
    {
        float output = sample;
        
        for (auto& band : bands)
        {
            if (band.enabled && band.coefficients != nullptr)
            {
                output = band.filter.processSample(output);
            }
        }
        
        return output;
    }
    
    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float* channelData = buffer.getWritePointer(channel);
            
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                channelData[sample] = processSample(channelData[sample]);
            }
        }
    }
    
    Band& getBand(int index) { return bands[index]; }
    const Band& getBand(int index) const { return bands[index]; }
    
private:
    Band bands[6];
    double sampleRate = 44100.0;
    
    void updateBandCoefficients(int bandIndex)
    {
        if (bandIndex < 0 || bandIndex >= 6 || sampleRate <= 0)
            return;
            
        auto& band = bands[bandIndex];
        
        switch (band.type)
        {
            case Bypass:
                band.coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, band.frequency);
                break;
                
            case HighPass:
                band.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, band.frequency, band.q);
                break;
                
            case LowShelf:
                band.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, band.frequency, band.q, juce::Decibels::decibelsToGain(band.gain));
                break;
                
            case Peak:
                band.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, band.frequency, band.q, juce::Decibels::decibelsToGain(band.gain));
                break;
                
            case HighShelf:
                band.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, band.frequency, band.q, juce::Decibels::decibelsToGain(band.gain));
                break;
                
            case LowPass:
                band.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, band.frequency, band.q);
                break;
        }
        
        if (band.coefficients != nullptr)
        {
            band.filter.coefficients = band.coefficients;
        }
    }
};
