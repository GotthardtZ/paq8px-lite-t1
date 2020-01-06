#ifndef PAQ8PX_FILE_HPP
#define PAQ8PX_FILE_HPP

#include <cerrno>
#include "String.hpp"

//The preferred slash for displaying
//We will change the BADSLASH to GOODSLASH before displaying a path string to the user
#ifdef WINDOWS
#define BADSLASH '/'
#define GOODSLASH '\\'
#else
#define BADSLASH '\\'
#define GOODSLASH '/'
#endif

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


//////////////////// IO functions and classes ///////////////////
// These functions/classes are responsible for all the file
// and directory operations.


// Wrappers to utf8 vs. wchar functions
// Linux i/o works with utf8 char* but on windows it's wchar_t*.
// We'll be using utf8 char* in all our functions, so we need to convert to/from
// wchar_t* when on windows.

//only for Windows: a class encapsulating a wchar string converted from a utf8 string
//purpose: to properly free the allocated string buffer on destruction
#ifdef WINDOWS
class WcharStr {
  private:
  //convert a utf8 string to a wchar string
  wchar_t *utf8_to_wchar_str(const char *utf8_str) {
    int buffersize     = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
    wchar_t *wchar_str = new wchar_t[buffersize];
    MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wchar_str, buffersize);
    return wchar_str;
  }

  public:
  wchar_t *wchar_str;
  WcharStr(const char *utf8_str) {
    wchar_str = utf8_to_wchar_str(utf8_str);
  }
  ~WcharStr() {
    delete[] wchar_str;
  }
};
#endif

//only for Windows: a class encapsulating a utf8 string converted from a wchar string
//purpose: to properly free the allocated string buffer on destruction
#ifdef WINDOWS
class Utf8Str {
  private:
  //convert a wchar string to a utf8 string
  char *wchar_to_utf8_str(const wchar_t *wchar_str) {
    int buffersize = WideCharToMultiByte(CP_UTF8, 0, wchar_str, -1, NULL, 0, NULL, NULL);
    char *utf8_str = new char[buffersize];
    WideCharToMultiByte(CP_UTF8, 0, wchar_str, -1, utf8_str, buffersize, NULL, NULL);
    return utf8_str;
  }

  public:
  char *utf8_str;
  Utf8Str(const wchar_t *wchar_str) {
    utf8_str = wchar_to_utf8_str(wchar_str);
  }
  ~Utf8Str() {
    delete[] utf8_str;
  }
};
#endif

#ifdef WINDOWS
#define STAT _stat
#else
#define STAT stat
#endif

static constexpr int READ = 0;
static constexpr int WRITE = 1;
static constexpr int APPEND = 2;

// Wrapper function (Linux vs Windows) to open a file
FILE *openfile(const char *filename, const int mode) {
  FILE *file;
#ifdef WINDOWS
  file = _wfopen(WcharStr(filename).wchar_str, mode == READ ? L"rb" : mode == WRITE ? L"wb+" : L"a");
#else
  file = fopen(filename, mode == READ ? "rb" : mode == WRITE ? "wb+" : "a");
#endif
  return file;
}

#ifdef WINDOWS
#define STAT _stat
#else
#define STAT stat
#endif

// Wrapper function (Linux vs Windows) to examine a path
bool statPath(const char *path, struct STAT &status) {
#ifdef WINDOWS
  return _wstat(WcharStr(path).wchar_str, &status);
#else
  return stat(path, &status);
#endif
}

//////////////////// Folder operations ///////////////////////////

//examines given "path" and returns:
//0: on error
//1: when exists and is a file
//2: when exists and is a directory
//3: when does not exist, but looks like a file       /abcd/efgh
//4: when does not exist, but looks like a directory  /abcd/efgh/
static int examinePath(const char *path) {
  struct STAT status;
  const bool success = statPath(path, status) == 0;
  if( !success ) {
    if( errno == ENOENT ) { //no such file or directory
      const int len = (int) strlen(path);
      if( len == 0 )
        return 0; //error: path is an empty string
      const char lastChar = path[len - 1];
      if( lastChar != '/' && lastChar != '\\' )
        return 3; //looks like a file
      else
        return 4; //looks like a directory
    }
    return 0; //error
  }
  if((status.st_mode & S_IFREG) != 0 )
    return 1; //existing file
  if((status.st_mode & S_IFDIR) != 0 )
    return 2; //existing directory
  return 0; //error: "path" may be a socket, symlink, named pipe, etc.
}

//creates a directory if it does not exist
static int makedir(const char *dir) {
  if( examinePath(dir) == 2 ) //existing directory
    return 2; //2: directory already exists, no need to create
#ifdef WINDOWS
  const bool created = (CreateDirectoryW(WcharStr(dir).wchar_str, nullptr) == TRUE);
#else
#ifdef UNIX
  const bool created = (mkdir(dir, 0777) == 0);
#else
#error Unknown target system
#endif
#endif
  return created ? 1 : 0; //0: failed, 1: created successfully
}

//creates directories recursively if they don't exist
static void makeDirectories(const char *filename) {
  String path(filename);
  uint64_t start = 0;
  if( path[1] == ':' )
    start = 2; //skip drive letter (c:)
  if( path[start] == '/' || path[start] == '\\' )
    start++; //skip leading slashes (root dir)
  for( uint64_t i = start; path[i] != 0; ++i ) {
    if( path[i] == '/' || path[i] == '\\' ) {
      char saveChar = path[i];
      path[i] = 0;
      const char *dirName = path.c_str();
      const int created = makedir(dirName);
      if( created == 0 ) {
        printf("Unable to create directory %s", dirName);
        quit();
      }
      if( created == 1 )
        printf("Created directory %s\n", dirName);
      path[i] = saveChar;
    }
  }
}


