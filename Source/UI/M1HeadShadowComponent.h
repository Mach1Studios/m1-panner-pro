#pragma once

#include <JuceHeader.h>

#include "juce_murka/JuceMurkaBaseComponent.h"
#include "MurkaBasicWidgets.h"
#include "MurkaInputEventsRegister.h"
#include "MurkaView.h"

#include "../Config.h"
#include "../PluginProcessor.h"
#include "M1Label.h"
#include "M1Knob.h"
#include "M1Checkbox.h"
#include "M1EQComponent.h"
#include "M1SpectrumView.h"

using namespace murka;

class M1HeadShadowComponent : public murka::View<M1HeadShadowComponent>
{
public:
    M1HeadShadowComponent& withProcessor(M1PannerAudioProcessor* p)
    {
        processor   = p;
        pannerState = &p->pannerSettings;
        return *this;
    }

    // Cursor control hooks (provided by parent)
    std::function<void()> cursorHide = [](){};
    std::function<void()> cursorShow = [](){};
    std::function<void()> cursorShowAndTeleportBack = [](){};

    void internalDraw(Murka& m)
    {
        if (processor == nullptr || pannerState == nullptr)
            return;

        // Panel background
        m.setColor(BACKGROUND_GREY);
        m.enableFill();
        m.drawRectangle(0, 0, shape.size.x, shape.size.y);

        const float margin = 12.0f;
        const float eqTitleH = 22.0f;
        const float controlsRowH = 128.0f;

        // EQ block geometry
        const float eqX = margin;
        const float eqY = 6.0f + eqTitleH; // under title
        const float eqW = shape.size.x - margin*2.0f;
        const float eqH = shape.size.y - eqY - margin - controlsRowH;

        // Inner plot margins (match M1EQComponent)
        const float L = 38.0f, R = 12.0f, T = 10.0f, B = 22.0f;
        const float plotW = eqW - (L + R);
        const float plotH = eqH - (T + B);

        // Titles
        m.setColor(APP_LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
        m.prepare<M1Label>({ margin, 6, 220, 18 })
            .withTextAlignment(TEXT_LEFT)
            .text("HEADSHADOW • EQ")
            .draw();

        // EQ plot & interaction
        auto& eq = m.prepare<M1EQComponent>({ eqX, eqY, eqW, eqH })
                        .withProcessorAndEQ(processor, &processor->headshadowEQ)
                        .withSelectedBandPtr(&selectedBand);
        eq.cursorHide = cursorHide;
        eq.cursorShow = cursorShowAndTeleportBack;
        eq.onBandSelected = [this](int i){ selectedBand = i; };
        eq.draw();

        // Spectrum overlay — place exactly over inner plot area
        auto& spectrum = m.prepare<M1SpectrumView>(MurkaShape(eqX + L, eqY + T, plotW, plotH));
        spectrum.fetchSpectrum = [this](std::vector<float>& output)
        {
            return processor->getCombinedSpectrum(output);
        };
        spectrum.visibleMinDb = -48.0f;   // match EQ grid
        spectrum.visibleMaxDb =  0.0f;   // match EQ grid
        // analyzer produces 0..1 from -100..0 dB -> convert back inside SpectrumView
        spectrum.sourceMinDb  = -100.0f;
        spectrum.sourceMaxDb  =    0.0f;
        spectrum.draw(); // draw AFTER EQ grid so it sits on top

        // ==== Bottom controls ===================================================
        const int knobW = 70;
        const int knobH = 84;
        const int labelOffsetY = 25;

        float y = eqY + eqH; // start under EQ
        m.setColor(APP_LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);

        // Left block: EQ band knobs (selected band)
        m.prepare<M1Label>({ margin, y, 260, 18 })
            .withTextAlignment(TEXT_LEFT)
            .text("HEADSHADOW • EQ CONTROLS")
            .draw();

        y += 44.0f;

        if (selectedBand >= 0 && selectedBand < 6)
        {
            auto& band = processor->headshadowEQ.getBand(selectedBand);

            // --- FREQ knob (20..20k Hz) ---
            auto& freqKnob = m.prepare<M1Knob>({ margin + 4, y, knobW, knobH })
                                 .controlling(&band.frequency);
            freqKnob.rangeFrom = 20.0f;
            freqKnob.rangeTo   = 20000.0f;
            freqKnob.floatingPointPrecision = 0;
            freqKnob.postfix = "Hz";
            freqKnob.cursorHide = cursorHide;
            freqKnob.cursorShow = cursorShowAndTeleportBack;
            freqKnob.draw();

            m.setColor(ENABLED_PARAM);
            auto& fLabel = m.prepare<M1Label>(MurkaShape(margin + 4, y - labelOffsetY, knobW, knobH));
            fLabel.label = "FREQ";
            fLabel.alignment = TEXT_CENTER;
            fLabel.enabled = true;
            fLabel.highlighted = freqKnob.hovered;
            fLabel.draw();

            // --- Q knob (0.1..10) ---
            auto& qKnob = m.prepare<M1Knob>({ margin + 4 + knobW + 8, y, knobW, knobH })
                              .controlling(&band.q);
            qKnob.rangeFrom = 0.10f;
            qKnob.rangeTo   = 10.0f;
            qKnob.floatingPointPrecision = 2;
            qKnob.cursorHide = cursorHide;
            qKnob.cursorShow = cursorShowAndTeleportBack;
            qKnob.draw();

            m.setColor(ENABLED_PARAM);
            auto& qLabel = m.prepare<M1Label>(MurkaShape(margin + 4 + knobW + 8, y - labelOffsetY, knobW, knobH));
            qLabel.label = "Q";
            qLabel.alignment = TEXT_CENTER;
            qLabel.enabled = true;
            qLabel.highlighted = qKnob.hovered;
            qLabel.draw();

            // --- GAIN knob (–24..+24 dB) ---
            auto& gKnob = m.prepare<M1Knob>({ margin + 4 + (knobW + 8) * 2, y, knobW, knobH })
                              .controlling(&band.gain);
            gKnob.rangeFrom = -24.0f;
            gKnob.rangeTo   =  24.0f;
            gKnob.floatingPointPrecision = 1;
            gKnob.postfix = "dB";
            gKnob.prefix = (band.gain > 0.0f ? "+" : "");
            gKnob.cursorHide = cursorHide;
            gKnob.cursorShow = cursorShowAndTeleportBack;
            gKnob.draw();

            m.setColor(ENABLED_PARAM);
            auto& gLabel = m.prepare<M1Label>(MurkaShape(margin + 4 + (knobW + 8) * 2, y - labelOffsetY, knobW, knobH));
            gLabel.label = "GAIN";
            gLabel.alignment = TEXT_CENTER;
            gLabel.enabled = true;
            gLabel.highlighted = gKnob.hovered;
            gLabel.draw();

            // --- update processor parameters with gesture wrapping ---
            auto& vts = processor->getValueTreeState();

            // helpers to get parameter IDs (same mapping used inside M1EQComponent)
            auto paramIdFreq = [&](int i)->juce::String
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
            };
            auto paramIdGain = [&](int i)->juce::String
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
            };
            auto paramIdQ = [&](int i)->juce::String
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
            };

