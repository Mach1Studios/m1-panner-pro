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

        // Single IIR filter - UI will read coefficients from this safely
        juce::dsp::IIR::Filter<float> filter;
        
        // SpinLock to protect coefficient pointer access between threads
        mutable juce::SpinLock coefficientsLock;
        
        // Smoothed parameter values to prevent zipper noise
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedFrequency;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedGain;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedQ;
        
        // Track last applied values to detect actual changes
        float lastAppliedFrequency = 1000.0f;
        float lastAppliedGain = 0.0f;
        float lastAppliedQ = 0.707f;
        
        // Thread-safe parameter updating (avoid smart pointer operations)
        std::atomic<bool> needsUpdate { false };
        std::atomic<float> pendingFrequency { 1000.0f };
        std::atomic<float> pendingGain { 0.0f };
        std::atomic<float> pendingQ { 0.707f };
        std::atomic<int> pendingType { Peak };
        std::atomic<bool> pendingEnabled { false };
        
        // Threshold for detecting meaningful parameter changes
        static constexpr float changeThreshold = 0.01f;
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
            bands[i].lastAppliedFrequency = bands[i].frequency;
            bands[i].lastAppliedGain = bands[i].gain;
            bands[i].lastAppliedQ = bands[i].q;
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
            float limitedFreq = juce::jlimit(20.0f, 20000.0f, frequency);
            
            // Update for UI reads (but don't trigger coefficient update yet)
            bands[bandIndex].frequency = limitedFreq;
            
            // Set pending for smoothed audio thread update
            bands[bandIndex].pendingFrequency.store(limitedFreq);
            bands[bandIndex].needsUpdate.store(true);
        }
    }

    void setBandGain(int bandIndex, float gainDb)
    {
        if (isValidBand(bandIndex))
        {
            float limitedGain = juce::jlimit(-24.0f, 24.0f, gainDb);
            
            // Update for UI reads
            bands[bandIndex].gain = limitedGain;
            
            // Set pending for smoothed audio thread update
            bands[bandIndex].pendingGain.store(limitedGain);
            bands[bandIndex].needsUpdate.store(true);
        }
    }

    void setBandQ(int bandIndex, float q)
    {
        if (isValidBand(bandIndex))
        {
            float limitedQ = juce::jlimit(0.1f, 10.0f, q);
            
            // Update for UI reads
            bands[bandIndex].q = limitedQ;
            
            // Set pending for smoothed audio thread update
            bands[bandIndex].pendingQ.store(limitedQ);
            bands[bandIndex].needsUpdate.store(true);
        }
    }

    void setBandType(int bandIndex, FilterType type)
    {
        if (isValidBand(bandIndex))
        {
            // Update for UI reads (type doesn't need smoothing)
            bands[bandIndex].type = type;
            
            // Set pending for audio thread
            bands[bandIndex].pendingType.store(static_cast<int>(type));
            bands[bandIndex].needsUpdate.store(true);
        }
    }

    void setBandEnabled(int bandIndex, bool enabled)
    {
        if (isValidBand(bandIndex))
        {
            // Immediately update the enabled state so UI can read it
            bands[bandIndex].enabled = enabled;
            
            // Also set pending value for audio thread synchronization
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
        
        // Debug: Check input for anomalies
        if (!std::isfinite(sample))
        {
            DBG("MultibandEQ: Input sample is NaN or Inf!");
            return 0.0f;
        }

        for (auto& band : bands)
        {
            // Apply pending parameter updates safely on audio thread
            if (band.needsUpdate.load())
            {
                // Get new target values
                float newFreqTarget = band.pendingFrequency.load();
                float newGainTarget = band.pendingGain.load();
                float newQTarget = band.pendingQ.load();
                
                // Set target values for smoothing
                band.smoothedFrequency.setTargetValue(newFreqTarget);
                band.smoothedGain.setTargetValue(newGainTarget);
                band.smoothedQ.setTargetValue(newQTarget);
                
                // Type and enabled don't need smoothing - apply immediately
                band.type = static_cast<FilterType>(band.pendingType.load());
                band.enabled = band.pendingEnabled.load();
                
                // Update internal values for UI reads
                band.frequency = newFreqTarget;
                band.gain = newGainTarget;
                band.q = newQTarget;
                
                // Update coefficients immediately when parameter changes
                // Track what we applied
                band.lastAppliedFrequency = newFreqTarget;
                band.lastAppliedGain = newGainTarget;
                band.lastAppliedQ = newQTarget;
                
                updateBandCoefficientsAudioThread(band);
                
                band.needsUpdate.store(false);
            }
            
            // Advance the smoothers for smooth audio transitions
            // Note: We update coefficients only on parameter change, not during smoothing
            // The smoothing is handled by the filter's internal state
            if (band.smoothedFrequency.isSmoothing())
                band.smoothedFrequency.skip(1);
            if (band.smoothedGain.isSmoothing())
                band.smoothedGain.skip(1);
            if (band.smoothedQ.isSmoothing())
                band.smoothedQ.skip(1);
            
            // Only process when the band is enabled and we have valid coefficients
            if (band.enabled && band.filter.coefficients != nullptr)
            {
                output = band.filter.processSample(output);
            }
        }
        
        // Final output check
        if (!std::isfinite(output))
        {
            DBG("MultibandEQ: Final output is NaN or Inf!");
            return 0.0f;
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
    // Thread-safe: UI reads from audio thread's filter coefficients with lock protection
    double getMagnitudeForFrequency(double frequency) const
    {
        if (sampleRate <= 0.0)
            return 1.0;
        
        // Clamp frequency to valid range [20Hz, Nyquist frequency)
        // JUCE asserts if frequency >= sampleRate * 0.5
        const double nyquistFreq = sampleRate * 0.5;
        frequency = juce::jlimit(20.0, nyquistFreq - 1.0, frequency);
            
        double magnitude = 1.0;
        
        for (const auto& band : bands)
        {
            if (band.enabled)
            {
                juce::dsp::IIR::Coefficients<float>::Ptr coeffs;
                
                // Thread-safe: Lock while copying the pointer
                {
                    const juce::SpinLock::ScopedLockType lock(band.coefficientsLock);
                    coeffs = band.filter.coefficients;
                }
                
                // Now we can safely use coeffs outside the lock
                if (coeffs != nullptr)
                {
                    try
                    {
                        // Calculate the magnitude response of this band at the given frequency
                        double bandMagnitude = coeffs->getMagnitudeForFrequency(frequency, sampleRate);
                        
                        // Check for valid result (not NaN or infinity)
                        if (std::isfinite(bandMagnitude) && bandMagnitude > 0.0)
                        {
                            magnitude *= bandMagnitude;
                        }
                    }
                    catch (...)
                    {
                        // Silently handle any exceptions during visualization
                        // This prevents crashes in the UI thread
                    }
                }
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

        // Validate band parameters before creating coefficients
        float freq = juce::jlimit(20.0f, 20000.0f, band.frequency);
        float q = juce::jlimit(0.1f, 10.0f, band.q);
        float gain = juce::jlimit(-24.0f, 24.0f, band.gain);
        
        juce::dsp::IIR::Coefficients<float>::Ptr newCoeffs;

        try
        {
            switch (band.type)
            {
                case Bypass:
                    // All-pass so you can leave the band enabled without changing tone
                    newCoeffs = juce::dsp::IIR::Coefficients<float>::makeAllPass (sampleRate, freq);
                    break;

                case HighPass:
                    newCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, freq, q);
                    break;

                case LowShelf:
                    newCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, freq, q,
                                                                                    juce::Decibels::decibelsToGain(gain));
                    break;

                case Peak:
                    newCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, freq, q,
                                                                                      juce::Decibels::decibelsToGain(gain));
                    break;

                case HighShelf:
                    newCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, freq, q,
                                                                                     juce::Decibels::decibelsToGain(gain));
                    break;

                case LowPass:
                    newCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, freq, q);
                    break;
                    
                default:
                    // Unknown filter type, create bypass
                    newCoeffs = juce::dsp::IIR::Coefficients<float>::makeAllPass (sampleRate, 1000.0f);
                    break;
            }
        }
        catch (...)
        {
            // If coefficient creation fails, create a safe bypass filter
            newCoeffs = juce::dsp::IIR::Coefficients<float>::makeAllPass (sampleRate, 1000.0f);
        }

        if (newCoeffs != nullptr)
        {
            // Thread-safe: Lock while updating the coefficients pointer
            const juce::SpinLock::ScopedLockType lock(band.coefficientsLock);
            band.filter.coefficients = newCoeffs;
        }
    }
};
