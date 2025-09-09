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

using namespace murka;

class M1HeadShadowComponent : public murka::View<M1HeadShadowComponent>
{
public:
    M1HeadShadowComponent& withProcessor (M1PannerAudioProcessor* p)
    {
        processor    = p;
        pannerState  = &p->pannerSettings;
        return *this;
    }

    void internalDraw (Murka& m)
    {
        if (processor == nullptr || pannerState == nullptr)
            return;

        // Panel background
        m.setColor(BACKGROUND_GREY);
        m.enableFill();
        m.drawRectangle(0, 0, shape.size.x, shape.size.y);

        // Title
        m.setColor(APP_LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
        m.prepare<M1Label>({ 10, 6, 220, 18 }).withTextAlignment(TEXT_LEFT).text("HEADSHADOW • EQ").draw();

        // Layout split: EQ top, controls bottom
        const float margin = 12.0f;
        const float rightW = 160;
        const float controls_row_H = 128;
        const float eqX = margin, eqY = 28.0f, eqW = shape.size.x - margin*3, eqH = shape.size.y - eqY - margin - controls_row_H;
        // Controls row
        const float colX = margin;
        const float delay_colX = (float)(shape.size.x*0.65) + margin;
        float y = eqH + margin*3 + eqY;
        
        // EQ area
        auto& eq = m.prepare<M1EQComponent>({ eqX, eqY, eqW, eqH }).withProcessorAndEQ(processor, &processor->headshadowEQ);
        eq.cursorHide = cursorHide;
        eq.cursorShow = cursorShow;
        eq.draw();

        // Delay µs
        m.setColor(APP_LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
        m.prepare<M1Label>({ delay_colX, y - 18, rightW, 16 }).withTextAlignment(TEXT_LEFT).text("HEADSHADOW • MICRODELAY").draw();

        auto& delayKnob = m.prepare<M1Knob>({ delay_colX, y, 70, 84 }).controlling((float*)&pannerState->headshadowDelayTime);
        delayKnob.rangeFrom = 1000.0f; // max
        delayKnob.rangeTo   = 200.0f;  // min (knob vertical sense)
        delayKnob.floatingPointPrecision = 0;
        delayKnob.postfix    = "µs";
        delayKnob.speed      = 350.0f;
        delayKnob.defaultValue = 600.0f;
        delayKnob.cursorHide = cursorHide;
        delayKnob.cursorShow = cursorShow;
        delayKnob.draw();

        if (delayKnob.changed)
        {
            auto& vts = processor->getValueTreeState();
            if (auto* p = vts.getParameter(M1PannerAudioProcessor::paramHeadshadowDelayTime))
                p->setValueNotifyingHost(p->convertTo0to1((float)pannerState->headshadowDelayTime));
        }

        // Wet Gain dB
        auto& wetKnob = m.prepare<M1Knob>({ delay_colX + 72, y, 70, 84 }).controlling(&pannerState->headshadowWetGain);
        wetKnob.rangeFrom = 6.0f;
        wetKnob.rangeTo   = -60.0f;
        wetKnob.floatingPointPrecision = 1;
        wetKnob.postfix    = " dB";
        wetKnob.speed      = 300.0f;
        wetKnob.defaultValue = -18.0f;
        wetKnob.cursorHide = cursorHide;
        wetKnob.cursorShow = cursorShow;
        wetKnob.draw();

        if (wetKnob.changed)
        {
            auto& vts = processor->getValueTreeState();
            if (auto* p = vts.getParameter(M1PannerAudioProcessor::paramHeadshadowWetGain))
                p->setValueNotifyingHost(p->convertTo0to1(pannerState->headshadowWetGain));
        }
    }

    // Cursor helpers (like other Murka widgets in project)
    std::function<void()> cursorHide = [&]() { /* optional: no-cursor while dragging */ };
    std::function<void()> cursorShow = [&]() { /* restore cursor */ };

private:
    M1PannerAudioProcessor* processor   { nullptr };
    PannerSettings*         pannerState { nullptr };
};
