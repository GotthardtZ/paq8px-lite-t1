#ifndef PAQ8PX_FILENAME_HPP
#define PAQ8PX_FILENAME_HPP

#include "fileUtils.hpp"
#include "../String.hpp"

/**
 * A class to represent a filename
 */
class FileName : public String {
public:
    FileName(const char *s = "") : String(s) {};

    [[nodiscard]] int lastSlashPos() const {
      int pos = findLast('/');
      if( pos < 0 )
        pos = findLast('\\');
      return pos; //has no path when -1
    }

    void keepFilename() {
      const int pos = lastSlashPos();
      stripStart(pos + 1); //don't keep last slash
    }

    void keepPath() {
      const int pos = lastSlashPos();
      stripEnd(strsize() - pos - 1); //keep last slash
    }

    void replaceSlashes() { //prepare path string for screen output
      const uint64_t sSize = strsize();
      for( uint64_t i = 0; i < sSize; i++ )
        if((*this)[i] == BADSLASH )
          (*this)[i] = GOODSLASH;
      chk_consistency();
    }
};


#endif //PAQ8PX_FILENAME_HPP
