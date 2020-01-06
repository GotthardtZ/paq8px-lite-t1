#ifndef PAQ8PX_UTILS_HPP
#define PAQ8PX_UTILS_HPP

#include <cstdint>
#include <algorithm>
#include <cstdio>
// Determining the proper printf() format specifier for 64 bit unsigned integers:
// - on Windows MSVC and MinGW-w64 use the MSVCRT runtime where it is "%I64u"
// - on Linux it is "%llu"
// The correct value is provided by the PRIu64 macro which is defined here:
#include <cinttypes> //PRIu64

// Platform-specific includes
#ifdef UNIX

#include <cstring> //strlen(), strcpy(), strcat(), strerror(), memset(), memcpy(), memmove()
#include <climits> //PATH_MAX (for OSX)
#include <unistd.h> //isatty()
#include <cerrno>  //errno

#else
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>   //CreateDirectoryW, CommandLineToArgvW, GetConsoleOutputCP, SetConsoleOutputCP
//GetCommandLineW, GetModuleFileNameW, GetStdHandle, GetTempFileName
//MultiByteToWideChar, WideCharToMultiByte,
//FileType, FILE_TYPE_PIPE, FILE_TYPE_DISK,
//uRetVal, DWORD, UINT, TRUE, MAX_PATH, CP_UTF8, etc.
#endif


#define DEFAULT_LEARNING_RATE 7

struct ErrorInfo {
    uint32_t data[2], sum, mask, collected;

    void reset() {
      memset(this, 0, sizeof(*this));
    }
};

static inline uint32_t square(uint32_t x) {
  return x * x;
}

static inline int min(int a, int b) { return std::min<int>(a, b); }

static inline uint64_t min(uint64_t a, uint64_t b) { return std::min<uint64_t>(a, b); }

static inline int max(int a, int b) { return std::max<int>(a, b); }

template<typename T>
constexpr bool isPowerOf2(T x) {
  return ((x & (x - 1)) == 0);
}

#ifndef NDEBUG
#if defined(UNIX)
#include <execinfo.h>
#define BACKTRACE() \
  { \
    void *callstack[128]; \
    int frames  = backtrace(callstack, 128); \
    char **strs = backtrace_symbols(callstack, frames); \
    for (int i = 0; i < frames; ++i) { \
      printf("%s\n", strs[i]); \
    } \
    free(strs); \
  }
#else
// TODO: How to implement this on Windows?
#define BACKTRACE() \
  { }
#endif
#endif

// A basic exception class to let catch() in main() know
// that the exception was thrown intentionally.
class IntentionalException : public std::exception {};

// Error handler: print message if any, and exit
void quit(const char *const message = nullptr) {
  if( message )
    printf("\n%s", message);
  printf("\n");
  throw IntentionalException();
}

//determine if output is redirected
static bool isOutputDirected() {
#ifdef WINDOWS
  DWORD FileType = GetFileType(GetStdHandle(STD_OUTPUT_HANDLE));
  return (FileType == FILE_TYPE_PIPE) || (FileType == FILE_TYPE_DISK);
#endif
#ifdef UNIX
  return !isatty(fileno(stdout));
#endif
}

static bool toScreen = !isOutputDirected();

/////////////////////// Global context /////////////////////////

typedef enum {
    DEFAULT = 0,
    FILECONTAINER,
    JPEG,
    HDR,
    IMAGE1,
    IMAGE4,
    IMAGE8,
    IMAGE8GRAY,
    IMAGE24,
    IMAGE32,
    AUDIO,
    AUDIO_LE,
    EXE,
    CD,
    ZLIB,
    BASE64,
    GIF,
    PNG8,
    PNG8GRAY,
    PNG24,
    PNG32,
    TEXT,
    TEXT_EOL,
    RLE,
    LZW
} BlockType;


