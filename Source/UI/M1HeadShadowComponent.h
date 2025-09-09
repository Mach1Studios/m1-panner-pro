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
        int knobWidth = 70;
        int knobHeight = 84;
        int labelOffsetY = 25;
        
        // EQ area
        auto& eq = m.prepare<M1EQComponent>({ eqX, eqY, eqW, eqH }).withProcessorAndEQ(processor, &processor->headshadowEQ);
        eq.cursorHide = cursorHide;
        eq.cursorShow = cursorShowAndTeleportBack;
        eq.draw();

        // [Spectrum] Combined output spectrum (sum of all output channels)
        auto& spectrum = m.prepare<M1SpectrumView>(MurkaShape(eqX, eqY, eqW, eqH));
        spectrum.fetchSpectrum = [this](std::vector<float>& out) {
            return processor->getCombinedSpectrum(out);
        };
        spectrum.draw();

        // Delay µs
        m.setColor(APP_LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
        m.prepare<M1Label>({ delay_colX, y - 18, rightW, 16 }).withTextAlignment(TEXT_LEFT).text("HEADSHADOW • MICRODELAY").draw();

        auto& hs_delay_time_knob = m.prepare<M1Knob>({ delay_colX, y, knobWidth, knobHeight }).controlling((float*)&pannerState->headshadowDelayTime);
        hs_delay_time_knob.rangeTo   = 200.0f;  // min
        hs_delay_time_knob.rangeFrom = 1000.0f; // max
        hs_delay_time_knob.floatingPointPrecision = 0;
        hs_delay_time_knob.postfix    = "µs";
        hs_delay_time_knob.speed      = 250;
        hs_delay_time_knob.defaultValue = 600.0f;
        hs_delay_time_knob.cursorHide = cursorHide;
        hs_delay_time_knob.cursorShow = cursorShowAndTeleportBack;
        hs_delay_time_knob.draw();
        
        auto& hs_dt_Label = m.prepare<M1Label>(MurkaShape(delay_colX, y - labelOffsetY, knobWidth, knobHeight));
        hs_dt_Label.label = "TIME";
        hs_dt_Label.alignment = TEXT_CENTER;
        hs_dt_Label.enabled = true;
        hs_dt_Label.highlighted = hs_delay_time_knob.hovered;
        hs_dt_Label.draw();

        // Wet Gain dB
        auto& hs_wet_gain_knob = m.prepare<M1Knob>({ delay_colX + knobWidth + 2, y, knobWidth, knobHeight }).controlling(&pannerState->headshadowWetGain);
        hs_wet_gain_knob.rangeTo   = -60.0f;
        hs_wet_gain_knob.rangeFrom = 6.0f;
        hs_wet_gain_knob.floatingPointPrecision = 1;
        hs_wet_gain_knob.postfix    = "dB";
        hs_wet_gain_knob.speed      = 250;
        hs_wet_gain_knob.defaultValue = -18.0f;
        hs_wet_gain_knob.cursorHide = cursorHide;
        hs_wet_gain_knob.cursorShow = cursorShowAndTeleportBack;
        hs_wet_gain_knob.draw();
        
        auto& hs_wg_Label = m.prepare<M1Label>(MurkaShape(delay_colX + knobWidth + 2, y - labelOffsetY, knobWidth, knobHeight));
        hs_wg_Label.label = "WET GAIN";
        hs_wg_Label.alignment = TEXT_CENTER;
        hs_wg_Label.enabled = true;
        hs_wg_Label.highlighted = hs_wet_gain_knob.hovered;
        hs_wg_Label.draw();
    }

    // Cursor helpers (like other Murka widgets in project)
    std::function<void()> cursorHide = [&]() { /* optional: no-cursor while dragging */ };
    std::function<void()> cursorShow = [&]() { /* restore cursor */ };
    std::function<void()> cursorShowAndTeleportBack = [](){};

private:
    M1PannerAudioProcessor* processor   { nullptr };
    PannerSettings*         pannerState { nullptr };
};
