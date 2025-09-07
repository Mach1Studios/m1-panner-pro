#pragma once

#include <JuceHeader.h>
#include <functional>
#include "juce_murka/JuceMurkaBaseComponent.h"
#include "MurkaBasicWidgets.h"
#include "MurkaInputEventsRegister.h"
#include "MurkaView.h"
#include "../Config.h"
#include "../MultibandEQ.h"
#include "M1Knob.h"
#include "M1Label.h"

using namespace murka;

//==============================================================================
/**
 * Professional EQ Component with interactive frequency response visualization
 * Features draggable handles, real-time curve updates, and comprehensive controls
 */
class M1EQComponent : public murka::View<M1EQComponent>
{
public:
    struct EQBandUI
    {
        float frequency = 1000.0f;
        float gain = 0.0f;
        float q = 0.707f;
        int type = 3; // Peak
        bool enabled = false;
        bool dragging = false;
        bool hovered = false;
        juce::Colour color = juce::Colours::orange;
        
        // UI state
        juce::Point<float> handlePosition;
        float handleRadius = 8.0f;
    };
    
    M1EQComponent()
    {
        // Initialize band colors
        bandColors[0] = juce::Colour(255, 100, 100); // Red
        bandColors[1] = juce::Colour(255, 165, 0);   // Orange  
        bandColors[2] = juce::Colour(255, 255, 100); // Yellow
        bandColors[3] = juce::Colour(100, 255, 100); // Green
        bandColors[4] = juce::Colour(100, 200, 255); // Blue
        bandColors[5] = juce::Colour(200, 100, 255); // Purple
        
        // Initialize band UI data
        for (int i = 0; i < 6; ++i)
        {
            bands[i].color = bandColors[i];
        }
        
        // Set default band configurations
        bands[0] = {80.0f, 0.0f, 0.707f, 1, false, false, false, bandColors[0]};    // HPF
        bands[1] = {200.0f, 0.0f, 0.707f, 2, false, false, false, bandColors[1]};   // Low shelf
        bands[2] = {800.0f, 0.0f, 1.0f, 3, false, false, false, bandColors[2]};     // Low mid
        bands[3] = {3200.0f, 0.0f, 1.0f, 3, false, false, false, bandColors[3]};    // High mid
        bands[4] = {8000.0f, 0.0f, 0.707f, 4, false, false, false, bandColors[4]};  // High shelf
        bands[5] = {12000.0f, 0.0f, 0.707f, 5, false, false, false, bandColors[5]}; // LPF
    }
    
    void internalDraw(Murka& m)
    {
        if (!isActive) return;
        
        // Update band data from processor if available
        updateBandDataFromProcessor();
        
        // Draw main EQ visualization area
        drawEQBackground(m);
        drawFrequencyGrid(m);
        drawGainGrid(m);
        drawFrequencyResponse(m);
        drawBandHandles(m);
        
        // Handle mouse interactions
        handleMouseInteraction(m);
        
        // Draw band controls panel
        drawBandControlsPanel(m);
    }
    
    void setMultibandEQ(MultibandEQ* eq) { multibandEQ = eq; }
    void setActive(bool active) { isActive = active; }
    
    // Public member variables
    bool isActive = true;
    MultibandEQ* multibandEQ = nullptr;
    
    // Cursor control functions (passed from parent)
    std::function<void()> cursorHide = [](){};
    std::function<void()> cursorShow = [](){};
    std::function<void()> cursorShowAndTeleportBack = [](){};
    
private:
    EQBandUI bands[6];
    juce::Colour bandColors[6];
    
    // UI layout
    float eqAreaX = 20.0f;
    float eqAreaY = 20.0f;
    float eqAreaWidth = 0.0f;
    float eqAreaHeight = 0.0f;
    
    // Interaction state
    int draggedBand = -1;
    juce::Point<float> dragStartPos;
    float dragStartFreq = 0.0f;
    float dragStartGain = 0.0f;
    
