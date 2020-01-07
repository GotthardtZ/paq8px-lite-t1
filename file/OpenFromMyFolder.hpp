#ifndef PAQ8PX_OPENFROMMYFOLDER_HPP
#define PAQ8PX_OPENFROMMYFOLDER_HPP

#include <mach-o/dyld.h>
#include "FileDisk.hpp"

namespace ofmf {
    static char myPathError[] = "Can't determine my path.";
}

/**
 * Helper class: open a file from the executable's folder
 */
class OpenFromMyFolder {
private:
public:

    /**
     * This static method will open the executable itself for reading
     * @param f
     */
    static void myself(FileDisk *f) {
#ifdef WINDOWS
      int i;
      Array<wchar_t> myFileName(MAX_PATH);
      if ((i = GetModuleFileNameW(nullptr, &myFileName[0], MAX_PATH)) && i < MAX_PATH && i != 0) {
        f->open(Utf8Str(&myFileName[0]).utf8_str, true);
      } else
        quit(myPathError);
#endif
#ifdef __APPLE__
#endif
      char myFileName[PATH_MAX];
      uint32_t size = sizeof(myFileName);
      if( _NSGetExecutablePath(myFileName, &size) == 0 ) {
        f->open(&myFileName[0], true);
      } else {
        quit(ofmf::myPathError);
      }
#ifdef UNIX
#ifndef __APPLE__
      Array<char> myFileName(PATH_MAX + 1);
      if( readlink("/proc/self/exe", &myFileName[0], PATH_MAX) != -1 )
        f->open(&myFileName[0], true);
      else
        quit(myPathError);
#endif
#endif
    }

    /**
     * This static method will open a file from the executable's folder.
     * Only ASCII file names are supported.
     * @param f
     * @param filename
     */
    static void anotherFile(FileDisk *f, const char *filename) {
      const uint64_t fLength = strlen(filename) + 1;
#ifdef WINDOWS
      int i;
      Array<char> myFileName(MAX_PATH + fLength);
      if ((i = GetModuleFileName(nullptr, &myFileName[0], MAX_PATH)) && i < MAX_PATH && i != 0) {
        char *endofpath = strrchr(&myFileName[0], '\\');
#endif
#ifdef __APPLE__
      char myFileName[PATH_MAX + fLength];
      uint32_t size = sizeof(myFileName);
      if( _NSGetExecutablePath(myFileName, &size) == 0 ) {
        char *endOfPath = strrchr(&myFileName[0], '/');
#endif
#ifdef UNIX
#ifndef __APPLE__
        char myFileName[PATH_MAX + fLength];
        if( readlink("/proc/self/exe", myFileName, PATH_MAX) != -1 ) {
          char *endOfPath = strrchr(&myFileName[0], '/');
#endif
#endif
        if( endOfPath == nullptr ) {
          quit(ofmf::myPathError);
        }
        endOfPath++;
        strcpy(endOfPath, filename); //append filename to my path
        f->open(&myFileName[0], true);
      } else {
        quit(ofmf::myPathError);
      }
    }

};

#endif //PAQ8PX_OPENFROMMYFOLDER_HPP
