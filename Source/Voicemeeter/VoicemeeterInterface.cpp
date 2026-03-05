/*
 * VoicemeeterInterface.cpp
 * VoicemeeterHost — Voicemeeter Audio Device Integration
 */

#include "VoicemeeterInterface.h"

#if JUCE_WINDOWS

#include <array>
#include <string_view>
#include <ranges>
#include <windows.h>
#include <cstring>

// ─── Logging ─────────────────────────────────────────────────

static void vmLog (const juce::String& msg)
{
    juce::String line = juce::Time::getCurrentTime().toString (true, true, true, true)
                        + "  " + msg + "\n";
    OutputDebugStringW (line.toWideCharPointer());
}

#define VMLOG(x) vmLog(x)

// ─── Registry helpers ────────────────────────────────────────

constexpr std::wstring_view vmRegKey    = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\VB:Voicemeeter {17359A74-1236-5467}";
constexpr std::wstring_view vmRegKeyWow = L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\VB:Voicemeeter {17359A74-1236-5467}";

#ifdef _WIN64
constexpr std::wstring_view vmDllName = L"VoicemeeterRemote64.dll";
#else
constexpr std::wstring_view vmDllName = L"VoicemeeterRemote.dll";
#endif

static juce::String readRegistryString (std::wstring_view keyPath, std::wstring_view valueName)
{
    HKEY hkey = nullptr;
    std::array<wchar_t, 512> buffer{};
    DWORD size = sizeof (buffer);

    if (RegOpenKeyExW (HKEY_LOCAL_MACHINE, keyPath.data(), 0, KEY_READ, &hkey) == ERROR_SUCCESS)
    {
        RegQueryValueExW (hkey, valueName.data(), nullptr, nullptr,
                          reinterpret_cast<LPBYTE> (buffer.data()), &size);
        RegCloseKey (hkey);
    }
    return juce::String (buffer.data());
}

// ─── Static audio callback trampoline ────────────────────────

static long __stdcall voicemeeterStaticCallback (void* lpUser, long nCommand,
                                                  void* lpData, long nnn)
{
    if (auto* device = static_cast<VoicemeeterAudioIODevice*> (lpUser))
        device->handleVoicemeeterCallback (nCommand, lpData, nnn);
    return 0;
}

// ═══════════════════════════════════════════════════════════════
// VoicemeeterAPI
// ═══════════════════════════════════════════════════════════════

VoicemeeterAPI& VoicemeeterAPI::getInstance()
{
    static VoicemeeterAPI instance;
    return instance;
}

VoicemeeterAPI::VoicemeeterAPI()
{
    std::memset (&vmr, 0, sizeof (vmr));

    juce::String uninstallString = readRegistryString (vmRegKey, L"UninstallString");
    VMLOG ("Registry UninstallString = " + uninstallString);

    if (uninstallString.isEmpty())
        uninstallString = readRegistryString (vmRegKeyWow, L"UninstallString");

    if (uninstallString.isEmpty()) { VMLOG ("ERROR: Voicemeeter not found"); return; }

    int lastSlash = uninstallString.lastIndexOfChar ('\\');
    if (lastSlash < 0) { VMLOG ("ERROR: bad uninstall path"); return; }

    installDirectory = uninstallString.substring (0, lastSlash);
    VMLOG ("installDirectory = " + installDirectory);

    juce::String dllPath = installDirectory + "\\" + juce::String (vmDllName.data());
    VMLOG ("Loading DLL: " + dllPath);
    dllModule = (void*) LoadLibraryW (dllPath.toWideCharPointer());

    if (dllModule == nullptr) { VMLOG ("ERROR: LoadLibrary failed"); return; }
    VMLOG ("DLL loaded OK");

    auto hModule = (HMODULE) dllModule;

#define VM_LOAD(name)  vmr.name = (T_##name) GetProcAddress (hModule, #name)
    VM_LOAD (VBVMR_Login);
    VM_LOAD (VBVMR_Logout);
    VM_LOAD (VBVMR_GetVoicemeeterType);
    VM_LOAD (VBVMR_GetVoicemeeterVersion);
    VM_LOAD (VBVMR_IsParametersDirty);
    VM_LOAD (VBVMR_AudioCallbackRegister);
    VM_LOAD (VBVMR_AudioCallbackStart);
    VM_LOAD (VBVMR_AudioCallbackStop);
    VM_LOAD (VBVMR_AudioCallbackUnregister);
#undef VM_LOAD

    dllLoaded = (vmr.VBVMR_Login != nullptr
              && vmr.VBVMR_Logout != nullptr
              && vmr.VBVMR_GetVoicemeeterType != nullptr
              && vmr.VBVMR_AudioCallbackRegister != nullptr
              && vmr.VBVMR_AudioCallbackStart != nullptr
              && vmr.VBVMR_AudioCallbackStop != nullptr
              && vmr.VBVMR_AudioCallbackUnregister != nullptr);

    VMLOG (juce::String ("dllLoaded = ") + (dllLoaded ? "true" : "false"));
}

