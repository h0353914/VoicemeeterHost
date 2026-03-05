/*
 * VoicemeeterInterface.h
 * VoicemeeterHost — Voicemeeter Audio Device Integration
 *
 * Implements JUCE AudioIODevice / AudioIODeviceType backed by
 * Voicemeeter Remote API.  Each device instance corresponds to
 * one Voicemeeter output bus (A1–A5 / B1–B3) in insert-effect mode.
 *
 * Windows-only.
 */

#pragma once

#include <JuceHeader.h>

#if JUCE_WINDOWS

#include "VoicemeeterRemote.h"
#include <atomic>
#include <memory>

// ─── VoicemeeterAPI singleton ────────────────────────────────
class VoicemeeterAPI
{
public:
    static VoicemeeterAPI& getInstance();

    [[nodiscard]] bool               isAvailable()  const noexcept { return dllLoaded; }
    [[nodiscard]] T_VBVMR_INTERFACE& getInterface() noexcept       { return vmr; }
    [[nodiscard]] int                detectTypeFromRegistry() const;

private:
    VoicemeeterAPI();
    ~VoicemeeterAPI();

    void* dllModule = nullptr;
    T_VBVMR_INTERFACE vmr{};
    bool  dllLoaded = false;
    juce::String installDirectory;
};

// ─── VoicemeeterAudioIODevice ────────────────────────────────
class VoicemeeterAudioIODevice : public juce::AudioIODevice
{
public:
    VoicemeeterAudioIODevice (const juce::String& outputBusName,
                              const juce::String& inputBusName,
                              int inputBusIndex,
                              int outputBusIndex);
    ~VoicemeeterAudioIODevice() override;

    // AudioIODevice interface
    [[nodiscard]] juce::StringArray getOutputChannelNames() override;
    [[nodiscard]] juce::StringArray getInputChannelNames()  override;
    [[nodiscard]] std::optional<juce::BigInteger> getDefaultOutputChannels() const override;
    [[nodiscard]] std::optional<juce::BigInteger> getDefaultInputChannels()  const override;
    [[nodiscard]] juce::Array<double> getAvailableSampleRates() override;
    [[nodiscard]] juce::Array<int>    getAvailableBufferSizes() override;
    [[nodiscard]] int getDefaultBufferSize() override;

    [[nodiscard]] juce::String open (const juce::BigInteger& inputChannels,
                                     const juce::BigInteger& outputChannels,
                                     double sampleRate, int bufferSizeSamples) override;
    void close() override;
    [[nodiscard]] bool isOpen() override;
    void start (juce::AudioIODeviceCallback* callback) override;
    void stop()  override;
    [[nodiscard]] bool isPlaying() override;

    [[nodiscard]] int    getCurrentBufferSizeSamples() override;
    [[nodiscard]] double getCurrentSampleRate()        override;
    [[nodiscard]] int    getCurrentBitDepth()           override;
    [[nodiscard]] int    getOutputLatencyInSamples()    override;
    [[nodiscard]] int    getInputLatencyInSamples()     override;
    [[nodiscard]] juce::BigInteger getActiveOutputChannels() const override;
    [[nodiscard]] juce::BigInteger getActiveInputChannels()  const override;
    [[nodiscard]] juce::String     getLastError() override;

    [[nodiscard]] const juce::String& getInputBusName()  const noexcept { return inputBusName; }
    [[nodiscard]] const juce::String& getOutputBusName() const          { return getName(); }

    /** Called from the static Voicemeeter callback on the audio thread. */
    void handleVoicemeeterCallback (long nCommand, void* lpData, long nnn);

private:
    static constexpr int channelsPerBus = 8;
    int inputBusIndex  = 0;
    int outputBusIndex = 0;
    juce::String inputBusName;

    bool deviceOpen          = false;
    bool devicePlaying       = false;
    bool loggedIn            = false;
    bool callbackRegistered  = false;

    std::atomic<juce::AudioIODeviceCallback*> juceCallback { nullptr };

    juce::BigInteger activeInputChannels;
    juce::BigInteger activeOutputChannels;

    double currentSampleRate = 48000.0;
    int    currentBufferSize = 512;
    juce::String lastError;

    std::shared_ptr<std::atomic<bool>> aliveFlag =
        std::make_shared<std::atomic<bool>> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoicemeeterAudioIODevice)
};

// ─── VoicemeeterAudioIODeviceType ────────────────────────────
class VoicemeeterAudioIODeviceType : public juce::AudioIODeviceType
{
public:
    VoicemeeterAudioIODeviceType();
    ~VoicemeeterAudioIODeviceType() override;

    void scanForDevices() override;
    [[nodiscard]] juce::StringArray getDeviceNames (bool wantInputNames = false) const override;
    [[nodiscard]] int  getDefaultDeviceIndex (bool forInput) const override;
    [[nodiscard]] int  getIndexOfDevice (juce::AudioIODevice* device, bool asInput) const override;
    [[nodiscard]] bool hasSeparateInputsAndOutputs() const override;
    [[nodiscard]] juce::AudioIODevice* createDevice (const juce::String& outputDeviceName,
                                                      const juce::String& inputDeviceName) override;

private:
    juce::StringArray  deviceNames;
    juce::Array<int>   deviceBusIndices;
    juce::Array<bool>  deviceIsInput;
    int voicemeeterType = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoicemeeterAudioIODeviceType)
};

// ─── Custom AudioDeviceManager that registers Voicemeeter ────
class VoicemeeterDeviceManager : public juce::AudioDeviceManager
{
public:
    void createAudioDeviceTypes (juce::OwnedArray<juce::AudioIODeviceType>& types) override;
};

#endif // JUCE_WINDOWS
