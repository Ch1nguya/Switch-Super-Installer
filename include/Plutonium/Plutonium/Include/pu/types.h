/**
 * @file switch/types.h
 * @brief Various system types.
 * @copyright libnx Authors
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdalign.h>

/// The maximum value of a u64.
#define U64_MAX	UINT64_MAX

#ifndef SSIZE_MAX
#ifdef SIZE_MAX
#define SSIZE_MAX ((SIZE_MAX) >> 1)
#endif
#endif

typedef uint8_t u8;       ///<   8-bit unsigned integer.
typedef uint16_t u16;     ///<  16-bit unsigned integer.
typedef uint32_t u32;     ///<  32-bit unsigned integer.
typedef uint64_t u64;     ///<  64-bit unsigned integer.
typedef __uint128_t u128; ///< 128-bit unsigned integer.

typedef int8_t s8;       ///<   8-bit signed integer.
typedef int16_t s16;     ///<  16-bit signed integer.
typedef int32_t s32;     ///<  32-bit signed integer.
typedef int64_t s64;     ///<  64-bit signed integer.
typedef __int128_t s128; ///< 128-bit unsigned integer.

typedef volatile u8 vu8;     ///<   8-bit volatile unsigned integer.
typedef volatile u16 vu16;   ///<  16-bit volatile unsigned integer.
typedef volatile u32 vu32;   ///<  32-bit volatile unsigned integer.
typedef volatile u64 vu64;   ///<  64-bit volatile unsigned integer.
typedef volatile u128 vu128; ///< 128-bit volatile unsigned integer.

typedef volatile s8 vs8;     ///<   8-bit volatile signed integer.
typedef volatile s16 vs16;   ///<  16-bit volatile signed integer.
typedef volatile s32 vs32;   ///<  32-bit volatile signed integer.
typedef volatile s64 vs64;   ///<  64-bit volatile signed integer.
typedef volatile s128 vs128; ///< 128-bit volatile signed integer.

typedef u32 Handle;                 ///< Kernel object handle.
typedef u32 Result;                 ///< Function error code result type.
typedef void (*ThreadFunc)(void *); ///< Thread entrypoint function.
typedef void (*VoidFn)(void);       ///< Function without arguments nor return value.

/// Creates a bitmask from a bit number.
#ifndef BIT
#define BIT(n) (1U<<(n))
#endif

/// Marks a function as not returning, for the purposes of compiler optimization.
#ifndef NORETURN
#define NORETURN   __attribute__((noreturn))
#endif

/// Performs a dummy operation on the specified argument in order to silence compiler warnings about unused arguments.
#ifndef IGNORE_ARG
#define IGNORE_ARG(x) (void)(x)
#endif

/// Flags a function as deprecated.
#ifndef DEPRECATED
#ifndef LIBNX_NO_DEPRECATION
#define DEPRECATED __attribute__ ((deprecated))
#else
#define DEPRECATED
#endif
#endif

/// Flags a function as (always) inline.
#define NX_INLINE __attribute__((always_inline)) static inline

/// Flags a function as constexpr in C++14 and above; or as (always) inline otherwise.
#if __cplusplus >= 201402L
#define NX_CONSTEXPR NX_INLINE constexpr
#else
#define NX_CONSTEXPR NX_INLINE
#endif

/// Invalid handle.
#define INVALID_HANDLE ((Handle) 0)


typedef enum {
    KEY_A            = BIT(0),       ///< A
    KEY_B            = BIT(1),       ///< B
    KEY_X            = BIT(2),       ///< X
    KEY_Y            = BIT(3),       ///< Y
    KEY_LSTICK       = BIT(4),       ///< Left Stick Button
    KEY_RSTICK       = BIT(5),       ///< Right Stick Button
    KEY_L            = BIT(6),       ///< L
    KEY_R            = BIT(7),       ///< R
    KEY_ZL           = BIT(8),       ///< ZL
    KEY_ZR           = BIT(9),       ///< ZR
    KEY_PLUS         = BIT(10),      ///< Plus
    KEY_MINUS        = BIT(11),      ///< Minus
    KEY_DLEFT        = BIT(12),      ///< D-Pad Left
    KEY_DUP          = BIT(13),      ///< D-Pad Up
    KEY_DRIGHT       = BIT(14),      ///< D-Pad Right
    KEY_DDOWN        = BIT(15),      ///< D-Pad Down
    KEY_LSTICK_LEFT  = BIT(16),      ///< Left Stick Left
    KEY_LSTICK_UP    = BIT(17),      ///< Left Stick Up
    KEY_LSTICK_RIGHT = BIT(18),      ///< Left Stick Right
    KEY_LSTICK_DOWN  = BIT(19),      ///< Left Stick Down
    KEY_RSTICK_LEFT  = BIT(20),      ///< Right Stick Left
    KEY_RSTICK_UP    = BIT(21),      ///< Right Stick Up
    KEY_RSTICK_RIGHT = BIT(22),      ///< Right Stick Right
    KEY_RSTICK_DOWN  = BIT(23),      ///< Right Stick Down
    KEY_SL_LEFT      = BIT(24),      ///< SL on Left Joy-Con
    KEY_SR_LEFT      = BIT(25),      ///< SR on Left Joy-Con
    KEY_SL_RIGHT     = BIT(26),      ///< SL on Right Joy-Con
    KEY_SR_RIGHT     = BIT(27),      ///< SR on Right Joy-Con

    KEY_HOME         = BIT(18),      ///< HOME button, only available for use with HiddbgHdlsState::buttons.
    KEY_CAPTURE      = BIT(19),      ///< Capture button, only available for use with HiddbgHdlsState::buttons.

    // Pseudo-key for at least one finger on the touch screen
    KEY_TOUCH       = BIT(28),

    // Buttons by orientation (for single Joy-Con), also works with Joy-Con pairs, Pro Controller
    KEY_JOYCON_RIGHT = BIT(0),
    KEY_JOYCON_DOWN  = BIT(1),
    KEY_JOYCON_UP    = BIT(2),
    KEY_JOYCON_LEFT  = BIT(3),

    // Generic catch-all directions, also works for single Joy-Con
    KEY_UP    = KEY_DUP     | KEY_LSTICK_UP    | KEY_RSTICK_UP,    ///< D-Pad Up or Sticks Up
    KEY_DOWN  = KEY_DDOWN   | KEY_LSTICK_DOWN  | KEY_RSTICK_DOWN,  ///< D-Pad Down or Sticks Down
    KEY_LEFT  = KEY_DLEFT   | KEY_LSTICK_LEFT  | KEY_RSTICK_LEFT,  ///< D-Pad Left or Sticks Left
    KEY_RIGHT = KEY_DRIGHT  | KEY_LSTICK_RIGHT | KEY_RSTICK_RIGHT, ///< D-Pad Right or Sticks Right
    KEY_SL    = KEY_SL_LEFT | KEY_SL_RIGHT,                        ///< SL on Left or Right Joy-Con
    KEY_SR    = KEY_SR_LEFT | KEY_SR_RIGHT,                        ///< SR on Left or Right Joy-Con
} HidControllerKeys;

/// HidControllerID
typedef enum {
    CONTROLLER_PLAYER_1 = 0,
    CONTROLLER_PLAYER_2 = 1,
    CONTROLLER_PLAYER_3 = 2,
    CONTROLLER_PLAYER_4 = 3,
    CONTROLLER_PLAYER_5 = 4,
    CONTROLLER_PLAYER_6 = 5,
    CONTROLLER_PLAYER_7 = 6,
    CONTROLLER_PLAYER_8 = 7,
    CONTROLLER_HANDHELD = 8,
    CONTROLLER_UNKNOWN  = 9,
    CONTROLLER_P1_AUTO = 10, ///< Not an actual HID-sysmodule ID. Only for hidKeys*()/hidJoystickRead()/hidSixAxisSensorValuesRead()/hidGetControllerType()/hidGetControllerColors()/hidIsControllerConnected(). Automatically uses CONTROLLER_PLAYER_1 when connected, otherwise uses CONTROLLER_HANDHELD.
} HidControllerID;

/// touchPosition
typedef struct touchPosition {
    u32 id;
    u32 px;
    u32 py;
    u32 dx;
    u32 dy;
    u32 angle;
} touchPosition;

/// HidVector
typedef struct HidVector {
    float x;
    float y;
    float z;
} HidVector;

/// SixAxisSensorValues
typedef struct SixAxisSensorValues {
    HidVector accelerometer;
    HidVector gyroscope;
    HidVector unk;
    HidVector orientation[3];
} SixAxisSensorValues;

/// HID controller type
typedef enum {
    TYPE_PROCONTROLLER = BIT(0),
    TYPE_HANDHELD      = BIT(1),
    TYPE_JOYCON_PAIR   = BIT(2),
    TYPE_JOYCON_LEFT   = BIT(3),
    TYPE_JOYCON_RIGHT  = BIT(4),

    TYPE_SYSTEM_EXT    = BIT(29),
    TYPE_SYSTEM        = BIT(30),
} HidControllerType;

#define R_SUCCEEDED(res)   ((res)==0)