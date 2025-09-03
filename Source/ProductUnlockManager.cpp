#include "ProductUnlockManager.h"
#include "AlertData.h"

//==============================================================================
ProductUnlockManager::ProductUnlockManager()
{
    // Initialize the product unlock manager
}

ProductUnlockManager::~ProductUnlockManager()
{
    // Cleanup
}

void ProductUnlockManager::initialize()
{
    loadUnlockStatus();
    
    // If no license found, start trial period
    if (!licenseValidated && trialStartTime == juce::Time())
    {
        startTrialPeriod();
    }
    
    updateFeatureLevel();
}

bool ProductUnlockManager::isFeatureUnlocked(UnlockableFeature feature) const
{
    switch (feature)
    {
        case UnlockableFeature::ITDProcessing:
            return currentFeatureLevel >= FeatureLevel::Pro;
            
        default:
            return false;
    }
}

int ProductUnlockManager::getTrialDaysRemaining() const
{
    if (licenseValidated || trialStartTime == juce::Time())
        return -1; // Not in trial mode
        
    auto elapsed = juce::Time::getCurrentTime() - trialStartTime;
    int daysElapsed = (int)(elapsed.inDays());
    return juce::jmax(0, trialDurationDays - daysElapsed);
}

bool ProductUnlockManager::isTrialExpired() const
{
    if (licenseValidated)
        return false;
        
    return getTrialDaysRemaining() <= 0;
}

bool ProductUnlockManager::attemptUnlockWithKey(const juce::String& licenseKey)
{
    if (validateLicenseKey(licenseKey))
    {
        currentLicenseKey = licenseKey;
        licenseValidated = true;
        updateFeatureLevel();
        saveUnlockStatus();
        
        if (onStatusMessageChanged)
            onStatusMessageChanged("License activated successfully!");
            
        return true;
    }
    
    if (onStatusMessageChanged)
        onStatusMessageChanged("Invalid license key. Please check and try again.");
        
    return false;
}

juce::String ProductUnlockManager::getLicenseKeyForCurrentUser() const
{
    return currentLicenseKey;
}

void ProductUnlockManager::clearLicenseKey()
{
    currentLicenseKey.clear();
    licenseValidated = false;
    updateFeatureLevel();
    saveUnlockStatus();
    
    if (onStatusMessageChanged)
        onStatusMessageChanged("License cleared. Running in trial mode.");
}

juce::String ProductUnlockManager::getStatusMessage() const
{
    if (licenseValidated)
    {
        return "Licensed to: " + juce::SystemStats::getFullUserName() + " (Full Version)";
    }
    else if (isTrialExpired())
    {
        return "Trial period expired. Please purchase a license to continue using all features.";
    }
    else
    {
        int daysLeft = getTrialDaysRemaining();
        return "Trial mode: " + juce::String(daysLeft) + " day" + (daysLeft != 1 ? "s" : "") + " remaining";
    }
}

juce::String ProductUnlockManager::getUnlockInstructions() const
{
    return "To unlock the full version of " + juce::String(productName) + 
           ", please enter your license key or visit our website to purchase a license.";
}

void ProductUnlockManager::onLicenseValidationComplete(bool success, const juce::String& message)
{
    if (success)
    {
        licenseValidated = true;
        updateFeatureLevel();
        if (onStatusMessageChanged)
            onStatusMessageChanged("License validation successful!");
    }
    else
    {
        if (onStatusMessageChanged)
            onStatusMessageChanged(message.isEmpty() ? "License validation failed." : message);
    }
}

void ProductUnlockManager::updateFeatureLevel()
{
    FeatureLevel newLevel = FeatureLevel::Trial;
    
    if (licenseValidated)
    {
        newLevel = FeatureLevel::Pro; // For now, all valid licenses unlock Pro features
    }
    else if (!isTrialExpired())
    {
        newLevel = FeatureLevel::Trial;
    }
    
    if (newLevel != currentFeatureLevel)
    {
        currentFeatureLevel = newLevel;
        if (onFeatureLevelChanged)
            onFeatureLevelChanged(currentFeatureLevel);
    }
}

void ProductUnlockManager::saveUnlockStatus()
{
    juce::PropertiesFile::Options options;
    options.applicationName = productName;
    options.folderName = companyName;
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    
    auto properties = std::make_unique<juce::PropertiesFile>(options);
    
    if (licenseValidated && !currentLicenseKey.isEmpty())
    {
        // Encrypt the license key before storing
        juce::String encryptedKey = encryptLicenseData(currentLicenseKey);
        properties->setValue("licenseKey", encryptedKey);
        properties->setValue("licenseValidated", licenseValidated);
    }
    else
    {
        properties->removeValue("licenseKey");
        properties->setValue("licenseValidated", false);
    }
    
    if (trialStartTime != juce::Time())
    {
        properties->setValue("trialStartTime", (juce::int64)trialStartTime.toMilliseconds());
    }
    
    properties->saveIfNeeded();
}

