#pragma once

#include <JuceHeader.h>
#include <Mach1Encode.h>

#include "Config.h"
#include "AlertData.h"
#include "PannerOSC.h"
#include "TypesForDataExchange.h"
#include "ProductUnlockManager.h"
#include "RingBuffer.h"

//==============================================================================
/**
*/
class PannerOSC; // forward declare for PannerOSC
class M1PannerAudioProcessor : public juce::AudioProcessor, juce::AudioProcessorValueTreeState::Listener, juce::Timer
{
public:
    //==============================================================================
    M1PannerAudioProcessor();
    ~M1PannerAudioProcessor() override;

    static AudioProcessor::BusesProperties getHostSpecificLayout()
    {
        // This determines the initial bus i/o for plugin on construction and depends on the `isBusesLayoutSupported()`
        juce::PluginHostType hostType;

        // Pro Tools specific layout
        if (hostType.isProTools() || hostType.getPluginLoadedAs() == AudioProcessor::wrapperType_AAX)
        {
            // Pro Tools needs a fixed, stable initial configuration
            return BusesProperties()
                .withInput("Default Input", juce::AudioChannelSet::stereo(), true)
                .withOutput("Default Output", juce::AudioChannelSet::create7point1(), true);
        }

        // Multichannel DAWs
        if (hostType.isReaper() || hostType.isNuendo() || hostType.isDaVinciResolve() || hostType.isArdour())
        {
            if (hostType.getPluginLoadedAs() == AudioProcessor::wrapperType_VST3)
            {
                return BusesProperties()
                    // VST3 requires named plugin configurations only
                    .withInput("Input", juce::AudioChannelSet::namedChannelSet(6), true)
                    .withOutput("Mach1 Out", juce::AudioChannelSet::ambisonic(5), true); // 36 named channel
            }
            else
            {
                return BusesProperties()
                    .withInput("Input", juce::AudioChannelSet::namedChannelSet(6), true)
                    .withOutput("Mach1 Out", juce::AudioChannelSet::discreteChannels(60), true);
            }
        }

        if (hostType.getPluginLoadedAs() == AudioProcessor::wrapperType_Unity)
        {
            return BusesProperties()
                .withInput("Input", juce::AudioChannelSet::stereo(), true)
                .withOutput("Mach1 Out", juce::AudioChannelSet::discreteChannels(8), true);
        }

        // STREAMING Panner instance
        return BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true);
    }

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    std::vector<int> output_channel_indices; // For reordering channel indices based on specific DAW hosts (example: reordering for ProTools 7.1 channel order)
    void fillChannelOrderArray(int numM1OutputChannels);

#ifndef CUSTOM_CHANNEL_LAYOUT
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void writeToDelayBuffer(juce::AudioSampleBuffer& buffer,
        const int channelIn,
        const int channelOut,
        const int writePos,
        float startGain,
        float endGain,
        bool replacing);

    void readFromDelayBuffer(juce::AudioSampleBuffer& buffer,
        const int channelIn,
        const int channelOut,
        const int readPos,
        float startGain,
        float endGain,
        bool replacing);

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameter Setup
    juce::AudioProcessorValueTreeState& getValueTreeState();
    static juce::String paramAzimuth;
    static juce::String paramElevation; // also Z
    static juce::String paramDiverge;
    static juce::String paramGain;
    static juce::String paramStereoOrbitAzimuth;
    static juce::String paramStereoSpread;
    static juce::String paramStereoInputBalance;
    static juce::String paramAutoOrbit;
    static juce::String paramIsotropicEncodeMode;
    static juce::String paramEqualPowerEncodeMode;
    static juce::String paramGainCompensationMode;
#ifndef CUSTOM_CHANNEL_LAYOUT
    static juce::String paramInputMode;
    static juce::String paramOutputMode;
