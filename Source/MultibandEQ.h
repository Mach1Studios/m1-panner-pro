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
        float      frequency  = 1000.0f;   // Hz
        float      gain       = 0.0f;      // dB
        float      q          = 0.707f;    // Q factor
        FilterType type       = Peak;
        bool       enabled    = false;

        // Keep a simple IIR filter per band
        juce::dsp::IIR::Filter<float>                    filter;
        juce::dsp::IIR::Coefficients<float>::Ptr         coefficients;
    };

    MultibandEQ()
    {
        // Default bands
        bands[0] = {  80.0f,   0.0f, 0.707f, HighPass,  false };  // HPF
        bands[1] = { 200.0f,   0.0f, 0.707f, LowShelf,  false };  // Low shelf
        bands[2] = { 800.0f,   0.0f, 1.0f,   Peak,      false };  // Low mid
        bands[3] = { 3200.0f,  0.0f, 1.0f,   Peak,      false };  // High mid
        bands[4] = { 8000.0f,  0.0f, 0.707f, HighShelf, false };  // High shelf
        bands[5] = { 12000.0f, 0.0f, 0.707f, LowPass,   false };  // LPF

        // Safe default; prepare() will set the correct sample rate and refresh
        sampleRate = 44100.0;

        for (int i = 0; i < 6; ++i)
            updateBandCoefficients(i);
    }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        for (auto& band : bands)
            band.filter.prepare(spec);

        // Refresh all coefficients at the new sample rate
        for (int i = 0; i < 6; ++i)
            updateBandCoefficients(i);
    }

    void reset()
    {
        for (auto& band : bands)
            band.filter.reset();
    }

    void setBandFrequency(int bandIndex, float frequency)
    {
        if (isValidBand(bandIndex))
        {
            bands[bandIndex].frequency = juce::jlimit(20.0f, 20000.0f, frequency);
            updateBandCoefficients(bandIndex);
        }
    }

    void setBandGain(int bandIndex, float gainDb)
    {
        if (isValidBand(bandIndex))
        {
            bands[bandIndex].gain = juce::jlimit(-24.0f, 24.0f, gainDb);
            updateBandCoefficients(bandIndex);
        }
    }

    void setBandQ(int bandIndex, float q)
    {
        if (isValidBand(bandIndex))
        {
            bands[bandIndex].q = juce::jlimit(0.1f, 10.0f, q);
            updateBandCoefficients(bandIndex);
        }
    }

    void setBandType(int bandIndex, FilterType type)
    {
        if (isValidBand(bandIndex))
        {
            bands[bandIndex].type = type;
            updateBandCoefficients(bandIndex);
        }
    }

    void setBandEnabled(int bandIndex, bool enabled)
    {
        if (isValidBand(bandIndex))
            bands[bandIndex].enabled = enabled;
    }

    // Process a single sample (used in your headshadow path)
    float processSample(float sample)
    {
        float output = sample;

        for (auto& band : bands)
        {
            // Only process when the band is enabled and we have valid coefficients
            if (band.enabled && band.coefficients != nullptr)
                output = band.filter.processSample(output); // <-- instance call, not static
        }

        return output;
    }

    // Convenience buffer processing (not used by headshadow path)
    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float* data = buffer.getWritePointer(channel);
            for (int n = 0; n < buffer.getNumSamples(); ++n)
                data[n] = processSample(data[n]);
        }
    }

    Band&       getBand(int index)       { jassert(isValidBand(index)); return bands[index]; }
    const Band& getBand(int index) const { jassert(isValidBand(index)); return bands[index]; }
    
    // Calculate magnitude response at a given frequency for visualization
    double getMagnitudeForFrequency(double frequency) const
    {
        double magnitude = 1.0;
        
        for (const auto& band : bands)
        {
            if (band.enabled && band.coefficients != nullptr)
            {
                // Calculate the magnitude response of this band at the given frequency
                double bandMagnitude = band.coefficients->getMagnitudeForFrequency(frequency, sampleRate);
                magnitude *= bandMagnitude;
            }
        }
        
        return magnitude;
    }

private:
    Band   bands[6];
    double sampleRate = 44100.0;

    static bool isValidBand(int idx) noexcept { return idx >= 0 && idx < 6; }

    void updateBandCoefficients(int bandIndex)
    {
        if (!isValidBand(bandIndex) || sampleRate <= 0.0)
            return;

        auto& band = bands[bandIndex];
        juce::dsp::IIR::Coefficients<float>::Ptr newCoeffs;

        switch (band.type)
        {
            case Bypass:
                // All-pass so you can leave the band enabled without changing tone
                newCoeffs = juce::dsp::IIR::Coefficients<float>::makeAllPass (sampleRate, band.frequency);
                break;

            case HighPass:
                newCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, band.frequency, band.q);
                break;

            case LowShelf:
                newCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, band.frequency, band.q,
                                                                                juce::Decibels::decibelsToGain(band.gain));
                break;

            case Peak:
                newCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, band.frequency, band.q,
                                                                                  juce::Decibels::decibelsToGain(band.gain));
                break;

            case HighShelf:
                newCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, band.frequency, band.q,
                                                                                 juce::Decibels::decibelsToGain(band.gain));
                break;

            case LowPass:
                newCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, band.frequency, band.q);
                break;
        }

        if (newCoeffs != nullptr)
        {
            // Store and apply to the filter.
            // NB: IIR::Filter has no public `.state` – assign the coefficients pointer.
            band.coefficients          = newCoeffs;
            band.filter.coefficients   = newCoeffs;
        }
    }
};
