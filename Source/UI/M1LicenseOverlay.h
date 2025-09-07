#pragma once

#include "MurkaBasicWidgets.h"
#include "MurkaView.h"
#include "../Config.h"
#include "../ProductUnlockManager.h"

using namespace murka;

//==============================================================================
/**
 * License management overlay component - similar to M1AlertComponent
 * Displays as a modal overlay for license key entry and status
 */
class M1LicenseOverlay : public murka::View<M1LicenseOverlay>
{
public:
    void internalDraw(Murka& m)
    {
        if (!overlayActive || !productUnlockManager) return;

        // Dark overlay background
        m.setColor(40, 40, 40, 200);
        m.drawRectangle(0, 0, shape.size.x, shape.size.y);

        // License panel background
        const auto centerX = shape.size.x * 0.5f;
        const auto centerY = shape.size.y * 0.5f;

        // Panel dimensions
        const float panelWidth = 450;
        const float panelHeight = getCurrentPanelHeight();

        // Panel background with outline
        m.setColor(ENABLED_PARAM);
        m.drawRectangle(centerX - panelWidth * 0.5f,
                        centerY - panelHeight * 0.5f,
                        panelWidth,
                        panelHeight);
        
        // Inner background
        m.setColor(BACKGROUND_GREY);
        m.drawRectangle(centerX - panelWidth * 0.5f + 1,
                        centerY - panelHeight * 0.5f + 1,
                        panelWidth - 2,
                        panelHeight - 2);

        // Current Y position for drawing elements
        float currentY = centerY - panelHeight * 0.5f + 20;

        // Title
        m.setColor(LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 22);
        m.prepare<murka::Label>({
            centerX - panelWidth * 0.5f + 20,
            currentY,
            panelWidth - 40,
            30
        }).withAlignment(TEXT_CENTER).text("M1-Panner Pro License").draw();
        
        currentY += 40;

        // Status indicator and text
        drawStatusSection(m, centerX, currentY, panelWidth);
        currentY += 60;

        // Trial info (if in trial mode)
        if (productUnlockManager->isTrialMode())
        {
            drawTrialInfo(m, centerX, currentY, panelWidth);
            currentY += 30;
        }

        // License key entry section
        if (!productUnlockManager->isFullyUnlocked())
        {
            drawLicenseKeyEntry(m, centerX, currentY, panelWidth);
            currentY += 80;
        }

        // Buttons
        drawButtons(m, centerX, currentY, panelWidth);
    }

private:
    float getCurrentPanelHeight() const
    {
        float baseHeight = 200;
        if (productUnlockManager && productUnlockManager->isTrialMode()) baseHeight += 30;
        if (productUnlockManager && !productUnlockManager->isFullyUnlocked()) baseHeight += 80;
        return baseHeight;
    }

    void drawStatusSection(Murka& m, float centerX, float currentY, float panelWidth)
    {
        if (!productUnlockManager) return;

        // Status indicator circle
        float indicatorSize = 5;
        float indicatorX = centerX - panelWidth * 0.5f + 30;
        
        juce::Colour indicatorColour;
        switch (productUnlockManager->getCurrentFeatureLevel())
        {
            case ProductUnlockManager::FeatureLevel::Trial:
                indicatorColour = productUnlockManager->isTrialExpired() ? 
                    juce::Colours::red : juce::Colours::orange;
                break;
            case ProductUnlockManager::FeatureLevel::Standard:
                indicatorColour = juce::Colours::yellow;
                break;
            case ProductUnlockManager::FeatureLevel::Pro:
                indicatorColour = juce::Colours::green;
                break;
        }
        
        m.setColor(indicatorColour.getRed(), indicatorColour.getGreen(), 
                   indicatorColour.getBlue(), indicatorColour.getAlpha());
        m.drawCircle(indicatorX, currentY + indicatorSize/2, indicatorSize);

        // Status text
        m.setColor(LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 16);
        juce::String statusText = productUnlockManager->getStatusMessage();
        m.prepare<murka::Label>({
            indicatorX + indicatorSize + 10,
            currentY,
            panelWidth - 80,
            25
        }).withAlignment(TEXT_LEFT).text(statusText.toStdString()).draw();
    }

