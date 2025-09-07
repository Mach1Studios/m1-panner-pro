#pragma once

#include <JuceHeader.h>

#include "juce_murka/JuceMurkaBaseComponent.h"
#include "MurkaBasicWidgets.h"
#include "MurkaInputEventsRegister.h"
#include "MurkaView.h"
#include "../Config.h"
#include "../PluginProcessor.h"
#include "../MultibandEQ.h"
#include "M1Knob.h"
#include "M1Label.h"
#include "M1EQComponent.h"

using namespace murka;

//==============================================================================
/**
 * Modal window component for Headshadow processing controls
 * Features EQ visualization and delay controls
 */
class M1HeadShadowComponent : public murka::View<M1HeadShadowComponent>
{
    PannerSettings* pannerState = nullptr;

public:
    void internalDraw(Murka& m)
    {
        if (!isActive) return;
        
        // Safety check - ensure pannerState is initialized
        if (!pannerState && processor) {
            pannerState = &processor->pannerSettings;
        }
        
        // If still no pannerState, can't draw knobs safely
        if (!pannerState) return;
        
        // Update parameters from processor
        updateParameters();

        // Draw main window background
        m.pushStyle();
        float windowX = 5;
        float windowY = 5;
        float windowW = shape.size.x - 10;
        float windowH = shape.size.y - 10;

        // Border
        m.setColor(ENABLED_PARAM);
        m.drawRectangle(windowX, windowY, windowW, windowW);
        
        // Background
        m.setColor(BACKGROUND_GREY);
        m.drawRectangle(windowX+1, windowY+1, windowW - 2, windowW - 2);
        
        // Draw title bar
        m.setColor(GRID_LINES_2);
        m.drawRectangle(windowX+2, windowY+2, windowW-4, 20);
        
        // Draw title text
        m.setColor(ENABLED_PARAM);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 14);
        m.prepare<murka::Label>({windowX + 15, windowY + 5, 200, 20}).text("HEADSHADOW PROCESSING").draw();
        
        // Draw close button (X)
        float closeX = windowX + windowW - 19;
        float closeY = windowY + 2;
        float closeSize = 18;
        
        bool closeHovered = MurkaShape(closeX, closeY, closeSize, closeSize).inside(mousePosition());
        m.setColor(closeHovered ? 200 : 150, closeHovered ? 100 : 100, closeHovered ? 100 : 100, 255);
        m.drawRectangle(closeX, closeY, closeSize, closeSize);
        
        m.setColor(255, 255, 255, 255);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 18);
        m.prepare<murka::Label>({closeX, closeY-1, 15, 15}).text("×").draw();
        
        // Handle close button click
        if (closeHovered && mouseDownPressed(0))
        {
            // Close the modal by setting the UI state to false
            if (pannerState)
            {
                pannerState->showHeadshadowUI = false;
            }
            shouldClose = true;
        }
        
        // Draw EQ component
        float eqX = windowX + 20;
        float eqY = windowY + 60;
        float eqW = windowW - 40;
        float eqH = windowH - 250;
        
        auto& eqComponent = m.prepare<M1EQComponent>(MurkaShape(eqX, eqY, eqW, eqH));
        eqComponent.setActive(true);
        eqComponent.setMultibandEQ(&processor->headshadowEQ);
        eqComponent.cursorHide = cursorHide;
        eqComponent.cursorShow = cursorShow;
        eqComponent.cursorShowAndTeleportBack = cursorShowAndTeleportBack;
        eqComponent.draw();
        
        // Draw delay controls at bottom
        float controlsY = eqY + eqH + 20;
        drawDelayControls(m, windowX + 20, controlsY, windowW - 40, 150);
        
        m.popStyle();
    }
    
    void updateParameters()
    {
        if (!processor) return;
        
        auto& params = processor->getValueTreeState();
        
        // Update knob parameters
        delayTime = params.getParameter(processor->paramHeadshadowDelayTime)->getValue() * 10000.0f; // Convert from normalized
        wetGain = params.getParameter(processor->paramHeadshadowWetGain)->getValue() * 66.0f - 60.0f; // Convert from normalized (-60 to +6)
        
        // EQ parameters are now handled by the M1EQComponent directly
    }
    
    // Public interface methods
    void setActive(bool active) { isActive = active; }
    void setProcessor(M1PannerAudioProcessor* p) { 
        processor = p; 
        if (processor) {
            pannerState = &processor->pannerSettings;
        }
    }
    
    // Public member variables
    bool isActive = true;
    bool shouldClose = false;
    M1PannerAudioProcessor* processor = nullptr;
    
    // Cursor control functions (passed from parent)
    std::function<void()> cursorHide = [](){};
    std::function<void()> cursorShow = [](){};
    std::function<void()> cursorShowAndTeleportBack = [](){};
    
