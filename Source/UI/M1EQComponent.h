#pragma once

#include <JuceHeader.h>
#include "juce_murka/JuceMurkaBaseComponent.h"

#include "MurkaBasicWidgets.h"
#include "MurkaInputEventsRegister.h"
#include "MurkaView.h"

#include "../Config.h"
#include "../PluginProcessor.h"
#include "../MultibandEQ.h"
#include "M1Label.h"
#include "M1Knob.h"
#include "M1Checkbox.h"

#include <array>
#include <cmath>

using namespace murka;

class M1EQComponent : public murka::View<M1EQComponent>
{
public:
    // Provide processor + DSP pointers (fluent builder pattern for Murka)
    M1EQComponent& withProcessorAndEQ (M1PannerAudioProcessor* p, MultibandEQ* eq)
    {
        processor = p;
        eqPtr     = eq;
        return *this;
    }

    void internalDraw (Murka& m)
    {
        if (processor == nullptr || eqPtr == nullptr)
            return;

        // Theme
        m.setLineWidth(1);
        m.setColor(BACKGROUND_GREY);
        m.drawRectangle(0, 0, shape.size.x, shape.size.y);

        // Plot area (leave a small margin for labels)
        const float L = 38.0f, R = 12.0f, T = 10.0f, B = 22.0f;
        const float W = shape.size.x - (L + R);
        const float H = shape.size.y - (T + B);

        // Axes ranges
        const float fMin = 20.0f, fMax = 20000.0f;
        const float gMin = -24.0f, gMax =  24.0f;

        // Grid
        drawGrid(m, L, T, W, H, fMin, fMax, gMin, gMax);

        // Response curve
        drawResponse(m, L, T, W, H, fMin, fMax, gMin, gMax);

        // Bands (nodes + interactions)
        handleNodes(m, L, T, W, H, fMin, fMax, gMin, gMax);
    }

    // Cursor control functions (passed from parent)
    std::function<void()> cursorHide = [](){};
    std::function<void()> cursorShow = [](){};

private:
    M1PannerAudioProcessor* processor { nullptr };
    MultibandEQ*            eqPtr     { nullptr };

    int   hoverBand     = -1;
    int   draggingBand  = -1;
    bool  freqGainGestureOpen = false;
    bool  qGestureOpen        = false;

    // Mapping helpers (log frequency, linear dB)
    static inline float freqToX (float f, float L, float W, float fMin, float fMax)
    {
        float t = std::log10(juce::jlimit(fMin, fMax, f) / fMin) / std::log10(fMax / fMin);
        return L + t * W;
    }
    static inline float xToFreq (float x, float L, float W, float fMin, float fMax)
    {
        float t = juce::jlimit(0.0f, 1.0f, (x - L) / W);
        return fMin * std::pow(fMax / fMin, t);
    }
    static inline float gainToY (float dB, float T, float H, float gMin, float gMax)
    {
        float t = juce::jmap(juce::jlimit(gMin, gMax, dB), gMin, gMax, 1.0f, 0.0f);
        return T + t * H;
    }
    static inline float yToGain (float y, float T, float H, float gMin, float gMax)
    {
        float t = juce::jlimit(0.0f, 1.0f, (y - T) / H);
        return juce::jmap(1.0f - t, 0.0f, 1.0f, gMin, gMax);
    }