VoicemeeterAPI::~VoicemeeterAPI()
{
    if (dllModule != nullptr)
        FreeLibrary ((HMODULE) dllModule);
}

int VoicemeeterAPI::detectTypeFromRegistry() const
{
    juce::String uninstallString = readRegistryString (vmRegKey, L"UninstallString");
    if (uninstallString.isEmpty())
        uninstallString = readRegistryString (vmRegKeyWow, L"UninstallString");
    if (uninstallString.isEmpty())
        return 0;

    auto lower = uninstallString.toLowerCase();
    if (lower.contains ("voicemeeter8setup") || lower.contains ("voicemeeterpotato"))
        return 3;
    if (lower.contains ("voicemeeterprosetup"))
        return 2;
    return 1;
}

// ═══════════════════════════════════════════════════════════════
// VoicemeeterAudioIODevice
// ═══════════════════════════════════════════════════════════════

VoicemeeterAudioIODevice::VoicemeeterAudioIODevice (const juce::String& outputBusName,
                                                    const juce::String& inBusName,
                                                    int inBusIdx, int outBusIdx)
    : juce::AudioIODevice (outputBusName, "Voicemeeter"),
      inputBusIndex (inBusIdx), outputBusIndex (outBusIdx),
      inputBusName (inBusName)
{
}

VoicemeeterAudioIODevice::~VoicemeeterAudioIODevice()
{
    aliveFlag->store (false);
    close();
}

juce::StringArray VoicemeeterAudioIODevice::getOutputChannelNames()
{
    static const std::array<const char*, 8> channelLabels { "L", "R", "Center", "LFE", "SL", "SR", "BL", "BR" };

    juce::StringArray names;
    for (int i : std::views::iota (1, channelsPerBus + 1))
    {
        const auto label = (i <= (int) channelLabels.size()) ? channelLabels[(size_t) (i - 1)] : "Ch";
        names.add ("ch"+juce::String (i) + "(" + juce::String (label) + ")");
    }

    return names;
}

juce::StringArray VoicemeeterAudioIODevice::getInputChannelNames() { return getOutputChannelNames(); }

std::optional<juce::BigInteger> VoicemeeterAudioIODevice::getDefaultOutputChannels() const
{
    juce::BigInteger ch; ch.setBit (0); ch.setBit (1); return ch;
}

std::optional<juce::BigInteger> VoicemeeterAudioIODevice::getDefaultInputChannels() const
{
    return getDefaultOutputChannels();
}

juce::Array<double> VoicemeeterAudioIODevice::getAvailableSampleRates()
{ return { 22050.0, 24000.0, 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 }; }

juce::Array<int> VoicemeeterAudioIODevice::getAvailableBufferSizes()
{ return { 64, 128, 256, 480, 512, 1024, 2048 }; }

int VoicemeeterAudioIODevice::getDefaultBufferSize() { return 512; }

// ─── open / close ────────────────────────────────────────────

