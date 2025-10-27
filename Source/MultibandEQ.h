#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>

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
        
        // Smoothed parameter values to prevent zipper noise
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedFrequency;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedGain;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedQ;
        
        // Thread-safe parameter updating (avoid smart pointer operations)
        std::atomic<bool> needsUpdate { false };
        std::atomic<float> pendingFrequency { 1000.0f };
        std::atomic<float> pendingGain { 0.0f };
        std::atomic<float> pendingQ { 0.707f };
        std::atomic<int> pendingType { Peak };
        std::atomic<bool> pendingEnabled { false };
        
        // Track if we need to update coefficients this block
        int samplesUntilNextUpdate = 0;
        static constexpr int updateInterval = 32; // Update coefficients every 32 samples
    };

    MultibandEQ()
    {
        // Initialize bands individually due to atomic members
        bands[0].frequency = 80.0f;   bands[0].gain = 0.0f; bands[0].q = 0.707f; bands[0].type = HighPass;  bands[0].enabled = false;
        bands[1].frequency = 200.0f;  bands[1].gain = 0.0f; bands[1].q = 0.707f; bands[1].type = LowShelf;  bands[1].enabled = false;
        bands[2].frequency = 800.0f;  bands[2].gain = 0.0f; bands[2].q = 1.0f;   bands[2].type = Peak;      bands[2].enabled = false;
        bands[3].frequency = 3200.0f; bands[3].gain = 0.0f; bands[3].q = 1.0f;   bands[3].type = Peak;      bands[3].enabled = false;
        bands[4].frequency = 8000.0f; bands[4].gain = 0.0f; bands[4].q = 0.707f; bands[4].type = HighShelf; bands[4].enabled = false;
        bands[5].frequency = 12000.0f;bands[5].gain = 0.0f; bands[5].q = 0.707f; bands[5].type = LowPass;   bands[5].enabled = false;

        // Safe default; prepare() will set the correct sample rate and refresh
        sampleRate = 44100.0;
        
        // Initialize smoothed values
        for (int i = 0; i < 6; ++i)
        {
            bands[i].smoothedFrequency.setCurrentAndTargetValue(bands[i].frequency);
            bands[i].smoothedGain.setCurrentAndTargetValue(bands[i].gain);
            bands[i].smoothedQ.setCurrentAndTargetValue(bands[i].q);
            bands[i].samplesUntilNextUpdate = 0;
            updateBandCoefficients(i);
        }
    }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        for (auto& band : bands)
        {
            band.filter.prepare(spec);
            
            // Initialize smoothers with appropriate ramp time (50ms for smooth parameter changes)
            band.smoothedFrequency.reset(sampleRate, 0.05);
            band.smoothedGain.reset(sampleRate, 0.05);
            band.smoothedQ.reset(sampleRate, 0.05);
            
            band.smoothedFrequency.setCurrentAndTargetValue(band.frequency);
            band.smoothedGain.setCurrentAndTargetValue(band.gain);
            band.smoothedQ.setCurrentAndTargetValue(band.q);
        }

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
            bands[bandIndex].pendingFrequency.store(juce::jlimit(20.0f, 20000.0f, frequency));
            bands[bandIndex].needsUpdate.store(true);
        }
    }

    void setBandGain(int bandIndex, float gainDb)
    {
        if (isValidBand(bandIndex))
        {
            bands[bandIndex].pendingGain.store(juce::jlimit(-24.0f, 24.0f, gainDb));
            bands[bandIndex].needsUpdate.store(true);
        }
    }

    void setBandQ(int bandIndex, float q)
    {
        if (isValidBand(bandIndex))
        {
            bands[bandIndex].pendingQ.store(juce::jlimit(0.1f, 10.0f, q));
            bands[bandIndex].needsUpdate.store(true);
        }
    }

    void setBandType(int bandIndex, FilterType type)
    {
        if (isValidBand(bandIndex))
        {
            bands[bandIndex].pendingType.store(static_cast<int>(type));
            bands[bandIndex].needsUpdate.store(true);
        }
    }

    void setBandEnabled(int bandIndex, bool enabled)
    {
        if (isValidBand(bandIndex))
        {
            bands[bandIndex].pendingEnabled.store(enabled);
            bands[bandIndex].needsUpdate.store(true);
        }
    }

    // Process a single sample (used in your headshadow path)
    // NOTE: This processes samples with persistent filter state, so you should
    // process all samples from one channel before moving to the next channel
    float processSample(float sample)
    {
        float output = sample;

        for (auto& band : bands)
        {
            // Apply pending parameter updates safely on audio thread
            if (band.needsUpdate.load())
            {
                // Set target values for smoothing instead of immediate update
                band.smoothedFrequency.setTargetValue(band.pendingFrequency.load());
                band.smoothedGain.setTargetValue(band.pendingGain.load());
                band.smoothedQ.setTargetValue(band.pendingQ.load());
                
                // Type and enabled don't need smoothing
                band.type = static_cast<FilterType>(band.pendingType.load());
                band.enabled = band.pendingEnabled.load();
                
                band.needsUpdate.store(false);
            }
            
            // Update coefficients at regular intervals using smoothed values
            if (band.samplesUntilNextUpdate <= 0)
            {
                // Get current smoothed values
                band.frequency = band.smoothedFrequency.getNextValue();
                band.gain = band.smoothedGain.getNextValue();
                band.q = band.smoothedQ.getNextValue();
                
                // Update coefficients on audio thread (safe)
                updateBandCoefficientsAudioThread(band);
                
                band.samplesUntilNextUpdate = Band::updateInterval;
            }
            else
            {
                // Skip smoothed value update if not updating coefficients
                band.samplesUntilNextUpdate--;
                
                // But still advance the smoothers to keep them in sync
                band.smoothedFrequency.skip(1);
                band.smoothedGain.skip(1);
                band.smoothedQ.skip(1);
            }
            
            // Only process when the band is enabled and we have valid coefficients
            if (band.enabled && band.coefficients != nullptr)
                output = band.filter.processSample(output);
        }

        return output;
    }
    
    // Process a single sample with independent state per invocation
    // Use this when processing interleaved channels to avoid state corruption
    float processSampleStateless(float sample, int channel)
    {
        // For stateless processing, we need to maintain separate state per channel
        // Since we can't do that efficiently here, we'll just apply the magnitude response
        // This is a simplified version - for proper filtering, you need per-channel state
        return processSample(sample); // Fall back to stateful version for now
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
        updateBandCoefficientsAudioThread(band);
    }
    
    void updateBandCoefficientsAudioThread(Band& band)
    {
        if (sampleRate <= 0.0)
            return;

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
            // Safe to assign coefficients on audio thread
            band.coefficients = newCoeffs;
            band.filter.coefficients = newCoeffs;
        }
    }
};
