// /Source/UI/M1SpectrumView.h
#pragma once

#include "MurkaBasicWidgets.h"
#include "MurkaView.h"
#include "../Config.h"
#include "../M1SpectrumAnalyser.h"

using namespace murka;

// Simple log-frequency spectrum trace view
class M1SpectrumView : public murka::View<M1SpectrumView>
{
public:
    // Provided by parent: fetches latest magnitudes [0..1], scopeSize points
    // Returns true if the frame was new since last call
    std::function<bool(std::vector<float>&)> fetchSpectrum;

    void internalDraw(Murka& m)
    {
        // Grid (vertical log lines)
        const float w = shape.size.x, h = shape.size.y;
        const float padL = 4.0f, padR = 4.0f, padT = 2.0f, padB = 2.0f;

        auto logX = [&](float freqHz)
        {
            float norm = std::log10(freqHz / 20.0f) / std::log10(20000.0f / 20.0f);
            return padL + norm * (w - padL - padR);
        };

        // Fetch data
        data.resize(M1SpectrumAnalyser::scopeSize);
        if (fetchSpectrum) fetchSpectrum(data);

        // Trace
        m.setColor(ENABLED_PARAM);
        for (int i = 0; i < (int)data.size() - 1; ++i)
        {
            float normX0 = (float)i / (float)(data.size() - 1);
            float normX1 = (float)(i + 1) / (float)(data.size() - 1);

            float x0 = padL + normX0 * (w - padL - padR);
            float x1 = padL + normX1 * (w - padL - padR);
            float y0 = padT + (1.0f - data[(size_t)i]) * (h - padT - padB);
            float y1 = padT + (1.0f - data[(size_t)(i + 1)]) * (h - padT - padB);

            m.drawLine(x0, y0, x1, y1);
        }
    }

private:
    std::vector<float> data;
};
