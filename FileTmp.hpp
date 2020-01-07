#ifndef PAQ8PX_FILETMP_HPP
#define PAQ8PX_FILETMP_HPP

/**
 * This class is responsible for temporary files in RAM or on disk
 * Initially it uses RAM for temporary file content.
 * In case of the content size in RAM grows too large, it is written to disk,
 * the RAM is freed and all subsequent file operations will use the file on disk.
 */
class FileTmp : public File {
private:
    //file content in ram
    Array<uint8_t> *contentInRam; //content of file
    uint64_t filePos;
    uint64_t fileSize;

    void forgetContentInRam() {
      if( contentInRam ) {
        delete contentInRam;
        contentInRam = nullptr;
        filePos = 0;
        fileSize = 0;
      }
    }

    //file on disk
    FileDisk *fileOnDisk;

    void forgetFileOnDisk() {
      if( fileOnDisk ) {
        fileOnDisk->close();
        delete fileOnDisk;
        fileOnDisk = nullptr;
      }
    }
    //switch: ram->disk
#ifdef NDEBUG
    static constexpr uint32_t MAX_RAM_FOR_TMP_CONTENT = 64 * 1024 * 1024; //64 MB (per file)
#else
    static constexpr uint32_t MAX_RAM_FOR_TMP_CONTENT = 64; // to trigger switching to disk earlier - for testing
#endif

    void ramToDisk() {
      assert(fileOnDisk == nullptr);
      fileOnDisk = new FileDisk();
      fileOnDisk->createTmp();
      if( fileSize > 0 )
        fileOnDisk->blockWrite(&((*contentInRam)[0]), fileSize);
      fileOnDisk->setpos(filePos);
      forgetContentInRam();
    }

public:
    FileTmp() {
      contentInRam = new Array<uint8_t>(0);
      filePos = 0;
      fileSize = 0;
      fileOnDisk = nullptr;
    }

    ~FileTmp() override { close(); }

    /**
     *  This method is forbidden for temporary files.
     */
    bool open(const char *, bool) override {
      assert(false);
      return false;
    }
    /**
     *  This method is forbidden for temporary files.
     */
    void create(const char *) override { assert(false); }
    void close() override {
      forgetContentInRam();
      forgetFileOnDisk();
    }

    int getchar() override {
      if( contentInRam ) {
        if( filePos >= fileSize )
          return EOF;
        else {
          const uint8_t c = (*contentInRam)[filePos];
          filePos++;
          return c;
        }
      } else
        return fileOnDisk->getchar();
    }

    void putChar(uint8_t c) override {
      if( contentInRam ) {
        if( filePos < MAX_RAM_FOR_TMP_CONTENT ) {
          if( filePos == fileSize ) {
            contentInRam->pushBack(c);
            fileSize++;
          } else
            (*contentInRam)[filePos] = c;
          filePos++;
          return;
        } else
          ramToDisk();
      }
      fileOnDisk->putChar(c);
    }

    uint64_t blockRead(uint8_t *ptr, uint64_t count) override {
      if( contentInRam ) {
        const uint64_t available = fileSize - filePos;
        if( available < count )
          count = available;
        if( count > 0 )
          memcpy(ptr, &((*contentInRam)[filePos]), count);
        filePos += count;
        return count;
      } else
        return fileOnDisk->blockRead(ptr, count);
    }

    void blockWrite(uint8_t *ptr, uint64_t count) override {
      if( contentInRam ) {
        if( filePos + count <= MAX_RAM_FOR_TMP_CONTENT ) {
          contentInRam->resize((filePos + count));
          if( count > 0 )
            memcpy(&((*contentInRam)[filePos]), ptr, count);
          fileSize += count;
          filePos += count;
          return;
        } else
          ramToDisk();
      }
      fileOnDisk->blockWrite(ptr, count);
    }

    void setpos(uint64_t newPos) override {
      if( contentInRam ) {
        if( newPos > fileSize )
          ramToDisk(); //panic: we don't support seeking past end of file (but stdio does) - let's switch to disk
        else {
          filePos = newPos;
          return;
        }
      }
      fileOnDisk->setpos(newPos);
    }

    void setEnd() override {
      if( contentInRam )
        filePos = fileSize;
      else
        fileOnDisk->setEnd();
    }

    uint64_t curPos() override {
      if( contentInRam )
        return filePos;
      else
        return fileOnDisk->curPos();
    }

    bool eof() override {
      if( contentInRam )
        return filePos >= fileSize;
      else
        return fileOnDisk->eof();
    }
};

#endif //PAQ8PX_FILETMP_HPP