#endif

    // ITD Headshadow parameters (Pro feature)
    static juce::String paramHeadshadowActive;
    static juce::String paramHeadshadowDelayTime;
    static juce::String paramHeadshadowWetGain;

    // Variables from processor for UI
    juce::Array<float> outputMeterValuedB;

    double processorSampleRate = 44100; // only has to be something for the initilizer to work
    void m1EncodeChangeInputOutputMode(Mach1EncodeInputMode inputMode, Mach1EncodeOutputMode outputMode);
    PannerSettings pannerSettings;
    float gain_comp_in_db = 0;
    
    // ITD Headshadow processing (Pro feature)
    Mach1Encode<float> m1EncodeInverse;
    bool headshadowActive = false;
    int headshadowDelayTime = 600; // Default 600 microseconds (0.6ms)
    float headshadowWetGain = 0.0f;   // Default no wet signal

    // External components
    MixerSettings monitorSettings;
    HostTimelineData hostTimelineData;
    juce::PluginHostType hostType;
    void updateTrackProperties(const TrackProperties& properties) override { track_properties = properties; }
    TrackProperties getTrackProperties() { return track_properties; }
    bool layoutCreated = false;
    bool lockOutputLayout = false;

    // update m1encode obj points
    bool needToUpdateM1EncodePoints = false;
    void updateM1EncodePoints();

    // Communication to OrientationManager/Monitor and the rest of the M1SpatialSystem
    void timerCallback() override;
    std::unique_ptr<PannerOSC> pannerOSC;
    juce::OSCColour osc_colour = { 0, 0, 0, 255 };

    // TODO: change this
    bool external_spatialmixer_active = false; // global detect spatialmixer

    // UI related utility functions
    struct Line2D
    {
        Line2D(double x, double y, double x2, double y2) : x{ x }, y{ y }, x2{ x2 }, y2{ y2 } {}
        MurkaPoint p() const
        {
            return { x, y };
        }
        MurkaPoint v() const
        {
            return { x2 - x, y2 - y };
        }
        double x, y, x2, y2;
    };

    inline double intersection(const Line2D& a, const Line2D& b)
    {
        const double Precision = std::sqrt(std::numeric_limits<double>::epsilon());
        double d = a.v().x * b.v().y - a.v().y * b.v().x;
        if (std::abs(d) < Precision)
            return std::numeric_limits<double>::quiet_NaN();
        else
        {
            double n = (b.p().x - a.p().x) * b.v().y
                       - (b.p().y - a.p().y) * b.v().x;
            return n / d;
        }
    }

    inline MurkaPoint intersection_point(const Line2D& a, const Line2D& b)
    {
        // Line2D has an operator () (double r) returning p() + r * v()
        return a.p() + a.v() * (intersection(a, b));
    }

    void convertRCtoXYRaw(float r, float d, float& x, float& y);
    void convertXYtoRCRaw(float x, float y, float& r, float& d);

    // Add mute states vector for each input channel
    std::vector<bool> channelMuteStates;

    // This will be set by the UI or editor so we can notify it of alerts
    std::function<void(const Mach1::AlertData&)> postAlertToUI;
    void postAlert(const Mach1::AlertData& alert);
    std::vector<Mach1::AlertData> pendingAlerts;

    // Flag to prevent recursive parameter conversion during UI coordinate updates
    bool updatingCoordinatesFromUI = false;

    // Parameter ownership flags - track which UI control is actively managing each parameter
    bool azimuthOwnedByUI = false;
    bool elevationOwnedByUI = false;
    bool divergeOwnedByUI = false;

    // Reticle priority flags to prevent main and overlay reticles from fighting over azimuth parameter
    bool mainReticleActive = false;
    bool overlayReticleActive = false;

    // Track last UI-set values for tolerance-based feedback prevention
    float lastUISetAzimuth = 0.0f;
    float lastUISetElevation = 0.0f;
    float lastUISetDiverge = 0.0f;
    static constexpr float PARAMETER_TOLERANCE = 0.1f;  // Tolerance for parameter comparison

    // Product unlocking and licensing
    std::unique_ptr<ProductUnlockManager> productUnlockManager;
    ProductUnlockManager* getProductUnlockManager() const { return productUnlockManager.get(); }
    bool isFeatureUnlocked(ProductUnlockManager::UnlockableFeature feature) const;

private:
    TrackProperties track_properties;
    void createLayout();

    juce::UndoManager mUndoManager;
    juce::AudioProcessorValueTreeState parameters;

    // Channel input
    std::vector<std::vector<float>> audioDataIn;
    std::vector<std::vector<juce::LinearSmoothedValue<float>>> smoothedChannelCoeffs;

    inline void processBuffers(AudioSampleBuffer& buffer,
        std::vector<int> orderChans,
        std::vector<std::vector<float>> delayCoeffs);

    // ITD Headshadow delay processing
    std::unique_ptr<RingBuffer> headshadowDelayBuffer;
    std::vector<std::vector<float>> headshadowAudioDataIn; // Copied input signals for headshadow processing
    std::vector<std::vector<juce::LinearSmoothedValue<float>>> headshadowSmoothedChannelCoeffs; // For m1EncodeInverse
    juce::SmoothedValue<int, juce::ValueSmoothingTypes::Linear> headshadowDelayTimeSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> headshadowWetGainSmoother;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(M1PannerAudioProcessor)
};
