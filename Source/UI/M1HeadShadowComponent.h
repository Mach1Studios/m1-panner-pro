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
        float closeY = windowY + 1;
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
        
        // Draw EQ visualization area
        float eqX = windowX + 20;
        float eqY = windowY + 60;
        float eqW = windowW - 40;
        float eqH = windowH - 250;
        drawEQVisualization(m, eqX, eqY, eqW, eqH);
        
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
        
        // Update EQ band parameters
        for (int i = 0; i < 6; ++i)
        {
            std::string bandNum = std::to_string(i + 1);
            
            // Get normalized parameter values and convert to actual ranges
            float freqNorm = params.getParameter(("HeadshadowEQBand" + bandNum + "Freq").c_str())->getValue();
            float gainNorm = params.getParameter(("HeadshadowEQBand" + bandNum + "Gain").c_str())->getValue();
            float qNorm = params.getParameter(("HeadshadowEQBand" + bandNum + "Q").c_str())->getValue();
            float typeNorm = params.getParameter(("HeadshadowEQBand" + bandNum + "Type").c_str())->getValue();
            float enabledNorm = params.getParameter(("HeadshadowEQBand" + bandNum + "Enabled").c_str())->getValue();
            
            // Convert normalized values to actual ranges
            eqBands[i].frequency = 20.0f + freqNorm * (20000.0f - 20.0f);
            eqBands[i].gain = gainNorm * 48.0f - 24.0f; // -24 to +24 dB
            eqBands[i].q = 0.1f + qNorm * (10.0f - 0.1f);
            eqBands[i].type = static_cast<int>(typeNorm * 5.0f);
            eqBands[i].enabled = enabledNorm > 0.5f;
        }
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
    
    // EQ band data
    struct EQBandData
    {
        float frequency = 1000.0f;
        float gain = 0.0f;
        float q = 0.707f;
        int type = 3; // Peak
        bool enabled = false;
    };
    EQBandData eqBands[6];
    
    void drawEQVisualization(Murka& m, float x, float y, float w, float h)
    {
        // Draw EQ background
        m.setColor(25, 25, 25, 255);
        m.drawRectangle(x, y, w, h);
        
        // Draw grid lines
        m.setColor(60, 60, 60, 255);
        m.setLineWidth(1);
        
        // Horizontal grid lines (dB)
        for (int db = -20; db <= 20; db += 5)
        {
            float gridY = y + h/2 - (db / 40.0f) * h;
            m.drawLine(x, gridY, x + w, gridY);
            
            // Draw dB labels
            if (db % 10 == 0)
            {
                m.setColor(120, 120, 120, 255);
                m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 10);
                m.prepare<murka::Label>({x + 5, gridY - 15, 50, 20}).text(std::to_string(db) + "dB").draw();
                m.setColor(60, 60, 60, 255);
            }
        }
        
        // Vertical grid lines (frequency)
        float freqs[] = {100, 1000, 10000};
        for (float freq : freqs)
        {
            float logFreq = std::log10(freq / 20.0f) / std::log10(20000.0f / 20.0f);
            float gridX = x + logFreq * w;
            m.drawLine(gridX, y, gridX, y + h);
            
            // Draw frequency labels
            m.setColor(120, 120, 120, 255);
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 10);
            std::string freqLabel = freq >= 1000 ? std::to_string((int)(freq / 1000)) + "k" : std::to_string((int)freq);
            m.prepare<murka::Label>({gridX - 15, y + h + 5, 30, 20}).text(freqLabel).draw();
            m.setColor(60, 60, 60, 255);
        }
        
        // Draw EQ response curve
        drawEQResponse(m, x, y, w, h);
        
        // Draw EQ band controls
        drawEQBandControls(m, x, y, w, h);
    }
    
    void drawEQResponse(Murka& m, float x, float y, float w, float h)
    {
        // Calculate and draw EQ response curve
        m.setColor(100, 200, 100, 255);
        m.setLineWidth(2);
        
        std::vector<juce::Point<float>> curvePoints;
        
        for (int i = 0; i < w; i += 2)
        {
            float freqRatio = (float)i / w;
            float freq = 20.0f * std::pow(1000.0f, freqRatio); // Log scale from 20Hz to 20kHz
            
            float totalGain = 0.0f;
            
            // Calculate combined response from all enabled bands
            for (int band = 0; band < 6; ++band)
            {
                if (!eqBands[band].enabled) continue;
                
                float bandGain = calculateBandResponse(freq, eqBands[band]);
                totalGain += bandGain;
            }
            
            // Clamp total gain
            totalGain = juce::jlimit(-24.0f, 24.0f, totalGain);
            
            float responseY = y + h/2 - (totalGain / 48.0f) * h;
            curvePoints.push_back({x + i, responseY});
        }
        
        // Draw the curve
        for (size_t i = 1; i < curvePoints.size(); ++i)
        {
            m.drawLine(curvePoints[i-1].x, curvePoints[i-1].y, curvePoints[i].x, curvePoints[i].y);
        }
    }
    
    float calculateBandResponse(float freq, const EQBandData& band)
    {
        // Simple EQ response calculation for visualization
        float ratio = freq / band.frequency;
        
        switch (band.type)
        {
            case 0: // Bypass
                return 0.0f;
                
            case 1: // HighPass
                if (freq < band.frequency)
                    return -12.0f * std::log2(band.frequency / freq); // -12dB/octave rolloff
                return 0.0f;
                
            case 2: // LowShelf
            case 4: // HighShelf
            {
                float transition = (band.type == 2) ? (freq < band.frequency) : (freq > band.frequency);
                return transition ? band.gain * 0.5f : band.gain;
            }
            
            case 3: // Peak
            {
                float distance = std::abs(std::log2(ratio));
                float bandwidth = 1.0f / band.q;
                if (distance < bandwidth)
                    return band.gain * (1.0f - distance / bandwidth);
                return 0.0f;
            }
            
            case 5: // LowPass
                if (freq > band.frequency)
                    return -12.0f * std::log2(freq / band.frequency); // -12dB/octave rolloff
                return 0.0f;
        }
        
        return 0.0f;
    }
    
    void drawEQBandControls(Murka& m, float x, float y, float w, float h)
    {
        // Draw interactive EQ band handles
        for (int band = 0; band < 6; ++band)
        {
            if (!eqBands[band].enabled) continue;
            
            // Calculate position
            float logFreq = std::log10(eqBands[band].frequency / 20.0f) / std::log10(20000.0f / 20.0f);
            float handleX = x + logFreq * w;
            float handleY = y + h/2 - (eqBands[band].gain / 48.0f) * h;
            
            // Draw handle
            bool isHovered = MurkaShape(handleX - 8, handleY - 8, 16, 16).inside(mousePosition());
            
            m.setColor(isHovered ? 255 : 200, isHovered ? 200 : 150, 100, 255);
            m.drawCircle(handleX, handleY, isHovered ? 8 : 6);
            
            // Draw band number
            m.setColor(255, 255, 255, 255);
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 10);
            m.prepare<murka::Label>({handleX - 5, handleY - 5, 10, 10}).text(std::to_string(band + 1)).draw();
            
            // Handle dragging (simplified - would need proper mouse handling)
            if (isHovered && mouseDownPressed(0))
            {
                // Update band parameters based on mouse position
                // This would need proper implementation with parameter updates
            }
        }
    }
    
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
