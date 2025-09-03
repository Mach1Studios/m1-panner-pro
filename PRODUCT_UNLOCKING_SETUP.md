# M1 Panner Pro - Product Unlocking Implementation Guide

## Overview

This document explains the product unlocking system implemented for M1 Panner Pro, including setup requirements, usage, and customization options.

## Features Implemented

### Core Components

1. **ProductUnlockManager** - Main licensing system
2. **M1LicenseComponent** - UI for license management
3. **LicenseStatusIndicator** - Compact status display
4. **Feature Gating** - Automatic feature restriction based on license level

### License Levels

- **Trial** (30 days) - Basic functionality
- **Standard** - Advanced spatial modes, OSC networking, preset management
- **Pro** - All features including ITD processing, multi-channel output (>8 channels)

### Protected Features

1. **Advanced Spatial Modes** (Standard+)
   - Isotropic encoding
   - Equal power encoding

2. **ITD Processing** (Pro)
   - Inter-aural time delay processing
   - Advanced spatial audio enhancement

3. **Multi-Channel Output** (Pro)
   - M1Spatial 14-channel output mode
   - Maximum spatial resolution

4. **OSC Networking** (Standard+)
   - Network communication features
   - Remote control capabilities

5. **Advanced Automation** (Pro)
   - Enhanced automation recording
   - Advanced parameter control

## Setup Requirements

### 1. JUCE Module Dependencies

The following JUCE modules are required (already configured in CMakeLists.txt):

```cmake
juce::juce_product_unlocking
juce::juce_cryptography  # For license encryption
juce::juce_data_structures  # For settings storage
```

### 2. Build Configuration

No additional build flags are required. The system is automatically enabled when the files are included.

### 3. File Structure

```
Source/
├── ProductUnlockManager.h
├── ProductUnlockManager.cpp
└── UI/
    ├── M1LicenseComponent.h
    └── M1LicenseComponent.cpp
```

## Usage

### Basic Integration

The system is automatically initialized in `M1PannerAudioProcessor`:

```cpp
// In constructor
productUnlockManager = std::make_unique<ProductUnlockManager>();
productUnlockManager->initialize();

// Check features
if (isFeatureUnlocked(ProductUnlockManager::UnlockableFeature::ITDProcessing))
{
    // Enable ITD processing
}
```

### UI Integration

Add license components to your UI:

```cpp
// Full license management panel
auto licenseComponent = std::make_unique<M1LicenseComponent>(*processor.getProductUnlockManager());

// Compact status indicator
auto statusIndicator = std::make_unique<LicenseStatusIndicator>(*processor.getProductUnlockManager());
```

### Feature Gating

Use the `FeatureGate` helper for easy feature checking:

```cpp
if (FeatureGate::isUnlocked(ProductUnlockManager::UnlockableFeature::AdvancedSpatialModes))
{
    // Enable advanced features
}
else
{
    FeatureGate::showFeatureLockedDialog(
        ProductUnlockManager::UnlockableFeature::AdvancedSpatialModes,
        parentComponent
    );
}
```

## License Key Format

The system uses a simple format for license keys:

```
MACH1-XXXXX-XXXXX-XXXXX-XXXXX
```

Where each X represents an alphanumeric character.

### Example Valid Keys

```
MACH1-12345-ABCDE-67890-FGHIJ
MACH1-TRIAL-STANDARD-PRO-DEMO
```

## Security Features

### Current Implementation

1. **Machine ID Binding** - Keys are encrypted with machine-specific data
2. **Format Validation** - Basic format checking
3. **Encrypted Storage** - License keys are encrypted before storage
4. **Trial Period Tracking** - Secure trial period management

### Production Enhancements (Recommended)

For production use, consider implementing:

1. **Online Validation** - Server-side license verification
2. **Cryptographic Signatures** - RSA/ECDSA signed license keys
3. **Hardware Fingerprinting** - More sophisticated machine identification
4. **Tamper Detection** - Protection against license file modification
5. **Revocation Lists** - Ability to revoke compromised keys

## Customization

### Adding New Features

1. Add to `UnlockableFeature` enum in `ProductUnlockManager.h`
2. Update `isFeatureUnlocked()` logic
3. Add feature gating in relevant code sections

### Modifying License Levels

Update the `FeatureLevel` enum and corresponding logic in `isFeatureUnlocked()`.

### Custom License Key Format

Modify `validateLicenseKey()` in `ProductUnlockManager.cpp` to implement your desired format.

## Testing

### Trial Mode Testing

1. Delete application settings to reset trial
2. Verify 30-day trial period
3. Test feature restrictions in trial mode

### License Key Testing

Use these test keys for development:

```
MACH1-TEST1-STAND-ARD12-34567  # Standard license
MACH1-TEST2-PROLI-CENSE-89012  # Pro license
```

### Feature Gating Testing

1. Test each protected feature in trial mode
2. Verify proper dialog display for locked features
3. Test feature unlocking with valid licenses

## Deployment Considerations

### Settings Storage

License information is stored in platform-specific locations:

- **macOS**: `~/Library/Application Support/M1 Panner Pro/`
- **Windows**: `%APPDATA%/M1 Panner Pro/`
- **Linux**: `~/.config/M1 Panner Pro/`

### Distribution

1. **Trial Distribution** - No special requirements
2. **Licensed Distribution** - Can include pre-activated licenses
3. **Update Handling** - License status persists across updates

## Troubleshooting

### Common Issues

1. **License Not Persisting**
   - Check file permissions in settings directory
   - Verify encryption/decryption is working

2. **Features Not Unlocking**
   - Verify `isFeatureUnlocked()` logic
   - Check parameter change handlers

3. **UI Not Updating**
   - Ensure timer callbacks are working
   - Verify callback functions are set

### Debug Information

Enable debug logging to see license status:

```cpp
DBG("[LICENSE] Current level: " + String((int)productUnlockManager->getCurrentFeatureLevel()));
DBG("[LICENSE] Trial days remaining: " + String(productUnlockManager->getTrialDaysRemaining()));
```

## Future Enhancements

### Planned Features

1. **Online Store Integration** - Direct purchase from plugin
2. **Subscription Support** - Time-based licensing
3. **Feature-Specific Licensing** - Individual feature unlocks
4. **Educational Discounts** - Student/academic licensing
5. **Volume Licensing** - Enterprise/studio licensing

### API Extensions

The system is designed to be extensible. Future versions may include:

- REST API for license management
- Cloud-based license synchronization
- Analytics and usage tracking
- Automated license renewal

## Support

For implementation questions or issues:

1. Check the debug output for licensing messages
2. Verify all required JUCE modules are linked
3. Test with known-good license keys
4. Review the example UI integration code

## Security Notice

This implementation provides basic licensing functionality suitable for development and testing. For production deployment, especially for commercial software, consider implementing additional security measures as outlined in the "Production Enhancements" section.

The current system is designed to be user-friendly rather than tamper-proof. It will deter casual piracy but should not be considered secure against determined attackers without additional hardening.
