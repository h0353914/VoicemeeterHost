/*
 * VoicemeeterRemote.h
 * VoicemeeterHost — Voicemeeter Remote API C 聲明
 *
 * Voicemeeter Remote SDK 的 C 風格 API 定義，包括：
 *   - 音訊資訊與緩衝區結構
 *   - 回調命令常數與音訊模式常數
 *   - 所有 API 函式指標型別
 *   - 統一的 API 介面結構（T_VBVMR_INTERFACE）
 *
 * 此標頭為外部 Voicemeeter SDK 的包裹聲明，
 * VoicemeeterAPI 類會動態載入並結合這些定義。
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * VBVMR_AUDIOINFO: 音訊資訊結構
     * 在 VBVMR_CBCOMMAND_STARTING 回調中傳遞，
     * 包含目前音訊設定的擁有率（samplerate）與每幀樣本數（nbSamplePerFrame）。
     */
    typedef struct tagVBVMR_AUDIOINFO
    {
        long samplerate;       ///< 目前擁有率（Hz），如 48000、44100
        long nbSamplePerFrame; ///< 每幀樣本數，如 512
    } VBVMR_T_AUDIOINFO, *VBVMR_LPT_AUDIOINFO;

    /**
     * VBVMR_AUDIOBUFFER: 音訊緩衝區結構
     * 在 VBVMR_CBCOMMAND_BUFFER_IN / BUFFER_OUT 回調中傳遞，
     * 包含讀寫指標陣列以存取各輸入輸出通道的音訊樣本。
     *   - audiobuffer_sr: 擁有率
     *   - audiobuffer_nbs: 本幀樣本數
     *   - audiobuffer_nbi: 可用輸入通道數
     *   - audiobuffer_nbo: 可用輸出通道數
     *   - audiobuffer_r[]: 輸入通道讀取指標陣列（最多 128 通道）
     *   - audiobuffer_w[]: 輸出通道寫入指標陣列（最多 128 通道）
     */
    typedef struct tagVBVMR_AUDIOBUFFER
    {
        long   audiobuffer_sr;   ///< 本幀擁有率
        long   audiobuffer_nbs;  ///< 本幀樣本數
        long   audiobuffer_nbi;  ///< 可用輸入通道數
        long   audiobuffer_nbo;  ///< 可用輸出通道數
        float* audiobuffer_r[128]; ///< 輸入通道讀取指標陣列
        float* audiobuffer_w[128]; ///< 輸出通道寫入指標陣列
    } VBVMR_T_AUDIOBUFFER, *VBVMR_LPT_AUDIOBUFFER;

    // ─── 音訊回調命令常數 ─────
    /** 音訊回調啟動命令：Voicemeeter 甫開始音訊串流。 */
    inline constexpr long VBVMR_CBCOMMAND_STARTING   = 1;
    /** 音訊回調結束命令：Voicemeeter 音訊串流將結束。 */
    inline constexpr long VBVMR_CBCOMMAND_ENDING     = 2;
    /** 音訊回調參數變化命令：Voicemeeter 設定有所改變。 */
    inline constexpr long VBVMR_CBCOMMAND_CHANGE     = 3;
    /** 音訊回調緩衝區輸入命令：可讀取輸入音訊資料。 */
    inline constexpr long VBVMR_CBCOMMAND_BUFFER_IN  = 10;
    /** 音訊回調緩衝區輸出命令：可寫入輸出音訊資料。 */
    inline constexpr long VBVMR_CBCOMMAND_BUFFER_OUT = 11;
    /** 音訊回調主通道命令：可存取主輸出通道音訊。 */
    inline constexpr long VBVMR_CBCOMMAND_BUFFER_MAIN= 20;

    // ─── 音訊回調模式常數 ─────
    /** 音訊回調模式：監聽輸入通道音訊。 */
    inline constexpr long VBVMR_AUDIOCALLBACK_IN   = 0x00000001;
    /** 音訊回調模式：監聽輸出通道音訊。 */
    inline constexpr long VBVMR_AUDIOCALLBACK_OUT  = 0x00000002;
    /** 音訊回調模式：監聽主輸出通道音訊。 */
    inline constexpr long VBVMR_AUDIOCALLBACK_MAIN = 0x00000004;

    /**
     * T_VBVMR_VBAUDIOCALLBACK: 音訊回調函式型別
     * 由 VBVMR_AudioCallbackRegister 登記，
     * Voicemeeter 音訊執行緒會定期呼叫此回調。
     *   - lpUser: 注冊時傳遞的使用者指標（通常為設備實例）
     *   - nCommand: 回調命令常數（VBVMR_CBCOMMAND_*）
     *   - lpData: 命令資料指標（型別依命令而定）
     *   - nnn: 保留參數（未使用）
     */
    typedef long (__stdcall* T_VBVMR_VBAUDIOCALLBACK)(void* lpUser, long nCommand,
                                                       void* lpData, long nnn);

    // ─── API 函式指標型別 ─────
    /** 登入 Voicemeeter SDK 的函式指標。 */
    typedef long (__stdcall* T_VBVMR_Login)(void);
    /** 登出 Voicemeeter SDK 的函式指標。 */
    typedef long (__stdcall* T_VBVMR_Logout)(void);
    /** 偵測 Voicemeeter 類型的函式指標（回傳 1=Voicemeeter、2=Banana、3=Potato）。 */
    typedef long (__stdcall* T_VBVMR_GetVoicemeeterType)(long* pType);
    /** 取得 Voicemeeter 版本的函式指標。 */
    typedef long (__stdcall* T_VBVMR_GetVoicemeeterVersion)(long* pVersion);
    /** 偵測 Voicemeeter 參數是否有變化的函式指標。 */
    typedef long (__stdcall* T_VBVMR_IsParametersDirty)(void);
    /**
     * 註冊音訊回調的函式指標。
     *   - mode: 回調模式（VBVMR_AUDIOCALLBACK_*）
     *   - pCallback: 回調函式指標
     *   - lpUser: 與回調綁定的使用者指標
     *   - szClientName: 客戶端名稱（64 位元 C 字串）
     */
    typedef long (__stdcall* T_VBVMR_AudioCallbackRegister)(long mode,
                                                             T_VBVMR_VBAUDIOCALLBACK pCallback,
                                                             void* lpUser,
                                                             char szClientName[64]);
    /** 啟動已註冊的音訊回調的函式指標。 */
    typedef long (__stdcall* T_VBVMR_AudioCallbackStart)(void);
    /** 停止已註冊的音訊回調的函式指標。 */
    typedef long (__stdcall* T_VBVMR_AudioCallbackStop)(void);
    /** 取消註冊音訊回調的函式指標。 */
    typedef long (__stdcall* T_VBVMR_AudioCallbackUnregister)(void);

    /**
     * T_VBVMR_INTERFACE: 統一 API 介面結構
     * VoicemeeterAPI 在動態載入 DLL 後，
     * 使用 GetProcAddress 填充此結構中的所有函式指標。
     * 所有成員指標必須成功綁定，否則初始化失敗。
     */
    typedef struct tagVBVMR_INTERFACE
    {
        T_VBVMR_Login                   VBVMR_Login;                   ///< 登入
        T_VBVMR_Logout                  VBVMR_Logout;                  ///< 登出
        T_VBVMR_GetVoicemeeterType      VBVMR_GetVoicemeeterType;      ///< 偵測類型
        T_VBVMR_GetVoicemeeterVersion   VBVMR_GetVoicemeeterVersion;   ///< 取得版本
        T_VBVMR_IsParametersDirty       VBVMR_IsParametersDirty;       ///< 檢查參數變化
        T_VBVMR_AudioCallbackRegister   VBVMR_AudioCallbackRegister;   ///< 註冊回調
        T_VBVMR_AudioCallbackStart      VBVMR_AudioCallbackStart;      ///< 啟動回調
        T_VBVMR_AudioCallbackStop       VBVMR_AudioCallbackStop;       ///< 停止回調
        T_VBVMR_AudioCallbackUnregister VBVMR_AudioCallbackUnregister; ///< 取消註冊回調
    } T_VBVMR_INTERFACE;

#ifdef __cplusplus
}
#endif

