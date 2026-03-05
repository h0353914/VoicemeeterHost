/*
 * VoicemeeterInterface.cpp
 * VoicemeeterHost — Voicemeeter 音訊設備整合實作
 *
 * 完整實作 VoicemeeterAPI、VoicemeeterAudioIODevice、
 * VoicemeeterAudioIODeviceType 與 VoicemeeterDeviceManager。
 * 資料流路：
 *   Registry 謇入 DLL 路徑 → LoadLibrary → GetProcAddress 綁定 API 函式
 *   open() 登入 SDK → AudioCallbackRegister → AudioCallbackStart → handleVoicemeeterCallback
 */

#include "VoicemeeterInterface.h"

#if JUCE_WINDOWS

#include <array>
#include <string_view>
#include <ranges>
#include <windows.h>
#include <cstring>

// ─── 日誌輔助工具 ─────────────────────────────────────────────────
/**
 * vmLog: 將含時間戳的訊息透過 Windows OutputDebugString 輸出。
 * VMLOG(x) macro: 簡化呼叫 vmLog。
 */
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

// ─── 靜態音訊回調 Trampoline ────────────────────────────────
/**
 * voicemeeterStaticCallback:
 * Voicemeeter SDK 所調用的靜態 C 回調函式。
 * 透過 lpUser 指標將調用转駁到對應的 VoicemeeterAudioIODevice 實例方法。
 */
static long __stdcall voicemeeterStaticCallback (void* lpUser, long nCommand,
                                                  void* lpData, long nnn)
{
    if (auto* device = static_cast<VoicemeeterAudioIODevice*> (lpUser))
        device->handleVoicemeeterCallback (nCommand, lpData, nnn);
    return 0;
}

// ═══════════════════════════════════════════════════════════════
// VoicemeeterAPI 實作
// 建構時透過 Registry 找到 DLL 路徑後動態載入，用 GetProcAddress 綁定所有 API。
// 判斷條件：所有必要入口均成功綁定則 dllLoaded = true。
// ═══════════════════════════════════════════════════════════════

VoicemeeterAPI& VoicemeeterAPI::getInstance()
{
    static VoicemeeterAPI instance;
    return instance;
}

