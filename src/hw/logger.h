#pragma once

// Log severity levels - higher number is more verbose.
#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_WARN 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_DEBUG 3

// Compile-time threshold (B16): LOG_* calls above this level expand to
// nothing, so disabled log lines - including any String concatenation in
// their arguments - cost zero flash/CPU. Override per-environment with
// `-D LOG_LEVEL=LOG_LEVEL_DEBUG` in platformio.ini build_flags.
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#define LOGGER Serial
#define LOG Serial.println

#if LOG_LEVEL >= LOG_LEVEL_WARN
#define LOG_WARN(x) Serial.println(x)
#else
#define LOG_WARN(x)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_INFO(x) Serial.println(x)
#else
#define LOG_INFO(x)
#endif

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_DEBUG(x) Serial.println(x)
#else
#define LOG_DEBUG(x)
#endif
