/*
 * VoicemeeterInterface.h
 * VoicemeeterHost — Voicemeeter 音訊設備整合
 *
 * 實作基於 Voicemeeter Remote API 的 JUCE AudioIODevice / AudioIODeviceType。
 * 每個設備實例對應一個 Voicemeeter 輸出通道（A1–A5 / B1–B3），
 * 以插入效果模式（insert-effect mode）陰拉音訊流。
 *
 * 僅限 Windows 平台使用（包刧1在 #if JUCE_WINDOWS 內）。
 */

#pragma once

#include <JuceHeader.h>

#if JUCE_WINDOWS

#include "VoicemeeterRemote.h"
#include <atomic>
#include <memory>

// ─── VoicemeeterAPI 單例 ─────────────────────────────────
/**
 * VoicemeeterAPI (單例模式)
 * 負責載入 VoicemeeterRemote DLL，并提供所有 API 函式指標結構体 T_VBVMR_INTERFACE。
 * 程式啟動時即自動初始化一次，往後全局使用。
 */
class VoicemeeterAPI
{
public:
    /** 取得全局唯一實例。 */
    static VoicemeeterAPI& getInstance();

    /** 返回 DLL 是否成功載入（若未安裝 Voicemeeter 則為 false）。 */
    [[nodiscard]] bool               isAvailable()  const noexcept { return dllLoaded; }
    /** 取得 Voicemeeter API 函式指標結構体的可變參照。 */
    [[nodiscard]] T_VBVMR_INTERFACE& getInterface() noexcept       { return vmr; }
    /** 透過 Machine Registry 偵測 Voicemeeter 的安裝類型（1=Voicemeeter, 2=Banana, 3=Potato）。 */
    [[nodiscard]] int                detectTypeFromRegistry() const;

private:
    VoicemeeterAPI();
    ~VoicemeeterAPI();

    void* dllModule = nullptr;       ///< LoadLibrary 回傳的 DLL 句柄
    T_VBVMR_INTERFACE vmr{};         ///< API 函式指標結構体
    bool  dllLoaded = false;         ///< DLL 載入且所有必要函式均已綁定
    juce::String installDirectory;   ///< Voicemeeter 安裝路徑（從 Registry 讀取）
};

// ─── VoicemeeterAudioIODevice ─────────────────────────────────
/**
 * VoicemeeterAudioIODevice
 * 容納剛好一個 Voicemeeter 輸入/輸出通道組的 JUCE 設備抽象。
 * 實作 AudioIODevice 介面：
 *   - open()/close(): 登入/登出 Voicemeeter SDK，註冊音訊回調。
 *   - start()/stop(): 啟動/停止 Voicemeeter 音訊回調。
 *   - handleVoicemeeterCallback(): 由靜態 Trampoline 回調，轉交 JUCE AudioIODeviceCallback。
 */
class VoicemeeterAudioIODevice : public juce::AudioIODevice
{
public:
    /** 建構工具：指定輸出/輸入通道名稱與 Voicemeeter 內部通道索引。 */
    VoicemeeterAudioIODevice (const juce::String& outputBusName,
                              const juce::String& inputBusName,
                              int inputBusIndex,
                              int outputBusIndex);
    ~VoicemeeterAudioIODevice() override;

    // ── AudioIODevice 介面實作 ──
    /** 回傳輸出通道名稱清單（每個通道名稱如 "Out 1", "Out 2" 等）。 */
    [[nodiscard]] juce::StringArray getOutputChannelNames() override;
    /** 回傳輸入通道名稱清單。 */
    [[nodiscard]] juce::StringArray getInputChannelNames()  override;
    /** 回傳預設輸出通道選笮（預設緩衝區除法流的前兩淡道）。 */
    [[nodiscard]] std::optional<juce::BigInteger> getDefaultOutputChannels() const override;
    /** 回傳預設輸入通道選笮。 */
    [[nodiscard]] std::optional<juce::BigInteger> getDefaultInputChannels()  const override;
    /** 回傳支援的擁有率清單（48000, 44100）。 */
    [[nodiscard]] juce::Array<double> getAvailableSampleRates() override;
    /** 回傳支援的緩衝區大小清單。 */
    [[nodiscard]] juce::Array<int>    getAvailableBufferSizes() override;
    /** 回傳預設緩衝區大小（預設 512）。 */
    [[nodiscard]] int getDefaultBufferSize() override;

    /** 登入 Voicemeeter SDK，註冊音訊回調。 */
    [[nodiscard]] juce::String open (const juce::BigInteger& inputChannels,
                                     const juce::BigInteger& outputChannels,
                                     double sampleRate, int bufferSizeSamples) override;
    /** 登出 Voicemeeter SDK，取消注冊音訊回調。 */
    void close() override;
    /** 回傳設備是否已开啟。 */
    [[nodiscard]] bool isOpen() override;
    /** 啟動 Voicemeeter 音訊回調，設定 JUCE 回調指標。 */
    void start (juce::AudioIODeviceCallback* callback) override;
    /** 停止 Voicemeeter 音訊回調。 */
    void stop()  override;
    /** 回傳設備是否正在播放。 */
    [[nodiscard]] bool isPlaying() override;