juce::String VoicemeeterAudioIODevice::open (const juce::BigInteger& inputChannels,
                                              const juce::BigInteger& outputChannels,
                                              double sampleRate, int bufferSizeSamples)
{
    VMLOG ("=== open() outBus=" + getName() + "(" + juce::String (outputBusIndex) + ")"
           + " inBus=" + inputBusName + "(" + juce::String (inputBusIndex) + ")"
           + " sr=" + juce::String (sampleRate) + " buf=" + juce::String (bufferSizeSamples));
    close();

    auto& api = VoicemeeterAPI::getInstance();
    if (! api.isAvailable())
        return "Voicemeeter Remote API not available";

    auto& vmr = api.getInterface();

    long loginResult = vmr.VBVMR_Login();
    VMLOG ("VBVMR_Login() = " + juce::String (loginResult));
    if (loginResult < 0)
        return "Failed to login to Voicemeeter (error " + juce::String (loginResult) + ")";

    loggedIn = true;

    char clientName[64] = "VoicemeeterHost";
    long mode = VBVMR_AUDIOCALLBACK_OUT;
    long regResult = vmr.VBVMR_AudioCallbackRegister (mode, voicemeeterStaticCallback,
                                                       this, clientName);
    VMLOG ("AudioCallbackRegister() = " + juce::String (regResult));

    if (regResult == 1)
    {
        juce::String err = "Voicemeeter bus already in use by: " + juce::String (clientName);
        VMLOG ("ERROR: " + err);
        close();
        return err;
    }
    if (regResult != 0)
    {
        juce::String err = "Failed to register with Voicemeeter (error " + juce::String (regResult) + ")";
        VMLOG ("ERROR: " + err);
        close();
        return err;
    }

    callbackRegistered = true;

    activeInputChannels  = 0;
    activeOutputChannels = 0;
    for (int ch : std::views::iota (0, channelsPerBus))
    {
        if (inputChannels[ch])  activeInputChannels.setBit (ch);
        if (outputChannels[ch]) activeOutputChannels.setBit (ch);
    }

    currentSampleRate = sampleRate;
    currentBufferSize = bufferSizeSamples;
    deviceOpen = true;
    lastError  = {};
    VMLOG ("open() SUCCESS");
    return {};
}

void VoicemeeterAudioIODevice::close()
{
    VMLOG ("=== close()");
    stop();

    auto& api = VoicemeeterAPI::getInstance();
    if (api.isAvailable())
    {
        auto& vmr = api.getInterface();
        if (callbackRegistered) { vmr.VBVMR_AudioCallbackUnregister(); callbackRegistered = false; }
        if (loggedIn)           { vmr.VBVMR_Logout();                  loggedIn = false; }
    }
    deviceOpen = false;
}

bool VoicemeeterAudioIODevice::isOpen() { return deviceOpen; }

void VoicemeeterAudioIODevice::start (juce::AudioIODeviceCallback* callback)
{
    VMLOG ("=== start()");
    if (callback != nullptr && deviceOpen)
    {
        juceCallback.store (callback);
        callback->audioDeviceAboutToStart (this);

        auto& api = VoicemeeterAPI::getInstance();
        if (api.isAvailable())
        {
            long result = api.getInterface().VBVMR_AudioCallbackStart();
            VMLOG ("AudioCallbackStart() = " + juce::String (result));
            if (result != 0)
                lastError = "Failed to start callback (error " + juce::String (result) + ")";
        }
        devicePlaying = true;
    }
}

void VoicemeeterAudioIODevice::stop()
{
    if (devicePlaying)
    {
        auto& api = VoicemeeterAPI::getInstance();
        if (api.isAvailable())
            api.getInterface().VBVMR_AudioCallbackStop();

        auto* cb = juceCallback.exchange (nullptr);
        devicePlaying = false;
        if (cb != nullptr)
            cb->audioDeviceStopped();
    }
}

bool   VoicemeeterAudioIODevice::isPlaying()                  { return devicePlaying; }
int    VoicemeeterAudioIODevice::getCurrentBufferSizeSamples() { return currentBufferSize; }
double VoicemeeterAudioIODevice::getCurrentSampleRate()        { return currentSampleRate; }
int    VoicemeeterAudioIODevice::getCurrentBitDepth()          { return 32; }
int    VoicemeeterAudioIODevice::getOutputLatencyInSamples()   { return currentBufferSize; }
int    VoicemeeterAudioIODevice::getInputLatencyInSamples()    { return currentBufferSize; }