    void drawGrid (Murka& m, float L, float T, float W, float H,
                   float fMin, float fMax, float gMin, float gMax)
    {
        // fine grid
        m.setColor(GRID_LINES_1_RGBA);
        for (int gi = -24; gi <= 24; gi += 3)
        {
            const float y = gainToY((float)gi, T, H, gMin, gMax);
            m.drawLine(L, y, L + W, y);
        }
        // bold 0 dB axis
        m.setColor(GRID_LINES_3_RGBA);
        m.drawLine(L, gainToY(0.0f, T, H, gMin, gMax), L + W, gainToY(0.0f, T, H, gMin, gMax));

        // frequency ticks (log decade markers)
        m.setColor(GRID_LINES_2);
        const int freqMarks[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        for (int f : freqMarks)
        {
            float x = freqToX((float)f, L, W, fMin, fMax);
            m.drawLine(x, T, x, T + H);
        }

        // labels
        m.setColor(REF_LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE - 5);

        // gain labels
        for (int gi = -24; gi <= 24; gi += 6)
        {
            float y = gainToY((float)gi, T, H, gMin, gMax);
            m.prepare<M1Label>({ 2, y - 6, L - 4, 12 }).withTextAlignment(TEXT_RIGHT).text(juce::String(gi).toStdString()).draw();
        }
        // freq labels
        for (int f : freqMarks)
        {
            float x = freqToX((float)f, L, W, fMin, fMax);
            juce::String txt = (f >= 1000) ? juce::String(f/1000.0, 1) + "k" : juce::String(f);
            m.prepare<M1Label>({ x - 16, T + H + 4, 32, 12 }).withTextAlignment(TEXT_CENTER).text(txt.toStdString()).draw();
        }
    }

    void drawResponse (Murka& m, float L, float T, float W, float H,
                       float fMin, float fMax, float gMin, float gMax)
    {
        // sample magnitude
        static constexpr int N = 256;
        std::array<juce::Point<float>, N> pts{};

        for (int i = 0; i < N; ++i)
        {
            float t = (float)i / (float)(N - 1);
            float f = fMin * std::pow(fMax / fMin, t);
            double mag = eqPtr->getMagnitudeForFrequency((double)f);
            float dB  = juce::Decibels::gainToDecibels((float)juce::jmax(1.0e-6, mag));

            pts[(size_t)i].x = freqToX(f, L, W, fMin, fMax);
            pts[(size_t)i].y = gainToY(juce::jlimit(gMin, gMax, dB), T, H, gMin, gMax);
        }

        // draw
        m.setColor(ENABLED_PARAM);
        for (int i = 1; i < N; ++i)
            m.drawLine(pts[(size_t)(i-1)].x, pts[(size_t)(i-1)].y, pts[(size_t)i].x, pts[(size_t)i].y);
    }

    void handleNodes (Murka& m, float L, float T, float W, float H,
                      float fMin, float fMax, float gMin, float gMax)
    {
        auto& vts = processor->getValueTreeState();

        for (int bi = 0; bi < 6; ++bi)
        {
            auto& b = eqPtr->getBand(bi);

            // node position
            float x = freqToX(b.frequency, L, W, fMin, fMax);
            float y = gainToY  (b.gain,     T, H, gMin, gMax);

            // hover / hit test
            bool over = MurkaShape(x - 8, y - 8, 16, 16).inside(mousePosition());
            if (over) hoverBand = bi;
            if (!isHovered()) over = false;

            // color by enabled/type
            if (!b.enabled) m.setColor(DISABLED_PARAM);
            else            m.setColor(M1_ACTION_YELLOW);

            // draw node
            m.enableFill();
            m.drawCircle(x, y, over ? 7.0f : 6.0f);
            m.disableFill();
            m.setColor(GRID_LINES_3_RGBA);
            m.drawCircle(x, y, 10.0f);

            // Q indicator (ring thickness/size hint)
            if (b.enabled)
            {
                m.setColor(GRID_LINES_4_RGB);
                float qWidth = juce::jlimit(3.0f, 18.0f, 18.0f / b.q);
                m.drawCircle(x, y, 10.0f + qWidth * 0.6f);
            }

            // type badge
            m.setColor(REF_LABEL_TEXT_COLOR);
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE - 7);
            m.prepare<M1Label>({ x + 8, y - 6, 40, 12 })
                .withTextAlignment(TEXT_LEFT)
                .text(filterTypeShort(b.type))
                .draw();

            // interactions per-node
            if (mouseDownPressed(0) && over && draggingBand < 0)
            {
                draggingBand = bi;
                // open gestures for freq+gain
                beginBandGesture(vts, bi, true, false);
                cursorHide();
            }

            if (draggingBand == bi && mouseDown(0))
            {
                // modify freq/gain or Q with Alt
                if (isKeyHeld(murka::MurkaKey::MURKA_KEY_ALT))
                {
                    if (!qGestureOpen) beginBandGesture(vts, bi, false, true);
                    float qDelta = -mouseDelta().y * 0.01f;
                    eqPtr->setBandQ(bi, juce::jlimit(0.1f, 10.0f, b.q + qDelta));

                    auto* pQ = vts.getParameter(paramIdQ(bi));
                    pQ->setValueNotifyingHost(pQ->convertTo0to1(eqPtr->getBand(bi).q));
                }
                else
                {
                    if (!freqGainGestureOpen) beginBandGesture(vts, bi, true, false);
                    float newF = xToFreq(mousePosition().x, L, W, fMin, fMax);
                    float newG = yToGain(mousePosition().y, T, H, gMin, gMax);

                    eqPtr->setBandFrequency(bi, newF);
                    eqPtr->setBandGain     (bi, newG);

                    auto* pF = vts.getParameter(paramIdFreq(bi));
                    auto* pG = vts.getParameter(paramIdGain(bi));
                    pF->setValueNotifyingHost(pF->convertTo0to1(eqPtr->getBand(bi).frequency));
                    pG->setValueNotifyingHost(pG->convertTo0to1(eqPtr->getBand(bi).gain));
                }
            }

            if (draggingBand == bi && !mouseDown(0))
            {
                endBandGesture(vts, bi);
                draggingBand = -1;
                cursorShow();
            }

            // double click toggles enabled
            if (doubleClick() && over)
            {
                bool en = !b.enabled;
                eqPtr->setBandEnabled(bi, en);
                if (auto* pE = vts.getParameter(paramIdEnabled(bi)))
                {
                    pE->beginChangeGesture();
                    pE->setValueNotifyingHost(en ? 1.0f : 0.0f);
                    pE->endChangeGesture();
                }
            }

            // right click cycles type
            if (mouseDownPressed(1) && over)
            {
                auto next = static_cast<MultibandEQ::FilterType>((static_cast<int>(b.type) + 1) % 6);
                eqPtr->setBandType(bi, next);
                if (auto* pT = vts.getParameter(paramIdType(bi)))
                {
                    pT->beginChangeGesture();
                    pT->setValueNotifyingHost(pT->convertTo0to1((float) next));
                    pT->endChangeGesture();
                }
            }
        }
    }