    /** 取得目前緩衝區大小。 */
    [[nodiscard]] int    getCurrentBufferSizeSamples() override;
    /** 取得目前擁有率。 */
    [[nodiscard]] double getCurrentSampleRate()        override;
    /** 取得位元深度（固定回傳 32）。 */
    [[nodiscard]] int    getCurrentBitDepth()           override;
    /** 取得輸出延遲樣本數。 */
    [[nodiscard]] int    getOutputLatencyInSamples()    override;
    /** 取得輸入延遲樣本數。 */
    [[nodiscard]] int    getInputLatencyInSamples()     override;
    /** 取得目前活躍的輸出通道選笮。 */
    [[nodiscard]] juce::BigInteger getActiveOutputChannels() const override;
    /** 取得目前活躍的輸入通道選笮。 */
    [[nodiscard]] juce::BigInteger getActiveInputChannels()  const override;
    /** 取得最近一次錯誤訊息字串。 */
    [[nodiscard]] juce::String     getLastError() override;

    /** 取得輸入通道名稱的參照。 */
    [[nodiscard]] const juce::String& getInputBusName()  const noexcept { return inputBusName; }
    /** 取得輸出通道名稱（即設備名稱）的參照。 */
    [[nodiscard]] const juce::String& getOutputBusName() const          { return getName(); }

    /** 由靜態 Trampoline 回調呼叫，在音訊線程上轉交輸入/輸出緩衝區。 */
    void handleVoicemeeterCallback (long nCommand, void* lpData, long nnn);

private:
    static constexpr int channelsPerBus = 8; ///< 每個通道組最多 8 ch
    int inputBusIndex  = 0;   ///< Voicemeeter 輸入通道索引
    int outputBusIndex = 0;   ///< Voicemeeter 輸出通道索引
    juce::String inputBusName; ///< 輸入通道組的顯示名稱

    bool deviceOpen          = false; ///< 設備已開啟
    bool devicePlaying       = false; ///< 設備正在播放
    bool loggedIn            = false; ///< 已登入 Voicemeeter SDK
    bool callbackRegistered  = false; ///< 已完成 AudioCallbackRegister

    /** JUCE 音訊回調（原子操作，安全跨線程讀寫）。 */
    std::atomic<juce::AudioIODeviceCallback*> juceCallback { nullptr };

    juce::BigInteger activeInputChannels;   ///< 目前活躍輸入通道選笮
    juce::BigInteger activeOutputChannels;  ///< 目前活躍輸出通道選笮

    double currentSampleRate = 48000.0; ///< 目前擁有率
    int    currentBufferSize = 512;     ///< 目前緩衝區大小
    juce::String lastError;             ///< 最近一次錯誤訊息

    /** aliveFlag 共享指標：從回調 lambda 中安全判斷設備是否仍存活。 */
    std::shared_ptr<std::atomic<bool>> aliveFlag =
        std::make_shared<std::atomic<bool>> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoicemeeterAudioIODevice)
};

// ─── VoicemeeterAudioIODeviceType ────────────────────────────
/**
 * VoicemeeterAudioIODeviceType
 * 實作 JUCE AudioIODeviceType 介面，負責列舉與建立 VoicemeeterAudioIODevice。
 * scanForDevices() 會偵測 Voicemeeter 类型（Voicemeeter / Banana / Potato）并利用
 * 對應的通道數建立設備清單。
 */
class VoicemeeterAudioIODeviceType : public juce::AudioIODeviceType
{
public:
    VoicemeeterAudioIODeviceType();
    ~VoicemeeterAudioIODeviceType() override;

    /** 偵測 Voicemeeter 無線設備類型，建立對應的設備清單。 */
    void scanForDevices() override;
    /** 回傳所有可用設備的名稱清單。 */
    [[nodiscard]] juce::StringArray getDeviceNames (bool wantInputNames = false) const override;
    /** 回傳預設設備索引。 */
    [[nodiscard]] int  getDefaultDeviceIndex (bool forInput) const override;
    /** 找到指定設備的索引。 */
    [[nodiscard]] int  getIndexOfDevice (juce::AudioIODevice* device, bool asInput) const override;
    /** 回傳 true，表示輸入與輸出分配独立的設備定義。 */
    [[nodiscard]] bool hasSeparateInputsAndOutputs() const override;
    /** 根據輸出/輸入設備名稱建立對應的 VoicemeeterAudioIODevice。 */
    [[nodiscard]] juce::AudioIODevice* createDevice (const juce::String& outputDeviceName,
                                                      const juce::String& inputDeviceName) override;

private:
    juce::StringArray  deviceNames;       ///< 偵測到的設備名稱
    juce::Array<int>   deviceBusIndices;  ///< 對應的通道索引
    juce::Array<bool>  deviceIsInput;     ///< 設備是否為輸入端
    int voicemeeterType = 0;             ///< Voicemeeter 类型（1/2/3）

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoicemeeterAudioIODeviceType)
};

// ─── 自訂 AudioDeviceManager，註冊 Voicemeeter 設備類型 ───
/**
 * VoicemeeterDeviceManager
 * 繼承 juce::AudioDeviceManager，覆寫 createAudioDeviceTypes()
 * 以將 VoicemeeterAudioIODeviceType 加入設備型清單。
 * SystemTrayMenu 在 Windows 平台上使用此類而非標準 AudioDeviceManager。
 */
class VoicemeeterDeviceManager : public juce::AudioDeviceManager
{
public:
    /** 覆寫以加入 Voicemeeter 設備類型，再呼叫父類建立標準類型。 */
    void createAudioDeviceTypes (juce::OwnedArray<juce::AudioIODeviceType>& types) override;
};

#endif // JUCE_WINDOWS
