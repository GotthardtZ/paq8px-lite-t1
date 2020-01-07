#ifndef PAQ8PX_FILEDISK_HPP
#define PAQ8PX_FILEDISK_HPP

/**
 * This class is responsible for files on disk.
 * It simply passes function calls to stdio
 */
class FileDisk : public File {
protected:
    FILE *file;

public:
    FileDisk() { file = nullptr; }

    ~FileDisk() override { close(); }

    bool open(const char *filename, bool mustSucceed) override {
      assert(file == nullptr);
      file = openfile(filename, READ);
      const bool success = (file != nullptr);
      if( !success && mustSucceed ) {
        printf("Unable to open file %s (%s)", filename, strerror(errno));
        quit();
      }
      return success;
    }

    void create(const char *filename) override {
      assert(file == nullptr);
      makeDirectories(filename);
      file = openfile(filename, WRITE);
      if( !file ) {
        printf("Unable to create file %s (%s)", filename, strerror(errno));
        quit();
      }
    }

    void createTmp() {
      assert(file == nullptr);
      file = makeTmpFile();
      if( !file ) {
        printf("Unable to create temporary file (%s)", strerror(errno));
        quit();
      }
    }

    void close() override {
      if( file )
        fclose(file);
      file = nullptr;
    }

    int getchar() override { return fgetc(file); }

    void putChar(uint8_t c) override { fputc(c, file); }

    uint64_t blockRead(uint8_t *ptr, uint64_t count) override { return fread(ptr, 1, count, file); }

    void blockWrite(uint8_t *ptr, uint64_t count) override { fwrite(ptr, 1, count, file); }

    void setpos(uint64_t newPos) override { fseeko(file, newPos, SEEK_SET); }

    void setEnd() override { fseeko(file, 0, SEEK_END); }

    uint64_t curPos() override { return ftello(file); }

    bool eof() override { return feof(file) != 0; }
};

#endif //PAQ8PX_FILEDISK_HPP