/////////////////////////// File /////////////////////////////
// The main purpose of these classes is to keep temporary files in
// RAM as mush as possible. The default behaviour is to simply pass
// function calls to the operating system - except in case of temporary
// files.

// Helper function: create a temporary file
//
// On Windows when using tmpFile() the temporary file may be created
// in the root directory causing access denied error when User Account Control (UAC) is on.
// To avoid this issue with tmpFile() we simply use fopen() instead.
// We create the temporary file in the directory where the executable is launched from.
// Luckily the MS c runtime library provides two (MS specific) fopen() flags: "T"emporary and "D"elete.

FILE *makeTmpFile() {
#if defined(WINDOWS)
  char szTempFileName[MAX_PATH];
  const UINT uRetVal = GetTempFileName(TEXT("."), TEXT("tmp"), 0, szTempFileName);
  if (uRetVal == 0) return nullptr;
  return fopen(szTempFileName, "w+bTD");
#else
  return tmpfile();
#endif
}

//This is the base class.
//This is an abstract class for all the required file operations.

class File {
public:
    virtual ~File() {};
    virtual bool open(const char *filename, bool mustSucceed) = 0;
    virtual void create(const char *filename) = 0;
    virtual void close() = 0;
    virtual int getchar() = 0;
    virtual void putChar(uint8_t c) = 0;

    void append(const char *s) {
      for( int i = 0; s[i]; i++ )
        putChar((uint8_t) s[i]);
    }

    virtual uint64_t blockRead(uint8_t *ptr, uint64_t count) = 0;
    virtual void blockWrite(uint8_t *ptr, uint64_t count) = 0;

    uint32_t get32() { return (getchar() << 24) | (getchar() << 16) | (getchar() << 8) | (getchar()); }

    void put32(uint32_t x) {
      putChar((x >> 24) & 255);
      putChar((x >> 16) & 255);
      putChar((x >> 8) & 255);
      putChar(x & 255);
    }

    uint64_t getVLI() {
      uint64_t i = 0;
      int k = 0;
      uint8_t b = 0;
      do {
        b = getchar();
        i |= uint64_t((b & 0x7FU) << k);
        k += 7;
      } while((b >> 7) > 0 );
      return i;
    }

    void putVLI(uint64_t i) {
      while( i > 0x7F ) {
        putChar(0x80U | (i & 0x7FU));
        i >>= 7U;
      }
      putChar(uint8_t(i));
    }

    virtual void setpos(uint64_t newPos) = 0;
    virtual void setEnd() = 0;
    virtual uint64_t curPos() = 0;
    virtual bool eof() = 0;
};

// This class is responsible for files on disk
// It simply passes function calls to stdio

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

    bool open(const char *, bool) override {
      assert(false);
      return false;
    } //this method is forbidden for temporary files
    void create(const char *) override { assert(false); } //this method is forbidden for temporary files
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

// Helper class: open a file from the executable's folder

static char myPathError[] = "Can't determine my path.";

class OpenFromMyFolder {
private:
public:
    //this static method will open the executable itself for reading
    static void myself(FileDisk *f) {
#ifdef WINDOWS
      int i;
      Array<wchar_t> myFileName(MAX_PATH);
      if ((i = GetModuleFileNameW(nullptr, &myFileName[0], MAX_PATH)) && i < MAX_PATH && i != 0) {
        f->open(Utf8Str(&myFileName[0]).utf8_str, true);
      } else
        quit(myPathError);
#else
      // TODO: this doesn't work on mac OS
      Array<char> myFileName(PATH_MAX + 1);
      if( readlink("/proc/self/exe", &myFileName[0], PATH_MAX) != -1 )
        f->open(&myFileName[0], true);
      else
        quit(myPathError);
#endif
    }

    //this static method will open a file from the executable's folder
    //only ASCII file names are supported
    static void anotherFile(FileDisk *f, const char *filename) {
      const uint64_t fLength = strlen(filename) + 1;
#ifdef WINDOWS
      int i;
      Array<char> myFileName(MAX_PATH + fLength);
      if ((i = GetModuleFileName(nullptr, &myFileName[0], MAX_PATH)) && i < MAX_PATH && i != 0) {
        char *endofpath = strrchr(&myFileName[0], '\\');
#endif
#ifdef UNIX
      // TODO: this doesn't work on mac OS
      char myFileName[PATH_MAX + fLength];
      if( readlink("/proc/self/exe", myFileName, PATH_MAX) != -1 ) {
        char *endOfPath = strrchr(&myFileName[0], '/');
#endif
        if( endOfPath == nullptr )
          quit(myPathError);
        endOfPath++;
        strcpy(endOfPath, filename); //append filename to my path
        f->open(&myFileName[0], true);
      } else
        quit(myPathError);
    }
};

// Verify that the specified file exists and is readable, determine file size
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
  FILE *f = openfile(filename, APPEND);
  if( f == nullptr )
    printf("Warning: could not log compression results to %s\n", filename);
  else {
    fprintf(f, "%s", s);
    fclose(f);
  }
}


#endif //PAQ8PX_FILE_HPP