VoicemeeterAPI::VoicemeeterAPI()
{
    std::memset (&vmr, 0, sizeof (vmr));

    // 從 HKLM Registry 讀取 UninstallString 以尋找安裝路徑
    juce::String uninstallString = readRegistryString (vmRegKey, L"UninstallString");
    VMLOG ("Registry UninstallString = " + uninstallString);

    if (uninstallString.isEmpty())
        uninstallString = readRegistryString (vmRegKeyWow, L"UninstallString");

    if (uninstallString.isEmpty()) { VMLOG ("ERROR: Voicemeeter not found"); return; }

    // 從 UninstallString 拼出路徑（去掉最後一個 '\' 後的部分）
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

    // 用 VM_LOAD macro 綁定必要的 Voicemeeter Remote API 入口
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

/**
 * VoicemeeterAPI::~VoicemeeterAPI() 析構函式
 * 釋放載入的 Voicemeeter Remote DLL。
 */
VoicemeeterAPI::~VoicemeeterAPI()
{
    if (dllModule != nullptr)
        FreeLibrary ((HMODULE) dllModule);
}

/**
 * VoicemeeterAPI::detectTypeFromRegistry() 從 Registry 偵測 Voicemeeter 類型
 * 檢查 Uninstall 字串內容判斷安裝的 Voicemeeter 版本：
 *   - 1: Voicemeeter 標準版
 *   - 2: Voicemeeter Banana 版（更多輸出）
 *   - 3: Voicemeeter Potato 版（最多輸出）
 *   - 0: 未找到或無法判定
 */
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
// VoicemeeterAudioIODevice 實作
// 建構時存儲通道索引；析構時清理資源。
// ═══════════════════════════════════════════════════════════════

/**
 * VoicemeeterAudioIODevice::VoicemeeterAudioIODevice() 建構函式
 * 初始化JUCE AudioIODevice 基類，存儲輸入/輸出總線索引。
 * 
 * 參數：
 *   - outputBusName: 設備名稱（例如「Output A1」）
 *   - inBusName: 輸入總線名稱
 *   - inBusIdx: 輸入總線索引
 *   - outBusIdx: 輸出總線索引
 */
VoicemeeterAudioIODevice::VoicemeeterAudioIODevice (const juce::String& outputBusName,
                                                    const juce::String& inBusName,
                                                    int inBusIdx, int outBusIdx)
    : juce::AudioIODevice (outputBusName, "Voicemeeter"),
      inputBusIndex (inBusIdx), outputBusIndex (outBusIdx),
      inputBusName (inBusName)
{
}

/**
 * VoicemeeterAudioIODevice::~VoicemeeterAudioIODevice() 析構函式
 * 標記設備已銷毀、關閉設備並釋放所有資源。
 */
VoicemeeterAudioIODevice::~VoicemeeterAudioIODevice()
{
    aliveFlag->store (false);  // 通知所有異步任務此設備已銷毀
    close();                    // 清理 Voicemeeter 連接和回調
}

/**
 * VoicemeeterAudioIODevice::getOutputChannelNames() 取得輸出通道名稱清單
 * 為每個通道產生「chN(標籤)」格式的名稱，如「ch1(L)」、「ch2(R)」等。
 */
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

/**
 * VoicemeeterAudioIODevice::getInputChannelNames() 取得輸入通道名稱清單
 * 與輸出通道名稱相同。
 */
juce::StringArray VoicemeeterAudioIODevice::getInputChannelNames() { return getOutputChannelNames(); }

/**
 * VoicemeeterAudioIODevice::getDefaultOutputChannels() 取得預設輸出通道選笮
 * 預設啟用第 0 與第 1 通道（L 與 R 立體聲）。
 */
std::optional<juce::BigInteger> VoicemeeterAudioIODevice::getDefaultOutputChannels() const
{
    juce::BigInteger ch; ch.setBit (0); ch.setBit (1); return ch;
}

/**
 * VoicemeeterAudioIODevice::getDefaultInputChannels() 取得預設輸入通道選笮
 * 與輸出通道預設選笮相同。
 */
std::optional<juce::BigInteger> VoicemeeterAudioIODevice::getDefaultInputChannels() const
{
    return getDefaultOutputChannels();
}

/**
 * VoicemeeterAudioIODevice::getAvailableSampleRates() 取得支援的擁有率清單
 * 回傳常見的擁有率列表：22050、24000、44100、48000、88200、96000、176400、192000 Hz。
 */
juce::Array<double> VoicemeeterAudioIODevice::getAvailableSampleRates()
{ return { 22050.0, 24000.0, 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 }; }

/**
 * VoicemeeterAudioIODevice::getAvailableBufferSizes() 取得支援的緩衝區大小清單
 * 回傳可用的縮衝位置單位：64、128、256、480、512、1024、2048 樣本數。
 */
juce::Array<int> VoicemeeterAudioIODevice::getAvailableBufferSizes()
{ return { 64, 128, 256, 480, 512, 1024, 2048 }; }

/**
 * VoicemeeterAudioIODevice::getDefaultBufferSize() 取得預設緩衝區大小
 * 回傳 512 樣本。
 */
int VoicemeeterAudioIODevice::getDefaultBufferSize() { return 512; }

// ─── open / close ────────────────────────────────────────────────

/**
 * VoicemeeterAudioIODevice::open() 開啟設備
 * 登入 Voicemeeter SDK、設定音訊回調、儲存通道設置與采樣參數。
 * 成功回傳空字串；失敗回傳詳細的錯誤訊息。
 * 
 * 主要步驟：
 * 1. 登入 Voicemeeter Remote SDK
 * 2. 註冊音訊回調處理器
 * 3. 設定活躍的輸入/輸出通道
 * 4. 儲存采樣率與緩衝區大小
 */
juce::String VoicemeeterAudioIODevice::open (const juce::BigInteger& inputChannels,
                                              const juce::BigInteger& outputChannels,
                                              double sampleRate, int bufferSizeSamples)
{
    VMLOG ("=== open() outBus=" + getName() + "(" + juce::String (outputBusIndex) + ")"
           + " inBus=" + inputBusName + "(" + juce::String (inputBusIndex) + ")"
           + " sr=" + juce::String (sampleRate) + " buf=" + juce::String (bufferSizeSamples));
    close();  // 確保舊實例已關閉

    auto& api = VoicemeeterAPI::getInstance();
    if (! api.isAvailable())
        return "Voicemeeter Remote API not available";

    auto& vmr = api.getInterface();

    // 第一步：登入 Voicemeeter SDK
    long loginResult = vmr.VBVMR_Login();
    VMLOG ("VBVMR_Login() = " + juce::String (loginResult));
    if (loginResult < 0)
        return "Failed to login to Voicemeeter (error " + juce::String (loginResult) + ")";

    loggedIn = true;

    // 第二步：註冊音訊回調
    char clientName[64] = "VoicemeeterHost";
    long mode = VBVMR_AUDIOCALLBACK_OUT;
    long regResult = vmr.VBVMR_AudioCallbackRegister (mode, voicemeeterStaticCallback,
                                                       this, clientName);
    VMLOG ("AudioCallbackRegister() = " + juce::String (regResult));

    // 檢查回調註冊是否成功
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

    // 第三步：設定活躍通道
    activeInputChannels  = 0;
    activeOutputChannels = 0;
    for (int ch : std::views::iota (0, channelsPerBus))
    {
        if (inputChannels[ch])  activeInputChannels.setBit (ch);   // 啟用請求的輸入通道
        if (outputChannels[ch]) activeOutputChannels.setBit (ch);  // 啟用請求的輸出通道
    }

    // 第四步：儲存采樣率和緩衝區大小供回調使用
    currentSampleRate = sampleRate;
    currentBufferSize = bufferSizeSamples;
    deviceOpen = true;
    lastError  = {};
    VMLOG ("open() SUCCESS");
    return {};
}

/**
 * VoicemeeterAudioIODevice::close() 關閉設備
 * 停止音訊回調、取消註冊回調、登出 Voicemeeter SDK。
 * 確保所有資源正確釋放。
 */
void VoicemeeterAudioIODevice::close()
{
    VMLOG ("=== close()");
    stop();  // 先停止音訊播放

    auto& api = VoicemeeterAPI::getInstance();
    if (api.isAvailable())
    {
        auto& vmr = api.getInterface();
        // 取消註冊音訊回調
        if (callbackRegistered) { vmr.VBVMR_AudioCallbackUnregister(); callbackRegistered = false; }
        // 登出 SDK
        if (loggedIn)           { vmr.VBVMR_Logout();                  loggedIn = false; }
    }
    deviceOpen = false;
}

/**
 * VoicemeeterAudioIODevice::isOpen() 檢查設備是否已開啟
 */
bool VoicemeeterAudioIODevice::isOpen() { return deviceOpen; }

/**
 * VoicemeeterAudioIODevice::start() 啟動設備
 * 儲存 JUCE 回調指標、通知設備即將啟動、啟動 Voicemeeter 音訊回調。
 * 
 * 流程：
 * 1. 儲存 JUCE AudioIODeviceCallback 指標以便音訊回調時調用
 * 2. 呼叫 JUCE 的 audioDeviceAboutToStart() 讓 DSP 鏈初始化
 * 3. 啟動 Voicemeeter 的音訊回調，開始處理音訊數據
 */
void VoicemeeterAudioIODevice::start (juce::AudioIODeviceCallback* callback)
{
    VMLOG ("=== start()");
    if (callback != nullptr && deviceOpen)
    {
        juceCallback.store (callback);  // 儲存 JUCE 回調指標（線程安全）
        callback->audioDeviceAboutToStart (this);  // 通知 JUCE 設備即將開始

        auto& api = VoicemeeterAPI::getInstance();
        if (api.isAvailable())
        {
            // 啟動 Voicemeeter 音訊回調
            long result = api.getInterface().VBVMR_AudioCallbackStart();
            VMLOG ("AudioCallbackStart() = " + juce::String (result));
            if (result != 0)
                lastError = "Failed to start callback (error " + juce::String (result) + ")";
        }
        devicePlaying = true;  // 標記設備正在播放
    }
}

/**
 * VoicemeeterAudioIODevice::stop() 停止設備
 * 停止 Voicemeeter 音訊回調、清除 JUCE 回調指標、通知 JUCE 設備已停止。
 * 
 * 流程：
 * 1. 停止 Voicemeeter 的音訊回調
 * 2. 原子性地清除 JUCE 回調指標
 * 3. 呼叫 JUCE 的 audioDeviceStopped() 讓 DSP 鏈清理
 */
void VoicemeeterAudioIODevice::stop()
{
    if (devicePlaying)
    {
        auto& api = VoicemeeterAPI::getInstance();
        if (api.isAvailable())
            api.getInterface().VBVMR_AudioCallbackStop();  // 停止音訊回調

        auto* cb = juceCallback.exchange (nullptr);  // 原子性地取出並清除回調指標
        devicePlaying = false;  // 標記設備已停止播放
        if (cb != nullptr)
            cb->audioDeviceStopped();  // 通知 JUCE 設備已停止
    }
}

/** 檢查設備是否正在播放。 */
bool   VoicemeeterAudioIODevice::isPlaying()                  { return devicePlaying; }
/** 取得目前緩衝區大小（樣本數）。 */
int    VoicemeeterAudioIODevice::getCurrentBufferSizeSamples() { return currentBufferSize; }
/** 取得目前擁有率（Hz）。 */
double VoicemeeterAudioIODevice::getCurrentSampleRate()        { return currentSampleRate; }
/** 取得位元深度（固定回傳 32）。 */
int    VoicemeeterAudioIODevice::getCurrentBitDepth()          { return 32; }
/** 取得輸出延遲（樣本數）。 */
int    VoicemeeterAudioIODevice::getOutputLatencyInSamples()   { return currentBufferSize; }
/** 取得輸入延遲（樣本數）。 */
int    VoicemeeterAudioIODevice::getInputLatencyInSamples()    { return currentBufferSize; }

/** 取得目前活躍的輸出通道選笮。 */
juce::BigInteger VoicemeeterAudioIODevice::getActiveOutputChannels() const { return activeOutputChannels; }
/** 取得目前活躍的輸入通道選笮。 */
juce::BigInteger VoicemeeterAudioIODevice::getActiveInputChannels()  const { return activeInputChannels; }
/** 取得最近一次錯誤訊息。 */
juce::String     VoicemeeterAudioIODevice::getLastError()                  { return lastError; }

// ─── Audio callback handler ─────────────────────────────────

/**
 * VoicemeeterAudioIODevice::handleVoicemeeterCallback() Voicemeeter 音訊回調處理器
 * 
 * 此為靜態回調函式（通過 C-style 包裝器呼叫），處理 Voicemeeter Remote 的各種回調事件：
 * 
 * - VBVMR_CBCOMMAND_STARTING: 音訊引擎啟動時調用，讀取採樣率與緩衝區大小。
 * - VBVMR_CBCOMMAND_ENDING: 音訊引擎停止時調用。
 * - VBVMR_CBCOMMAND_CHANGE: 設備設定變更時調用，重新啟動音訊回調。
 * - VBVMR_CBCOMMAND_BUFFER_IN/BUFFER_OUT: 音訊緩衝區處理回調，
 *   從 Voicemeeter 讀取輸入並寫入輸出，與 JUCE 引擎交互。
 */
void VoicemeeterAudioIODevice::handleVoicemeeterCallback (long nCommand, void* lpData, long /*nnn*/)
{
    switch (nCommand)
    {
    case VBVMR_CBCOMMAND_STARTING:
    {
        // 音訊引擎啟動時調用，從 info 中讀取採樣率和緩衝區大小
        auto* info = (VBVMR_LPT_AUDIOINFO) lpData;
        currentSampleRate = static_cast<double> (info->samplerate);
        currentBufferSize = static_cast<int>    (info->nbSamplePerFrame);
        VMLOG ("STARTING: sr=" + juce::String (currentSampleRate) + " buf=" + juce::String (currentBufferSize));

        // 在 Message Thread 中通知 JUCE 音訊即將開始
        auto flag = aliveFlag;
        juce::MessageManager::callAsync ([this, flag]()
        {
            if (! flag->load()) return;  // 檢查設備是否仍活躍
            auto* cb = juceCallback.load();
            if (cb != nullptr)
                cb->audioDeviceAboutToStart (this);
        });
        break;
    }

    case VBVMR_CBCOMMAND_ENDING:
        // 音訊引擎停止時調用
        VMLOG ("ENDING");
        break;

    case VBVMR_CBCOMMAND_CHANGE:
    {
        // 設備設置變更時調用，重新啟動回調
        auto flag = aliveFlag;
        juce::MessageManager::callAsync ([this, flag]()
        {
            if (flag->load() && devicePlaying)
            {
                auto& api = VoicemeeterAPI::getInstance();
                if (api.isAvailable())
                    api.getInterface().VBVMR_AudioCallbackStart();  // 重新啟動回調
            }
        });
        break;
    }

    case VBVMR_CBCOMMAND_BUFFER_IN:
    case VBVMR_CBCOMMAND_BUFFER_OUT:
    {
        // 從 audiobuffer 中提取指定通道組的 float* 指標將其偵輸入給 JUCE
        auto* callback = juceCallback.load();
        if (callback == nullptr) break;

        auto* buffer = (VBVMR_LPT_AUDIOBUFFER) lpData;
        const int nbs     = buffer->audiobuffer_nbs;
        const int nbi     = buffer->audiobuffer_nbi;
        const int nbo     = buffer->audiobuffer_nbo;
        const int inBase  = inputBusIndex  * channelsPerBus; // Voicemeeter 輸入緩衝區偏移
        const int outBase = outputBusIndex * channelsPerBus; // Voicemeeter 輸出緩衝區偏移

        const int availableIn  = juce::jmin (channelsPerBus, juce::jmax (0, nbi - inBase));
        const int availableOut = juce::jmin (channelsPerBus, juce::jmax (0, nbo - outBase));

        if (availableIn <= 0 && availableOut <= 0) break;

        const float* inputPtrs[channelsPerBus]  = {};  // 輸入通道指標陣列
        float*       outputPtrs[channelsPerBus] = {};  // 輸出通道指標陣列
        int numActiveIn = 0, numActiveOut = 0;         // 活躍通道計數器

        // 蒐集活躍的輸入通道指標
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
// VoicemeeterAudioIODeviceType 實作
// scanForDevices: 登入 SDK 偵測類型，依不同類型建立對應的設備清單。
// createDevice: 結合輸入/輸出設備名建立對應 VoicemeeterAudioIODevice。
// ═══════════════════════════════════════════════════════════════

/**
 * VoicemeeterAudioIODeviceType::VoicemeeterAudioIODeviceType() 建構函式
 * 初始化 AudioIODeviceType，設定型別名稱為「Voicemeeter」。
 */
VoicemeeterAudioIODeviceType::VoicemeeterAudioIODeviceType()
    : juce::AudioIODeviceType ("Voicemeeter") {}

/**
 * VoicemeeterAudioIODeviceType::~VoicemeeterAudioIODeviceType() 析構函式
 */
VoicemeeterAudioIODeviceType::~VoicemeeterAudioIODeviceType() = default;

/**
 * VoicemeeterAudioIODeviceType::scanForDevices() 掃描 Voicemeeter 設備
 * 登入 Voicemeeter 偵測安裝的版本、建立對應的輸出設備清單。
 */
void VoicemeeterAudioIODeviceType::scanForDevices()
{
    deviceNames.clear();
    deviceBusIndices.clear();
    deviceIsInput.clear();
    voicemeeterType = 0;

    auto& api = VoicemeeterAPI::getInstance();
    if (! api.isAvailable()) return;

    auto& vmr = api.getInterface();
    // 登入 Voicemeeter Remote SDK 以獲取系統資訊
    long loginRep = vmr.VBVMR_Login();
    if (loginRep >= 0)
    {
        long vmType = 0;
        // 檢查 Voicemeeter 類型（標準版、Banana 版或 Potato 版）
        if (vmr.VBVMR_IsParametersDirty != nullptr &&
            vmr.VBVMR_GetVoicemeeterType != nullptr)
        {
            if (vmr.VBVMR_IsParametersDirty() >= 0)
                vmr.VBVMR_GetVoicemeeterType (&vmType);
        }
        vmr.VBVMR_Logout();
        if (vmType > 0) voicemeeterType = static_cast<int> (vmType);
    }

    // 若無法從 SDK 獲取類型，則嘗試從 Registry 檢測
    if (voicemeeterType == 0)
        voicemeeterType = api.detectTypeFromRegistry();
    // 預設為 Voicemeeter 標準版（2HW + 1VB）
    if (voicemeeterType == 0)
        voicemeeterType = 1;

    // 依類型建立：Voicemeeter=2HW+1VB，Banana=3HW+2VB，Potato=5HW+3VB
    int numHwOut = 2, numVirtOut = 1;
    switch (voicemeeterType)
    {
        case 1: numHwOut = 2; numVirtOut = 1; break;  // 標準版
        case 2: numHwOut = 3; numVirtOut = 2; break;  // Banana 版
        case 3: numHwOut = 5; numVirtOut = 3; break;  // Potato 版
        default: break;
    }

    // 添加硬體輸出（Output A）
    for (int i = 0; i < numHwOut; ++i)
    {
        deviceNames.add ("Output A" + juce::String (i + 1));
        deviceBusIndices.add (i);
        deviceIsInput.add (false);
    }
    // 添加虛擬輸出（Output B）
    for (int i = 0; i < numVirtOut; ++i)
    {
        deviceNames.add ("Output B" + juce::String (i + 1));
        deviceBusIndices.add (numHwOut + i);
        deviceIsInput.add (false);
    }

    VMLOG ("Devices: " + deviceNames.joinIntoString (", "));
}

/**
 * VoicemeeterAudioIODeviceType::getDeviceNames() 取得設備名稱清單
 */
juce::StringArray VoicemeeterAudioIODeviceType::getDeviceNames (bool) const { return deviceNames; }

/**
 * VoicemeeterAudioIODeviceType::getDefaultDeviceIndex() 取得預設設備索引
 * 若設備清單為空回傳 -1；否則回傳 0（第一個設備）。
 */
int VoicemeeterAudioIODeviceType::getDefaultDeviceIndex (bool) const
{ return deviceNames.isEmpty() ? -1 : 0; }

/**
 * VoicemeeterAudioIODeviceType::getIndexOfDevice() 取得設備索引
 * 根據 AudioIODevice 指標找出對應的設備名稱，
 * 然後在設備清單中查找該名稱的索引；若未找到回傳 -1。
 */
int VoicemeeterAudioIODeviceType::getIndexOfDevice (juce::AudioIODevice* device, bool asInput) const
{
    if (auto* vm = dynamic_cast<VoicemeeterAudioIODevice*> (device))
    {
        // 根據 asInput 參數決定使用輸入或輸出設備名稱
        auto name = asInput ? vm->getInputBusName() : vm->getOutputBusName();
        return deviceNames.indexOf (name);
    }
    return -1;
}

/**
 * VoicemeeterAudioIODeviceType::hasSeparateInputsAndOutputs() 輸入輸出分離
 * 回傳 true 表示輸入與輸出為獨立設備指定。
 */
bool VoicemeeterAudioIODeviceType::hasSeparateInputsAndOutputs() const { return true; }

/**
 * VoicemeeterAudioIODeviceType::createDevice() 建立設備實例
 * 根據輸出/輸入設備名稱在清單中尋找相應索引，建立 VoicemeeterAudioIODevice 實例。
 * 若設備名稱無效則回傳 nullptr。
 */
juce::AudioIODevice* VoicemeeterAudioIODeviceType::createDevice (
    const juce::String& outputDeviceName, const juce::String& inputDeviceName)
{
    // 在設備名稱清單中查找輸出和輸入設備的索引
    int outIndex = deviceNames.indexOf (outputDeviceName);
    if (outIndex < 0 && outputDeviceName.isNotEmpty()) return nullptr;

    // 若輸入設備名稱為空，則使用輸出設備索引；否則在清單中查找
    int inIndex = inputDeviceName.isNotEmpty() ? deviceNames.indexOf (inputDeviceName) : outIndex;
    if (inIndex < 0) inIndex = outIndex;

    // 確定實際使用的設備名稱和總線索引
    auto devName = outIndex >= 0 ? deviceNames[outIndex] : deviceNames[inIndex];
    auto inName  = inIndex  >= 0 ? deviceNames[inIndex]  : devName;
    int  inBus   = inIndex  >= 0 ? deviceBusIndices[inIndex]  : 0;
    int  outBus  = outIndex >= 0 ? deviceBusIndices[outIndex] : inBus;

    // 創建並返回新的 VoicemeeterAudioIODevice 實例
    return new VoicemeeterAudioIODevice (devName, inName, inBus, outBus);
}

// ═══════════════════════════════════════════════════════════════
// VoicemeeterDeviceManager 實作
// 覆寫 createAudioDeviceTypes，將 VoicemeeterAudioIODeviceType 加入設備類型清單。
// ═══════════════════════════════════════════════════════════════

/**
 * VoicemeeterDeviceManager::createAudioDeviceTypes() 建立設備類型清單
 * 將 VoicemeeterAudioIODeviceType 加入 JUCE 的設備類型清單。
 */
void VoicemeeterDeviceManager::createAudioDeviceTypes (juce::OwnedArray<juce::AudioIODeviceType>& types)
{
    types.add (new VoicemeeterAudioIODeviceType());
}

#endif // JUCE_WINDOWS
