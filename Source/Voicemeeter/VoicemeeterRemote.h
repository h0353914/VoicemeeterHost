/*
 * VoicemeeterRemote.h
 * VoicemeeterHost — Voicemeeter Remote API C declarations
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct tagVBVMR_AUDIOINFO
    {
        long samplerate;
        long nbSamplePerFrame;
    } VBVMR_T_AUDIOINFO, *VBVMR_LPT_AUDIOINFO;

    typedef struct tagVBVMR_AUDIOBUFFER
    {
        long   audiobuffer_sr;
        long   audiobuffer_nbs;
        long   audiobuffer_nbi;
        long   audiobuffer_nbo;
        float* audiobuffer_r[128];
        float* audiobuffer_w[128];
    } VBVMR_T_AUDIOBUFFER, *VBVMR_LPT_AUDIOBUFFER;

    inline constexpr long VBVMR_CBCOMMAND_STARTING   = 1;
    inline constexpr long VBVMR_CBCOMMAND_ENDING     = 2;
    inline constexpr long VBVMR_CBCOMMAND_CHANGE     = 3;
    inline constexpr long VBVMR_CBCOMMAND_BUFFER_IN  = 10;
    inline constexpr long VBVMR_CBCOMMAND_BUFFER_OUT = 11;
    inline constexpr long VBVMR_CBCOMMAND_BUFFER_MAIN= 20;

    inline constexpr long VBVMR_AUDIOCALLBACK_IN   = 0x00000001;
    inline constexpr long VBVMR_AUDIOCALLBACK_OUT  = 0x00000002;
    inline constexpr long VBVMR_AUDIOCALLBACK_MAIN = 0x00000004;

    typedef long (__stdcall* T_VBVMR_VBAUDIOCALLBACK)(void* lpUser, long nCommand,
                                                       void* lpData, long nnn);

    typedef long (__stdcall* T_VBVMR_Login)(void);
    typedef long (__stdcall* T_VBVMR_Logout)(void);
    typedef long (__stdcall* T_VBVMR_GetVoicemeeterType)(long* pType);
    typedef long (__stdcall* T_VBVMR_GetVoicemeeterVersion)(long* pVersion);
    typedef long (__stdcall* T_VBVMR_IsParametersDirty)(void);
    typedef long (__stdcall* T_VBVMR_AudioCallbackRegister)(long mode,
                                                             T_VBVMR_VBAUDIOCALLBACK pCallback,
                                                             void* lpUser,
                                                             char szClientName[64]);
    typedef long (__stdcall* T_VBVMR_AudioCallbackStart)(void);
    typedef long (__stdcall* T_VBVMR_AudioCallbackStop)(void);
    typedef long (__stdcall* T_VBVMR_AudioCallbackUnregister)(void);

    typedef struct tagVBVMR_INTERFACE
    {
        T_VBVMR_Login                   VBVMR_Login;
        T_VBVMR_Logout                  VBVMR_Logout;
        T_VBVMR_GetVoicemeeterType      VBVMR_GetVoicemeeterType;
        T_VBVMR_GetVoicemeeterVersion   VBVMR_GetVoicemeeterVersion;
        T_VBVMR_IsParametersDirty       VBVMR_IsParametersDirty;
        T_VBVMR_AudioCallbackRegister   VBVMR_AudioCallbackRegister;
        T_VBVMR_AudioCallbackStart      VBVMR_AudioCallbackStart;
        T_VBVMR_AudioCallbackStop       VBVMR_AudioCallbackStop;
        T_VBVMR_AudioCallbackUnregister VBVMR_AudioCallbackUnregister;
    } T_VBVMR_INTERFACE;

#ifdef __cplusplus
}
#endif