void ProductUnlockManager::loadUnlockStatus()
{
    juce::PropertiesFile::Options options;
    options.applicationName = productName;
    options.folderName = companyName;
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    
    auto properties = std::make_unique<juce::PropertiesFile>(options);
    
    // Load license information
    if (properties->containsKey("licenseKey"))
    {
        juce::String encryptedKey = properties->getValue("licenseKey");
        currentLicenseKey = decryptLicenseData(encryptedKey);
        licenseValidated = properties->getBoolValue("licenseValidated", false);
        
        // Re-validate the license key
        if (!currentLicenseKey.isEmpty() && !validateLicenseKey(currentLicenseKey))
        {
            currentLicenseKey.clear();
            licenseValidated = false;
        }
    }
    
    // Load trial information
    if (properties->containsKey("trialStartTime"))
    {
        juce::int64 startTimeMs = properties->getIntValue("trialStartTime", 0);
        if (startTimeMs > 0)
            trialStartTime = juce::Time(startTimeMs);
    }
}

bool ProductUnlockManager::validateLicenseKey(const juce::String& key) const
{
    if (key.isEmpty())
        return false;
    
    // Simple validation - in production, you'd want more sophisticated validation
    // This could include:
    // - Checksum validation
    // - Machine ID binding
    // - Online validation
    // - Cryptographic signatures
    
    // For now, implement a simple format check
    // Expected format: MACH1-XXXXX-XXXXX-XXXXX-XXXXX
    if (!key.startsWith("MACH1-"))
        return false;
        
    auto parts = juce::StringArray::fromTokens(key, "-", "");
    if (parts.size() != 5)
        return false;
        
    // Check each part has correct length
    for (int i = 1; i < parts.size(); ++i)
    {
        if (parts[i].length() != 5)
            return false;
    }
    
    // Additional validation could include:
    // - Machine ID check
    // - Online server validation
    // - Cryptographic signature verification
    
    return true;
}

void ProductUnlockManager::startTrialPeriod()
{
    trialStartTime = juce::Time::getCurrentTime();
    saveUnlockStatus();
}

juce::String ProductUnlockManager::generateMachineID() const
{
    // Generate a unique machine identifier
    juce::String machineInfo = juce::SystemStats::getComputerName() + 
                              juce::SystemStats::getOperatingSystemName() +
                              juce::SystemStats::getCpuVendor();
    
    return juce::String::toHexString(machineInfo.hashCode64());
}

juce::String ProductUnlockManager::encryptLicenseData(const juce::String& data) const
{
    // Simple XOR encryption - in production use proper encryption
    juce::String key = generateMachineID();
    juce::String result;
    
    for (int i = 0; i < data.length(); ++i)
    {
        char encrypted = data[i] ^ key[i % key.length()];
        result += juce::String::toHexString((int)(unsigned char)encrypted).paddedLeft('0', 2);
    }
    
    return result;
}

juce::String ProductUnlockManager::decryptLicenseData(const juce::String& encryptedData) const
{
    // Decrypt the XOR encrypted data
    juce::String key = generateMachineID();
    juce::String result;
    
    for (int i = 0; i < encryptedData.length(); i += 2)
    {
        juce::String hexByte = encryptedData.substring(i, i + 2);
        int byteValue = hexByte.getHexValue32();
        char decrypted = (char)byteValue ^ key[(i/2) % key.length()];
        result += decrypted;
    }
    
    return result;
}

//==============================================================================
// FeatureGate implementation
static ProductUnlockManager* g_productManager = nullptr;

bool FeatureGate::isUnlocked(ProductUnlockManager::UnlockableFeature feature)
{
    if (auto* manager = getManager())
        return manager->isFeatureUnlocked(feature);
    return false; // Locked by default if no manager
}

void FeatureGate::showFeatureLockedDialog(ProductUnlockManager::UnlockableFeature feature, 
                                         juce::Component* parentComponent)
{
    juce::String featureName;
    juce::String description;
    
    switch (feature)
    {
        case ProductUnlockManager::UnlockableFeature::ITDProcessing:
            featureName = "ITD Processing";
            description = "Inter-aural Time Delay processing requires a Pro license.";
            break;
            
        default:
            featureName = "Premium Feature";
            description = "This feature requires a valid license.";
            break;
    }
    
    // Simple alert for now - can be enhanced later
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Feature Locked: " + featureName,
        description + "\n\nVisit https://mach1.tech/m1-panner-pro to learn more.",
        "OK"
    );
}

ProductUnlockManager* FeatureGate::getManager()
{
    return g_productManager;
}

// Global manager access - you'll need to set this in your main application
void setGlobalProductManager(ProductUnlockManager* manager)
{
    g_productManager = manager;
}

// Helper function to get the global manager from the processor
extern ProductUnlockManager* getGlobalProductManagerFromProcessor();

ProductUnlockManager* getGlobalProductManagerFromProcessor()
{
    return g_productManager;
}