    // ---- parameter id helpers (processor exposes these statically) ----
    juce::String paramIdFreq(int i) const
    {
        switch (i)
        {
            case 0: return M1PannerAudioProcessor::paramHeadshadowEQBand1Freq;
            case 1: return M1PannerAudioProcessor::paramHeadshadowEQBand2Freq;
            case 2: return M1PannerAudioProcessor::paramHeadshadowEQBand3Freq;
            case 3: return M1PannerAudioProcessor::paramHeadshadowEQBand4Freq;
            case 4: return M1PannerAudioProcessor::paramHeadshadowEQBand5Freq;
            default:return M1PannerAudioProcessor::paramHeadshadowEQBand6Freq;
        }
    }
    juce::String paramIdGain(int i) const
    {
        switch (i)
        {
            case 0: return M1PannerAudioProcessor::paramHeadshadowEQBand1Gain;
            case 1: return M1PannerAudioProcessor::paramHeadshadowEQBand2Gain;
            case 2: return M1PannerAudioProcessor::paramHeadshadowEQBand3Gain;
            case 3: return M1PannerAudioProcessor::paramHeadshadowEQBand4Gain;
            case 4: return M1PannerAudioProcessor::paramHeadshadowEQBand5Gain;
            default:return M1PannerAudioProcessor::paramHeadshadowEQBand6Gain;
        }
    }
    juce::String paramIdQ(int i) const
    {
        switch (i)
        {
            case 0: return M1PannerAudioProcessor::paramHeadshadowEQBand1Q;
            case 1: return M1PannerAudioProcessor::paramHeadshadowEQBand2Q;
            case 2: return M1PannerAudioProcessor::paramHeadshadowEQBand3Q;
            case 3: return M1PannerAudioProcessor::paramHeadshadowEQBand4Q;
            case 4: return M1PannerAudioProcessor::paramHeadshadowEQBand5Q;
            default:return M1PannerAudioProcessor::paramHeadshadowEQBand6Q;
        }
    }
    juce::String paramIdType(int i) const
    {
        switch (i)
        {
            case 0: return M1PannerAudioProcessor::paramHeadshadowEQBand1Type;
            case 1: return M1PannerAudioProcessor::paramHeadshadowEQBand2Type;
            case 2: return M1PannerAudioProcessor::paramHeadshadowEQBand3Type;
            case 3: return M1PannerAudioProcessor::paramHeadshadowEQBand4Type;
            case 4: return M1PannerAudioProcessor::paramHeadshadowEQBand5Type;
            default:return M1PannerAudioProcessor::paramHeadshadowEQBand6Type;
        }
    }
    juce::String paramIdEnabled(int i) const
    {
        switch (i)
        {
            case 0: return M1PannerAudioProcessor::paramHeadshadowEQBand1Enabled;
            case 1: return M1PannerAudioProcessor::paramHeadshadowEQBand2Enabled;
            case 2: return M1PannerAudioProcessor::paramHeadshadowEQBand3Enabled;
            case 3: return M1PannerAudioProcessor::paramHeadshadowEQBand4Enabled;
            case 4: return M1PannerAudioProcessor::paramHeadshadowEQBand5Enabled;
            default:return M1PannerAudioProcessor::paramHeadshadowEQBand6Enabled;
        }
    }

    // gesture open/close helpers
    void beginBandGesture (juce::AudioProcessorValueTreeState& vts, int i, bool freqGain, bool q)
    {
        if (freqGain && !freqGainGestureOpen)
        {
            if (auto* pF = vts.getParameter(paramIdFreq(i)))  pF->beginChangeGesture();
            if (auto* pG = vts.getParameter(paramIdGain(i)))  pG->beginChangeGesture();
            freqGainGestureOpen = true;
        }
        if (q && !qGestureOpen)
        {
            if (auto* pQ = vts.getParameter(paramIdQ(i)))     pQ->beginChangeGesture();
            qGestureOpen = true;
        }
    }
    void endBandGesture (juce::AudioProcessorValueTreeState& vts, int i)
    {
        if (freqGainGestureOpen)
        {
            if (auto* pF = vts.getParameter(paramIdFreq(i)))  pF->endChangeGesture();
            if (auto* pG = vts.getParameter(paramIdGain(i)))  pG->endChangeGesture();
            freqGainGestureOpen = false;
        }
        if (qGestureOpen)
        {
            if (auto* pQ = vts.getParameter(paramIdQ(i)))     pQ->endChangeGesture();
            qGestureOpen = false;
        }
    }

    static const char* filterTypeShort (MultibandEQ::FilterType t)
    {
        switch (t)
        {
            case MultibandEQ::Bypass:    return "BP";
            case MultibandEQ::HighPass:  return "HP";
            case MultibandEQ::LowShelf:  return "LS";
            case MultibandEQ::Peak:      return "PK";
            case MultibandEQ::HighShelf: return "HS";
            case MultibandEQ::LowPass:   return "LP";
        }
        return "";
    }
};
