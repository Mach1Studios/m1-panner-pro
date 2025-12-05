#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>

/**
 * Single-header multiband EQ processor with 6 configurable bands
 * Each band supports frequency, gain, Q, and filter type settings
 * Supports multi-channel processing with independent filter state per channel
 */
class MultibandEQ
{
public:
    static constexpr int MaxChannels = 16; // Maximum supported output channels
    
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

        // Per-channel filter instances - each channel needs independent state
        std::array<juce::dsp::IIR::Filter<float>, MaxChannels> filters;
        
        // Shared coefficients (thread-safe pointer, used by all channel filters)
        juce::dsp::IIR::Coefficients<float>::Ptr coefficients;
        
        // SpinLock to protect coefficient pointer access between threads
        mutable juce::SpinLock coefficientsLock;
        
        // Thread-safe parameter updating (avoid smart pointer operations)
        std::atomic<bool> needsUpdate { false };
        std::atomic<float> pendingFrequency { 1000.0f };
        std::atomic<float> pendingGain { 0.0f };
        std::atomic<float> pendingQ { 0.707f };
        std::atomic<int> pendingType { Peak };
        std::atomic<bool> pendingEnabled { false };
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
        numChannels = 1;
        
        // Initialize all band coefficients
        for (int i = 0; i < 6; ++i)
        {
            updateBandCoefficients(i);
        }
    }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = juce::jmin(static_cast<int>(spec.numChannels), MaxChannels);

        // Prepare all per-channel filters
        juce::dsp::ProcessSpec monoSpec = spec;
        monoSpec.numChannels = 1;
        
        for (auto& band : bands)
        {
            for (int ch = 0; ch < MaxChannels; ++ch)
            {
                band.filters[ch].prepare(monoSpec);
            }
        }

        // Refresh all coefficients at the new sample rate
        for (int i = 0; i < 6; ++i)
            updateBandCoefficients(i);
    }

    void reset()
    {
        for (auto& band : bands)
        {
            for (int ch = 0; ch < MaxChannels; ++ch)
            {
                band.filters[ch].reset();
            }
        }
    }
    
    // Reset only a specific channel's filter state
    void resetChannel(int channel)
    {
        if (channel < 0 || channel >= MaxChannels)
            return;
            
        for (auto& band : bands)
        {
            band.filters[channel].reset();
        }
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

    // Call once per audio block to apply any pending parameter changes
    // This should be called BEFORE processing samples to avoid mid-block coefficient changes
    void applyPendingParameterUpdates()
    {
        for (int bandIdx = 0; bandIdx < 6; ++bandIdx)
        {
            auto& band = bands[bandIdx];
            
            if (band.needsUpdate.load())
            {
                // Get new target values
                float newFreqTarget = band.pendingFrequency.load();
                float newGainTarget = band.pendingGain.load();
                float newQTarget = band.pendingQ.load();
                
                // Type and enabled don't need smoothing - apply immediately
                band.type = static_cast<FilterType>(band.pendingType.load());
                band.enabled = band.pendingEnabled.load();
                
                // Update internal values for UI reads
                band.frequency = newFreqTarget;
                band.gain = newGainTarget;
                band.q = newQTarget;
                
                // Update coefficients - this creates new coefficients but does NOT reset filter state
                updateBandCoefficientsAudioThread(band);
                
                band.needsUpdate.store(false);
            }
        }
    }
    
    // Process a single sample for a specific channel
    // Each channel maintains independent filter state - no cross-channel contamination
    float processSample(float sample, int channel)
    {
        if (channel < 0 || channel >= MaxChannels)
            return sample;
            
        float output = sample;
        
        // Debug: Check input for anomalies
        if (!std::isfinite(sample))
        {
            return 0.0f;
        }

        for (auto& band : bands)
        {
            // Only process when the band is enabled and we have valid coefficients
            if (band.enabled && band.coefficients != nullptr)
            {
                // Use the channel-specific filter instance
                output = band.filters[channel].processSample(output);
            }
        }
        
        // Final output check
        if (!std::isfinite(output))
        {
            return 0.0f;
        }

        return output;
    }
    
    // Legacy single-channel processSample (uses channel 0)
    float processSample(float sample)
    {
        return processSample(sample, 0);
    }

    // Convenience buffer processing (not used by headshadow path)
    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        // Apply pending parameter updates once at block start
        applyPendingParameterUpdates();
        
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float* data = buffer.getWritePointer(channel);
            for (int n = 0; n < buffer.getNumSamples(); ++n)
                data[n] = processSample(data[n], channel);
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
                    coeffs = band.coefficients;
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
            
            // Store the shared coefficients
            band.coefficients = newCoeffs;
            
            // Update coefficients for all channel filters
            // NOTE: We do NOT reset filter state here - resetting causes clicks!
            // The filter will adapt smoothly to the new coefficients.
            // Only reset when explicitly needed (e.g., playback start)
            for (int ch = 0; ch < MaxChannels; ++ch)
            {
                band.filters[ch].coefficients = newCoeffs;
            }
        }
    }
    
private:
    Band   bands[6];
    double sampleRate = 44100.0;
    int    numChannels = 1;

    static bool isValidBand(int idx) noexcept { return idx >= 0 && idx < 6; }
};