    void drawTrialInfo(Murka& m, float centerX, float currentY, float panelWidth)
    {
        if (!productUnlockManager) return;

        int daysLeft = productUnlockManager->getTrialDaysRemaining();
        juce::String trialText;
        juce::Colour textColour;
        
        if (daysLeft > 0)
        {
            trialText = "Trial: " + juce::String(daysLeft) + " day" + (daysLeft != 1 ? "s" : "") + " remaining";
            textColour = juce::Colours::orange;
        }
        else
        {
            trialText = "Trial expired - Limited functionality";
            textColour = juce::Colours::red;
        }

        m.setColor(textColour.getRed(), textColour.getGreen(), textColour.getBlue());
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 14);
        m.prepare<murka::Label>({
            centerX - panelWidth * 0.5f + 20,
            currentY,
            panelWidth - 40,
            20
        }).withAlignment(TEXT_CENTER).text(trialText.toStdString()).draw();
    }

    void drawLicenseKeyEntry(Murka& m, float centerX, float currentY, float panelWidth)
    {
        // Instructions
        m.setColor(LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 14);
        m.prepare<murka::Label>({
            centerX - panelWidth * 0.5f + 20,
            currentY,
            panelWidth - 40,
            20
        }).withAlignment(TEXT_CENTER).text("Enter your license key:").draw();

        currentY += 25;

        // License key input field
        float inputWidth = panelWidth - 80;
        float inputHeight = 30;
        float inputX = centerX - inputWidth * 0.5f;

        // Input field border
        m.setColor(ENABLED_PARAM);
        m.drawRectangle(inputX - 1, currentY - 1, inputWidth + 2, inputHeight + 2);
        
        // Input field background
        m.setColor(DISABLED_PARAM);
        m.drawRectangle(inputX, currentY, inputWidth, inputHeight);

        // For now, show a simple instruction to use test keys
        m.setColor(REF_LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 12);
        
        m.prepare<murka::Label>({
            inputX + 10,
            currentY + 5,
            inputWidth - 20,
            20
        }).withAlignment(TEXT_CENTER).text("Use test keys: MACH1-TEST2-PROLI-CENSE-89012").draw();

        // Set a default test key for now
        if (licenseKeyText.isEmpty())
        {
            licenseKeyText = "MACH1-TEST2-PROLI-CENSE-89012";
        }

        currentY += 35;

        // Status message
        if (!statusMessage.isEmpty())
        {
            juce::Colour msgColour = statusMessageIsError ? juce::Colours::red : juce::Colours::green;
            m.setColor(msgColour.getRed(), msgColour.getGreen(), msgColour.getBlue());
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 12);
            m.prepare<murka::Label>({
                centerX - panelWidth * 0.5f + 20,
                currentY,
                panelWidth - 40,
                15
            }).withAlignment(TEXT_CENTER).text(statusMessage.toStdString()).draw();
        }
    }

    void drawButtons(Murka& m, float centerX, float currentY, float panelWidth)
    {
        float buttonWidth = 100;
        float buttonHeight = 30;
        float buttonSpacing = 20;

        if (productUnlockManager && productUnlockManager->isFullyUnlocked())
        {
            // Only show "Close" buttons for fully licensed users
        }
        else
        {
            // Show "Unlock" buttons for trial/unlicensed users
            bool unlockEnabled = !licenseKeyText.isEmpty() && 
                                licenseKeyText.startsWith("MACH1-") && 
                                licenseKeyText.length() >= 29;

            drawButton(m, centerX - buttonWidth - buttonSpacing/2, currentY, buttonWidth, buttonHeight,
                      "Unlock", unlockButtonHovered, [this]() { attemptUnlock(); }, unlockEnabled);
        }

        // Close button (always present)
        drawButton(m, centerX + panelWidth * 0.5f - buttonWidth - 20, currentY, buttonWidth, buttonHeight,
                  "Close", closeButtonHovered, [this]() { 
                      overlayActive = false;
                      if (onDismiss) onDismiss();
                  });
    }

    void drawButton(Murka& m, float x, float y, float width, float height, 
                   const std::string& text, bool& hovered, std::function<void()> onClick, bool enabled = true)
    {
        // Check if mouse is over this button
        bool isHovering = isPointInside(x, y, width, height);
        hovered = isHovering && enabled;

        // Button background
        if (enabled)
        {
            m.setColor(hovered ? ENABLED_PARAM : DISABLED_PARAM);
        }
        else
        {
            m.setColor(GRID_LINES_4_RGB);
        }
        m.drawRectangle(x, y, width, height);

        // Button text
        if (enabled)
        {
            m.setColor(hovered ? BACKGROUND_GREY : LABEL_TEXT_COLOR);
        }
        else
        {
            m.setColor(REF_LABEL_TEXT_COLOR);
        }
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 14);
        m.prepare<murka::Label>({
            x + 5,
            y + (height - 15) * 0.5f,
            width - 10,
            15
        }).withAlignment(TEXT_CENTER).text(text).draw();

        // Handle click
        if (hovered && mouseDownPressed(0) && enabled && onClick)
        {
            onClick();
        }
    }

    bool isPointInside(float x, float y, float width, float height)
    {
        auto mousePos = mousePosition();
        return mousePos.x >= x && mousePos.x <= x + width &&
               mousePos.y >= y && mousePos.y <= y + height;
    }

    void handleKeyboardInput(Murka& m)
    {
        // Simplified keyboard input - for now just handle basic text input
        // In a full implementation, you'd want proper text input handling
        
        // For now, we'll rely on the mouse interaction for the input field
        // and provide a simple placeholder implementation
    }

    void validateLicenseKeyFormat()
    {
        licenseKeyText = licenseKeyText.toUpperCase();
        
        if (licenseKeyText.isEmpty())
        {
            statusMessage = "";
            statusMessageIsError = false;
        }
        else if (!licenseKeyText.startsWith("MACH1-") || licenseKeyText.length() < 29)
        {
            statusMessage = "Format: MACH1-XXXXX-XXXXX-XXXXX-XXXXX";
            statusMessageIsError = true;
        }
        else
        {
            statusMessage = "";
            statusMessageIsError = false;
        }
    }

    void attemptUnlock()
    {
        if (!productUnlockManager || licenseKeyText.isEmpty())
        {
            statusMessage = "Please enter a license key";
            statusMessageIsError = true;
            return;
        }

        statusMessage = "Validating license...";
        statusMessageIsError = false;

        // Attempt to unlock
        if (productUnlockManager->attemptUnlockWithKey(licenseKeyText))
        {
            statusMessage = "License activated successfully!";
            statusMessageIsError = false;
            
            // Close overlay after a short delay
            unlockSuccessful = true;
            successTimer = 60; // ~1 second at 60fps
        }
        else
        {
            statusMessage = "Invalid license key. Please check and try again.";
            statusMessageIsError = true;
        }
    }

public:
    // Public interface
    // Component state - managed externally like alert system
    ProductUnlockManager* productUnlockManager = nullptr;
    bool overlayActive = false;
    std::function<void()> onDismiss;

    // Update method to handle success timer
    void update()
    {
        if (unlockSuccessful && successTimer > 0)
        {
            successTimer--;
            if (successTimer <= 0)
            {
                overlayActive = false;
                if (onDismiss) onDismiss();
                unlockSuccessful = false;
            }
        }
    }

private:
    // State variables
    juce::String licenseKeyText;
    juce::String statusMessage;
    bool statusMessageIsError = false;
    bool inputFieldActive = false;
    bool unlockSuccessful = false;
    int successTimer = 0;

    // Button hover states
    bool unlockButtonHovered = false;
    bool purchaseButtonHovered = false;
    bool closeButtonHovered = false;
};
