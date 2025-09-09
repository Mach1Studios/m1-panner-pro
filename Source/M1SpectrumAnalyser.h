// /Source/M1SpectrumAnalyser.h
#pragma once
#include <JuceHeader.h>

// Lightweight, header-only spectrum analyser that sums all channels to mono.
// Designed to be called from the audio thread without locks.
class M1SpectrumAnalyser
{
public:
    // 2048-point FFT is a good balance of resolution/cost for a plugin UI
    static constexpr int fftOrder   = 11;
    static constexpr int fftSize    = 1 << fftOrder;  // 2048
    static constexpr int scopeSize  = 256;            // UI points

    M1SpectrumAnalyser()
        : fft (fftOrder),
          window (fftSize, juce::dsp::WindowingFunction<float>::hann, false)
    {
        clear();
        scopeData.resize(scopeSize, 0.0f);
        scopeDataSmoothed.resize(scopeSize, 0.0f);
    }

    void prepare(double sr, int maxBlockSize)
    {
        sampleRate = (sr > 0.0 ? sr : 44100.0);
        monoScratch.resize(juce::jmax(1, maxBlockSize), 0.0f);
        reset();
    }

    void reset()
    {
        fifoIndex = 0;
        clear();
        std::fill(scopeData.begin(), scopeData.end(), 0.0f);
        std::fill(scopeDataSmoothed.begin(), scopeDataSmoothed.end(), 0.0f);
        newFrameReady.store(false);
    }

    // Push the OUTPUT buffer (all channels). This will sum to mono and feed the FFT FIFO.
    inline void pushAudioBuffer(const juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numSamples  = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numSamples <= 0 || numChannels <= 0) return;

        // pre-sized in prepare()
        auto* mono = monoScratch.data();
        // Sum all channels -> mono
        // (normalised by channel count to avoid huge level swings)
        const float norm = 1.0f / juce::jmax(1, numChannels);
        for (int i = 0; i < numSamples; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                s += buffer.getReadPointer(ch)[i];
            mono[i] = s * norm;
        }

        pushSamples(mono, numSamples);
    }

    // UI thread pulls smoothed, normalised [0..1] magnitudes
    // Returns true if new data since last call
    bool getMagnitudesCopy(std::vector<float>& out) const
    {
        out.resize(scopeSize);
        // It’s OK if we copy while writer updates: we only write whole frames and keep it lock-free
        for (int i = 0; i < scopeSize; ++i)
            out[i] = scopeDataSmoothed[i];
        // Only report "new" once
        return newFrameReady.exchange(false);
    }

private:
    // --- Audio-thread API ---
    inline void pushSamples(const float* samples, int num) noexcept
    {
        int i = 0;
        while (i < num)
        {
            const int space = fftSize - fifoIndex;
            const int toCopy = juce::jmin(space, num - i);
            memcpy(fifo.data() + fifoIndex, samples + i, (size_t)toCopy * sizeof(float));
            fifoIndex += toCopy;
            i += toCopy;

            if (fifoIndex == fftSize)
            {
                performFFT();      // do FFT now (small, bounded work)
                fifoIndex = 0;
            }
        }
    }

    inline void performFFT() noexcept
    {
        // Copy FIFO to fftData (real in, imag=0)
        std::fill(fftData.begin(), fftData.end(), 0.0f);
        memcpy(fftData.data(), fifo.data(), (size_t)fftSize * sizeof(float));

        // Window
        window.multiplyWithWindowingTable(fftData.data(), fftSize);

        // Spectrum (magnitudes only, 0..fftSize/2)
        fft.performFrequencyOnlyForwardTransform(fftData.data());

        // Convert to scope points (log-frequency mapping), normalise to [0..1]
        // dB range: clamp -100..0 dB
        constexpr float minDb = -100.0f, maxDb = 0.0f;
        const int nyquistBins = fftSize / 2;

        for (int i = 0; i < scopeSize; ++i)
        {
            const float norm = (float)i / (float)(scopeSize - 1);
            // 20 Hz .. 20 kHz log scale
            const float freq = 20.0f * std::pow(10.0f, norm * std::log10(20000.0f / 20.0f));
            int bin = (int)juce::jlimit(0.0f, (float)(nyquistBins - 1),
                                        freq * (float)fftSize / (float)sampleRate);

            const float mag = fftData[(size_t)bin];
            const float db  = 20.0f * std::log10(juce::jmax(mag, 1.0e-9f));
            const float v   = juce::jlimit(0.0f, 1.0f, juce::jmap(db, minDb, maxDb, 0.0f, 1.0f));

            scopeData[(size_t)i] = v;
        }

        // Smooth for stable drawing (attack fast, release slower)
        constexpr float attack = 0.6f; // higher -> faster to rise
        constexpr float release = 0.15f;

        for (int i = 0; i < scopeSize; ++i)
        {
            const float target = scopeData[(size_t)i];
            float& cur = scopeDataSmoothed[(size_t)i];
            const float coef = (target > cur) ? attack : release;
            cur = cur + coef * (target - cur);
        }

        newFrameReady.store(true);
    }

    void clear()
    {
        fifo.resize(fftSize, 0.0f);
        fftData.resize(fftSize * 2, 0.0f); // JUCE uses in-place size=2N when doing complex transforms
    }

    // --- Data ---
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::vector<float> fifo;
    std::vector<float> fftData;
    int fifoIndex = 0;

    std::vector<float> monoScratch;       // reused per block
    std::vector<float> scopeData;         // [0..1]
    std::vector<float> scopeDataSmoothed; // [0..1] presented to UI

    mutable std::atomic<bool> newFrameReady { false };
    double sampleRate = 44100.0;
};
