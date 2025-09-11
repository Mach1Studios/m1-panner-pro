// /Source/UI/M1SpectrumView.h
#pragma once

#include "MurkaBasicWidgets.h"
#include "MurkaView.h"
#include "../Config.h"
#include "../M1SpectrumAnalyser.h"

using namespace murka;

// Log-frequency spectrum trace. The parent must place this view
// exactly over the EQ inner plot area for perfect alignment.
class M1SpectrumView : public murka::View<M1SpectrumView>
{
public:
    // Provided by parent: fetches latest magnitudes [0..1], scopeSize points
    // Returns true if the frame was new since last call
    std::function<bool(std::vector<float>&)> fetchSpectrum;

    void internalDraw(Murka& m)
    {
        const float w = shape.size.x, h = shape.size.y;

        data.resize(M1SpectrumAnalyser::scopeSize);
        if (fetchSpectrum) fetchSpectrum(data);

        m.setColor(ENABLED_PARAM);
        for (int i = 0; i < (int)data.size() - 1; ++i)
        {
            const float nx0 = (float)i / (float)(data.size() - 1);
            const float nx1 = (float)(i + 1) / (float)(data.size() - 1);

            const float x0 = nx0 * w;
            const float x1 = nx1 * w;
            const float y0 = (1.0f - data[(size_t)i])     * h;
            const float y1 = (1.0f - data[(size_t)(i+1)]) * h;

            m.drawLine(x0, y0, x1, y1);
        }
    }

private:
    std::vector<float> data;
};
