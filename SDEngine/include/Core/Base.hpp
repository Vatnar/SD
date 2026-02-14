#pragma once

#if defined(__GNUC__) || defined(__clang__)
#define SD_DISABLE_WARNING_PUSH _Pragma("GCC diagnostic push")
#define SD_DISABLE_WARNING_POP _Pragma("GCC diagnostic pop")
#define SD_PRAGMA(x) _Pragma(#x)

#define SD_DISABLE_WARNING(w) SD_PRAGMA(GCC diagnostic ignored w)

#elif defined(_MSC_VER)
#define SD_DISABLE_WARNING_PUSH __pragma(warning(push))
#define SD_DISABLE_WARNING_POP __pragma(warning(pop))

#define SD_DISABLE_WARNING(w) __pragma(warning(disable : w))

#else
#define SD_DISABLE_WARNING_PUSH
#define SD_DISABLE_WARNING_POP
#define SD_DISABLE_WARNING(w)
#endif
