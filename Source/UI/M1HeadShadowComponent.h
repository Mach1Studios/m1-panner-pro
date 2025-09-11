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
#include "M1DropdownButton.h"
#include "M1DropdownMenu.h"

using namespace murka;

class M1HeadShadowComponent : public murka::View<M1HeadShadowComponent>
{
public:
    M1HeadShadowComponent& withProcessor(M1PannerAudioProcessor* p)
    {
        processor    = p;
        pannerState  = &p->pannerSettings;
        return *this;
    }

    void internalDraw(Murka& m)
    {
        if (processor == nullptr || pannerState == nullptr)
            return;

        // Panel background
        m.setColor(BACKGROUND_GREY);
        m.enableFill();
        m.drawRectangle(0, 0, shape.size.x, shape.size.y);

        // Layout split: EQ top, controls bottom
        const float margin = 12.0f;
        const float controlsRowH = 128.0f;

        const float eqX = margin;
        const float eqY = 28.0f;
        const float eqW = shape.size.x - margin * 3.0f;
        const float eqH = shape.size.y - eqY - margin - controlsRowH;

        // bottom rows start under the EQ
        float y = eqY + eqH;

        // Title
        m.setColor(APP_LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
        m.prepare<M1Label>({ margin, 6, 280, 18 }).withTextAlignment(TEXT_LEFT).text("HEADSHADOW • EQ").draw();

        // EQ area
        auto& eq = m.prepare<M1EQComponent>({ eqX, eqY, eqW, eqH }).withProcessorAndEQ(processor, &processor->headshadowEQ);
        eq.cursorHide = cursorHide;
        eq.cursorShow = cursorShowAndTeleportBack;
        eq.draw();

        // [Spectrum] Combined output spectrum (sum of all output channels)
        // Use the same inner-plot margins as M1EQComponent
        const float L = 38.0f, R = 12.0f, T = 10.0f, B = 22.0f;
        const float W = eqW - (L + R);
        const float H = eqH - (T + B);

        // IMPORTANT: position relative to eqX/eqY so axes line up
        auto& spectrum = m.prepare<M1SpectrumView>(MurkaShape(eqX + L, eqY + T + 2.0f, W, H + 6.0f));
        spectrum.fetchSpectrum = [this](std::vector<float>& output) { return processor->getCombinedSpectrum(output); };
        spectrum.draw();

        // ===== Bottom controls =====

        // ----- EQ Controls (selected band) -----
        m.setColor(APP_LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
        m.prepare<M1Label>({ eqX, y, 320, 18 }).withTextAlignment(TEXT_LEFT).text("HEADSHADOW • EQ CONTROLS").draw();

        y += 44.0f;
        const int knobW = 70, knobH = 84;
        const int labelOffsetY = 25;

        const auto paramIdFreq = [&](int i)->juce::String {
            switch (i) {
                case 0: return M1PannerAudioProcessor::paramHeadshadowEQBand1Freq;
                case 1: return M1PannerAudioProcessor::paramHeadshadowEQBand2Freq;
                case 2: return M1PannerAudioProcessor::paramHeadshadowEQBand3Freq;
                case 3: return M1PannerAudioProcessor::paramHeadshadowEQBand4Freq;
                case 4: return M1PannerAudioProcessor::paramHeadshadowEQBand5Freq;
                default:return M1PannerAudioProcessor::paramHeadshadowEQBand6Freq;
            }
        };
        const auto paramIdGain = [&](int i)->juce::String {
            switch (i) {
                case 0: return M1PannerAudioProcessor::paramHeadshadowEQBand1Gain;
                case 1: return M1PannerAudioProcessor::paramHeadshadowEQBand2Gain;
                case 2: return M1PannerAudioProcessor::paramHeadshadowEQBand3Gain;
                case 3: return M1PannerAudioProcessor::paramHeadshadowEQBand4Gain;
                case 4: return M1PannerAudioProcessor::paramHeadshadowEQBand5Gain;
                default:return M1PannerAudioProcessor::paramHeadshadowEQBand6Gain;
            }
        };
        const auto paramIdQ = [&](int i)->juce::String {
            switch (i) {
                case 0: return M1PannerAudioProcessor::paramHeadshadowEQBand1Q;
                case 1: return M1PannerAudioProcessor::paramHeadshadowEQBand2Q;
                case 2: return M1PannerAudioProcessor::paramHeadshadowEQBand3Q;
                case 3: return M1PannerAudioProcessor::paramHeadshadowEQBand4Q;
                case 4: return M1PannerAudioProcessor::paramHeadshadowEQBand5Q;
                default:return M1PannerAudioProcessor::paramHeadshadowEQBand6Q;
            }
        };
        const auto paramIdType = [&](int i)->juce::String {
            switch (i) {
                case 0: return M1PannerAudioProcessor::paramHeadshadowEQBand1Type;
                case 1: return M1PannerAudioProcessor::paramHeadshadowEQBand2Type;
                case 2: return M1PannerAudioProcessor::paramHeadshadowEQBand3Type;
                case 3: return M1PannerAudioProcessor::paramHeadshadowEQBand4Type;
                case 4: return M1PannerAudioProcessor::paramHeadshadowEQBand5Type;
                default:return M1PannerAudioProcessor::paramHeadshadowEQBand6Type;
            }
        };

        const int sel = eq.getSelectedBandIndex(); // new in M1EQComponent
        if (sel >= 0)
        {
            auto& selBand = processor->headshadowEQ.getBand(sel);

            // Frequency
            auto& kFreq = m.prepare<M1Knob>({ eqX +  4, y, knobW, knobH }).controlling(&selBand.frequency);
            kFreq.rangeFrom = 20.0f;  kFreq.rangeTo = 20000.0f;
            kFreq.floatingPointPrecision = 0; kFreq.postfix = "Hz"; kFreq.speed = 250;
            kFreq.cursorHide = cursorHide; kFreq.cursorShow = cursorShowAndTeleportBack;
            kFreq.draw();

            m.setColor(ENABLED_PARAM);
            auto& lFreq = m.prepare<M1Label>({ eqX +  4, y - labelOffsetY, knobW, knobH });
            lFreq.label = "FREQ"; lFreq.alignment = TEXT_CENTER; lFreq.enabled = true; lFreq.highlighted = kFreq.hovered; lFreq.draw();

            // Q
            auto& kQ = m.prepare<M1Knob>({ eqX +  4 + knobW + 6, y, knobW, knobH }).controlling(&selBand.q);
            kQ.rangeFrom = 0.1f;  kQ.rangeTo = 10.0f; kQ.floatingPointPrecision = 2; kQ.speed = 250;
            kQ.cursorHide = cursorHide; kQ.cursorShow = cursorShowAndTeleportBack;
            kQ.draw();

            auto& lQ = m.prepare<M1Label>({ eqX +  4 + knobW + 6, y - labelOffsetY, knobW, knobH });
            lQ.label = "Q"; lQ.alignment = TEXT_CENTER; lQ.enabled = true; lQ.highlighted = kQ.hovered; lQ.draw();

            // Gain
            auto& kGain = m.prepare<M1Knob>({ eqX +  4 + 2*(knobW + 6), y, knobW, knobH }).controlling(&selBand.gain);
            kGain.rangeFrom = -24.0f;  kGain.rangeTo = 24.0f; kGain.floatingPointPrecision = 1; kGain.postfix = "dB"; kGain.speed = 250;
            kGain.cursorHide = cursorHide; kGain.cursorShow = cursorShowAndTeleportBack;
            kGain.draw();

            auto& lGain = m.prepare<M1Label>({ eqX +  4 + 2*(knobW + 6), y - labelOffsetY, knobW, knobH });
            lGain.label = "GAIN"; lGain.alignment = TEXT_CENTER; lGain.enabled = true; lGain.highlighted = kGain.hovered; lGain.draw();

            // Filter Type Dropdown - aligned with knob labels
            const int dropdownW = 80;
            const int dropdownH = 20;
            const int dropdownX = eqX + 4 + 3*(knobW + 6);
            const int dropdownY = y + knobH - dropdownH - 8; // Position to align with knob bottom area

            // Filter type label - same height as other knob labels
            auto& lType = m.prepare<M1Label>({ dropdownX, y - labelOffsetY, dropdownW, knobH });
            lType.label = "TYPE"; lType.alignment = TEXT_LEFT; lType.enabled = true; lType.highlighted = false; lType.draw();

            // Helper function to get filter type name
            auto getFilterTypeName = [](MultibandEQ::FilterType type) -> std::string {
                switch (type) {
                    case MultibandEQ::Bypass:    return "BYPASS";
                    case MultibandEQ::HighPass:  return "HIPASS";
                    case MultibandEQ::LowShelf:  return "LOSHELF";
                    case MultibandEQ::Peak:      return "PEAK";
                    case MultibandEQ::HighShelf: return "HISHELF";
                    case MultibandEQ::LowPass:   return "LOPASS";
                    default: return "PEAK";
                }
            };

            auto& typeDropdownButton = m.prepare<M1DropdownButton>({ dropdownX, y, dropdownW, dropdownH })
                .withLabel(getFilterTypeName(selBand.type))
                .withOutline(true)
                .withTriangle(true)
                .withFontSize(DEFAULT_FONT_SIZE - 2)
                .withOutlineColor(MurkaColor(ENABLED_PARAM));
            typeDropdownButton.draw();

            // Filter type dropdown menu
            std::vector<std::string> filterTypeOptions = { "BYPASS", "HIPASS", "LOSHELF", "PEAK", "HISHELF", "LOPASS" };
            const int dropdownItemHeight = 18;
            auto& typeDropdownMenu = m.prepare<M1DropdownMenu>({ dropdownX, 
                                                                 y - filterTypeOptions.size() * dropdownItemHeight,
                                                                 dropdownW,
                                                                 filterTypeOptions.size() * dropdownItemHeight })
                .withOptions(filterTypeOptions);

            if (typeDropdownButton.pressed) {
                typeDropdownMenu.open();
                typeDropdownMenu.selectedOption = static_cast<int>(selBand.type);
            }

            typeDropdownMenu.optionHeight = dropdownItemHeight;
            typeDropdownMenu.fontSize = DEFAULT_FONT_SIZE - 2;
            typeDropdownMenu.draw();

            // Active checkbox - positioned under the dropdown
            const int checkboxY = y + dropdownH + 14 + labelOffsetY;
            auto& activeCheckbox = m.prepare<M1Checkbox>({ dropdownX, checkboxY, dropdownW, 20 })
                .controlling(&selBand.enabled)
                .withLabel("ACTIVE")
                .withFontSize(DEFAULT_FONT_SIZE - 2);
            activeCheckbox.draw();

            // Band selection indicator - moved to the right
            int bandIndicatorX = eqX + 230;
            m.setColor(APP_LABEL_TEXT_COLOR);
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
            auto bandText = " • BAND ";
            m.prepare<M1Label>({ bandIndicatorX, y - labelOffsetY - 20, 200, 18 }).withTextAlignment(TEXT_LEFT).text(bandText).draw();
            bandIndicatorX += 65;
            m.setColor(ENABLED_PARAM);
            auto band_selText = std::to_string(sel + 1);
            m.prepare<M1Label>({ bandIndicatorX, y - labelOffsetY - 20, 200, 18 }).withTextAlignment(TEXT_LEFT).text(band_selText).draw();

            // Push to host parameters when changed (preserves automation)
            bool typeChanged = false;
            if (typeDropdownMenu.changed) {
                auto newType = static_cast<MultibandEQ::FilterType>(typeDropdownMenu.selectedOption);
                if (newType != selBand.type) {
                    selBand.type = newType;
                    processor->headshadowEQ.setBandType(sel, newType);
                    typeChanged = true;
                }
            }

            // Handle active checkbox changes
            if (activeCheckbox.changed) {
                processor->headshadowEQ.setBandEnabled(sel, selBand.enabled);
            }

            if (kFreq.changed || kQ.changed || kGain.changed || typeChanged)
            {
                auto& vts = processor->getValueTreeState();

                if (kFreq.changed) {
                    if (auto* p = vts.getParameter(paramIdFreq(sel)))  p->setValueNotifyingHost(p->convertTo0to1(selBand.frequency));
                }
                if (kQ.changed) {
                    if (auto* p = vts.getParameter(paramIdQ(sel)))     p->setValueNotifyingHost(p->convertTo0to1(selBand.q));
                }
                if (kGain.changed) {
                    if (auto* p = vts.getParameter(paramIdGain(sel)))  p->setValueNotifyingHost(p->convertTo0to1(selBand.gain));
                }
                if (typeChanged) {
                    if (auto* p = vts.getParameter(paramIdType(sel)))  p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(typeDropdownMenu.selectedOption)));
                }
            }
        }

        // ----- Delay / Wet cluster -----
        float delayColX = (float)(shape.size.x * 0.62f) + 44.0f;

        m.setColor(APP_LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
        m.prepare<M1Label>({ delayColX - 44.0f, eqY + eqH, 260, 18 }).withTextAlignment(TEXT_LEFT).text("HEADSHADOW • MICRODELAY").draw();

        // Delay
        auto& kDelay = m.prepare<M1Knob>({ delayColX, y, knobW, knobH }).controlling((float*)&pannerState->headshadowDelayTime);
        kDelay.rangeFrom = 200.0f;  kDelay.rangeTo = 1000.0f; // fixed: ascending
        kDelay.floatingPointPrecision = 0; kDelay.postfix = "µs"; kDelay.speed = 250;
        kDelay.cursorHide = cursorHide; kDelay.cursorShow = cursorShowAndTeleportBack;
        kDelay.draw();

        m.setColor(ENABLED_PARAM);
        auto& lDelay = m.prepare<M1Label>({ delayColX, y - labelOffsetY, knobW, knobH });
        lDelay.label = "TIME"; lDelay.alignment = TEXT_CENTER; lDelay.enabled = true; lDelay.highlighted = kDelay.hovered; lDelay.draw();

        // Wet Gain
        auto& kWet = m.prepare<M1Knob>({ delayColX + knobW + 6, y, knobW, knobH }).controlling(&pannerState->headshadowWetGain);
        kWet.rangeFrom = -60.0f;  kWet.rangeTo = 6.0f;         // fixed: ascending
        kWet.floatingPointPrecision = 1; kWet.postfix = "dB"; kWet.speed = 250;
        kWet.cursorHide = cursorHide; kWet.cursorShow = cursorShowAndTeleportBack;
        kWet.draw();

        auto& lWet = m.prepare<M1Label>({ delayColX + knobW + 6, y - labelOffsetY, knobW, knobH });
        lWet.label = "WET GAIN"; lWet.alignment = TEXT_CENTER; lWet.enabled = true; lWet.highlighted = kWet.hovered; lWet.draw();
    }

    // Cursor control functions (passed from parent)
    std::function<void()> cursorHide = [](){};
    std::function<void()> cursorShow = [](){};
    std::function<void()> cursorShowAndTeleportBack = [](){};

private:
    M1PannerAudioProcessor* processor   { nullptr };
    PannerSettings*         pannerState { nullptr };
};