private:
    
    // Delay control values
    float delayTime = 1000.0f; // microseconds
    float wetGain = -12.0f;    // dB
    
    
    
    
    
    
    void drawDelayControls(Murka& m, float x, float y, float w, float h)
    {
        // Draw delay controls background
        m.setColor(BACKGROUND_GREY);
        m.drawRectangle(x, y, w, h);
        
        // Draw knobs using M1Knob-style rendering
        float knobX = shape.size.x - 200;
        float knobY = y + 50;
        int knobWidth = 70;
        int knobHeight = 87;
        int labelOffsetY = 25;
        m.setColor(ENABLED_PARAM);
        
        // Draw section title
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 14);
        m.prepare<murka::Label>({knobX, y + 5, 150, 30}).text("Delay Controls").draw();
        
        // Delay Time knob
        auto& hs_delay_time_knob = m.prepare<M1Knob>({ knobX, knobY, knobWidth, knobHeight }).controlling(&pannerState->headshadowDelayTime);
        hs_delay_time_knob.rangeFrom = 200;
        hs_delay_time_knob.rangeTo = 1000;
        hs_delay_time_knob.floatingPointPrecision = 0;
        hs_delay_time_knob.postfix = "μs";
        hs_delay_time_knob.speed = 250;
        hs_delay_time_knob.cursorHide = cursorHide;
        hs_delay_time_knob.cursorShow = cursorShowAndTeleportBack;
        hs_delay_time_knob.draw();
        
        auto& hs_dt_Label = m.prepare<M1Label>(MurkaShape(knobX, knobY - labelOffsetY, knobWidth, knobHeight));
        hs_dt_Label.label = "TIME";
        hs_dt_Label.alignment = TEXT_CENTER;
        hs_dt_Label.enabled = true;
        hs_dt_Label.highlighted = hs_delay_time_knob.hovered;
        hs_dt_Label.draw();
        
        // Wet Gain knob
        auto& hs_wet_gain_knob = m.prepare<M1Knob>({ knobX + 90, knobY, knobWidth, knobHeight }).controlling(&pannerState->headshadowWetGain);
        hs_wet_gain_knob.rangeFrom = -60;
        hs_wet_gain_knob.rangeTo = 6;
        hs_wet_gain_knob.floatingPointPrecision = 1;
        hs_wet_gain_knob.postfix = "dB";
        hs_wet_gain_knob.speed = 250;
        hs_wet_gain_knob.cursorHide = cursorHide;
        hs_wet_gain_knob.cursorShow = cursorShowAndTeleportBack;
        hs_wet_gain_knob.draw();
        
        auto& hs_wg_Label = m.prepare<M1Label>(MurkaShape(knobX + 90, knobY - labelOffsetY, knobWidth, knobHeight));
        hs_wg_Label.label = "WET GAIN";
        hs_wg_Label.alignment = TEXT_CENTER;
        hs_wg_Label.enabled = true;
        hs_wg_Label.highlighted = hs_wet_gain_knob.hovered;
        hs_wg_Label.draw();
    }
};
