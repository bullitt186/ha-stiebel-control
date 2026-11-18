/*
 * model_profile.h — what each heat pump model supports, in one place (#94).
 *
 * The model comes from the build flag -DHA_DEVICE_MODEL=<model> (set from the
 * `device_model` substitution). This file is the only place that looks at it:
 *   - preprocessor switches for the signal table and the language files
 *     (HA_CONTROLLER_WPMW2, HA_CONTROLLER_WPM3, HA_ELSTER_WPL_FAMILY),
 *   - `kModel`, a constexpr ModelProfile with the capability flags that the
 *     C++ code and the YAML lambdas read instead of comparing model names.
 *
 * Adding a model: add an HA_MODEL_ID_<model> number and a profile below (or let
 * it fall back to kProfileGeneric), plus its request table and model YAML.
 */

#ifndef MODEL_PROFILE_H
#define MODEL_PROFILE_H

#include "config.h"   // HA_DEVICE_MODEL

// ---------------------------------------------------------------------------
// Model identity (preprocessor). Unknown model names evaluate to 0 (generic).
// ---------------------------------------------------------------------------
#define HA_MODEL_ID_test_model 0
#define HA_MODEL_ID_unknown    0
#define HA_MODEL_ID_wpl9       1
#define HA_MODEL_ID_wpl10ac    2
#define HA_MODEL_ID_wpl13e     3
#define HA_MODEL_ID_wpl17      4
#define HA_MODEL_ID_wpl18e     5
#define HA_MODEL_ID_wpl23      6
#define HA_MODEL_ID_wpc7       7
#define HA_MODEL_ID_wpf10      8
#define HA_MODEL_ID_ttl33      9
#define HA_MODEL_ID_wpla7      10

#define HA_PP_CAT_(a, b) a##b
#define HA_PP_CAT(a, b) HA_PP_CAT_(a, b)
#define HA_MODEL_ID HA_PP_CAT(HA_MODEL_ID_, HA_DEVICE_MODEL)

// WPMW II manager (WPL13E, WPL18E, TTL 33): shared register corrections in the signal table
#if HA_MODEL_ID == HA_MODEL_ID_wpl13e || HA_MODEL_ID == HA_MODEL_ID_wpl18e || HA_MODEL_ID == HA_MODEL_ID_ttl33
  #define HA_CONTROLLER_WPMW2
#endif
// WPM3 manager (WPL 10 AC): its own operating mode names
#if HA_MODEL_ID == HA_MODEL_ID_wpl10ac
  #define HA_CONTROLLER_WPM3
#endif
// WPL-A manager (WPL-A 07): WPL-A status, pump and tuning registers in the signal table
#if HA_MODEL_ID == HA_MODEL_ID_wpla7
  #define HA_CONTROLLER_WPLA
#endif
// WPL family registers (CAN-B models and WPM3) in the signal table
#if HA_MODEL_ID == HA_MODEL_ID_wpl17 || HA_MODEL_ID == HA_MODEL_ID_wpl23 || \
    HA_MODEL_ID == HA_MODEL_ID_wpl10ac || HA_MODEL_ID == HA_MODEL_ID_wpc7 || \
    HA_MODEL_ID == HA_MODEL_ID_wpla7
  #define HA_ELSTER_WPL_FAMILY
#endif

#ifdef __cplusplus

// ---------------------------------------------------------------------------
// Capability flags
// ---------------------------------------------------------------------------
struct ModelProfile {
    bool wpmw2;                   // WPMW II manager
    bool wpmw2WritesVerified;     // WPMW II parameters, programs and heating circuit 2 written over
                                  // CAN and checked on the display (WPL13E, TTL 33)
    bool singleHeatingCircuit;    // only RAUMSOLLTEMP_I, no II/III entities
    bool canBRoutes;              // DHW and heating curve registers on KESSEL_CANB (WPL17)
    bool canBDhwRoutes;           // DHW setpoints written and read on KESSEL_CANB (WPL9)
    bool perMemberEnergy;         // energy totals from the HEIZMODUL_WPL counters (WPL17)
    bool ignoreUnusedIwsRuntime;  // drop VERD_1/KUEHL runtime answers the manager polls itself
                                  // (WPL13E: one compressor, no cooling)
    bool dhwActualOnKesselCanb;   // DHW actual temperature SPEICHERISTTEMP read from KESSEL_CANB (0x201)

    constexpr bool heatingCurveWritable() const { return canBRoutes || wpmw2WritesVerified; }
};

//                                       wpmw2  verified single canB   canBDhw perMember ignoreIws dhwActualCanb
inline constexpr ModelProfile kProfileGeneric{false, false,   false, false, false,  false,    false,    false};
inline constexpr ModelProfile kProfileWpl9   {false, false,   false, false, true,   false,    false,    false};
inline constexpr ModelProfile kProfileWpl13e {true,  true,    false, false, false,  false,    true,     false};
inline constexpr ModelProfile kProfileWpl17  {false, false,   true,  true,  false,  true,     false,    false};
// WPL18E polls the wpl13e table; WPMW II writes are not checked on its display yet
inline constexpr ModelProfile kProfileWpl18e {true,  false,   false, false, false,  false,    false,    false};
inline constexpr ModelProfile kProfileTtl33  {true,  true,    false, false, false,  false,    false,    false};
inline constexpr ModelProfile kProfileWpla7  {false, false,   true,  false, true,   false,    false,    true};

#if HA_MODEL_ID == HA_MODEL_ID_wpl9
inline constexpr const ModelProfile& kModel = kProfileWpl9;
#elif HA_MODEL_ID == HA_MODEL_ID_wpl13e
inline constexpr const ModelProfile& kModel = kProfileWpl13e;
#elif HA_MODEL_ID == HA_MODEL_ID_wpl17
inline constexpr const ModelProfile& kModel = kProfileWpl17;
#elif HA_MODEL_ID == HA_MODEL_ID_wpl18e
inline constexpr const ModelProfile& kModel = kProfileWpl18e;
#elif HA_MODEL_ID == HA_MODEL_ID_ttl33
inline constexpr const ModelProfile& kModel = kProfileTtl33;
#elif HA_MODEL_ID == HA_MODEL_ID_wpla7
inline constexpr const ModelProfile& kModel = kProfileWpla7;
#else
inline constexpr const ModelProfile& kModel = kProfileGeneric;
#endif

#endif // __cplusplus

#endif // MODEL_PROFILE_H