    // Frequency and gain ranges
    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;
    static constexpr float minGain = -24.0f;
    static constexpr float maxGain = 24.0f;
    
    void updateBandDataFromProcessor()
    {
        if (!multibandEQ) return;
        
        for (int i = 0; i < 6; ++i)
        {
            const auto& band = multibandEQ->getBand(i);
            bands[i].frequency = band.frequency;
            bands[i].gain = band.gain;
            bands[i].q = band.q;
            bands[i].type = static_cast<int>(band.type);
            bands[i].enabled = band.enabled;
        }
    }
    
    void drawEQBackground(Murka& m)
    {
        // Calculate EQ area dimensions
        eqAreaX = 20.0f;
        eqAreaY = 20.0f;
        eqAreaWidth = shape.size.x - 40.0f;
        eqAreaHeight = shape.size.y * 0.7f; // 70% for EQ, 30% for controls
        
        // Draw background
        m.setColor(25, 25, 25, 255);
        m.drawRectangle(eqAreaX, eqAreaY, eqAreaWidth, eqAreaHeight);
        
        // Draw border
        m.setColor(60, 60, 60, 255);
        m.setLineWidth(1);
        m.drawRectangle(eqAreaX-1, eqAreaY-1, eqAreaWidth+2, eqAreaHeight+2);
    }
    
    void drawFrequencyGrid(Murka& m)
    {
        m.setLineWidth(1);
        
        // Major frequency lines
        float majorFreqs[] = {50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
        for (float freq : majorFreqs)
        {
            float x = frequencyToX(freq);
            bool isMajor = (freq == 100 || freq == 1000 || freq == 10000);
            
            m.setColor(isMajor ? 60 : 40, isMajor ? 60 : 40, isMajor ? 60 : 40, 255);
            m.drawLine(x, eqAreaY, x, eqAreaY + eqAreaHeight);
            
            // Draw frequency labels
            if (isMajor)
            {
                m.setColor(120, 120, 120, 255);
                m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 10);
                std::string freqLabel = freq >= 1000 ? 
                    std::to_string((int)(freq / 1000)) + "k" : 
                    std::to_string((int)freq);
                m.prepare<murka::Label>({x - 15, eqAreaY + eqAreaHeight + 5, 30, 15})
                    .text(freqLabel).draw();
            }
        }
    }
    
    void drawGainGrid(Murka& m)
    {
        m.setLineWidth(1);
        
        // Horizontal grid lines (dB)
        for (int db = -20; db <= 20; db += 5)
        {
            float y = gainToY(db);
            bool isMajor = (db % 10 == 0);
            
            m.setColor(isMajor ? 60 : 40, isMajor ? 60 : 40, isMajor ? 60 : 40, 255);
            m.drawLine(eqAreaX, y, eqAreaX + eqAreaWidth, y);
            
            // Draw dB labels
            if (isMajor)
            {
                m.setColor(120, 120, 120, 255);
                m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 10);
                std::string dbLabel = (db > 0 ? "+" : "") + std::to_string(db) + "dB";
                m.prepare<murka::Label>({eqAreaX + 5, y - 8, 50, 16})
                    .text(dbLabel).draw();
            }
        }
        
