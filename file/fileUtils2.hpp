#ifndef PAQ8PX_FILEUTILS2_HPP
#define PAQ8PX_FILEUTILS2_HPP

#include "FileDisk.hpp"

/**
 * Verify that the specified file exists and is readable, determine file size
 * @param filename
 * @return
 */
static uint64_t getFileSize(const char *filename) {
  FileDisk f;
  f.open(filename, true);
  f.setEnd();
  const auto fileSize = f.curPos();
  f.close();
  if((fileSize >> 31U) != 0 )
    quit("Large files not supported.");
  return fileSize;
}

static void appendToFile(const char *filename, const char *s) {
  FILE *f = openFile(filename, APPEND);
  if( f == nullptr )
    printf("Warning: could not log compression results to %s\n", filename);
  else {
    fprintf(f, "%s", s);
    fclose(f);
  }
}

#endif //PAQ8PX_FILEUTILS2_HPP