juce::BigInteger VoicemeeterAudioIODevice::getActiveOutputChannels() const { return activeOutputChannels; }
juce::BigInteger VoicemeeterAudioIODevice::getActiveInputChannels()  const { return activeInputChannels; }
juce::String     VoicemeeterAudioIODevice::getLastError()                  { return lastError; }

// ─── Audio callback handler ─────────────────────────────────

void VoicemeeterAudioIODevice::handleVoicemeeterCallback (long nCommand, void* lpData, long /*nnn*/)
{
    switch (nCommand)
    {
    case VBVMR_CBCOMMAND_STARTING:
    {
        auto* info = (VBVMR_LPT_AUDIOINFO) lpData;
        currentSampleRate = static_cast<double> (info->samplerate);
        currentBufferSize = static_cast<int>    (info->nbSamplePerFrame);
        VMLOG ("STARTING: sr=" + juce::String (currentSampleRate) + " buf=" + juce::String (currentBufferSize));

        auto flag = aliveFlag;
        juce::MessageManager::callAsync ([this, flag]()
        {
            if (! flag->load()) return;
            auto* cb = juceCallback.load();
            if (cb != nullptr)
                cb->audioDeviceAboutToStart (this);
        });
        break;
    }

    case VBVMR_CBCOMMAND_ENDING:
        VMLOG ("ENDING");
        break;

    case VBVMR_CBCOMMAND_CHANGE:
    {
        auto flag = aliveFlag;
        juce::MessageManager::callAsync ([this, flag]()
        {
            if (flag->load() && devicePlaying)
            {
                auto& api = VoicemeeterAPI::getInstance();
                if (api.isAvailable())
                    api.getInterface().VBVMR_AudioCallbackStart();
            }
        });
        break;
    }

    case VBVMR_CBCOMMAND_BUFFER_IN:
    case VBVMR_CBCOMMAND_BUFFER_OUT:
    {
        auto* callback = juceCallback.load();
        if (callback == nullptr) break;

        auto* buffer = (VBVMR_LPT_AUDIOBUFFER) lpData;
        const int nbs     = buffer->audiobuffer_nbs;
        const int nbi     = buffer->audiobuffer_nbi;
        const int nbo     = buffer->audiobuffer_nbo;
        const int inBase  = inputBusIndex  * channelsPerBus;
        const int outBase = outputBusIndex * channelsPerBus;

        const int availableIn  = juce::jmin (channelsPerBus, juce::jmax (0, nbi - inBase));
        const int availableOut = juce::jmin (channelsPerBus, juce::jmax (0, nbo - outBase));

        if (availableIn <= 0 && availableOut <= 0) break;

        const float* inputPtrs[channelsPerBus]  = {};
        float*       outputPtrs[channelsPerBus] = {};
        int numActiveIn = 0, numActiveOut = 0;

        for (int ch : std::views::iota (0, channelsPerBus))
        {
            if (activeInputChannels[ch])
            {
                float* r = (ch < availableIn) ? buffer->audiobuffer_r[inBase + ch] : nullptr;
                inputPtrs[numActiveIn++] = r;
            }
        }
        for (int ch : std::views::iota (0, channelsPerBus))
        {
            if (activeOutputChannels[ch])
            {
                float* w = (ch < availableOut) ? buffer->audiobuffer_w[outBase + ch] : nullptr;
                outputPtrs[numActiveOut++] = w;
            }
        }

        if (numActiveIn > 0 || numActiveOut > 0)
        {
            juce::AudioIODeviceCallbackContext context;
            callback->audioDeviceIOCallbackWithContext (
                numActiveIn  > 0 ? inputPtrs  : nullptr, numActiveIn,
                numActiveOut > 0 ? outputPtrs : nullptr, numActiveOut,
                nbs, context);
        }
        break;
    }

    default: break;
    }
}

// ═══════════════════════════════════════════════════════════════
// VoicemeeterAudioIODeviceType
// ═══════════════════════════════════════════════════════════════

VoicemeeterAudioIODeviceType::VoicemeeterAudioIODeviceType()
    : juce::AudioIODeviceType ("Voicemeeter") {}