            // Gesture open/close tracking
            static bool freqGesture = false, qGesture = false, gainGesture = false;

            // Freq
            if (freqKnob.changed)
            {
                auto* p = vts.getParameter(paramIdFreq(selectedBand));
                if (freqKnob.draggingNow && !freqGesture) { p->beginChangeGesture(); freqGesture = true; }
                p->setValueNotifyingHost(p->convertTo0to1(band.frequency));
            }
            if (freqGesture && !freqKnob.draggingNow)
            {
                auto* p = vts.getParameter(paramIdFreq(selectedBand));
                p->endChangeGesture(); freqGesture = false;
            }

            // Q
            if (qKnob.changed)
            {
                auto* p = vts.getParameter(paramIdQ(selectedBand));
                if (qKnob.draggingNow && !qGesture) { p->beginChangeGesture(); qGesture = true; }
                p->setValueNotifyingHost(p->convertTo0to1(band.q));
            }
            if (qGesture && !qKnob.draggingNow)
            {
                auto* p = vts.getParameter(paramIdQ(selectedBand));
                p->endChangeGesture(); qGesture = false;
            }

            // Gain
            if (gKnob.changed)
            {
                auto* p = vts.getParameter(paramIdGain(selectedBand));
                if (gKnob.draggingNow && !gainGesture) { p->beginChangeGesture(); gainGesture = true; }
                p->setValueNotifyingHost(p->convertTo0to1(band.gain));
            }
            if (gainGesture && !gKnob.draggingNow)
            {
                auto* p = vts.getParameter(paramIdGain(selectedBand));
                p->endChangeGesture(); gainGesture = false;
            }
        }
        else
        {
            // No selection hint
            m.setColor(REF_LABEL_TEXT_COLOR);
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE - 2);
            m.prepare<M1Label>({ margin + 4, y + 8, 260, 18 })
                .withTextAlignment(TEXT_LEFT)
                .text("Click a band in the EQ to edit FREQ / Q / GAIN")
                .draw();
        }

        // Right block: Microdelay controls
        float rightBlockX = (float)(shape.size.x * 0.62);
        m.setColor(APP_LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
        m.prepare<M1Label>({ rightBlockX, eqY + eqH, 260, 18 })
            .withTextAlignment(TEXT_LEFT)
            .text("HEADSHADOW • MICRODELAY")
            .draw();

        float ky = eqY + eqH + 44.0f;
        rightBlockX += 44.0f;

        // Delay Time µs  (200 .. 1000)
        auto& hsDelay = m.prepare<M1Knob>({ rightBlockX, ky, knobW, knobH })
                            .controlling((float*)&pannerState->headshadowDelayTime);
        hsDelay.rangeFrom = 200.0f;   // min
        hsDelay.rangeTo   = 1000.0f;  // max
        hsDelay.floatingPointPrecision = 0;
        hsDelay.postfix = "µs";
        hsDelay.cursorHide = cursorHide;
        hsDelay.cursorShow = cursorShowAndTeleportBack;
        hsDelay.draw();

        m.setColor(ENABLED_PARAM);
        auto& hsDtLabel = m.prepare<M1Label>(MurkaShape(rightBlockX, ky - labelOffsetY, knobW, knobH));
        hsDtLabel.label = "TIME";
        hsDtLabel.alignment = TEXT_CENTER;
        hsDtLabel.enabled = true;
        hsDtLabel.highlighted = hsDelay.hovered;
        hsDtLabel.draw();

        // Wet Gain dB (–60 .. +6)
        auto& hsWet = m.prepare<M1Knob>({ rightBlockX + knobW + 2, ky, knobW, knobH })
                          .controlling(&pannerState->headshadowWetGain);
        hsWet.rangeFrom = -60.0f;
        hsWet.rangeTo   =   6.0f;
        hsWet.floatingPointPrecision = 1;
        hsWet.postfix = "dB";
        hsWet.cursorHide = cursorHide;
        hsWet.cursorShow = cursorShowAndTeleportBack;
        hsWet.draw();

        auto& hsWgLabel = m.prepare<M1Label>(MurkaShape(rightBlockX + knobW + 2, ky - labelOffsetY, knobW, knobH));
        hsWgLabel.label = "WET GAIN";
        hsWgLabel.alignment = TEXT_CENTER;
        hsWgLabel.enabled = true;
        hsWgLabel.highlighted = hsWet.hovered;
        hsWgLabel.draw();
    }

private:
    M1PannerAudioProcessor* processor   { nullptr };
    PannerSettings*         pannerState { nullptr };

    int selectedBand = 2; // default to a mid peak
};
