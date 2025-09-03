#pragma once

#include <JuceHeader.h>
#include "Config.h"

//==============================================================================
/**
 * Manages product unlocking and feature licensing for M1 Panner Pro
 * Handles both trial mode and full license validation
 */
class ProductUnlockManager
{
public:
    //==============================================================================
    enum class FeatureLevel
    {
        Trial = 0,      // Time limited pro features
        Standard,       // Standard features
        Pro            // All features unlocked
    };

    enum class UnlockableFeature
    {
        ITDProcessing // Inter-aural Time Delay processing
    };

    //==============================================================================
    ProductUnlockManager();
    ~ProductUnlockManager();

    // Initialization
    void initialize();
    
    // License management
    bool isFeatureUnlocked(UnlockableFeature feature) const;
    FeatureLevel getCurrentFeatureLevel() const { return currentFeatureLevel; }
    
    // Trial management
    bool isTrialMode() const { return currentFeatureLevel == FeatureLevel::Trial; }
    int getTrialDaysRemaining() const;
    bool isTrialExpired() const;
    
    // License key management
    bool attemptUnlockWithKey(const juce::String& licenseKey);
    juce::String getLicenseKeyForCurrentUser() const;
    void clearLicenseKey();
    
    // Status queries
    bool isFullyUnlocked() const { return currentFeatureLevel == FeatureLevel::Pro; }
    juce::String getStatusMessage() const;
    juce::String getUnlockInstructions() const;
    
    // Callbacks for UI updates
    std::function<void(FeatureLevel)> onFeatureLevelChanged;
    std::function<void(const juce::String&)> onStatusMessageChanged;

    // License validation callback
    void onLicenseValidationComplete(bool success, const juce::String& message);

private:
    //==============================================================================
    void updateFeatureLevel();
    void saveUnlockStatus();
    void loadUnlockStatus();
    bool validateLicenseKey(const juce::String& key) const;
    void startTrialPeriod();
    
    // License validation helpers
    juce::String generateMachineID() const;
    juce::String encryptLicenseData(const juce::String& data) const;
    juce::String decryptLicenseData(const juce::String& encryptedData) const;
    
    //==============================================================================
    // Note: OnlineUnlockStatus removed due to API compatibility issues
    FeatureLevel currentFeatureLevel = FeatureLevel::Trial;
    
    // Trial management
    juce::Time trialStartTime;
    static constexpr int trialDurationDays = 30;
    
    // License storage
    juce::String currentLicenseKey;
    bool licenseValidated = false;
    
    // Product identification
    static constexpr const char* productName = "M1-Panner-Pro";
    static constexpr const char* companyName = "Mach1";
    static constexpr const char* productVersion = "2.0";
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProductUnlockManager)
};

//==============================================================================
/**
 * Feature gate helper class for easy feature checking
 */
class FeatureGate
{
public:
    static bool isUnlocked(ProductUnlockManager::UnlockableFeature feature);
    static void showFeatureLockedDialog(ProductUnlockManager::UnlockableFeature feature, 
                                       juce::Component* parentComponent = nullptr);
    
private:
    static ProductUnlockManager* getManager();
};