inline bool hasRecursion(BlockType ft) {
  return ft == CD || ft == ZLIB || ft == BASE64 || ft == GIF || ft == RLE || ft == LZW || ft == FILECONTAINER;
}

inline bool hasInfo(BlockType ft) {
  return ft == IMAGE1 || ft == IMAGE4 || ft == IMAGE8 || ft == IMAGE8GRAY || ft == IMAGE24 || ft == IMAGE32 || ft == AUDIO ||
         ft == AUDIO_LE || ft == PNG8 || ft == PNG8GRAY || ft == PNG24 || ft == PNG32;
}

inline bool hasTransform(BlockType ft) {
  return ft == IMAGE24 || ft == IMAGE32 || ft == AUDIO_LE || ft == EXE || ft == CD || ft == ZLIB || ft == BASE64 || ft == GIF ||
         ft == TEXT_EOL || ft == RLE || ft == LZW;
}

inline bool isPNG(BlockType ft) { return ft == PNG8 || ft == PNG8GRAY || ft == PNG24 || ft == PNG32; }

uint32_t level = 0; //this value will be overwritten at the beginning of compression/decompression
#define MEM (uint64_t(65536) << level)

uint8_t options = 0;
#define OPTION_MULTIPLE_FILE_MODE 1U
#define OPTION_BRUTE 2U
#define OPTION_TRAINEXE 4U
#define OPTION_TRAINTXT 8U
#define OPTION_ADAPTIVE 16U
#define OPTION_SKIPRGB 32U

//////////////////// Cross-platform definitions /////////////////////////////////////

#ifdef _MSC_VER
#define fseeko(a, b, c) _fseeki64(a, b, c)
#define ftello(a) _ftelli64(a)
#else
#ifndef UNIX
#ifndef fseeko
#define fseeko(a, b, c) fseeko64(a, b, c)
#endif
#ifndef ftello
#define ftello(a) ftello64(a)
#endif
#endif
#endif

#ifdef WINDOWS
#define strcasecmp _stricmp
#endif

#if defined(__GNUC__) || defined(__clang__)
#define bswap(x) __builtin_bswap32(x)
#define bswap64(x) __builtin_bswap64(x)
#elif defined(_MSC_VER)
#define bswap(x) _byteswap_ulong(x)
#define bswap64(x) _byteswap_uint64(x)
#else
#define bswap(x) \
  +(((( x ) &0xff000000) >> 24) | +((( x ) &0x00ff0000) >> 8) | +((( x ) &0x0000ff00) << 8) | +((( x ) &0x000000ff) << 24))
#define bswap64(x) \
  +((x) >> 56) |
+   (((x)<<40) & 0x00FF000000000000) | \
+   (((x)<<24) & 0x0000FF0000000000) | \
+   (((x)<<8 ) & 0x000000FF00000000) | \
+   (((x)>>8 ) & 0x00000000FF000000) | \
+   (((x)>>24) & 0x0000000000FF0000) | \
+   (((x)>>40) & 0x000000000000FF00) | \
+   ((x) << 56))
#endif

#if defined(_MSC_VER)
#define ALWAYS_INLINE __forceinline
#elif defined(__GNUC__)
#define ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define ALWAYS_INLINE inline
#endif

#define TAB 0x09
#define NEW_LINE 0x0A
#define CARRIAGE_RETURN 0x0D
#define SPACE 0x20
#define QUOTE 0x22
#define APOSTROPHE 0x27

#ifndef NDEBUG
#if defined(UNIX)
#include <execinfo.h>
#define BACKTRACE() {\
      void* callstack[128]; \
      int frames = backtrace(callstack, 128); \
      char** strs = backtrace_symbols(callstack, frames); \
      for(int i = 0; i < frames; ++i) { \
        printf("%s\n", strs[i]); \
      } \
      free(strs); \
    }
#else
// TODO: How to implement this on Windows?
#define BACKTRACE() {}
#endif
#endif

#endif //PAQ8PX_UTILS_HPP