        // Draw 0dB line more prominently
        float zeroY = gainToY(0.0f);
        m.setColor(80, 80, 80, 255);
        m.setLineWidth(2);
        m.drawLine(eqAreaX, zeroY, eqAreaX + eqAreaWidth, zeroY);
        m.setLineWidth(1);
    }
    
    void drawFrequencyResponse(Murka& m)
    {
        if (!multibandEQ) return;
        
        m.setLineWidth(2);
        
        // Draw individual band responses
        for (int band = 0; band < 6; ++band)
        {
            if (!bands[band].enabled) continue;
            
            drawBandResponse(m, band);
        }
        
        // Draw combined response
        drawCombinedResponse(m);
    }
    
    void drawBandResponse(Murka& m, int bandIndex)
    {
        const auto& band = bands[bandIndex];
        
        // Set band color with transparency
        auto color = band.color;
        m.setColor(color.getRed(), color.getGreen(), color.getBlue(), 100);
        
        std::vector<juce::Point<float>> points;
        
        // Calculate response curve
        for (float x = eqAreaX; x < eqAreaX + eqAreaWidth; x += 2.0f)
        {
            float freq = xToFrequency(x);
            float response = calculateBandResponse(freq, band);
            float y = gainToY(response);
            points.push_back({x, y});
        }
        
        // Draw the curve
        for (size_t i = 1; i < points.size(); ++i)
        {
            m.drawLine(points[i-1].x, points[i-1].y, points[i].x, points[i].y);
        }
    }
    
    void drawCombinedResponse(Murka& m)
    {
        m.setColor(150, 255, 150, 255); // Bright green for combined response
        m.setLineWidth(3);
        
        std::vector<juce::Point<float>> points;
        
        // Calculate combined response
        for (float x = eqAreaX; x < eqAreaX + eqAreaWidth; x += 1.0f)
        {
            float freq = xToFrequency(x);
            float totalResponse = 0.0f;
            
            // Sum all enabled band responses
            for (int band = 0; band < 6; ++band)
            {
                if (bands[band].enabled)
                {
                    totalResponse += calculateBandResponse(freq, bands[band]);
                }
            }
            
            // Clamp total response
            totalResponse = juce::jlimit(minGain, maxGain, totalResponse);
            float y = gainToY(totalResponse);
            points.push_back({x, y});
        }
        
        // Draw the combined curve
        for (size_t i = 1; i < points.size(); ++i)
        {
            m.drawLine(points[i-1].x, points[i-1].y, points[i].x, points[i].y);
        }
        
        m.setLineWidth(1); // Reset line width
    }
    
    void drawBandHandles(Murka& m)
    {
        for (int i = 0; i < 6; ++i)
        {
            if (!bands[i].enabled) continue;
            
            drawBandHandle(m, i);
        }
    }
    
    void drawBandHandle(Murka& m, int bandIndex)
    {
        auto& band = bands[bandIndex];
        
        // Calculate handle position
        float x = frequencyToX(band.frequency);
        float y = gainToY(band.gain);
        band.handlePosition = {x, y};
        
        // Check if mouse is over handle
        auto mousePos = mousePosition();
        float distance = std::sqrt(std::pow(mousePos.x - x, 2) + std::pow(mousePos.y - y, 2));
        band.hovered = distance <= band.handleRadius + 4;
        
        // Draw handle
        auto color = band.color;
        float radius = band.hovered || band.dragging ? band.handleRadius + 2 : band.handleRadius;
        
        // Draw handle shadow
        m.setColor(0, 0, 0, 100);
        m.drawCircle(x + 1, y + 1, radius);
        
        // Draw handle background
        m.setColor(color.getRed(), color.getGreen(), color.getBlue(), 200);
        m.drawCircle(x, y, radius);
        
        // Draw handle border
        m.setColor(255, 255, 255, band.hovered ? 255 : 150);
        m.setLineWidth(2);
        m.disableFill();
        m.drawCircle(x, y, radius);
        m.enableFill();
        m.setLineWidth(1);
        
        // Draw band number
        m.setColor(255, 255, 255, 255);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 10);
        std::string bandNum = std::to_string(bandIndex + 1);
        m.prepare<murka::Label>({x - 5, y - 5, 10, 10}).text(bandNum).draw();
        
        // Draw Q visualization (width of influence)
        if (band.type == 3 && (band.hovered || band.dragging)) // Peak filter
        {
            drawQVisualization(m, bandIndex);
        }
    }
    
    void drawQVisualization(Murka& m, int bandIndex)
    {
        const auto& band = bands[bandIndex];
        
        // Calculate bandwidth based on Q
        float bandwidth = band.frequency / band.q;
        float lowFreq = band.frequency - bandwidth / 2;
        float highFreq = band.frequency + bandwidth / 2;
        
        float lowX = frequencyToX(lowFreq);
        float highX = frequencyToX(highFreq);
        float centerY = gainToY(band.gain);
        
        // Draw Q bandwidth indicator
        auto color = band.color;
        m.setColor(color.getRed(), color.getGreen(), color.getBlue(), 50);
        m.drawRectangle(lowX, centerY - 20, highX - lowX, 40);
        
        // Draw bandwidth lines
        m.setColor(color.getRed(), color.getGreen(), color.getBlue(), 150);
        m.setLineWidth(1);
        m.drawLine(lowX, eqAreaY, lowX, eqAreaY + eqAreaHeight);
        m.drawLine(highX, eqAreaY, highX, eqAreaY + eqAreaHeight);
    }
    
    void handleMouseInteraction(Murka& m)
    {
        juce::Point<float> mousePos = { mousePosition().x, mousePosition().y };
        bool mousePressed = mouseDownPressed(0);
        bool mouseDragging = mouseDragged(0);
        
        // Handle dragging
        if (draggedBand >= 0 && mouseDragging)
        {
            handleBandDrag(mousePos);
        }
        else if (mousePressed && draggedBand < 0)
        {
            // Check if clicking on a handle
            for (int i = 0; i < 6; ++i)
            {
                if (bands[i].enabled && bands[i].hovered)
                {
                    startBandDrag(i, mousePos);
                    break;
                }
            }
        }
        else if (!mousePressed && draggedBand >= 0)
        {
            // End drag
            endBandDrag();
        }
        
        // Update cursor
        bool overHandle = false;
        for (int i = 0; i < 6; ++i)
        {
            if (bands[i].enabled && bands[i].hovered)
            {
                overHandle = true;
                break;
            }
        }
        
        if (overHandle && !mouseDragging)
        {
            // Show resize cursor when over handle
        }
    }
    
    void startBandDrag(int bandIndex, juce::Point<float> mousePos)
    {
        draggedBand = bandIndex;
        dragStartPos = mousePos;
        dragStartFreq = bands[bandIndex].frequency;
        dragStartGain = bands[bandIndex].gain;
        bands[bandIndex].dragging = true;
        
        if (cursorHide)
            cursorHide();
    }
    
    void handleBandDrag(juce::Point<float> mousePos)
    {
        if (draggedBand < 0) return;
        
        auto& band = bands[draggedBand];
        
        // Calculate new frequency and gain
        float newFreq = xToFrequency(mousePos.x);
        float newGain = yToGain(mousePos.y);
        
        // Constrain values
        newFreq = juce::jlimit(minFreq, maxFreq, newFreq);
        newGain = juce::jlimit(minGain, maxGain, newGain);
        
        // Update band
        band.frequency = newFreq;
        band.gain = newGain;
        
        // Update processor if available
        if (multibandEQ)
        {
            multibandEQ->setBandFrequency(draggedBand, newFreq);
            multibandEQ->setBandGain(draggedBand, newGain);
        }
    }
    
    void endBandDrag()
    {
        if (draggedBand >= 0)
        {
            bands[draggedBand].dragging = false;
            draggedBand = -1;
            
            if (cursorShowAndTeleportBack)
                cursorShowAndTeleportBack();
        }
    }
    
    void drawBandControlsPanel(Murka& m)
    {
        float panelY = eqAreaY + eqAreaHeight + 30;
        float panelHeight = shape.size.y - panelY - 20;
        
        // Draw panel background
        m.setColor(35, 35, 35, 255);
        m.drawRectangle(eqAreaX, panelY, eqAreaWidth, panelHeight);
        
        // Draw band controls in a grid
        float bandWidth = eqAreaWidth / 6;
        
        for (int i = 0; i < 6; ++i)
        {
            drawBandControls(m, i, eqAreaX + i * bandWidth, panelY, bandWidth, panelHeight);
        }
    }
    
    void drawBandControls(Murka& m, int bandIndex, float x, float y, float w, float h)
    {
        auto& band = bands[bandIndex];
        auto color = band.color;
        
        // Draw band separator
        if (bandIndex > 0)
        {
            m.setColor(60, 60, 60, 255);
            m.drawLine(x, y, x, y + h);
        }
        
        // Draw band header
        m.setColor(color.getRed(), color.getGreen(), color.getBlue(), 255);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 12);
        std::string bandName = "Band " + std::to_string(bandIndex + 1);
        m.prepare<murka::Label>({x + 5, y + 5, w - 10, 20}).text(bandName).draw();
        
        // Draw enable/disable toggle
        float toggleY = y + 25;
        bool toggleHovered = MurkaShape(x + 5, toggleY, 15, 15).inside(mousePosition());
        
        m.setColor(band.enabled ? color.getRed() : 60, 
                   band.enabled ? color.getGreen() : 60, 
                   band.enabled ? color.getBlue() : 60, 255);
        m.drawRectangle(x + 5, toggleY, 15, 15);
        
        if (toggleHovered && mouseDownPressed(0))
        {
            band.enabled = !band.enabled;
            if (multibandEQ)
                multibandEQ->setBandEnabled(bandIndex, band.enabled);
        }
        
        // Draw parameter values
        m.setColor(200, 200, 200, 255);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 9);
        
        std::string freqText = std::to_string((int)band.frequency) + "Hz";
        std::string gainText = (band.gain >= 0 ? "+" : "") + 
                              std::to_string(band.gain).substr(0, 4) + "dB";
        std::string qText = "Q:" + std::to_string(band.q).substr(0, 4);
        
        m.prepare<murka::Label>({x + 5, y + 45, w - 10, 12}).text(freqText).draw();
        m.prepare<murka::Label>({x + 5, y + 58, w - 10, 12}).text(gainText).draw();
        m.prepare<murka::Label>({x + 5, y + 71, w - 10, 12}).text(qText).draw();
    }
    
    // Utility functions for coordinate conversion
    float frequencyToX(float frequency) const
    {
        float logFreq = std::log(frequency / minFreq) / std::log(maxFreq / minFreq);
        return eqAreaX + logFreq * eqAreaWidth;
    }
    
    float xToFrequency(float x) const
    {
        float ratio = (x - eqAreaX) / eqAreaWidth;
        return minFreq * std::pow(maxFreq / minFreq, ratio);
    }
    
    float gainToY(float gain) const
    {
        float ratio = (gain - minGain) / (maxGain - minGain);
        return eqAreaY + eqAreaHeight - (ratio * eqAreaHeight);
    }
    
    float yToGain(float y) const
    {
        float ratio = (eqAreaY + eqAreaHeight - y) / eqAreaHeight;
        return minGain + ratio * (maxGain - minGain);
    }
    
    float calculateBandResponse(float freq, const EQBandUI& band) const
    {
        float ratio = freq / band.frequency;
        
        switch (band.type)
        {
            case 0: // Bypass
                return 0.0f;
                
            case 1: // HighPass
                if (freq < band.frequency)
                    return -12.0f * std::log2(band.frequency / freq);
                return 0.0f;
                
            case 2: // LowShelf
            case 4: // HighShelf
            {
                bool isLow = (band.type == 2);
                float transition = isLow ? (freq < band.frequency) : (freq > band.frequency);
                return transition ? band.gain * 0.5f : band.gain;
            }
            
            case 3: // Peak
            {
                float octaves = std::log2(ratio);
                float bandwidth = 1.0f / band.q;
                float response = 1.0f / (1.0f + std::pow(octaves / bandwidth, 2.0f));
                return band.gain * response;
            }
            
            case 5: // LowPass
                if (freq > band.frequency)
                    return -12.0f * std::log2(freq / band.frequency);
                return 0.0f;
        }
        
        return 0.0f;
    }
};