VoicemeeterAudioIODeviceType::~VoicemeeterAudioIODeviceType() = default;

void VoicemeeterAudioIODeviceType::scanForDevices()
{
    deviceNames.clear();
    deviceBusIndices.clear();
    deviceIsInput.clear();
    voicemeeterType = 0;

    auto& api = VoicemeeterAPI::getInstance();
    if (! api.isAvailable()) return;

    auto& vmr = api.getInterface();
    long loginRep = vmr.VBVMR_Login();
    if (loginRep >= 0)
    {
        long vmType = 0;
        if (vmr.VBVMR_IsParametersDirty != nullptr &&
            vmr.VBVMR_GetVoicemeeterType != nullptr)
        {
            if (vmr.VBVMR_IsParametersDirty() >= 0)
                vmr.VBVMR_GetVoicemeeterType (&vmType);
        }
        vmr.VBVMR_Logout();
        if (vmType > 0) voicemeeterType = static_cast<int> (vmType);
    }

    if (voicemeeterType == 0)
        voicemeeterType = api.detectTypeFromRegistry();
    if (voicemeeterType == 0)
        voicemeeterType = 1;

    int numHwOut = 2, numVirtOut = 1;
    switch (voicemeeterType)
    {
        case 1: numHwOut = 2; numVirtOut = 1; break;
        case 2: numHwOut = 3; numVirtOut = 2; break;
        case 3: numHwOut = 5; numVirtOut = 3; break;
        default: break;
    }

    for (int i = 0; i < numHwOut; ++i)
    {
        deviceNames.add ("Output A" + juce::String (i + 1));
        deviceBusIndices.add (i);
        deviceIsInput.add (false);
    }
    for (int i = 0; i < numVirtOut; ++i)
    {
        deviceNames.add ("Output B" + juce::String (i + 1));
        deviceBusIndices.add (numHwOut + i);
        deviceIsInput.add (false);
    }

    VMLOG ("Devices: " + deviceNames.joinIntoString (", "));
}

juce::StringArray VoicemeeterAudioIODeviceType::getDeviceNames (bool) const { return deviceNames; }

int VoicemeeterAudioIODeviceType::getDefaultDeviceIndex (bool) const
{ return deviceNames.isEmpty() ? -1 : 0; }

int VoicemeeterAudioIODeviceType::getIndexOfDevice (juce::AudioIODevice* device, bool asInput) const
{
    if (auto* vm = dynamic_cast<VoicemeeterAudioIODevice*> (device))
    {
        auto name = asInput ? vm->getInputBusName() : vm->getOutputBusName();
        return deviceNames.indexOf (name);
    }
    return -1;
}

bool VoicemeeterAudioIODeviceType::hasSeparateInputsAndOutputs() const { return true; }

juce::AudioIODevice* VoicemeeterAudioIODeviceType::createDevice (
    const juce::String& outputDeviceName, const juce::String& inputDeviceName)
{
    int outIndex = deviceNames.indexOf (outputDeviceName);
    if (outIndex < 0 && outputDeviceName.isNotEmpty()) return nullptr;

    int inIndex = inputDeviceName.isNotEmpty() ? deviceNames.indexOf (inputDeviceName) : outIndex;
    if (inIndex < 0) inIndex = outIndex;

    auto devName = outIndex >= 0 ? deviceNames[outIndex] : deviceNames[inIndex];
    auto inName  = inIndex  >= 0 ? deviceNames[inIndex]  : devName;
    int  inBus   = inIndex  >= 0 ? deviceBusIndices[inIndex]  : 0;
    int  outBus  = outIndex >= 0 ? deviceBusIndices[outIndex] : inBus;

    return new VoicemeeterAudioIODevice (devName, inName, inBus, outBus);
}

// ═══════════════════════════════════════════════════════════════
// VoicemeeterDeviceManager
// ═══════════════════════════════════════════════════════════════

void VoicemeeterDeviceManager::createAudioDeviceTypes (juce::OwnedArray<juce::AudioIODeviceType>& types)
{
    types.add (new VoicemeeterAudioIODeviceType());
}

#endif // JUCE_WINDOWS
