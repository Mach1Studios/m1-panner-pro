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
//        // Background
//        m.setColor(BACKGROUND_GREY);
//        m.drawRectangle(0, 0, shape.size.x, shape.size.y);

//        // Outline
//        m.setColor(GRID_LINES_4_RGB);
//        m.drawRectangle(0, 0, shape.size.x, shape.size.y);

        // Grid (vertical log lines)
        const float w = shape.size.x, h = shape.size.y;
        const float padL = 4.0f, padR = 4.0f, padT = 2.0f, padB = 2.0f;

        auto logX = [&](float freqHz)
        {
            float norm = std::log10(freqHz / 20.0f) / std::log10(20000.0f / 20.0f);
            return padL + norm * (w - padL - padR);
        };

        const float freqs[] = { 31.5f, 63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };

        m.setColor(REF_LABEL_TEXT_COLOR);
        for (float f : freqs)
        {
            float x = logX(f);
            m.drawLine(x, padT, x, h - padB);
        }

        // Labels
        m.setColor(LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE - 8);
        for (float f : freqs)
        {
            float x = logX(f);
            std::string txt = (f >= 1000.0f ? std::to_string((int)(f/1000.0f)) + "k" : std::to_string((int)f));
            m.prepare<murka::Label>({ x - 12, h - 14, 24, 12 }).withAlignment(TEXT_CENTER).text(txt).draw();
        }

        // dB grid (horizontal)
        m.setColor(REF_LABEL_TEXT_COLOR);
        const float dbs[] = { 0.0f, -20.0f, -40.0f, -60.0f, -80.0f, -100.0f };
        auto yFromDb = [&](float db)
        {
            // we mapped -100..0 dB to 0..1, invert for screen Y
            float v = juce::jlimit(0.0f, 1.0f, juce::jmap(db, -100.0f, 0.0f, 0.0f, 1.0f));
            return padT + (1.0f - v) * (h - padT - padB);
        };
        for (float db : dbs)
        {
            float y = yFromDb(db);
            m.drawLine(padL, y, w - padR, y);
        }

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
