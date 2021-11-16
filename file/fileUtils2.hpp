#ifndef PAQ8PX_FILEUTILS2_HPP
#define PAQ8PX_FILEUTILS2_HPP

#include "FileDisk.hpp"

/**
 * Verify that the specified file exists and is readable, determine file size
  * @param filename
 * @return
 */
static auto getFileSize(const char *filename) -> uint64_t {
  FileDisk f;
  f.open(filename, true);
  f.setEnd();
  const auto fileSize = f.curPos();
  f.close();
  return fileSize;
}

#endif //PAQ8PX_FILEUTILS2_HPP
