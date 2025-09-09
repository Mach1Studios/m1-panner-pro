// /Source/UI/M1SpectrumView.h
#pragma once

#include "MurkaBasicWidgets.h"
#include "MurkaView.h"
#include "../Config.h"
#include "../M1SpectrumAnalyser.h"

using namespace murka;

// Log-frequency spectrum trace view, designed to overlay M1EQComponent's plot area.
class M1SpectrumView : public murka::View<M1SpectrumView>
{
public:
    // Provided by parent: fetches latest magnitudes (normalised 0..1 from analyser)
    // Returns true if the frame was new since last call
    std::function<bool(std::vector<float>&)> fetchSpectrum;

    // Frequency and dB spans (match EQ plot by default)
    float fMin = 20.0f,   fMax = 20000.0f;
    float visibleMinDb = -24.0f, visibleMaxDb = 24.0f;

    // How analyser normalised its 0..1 values (M1SpectrumAnalyser uses -100..0 dB)
    float sourceMinDb = -100.0f, sourceMaxDb = 0.0f;

    void internalDraw(Murka& m)
    {
        const float w = shape.size.x;
        const float h = shape.size.y;

        // fetch data
        data.resize(M1SpectrumAnalyser::scopeSize);
        if (fetchSpectrum) fetchSpectrum(data);

        auto freqForIndex = [&](int i)
        {
            const float t = (float)i / (float)(int(data.size()) - 1); // 0..1
            return fMin * std::pow(fMax / fMin, t);                   // log sweep
        };
        auto xForFreq = [&](float f)
        {
            const float t = std::log10(juce::jlimit(fMin, fMax, f) / fMin) / std::log10(fMax / fMin);
            return t * w;
        };
        auto yForDb = [&](float db)
        {
            const float d = juce::jlimit(visibleMinDb, visibleMaxDb, db);
            const float t = juce::jmap(d, visibleMinDb, visibleMaxDb, 1.0f, 0.0f); // 1 at min, 0 at max
            return t * h;
        };

        m.setColor(ENABLED_PARAM);
        for (int i = 0; i < (int)data.size() - 1; ++i)
        {
            // Convert analyser's 0..1 back to dB, then into EQ's vertical scale
            const float db0 = juce::jmap(data[(size_t)i],     0.0f, 1.0f, sourceMinDb, sourceMaxDb);
            const float db1 = juce::jmap(data[(size_t)(i+1)], 0.0f, 1.0f, sourceMinDb, sourceMaxDb);

            const float x0 = xForFreq(freqForIndex(i));
            const float x1 = xForFreq(freqForIndex(i+1));
            const float y0 = yForDb(db0);
            const float y1 = yForDb(db1);

            m.drawLine(x0, y0, x1, y1);
        }
    }

private:
    std::vector<float> data;
};
