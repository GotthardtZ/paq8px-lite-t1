/*
  PAQ8PX file compressor/archiver
  see README for information
  see DOC for technical details
  see CHANGELOG for version history
*/

//////////////////////// Versioning ////////////////////////////////////////

#define PROGNAME     "paq8px"
#define PROGVERSION  "153"  //update version here before publishing your changes
#define PROGYEAR     "2018"


//////////////////////// Build options /////////////////////////////////////

// Uncomment one or more of the following #define-s to disable compilation of certain models/transformations
// This is useful when
// - you would like to slim the executable by eliminating unnecessary code for benchmarks where exe size matters or
// - you would like to experiment with the model-mixture
// TODO: make more models "optional"
#define USE_ZLIB
#define USE_WAVMODEL
#define USE_TEXTMODEL //if you uncomment this line, you must uncomment the next line (USE_WORDMODEL) as well
#define USE_WORDMODEL


//////////////////////// Debug options /////////////////////////////////////

#define NVERBOSE // Remove (comment out) this line for more on-screen progress information
#define NDEBUG   // Remove (comment out) this line for debugging (turns on Array bound checks and asserts)
#include <assert.h>


//////////////////////// Target OS /////////////////////////////////////////

#if defined(_WIN32) || defined(_MSC_VER)
#ifndef WINDOWS
#define WINDOWS  //to compile for Windows
#endif
#endif

#if defined(unix) || defined(__unix__) || defined(__unix) || defined(__APPLE__)
#ifndef UNIX
#define UNIX //to compile for Unix, Linux, Solaris, MacOS / Darwin, etc)
#endif
#endif

#if !defined(WINDOWS ) && !defined(UNIX)
#error Unknown target system
#endif


//////////////////////// Includes /////////////////////////////////////////

// Determining the proper printf() format specifier for 64 bit unsigned integers:
// - on Windows MSVC and MinGW-w64 use the MSVCRT runtime where it is "%I64u"
// - on Linux it is "%llu"
// The correct value is provided by the PRIu64 macro which is defined here:
#include <cinttypes> //PRIu64   (C99 standard, available since VS 2013 RTM)

// Platform-specific includes
#ifdef UNIX
  #include <dirent.h> //opendir(), readdir(), dirent()
  #include <string.h> //strlen(), strcpy(), strcat(), strerror(), memset(), memcpy(), memmove()
  #include <limits.h> //PATH_MAX (for OSX)
  #include <unistd.h> //isatty()
  #include <errno.h>  //errno
#else
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <windows.h>
  #include <time.h>   //clock_t, CLOCKS_PER_SEC
#endif

// Platform-independent includes
#include <sys/stat.h> //stat(), mkdir()
#include <math.h>     //floor(), sqrt()
#include <stdexcept>  //std::exception
#include <algorithm>

#ifdef USE_ZLIB
#include <zlib.h>
#endif


//////////////////// Cross platform definitions /////////////////////////////////////

#ifdef _MSC_VER
#define fseeko(a,b,c) _fseeki64(a,b,c)
#define ftello(a) _ftelli64(a)
#else
#ifndef UNIX
#ifndef fseeko
#define fseeko(a,b,c) fseeko64(a,b,c)
#endif
#ifndef ftello
#define ftello(a) ftello64(a)
#endif
#endif
#endif

#ifdef WINDOWS
#define strcasecmp _stricmp
#endif


///////////////////////// SIMD Vectorization detection //////////////////////////////////

// Uncomment one or more of the following includes if you plan adding more SIMD dispatching
//#include <mmintrin.h>  //MMX
//#include <xmmintrin.h> //SSE
#include <emmintrin.h>   //SSE2
//#include <pmmintrin.h> //SSE3
//#include <tmmintrin.h> //SSSE3
//#include <smmintrin.h> //SSE4.1
//#include <nmmintrin.h> //SSE4.2
//#include <ammintrin.h> //SSE4A
#include <immintrin.h>   //AVX, AVX2
//#include <zmmintrin.h> //AVX512

//define CPUID
#ifdef _MSC_VER
#include <intrin.h>
#define cpuid(info, x) __cpuidex(info, x, 0)
#elif defined(__GNUC__)
#include <cpuid.h>
#define cpuid(info, x) __cpuid_count(x, 0, info[0], info[1], info[2], info[3])
#else
#error Unknown compiler
#endif

// Define interface to xgetbv instruction
static inline int64_t xgetbv (int ctr) {
#if (defined (_MSC_FULL_VER) && _MSC_FULL_VER >= 160040000) || (defined (__INTEL_COMPILER) && __INTEL_COMPILER >= 1200)
  return _xgetbv(ctr);
#elif defined(__GNUC__)
  uint32_t a, d;
  __asm("xgetbv" : "=a"(a),"=d"(d) : "c"(ctr) : );
  return a | (((uint64_t) d) << 32);
#else
#error Unknown compiler
#endif
}

/* Returns system's highest supported SIMD instruction set as
0: None
1: MMX
2: SSE
3: SSE2
4: SSE3
5: SSSE3
6: SSE4.1
7: SSE4.2
 : SSE4A //SSE4A is not supported on Intel, so we will exclude it
8: AVX
9: AVX2
 : AVX512 //TODO
*/
int simd_detect() {
  int cpuid_result[4] = {0,0,0,0};
  cpuid(cpuid_result, 0); // call cpuid function 0 ("Get vendor ID and highest basic calling parameter")
  if (cpuid_result[0] == 0) return 0; //cpuid is not supported
  cpuid(cpuid_result, 1);// call cpuid function 1 ("Processor Info and Feature Bits")
  if ((cpuid_result[3] & (1<<23)) == 0) return 0; //no MMX
  if ((cpuid_result[3] & (1<<25)) == 0) return 1; //no SSE
  //SSE: OK
  if ((cpuid_result[3] & (1<<26)) == 0) return 2; //no SSE2
  //SSE2: OK
  if ((cpuid_result[2] & (1<< 0)) == 0) return 3; //no SSE3
  //SSE3: OK
  if ((cpuid_result[2] & (1<< 9)) == 0) return 4; //no SSSE3
  //SSSE3: OK
  if ((cpuid_result[2] & (1<<19)) == 0) return 5; //no SSE4.1
  //SSE4.1: OK
  if ((cpuid_result[2] & (1<<20)) == 0) return 6; //no SSE4.2
  //SSE4.2: OK
  if ((cpuid_result[2] & (1<<27)) == 0) return 7; //no OSXSAVE (no XGETBV)
  if ((xgetbv(0) & 6) != 6)             return 7; //AVX is not enabled in OS
  if ((cpuid_result[2] & (1<<28)) == 0) return 7; //no AVX
  //AVX: OK
  cpuid(cpuid_result, 7); // call cpuid function 7 ("Extended Feature Bits")
  if ((cpuid_result[1] & (1<< 5)) == 0) return 8;  //no AVX2
  //AVX2: OK
  return 9;
}


//////////////////////// Type definitions /////////////////////////////////////////

// shortcuts for 8, 16, 32 bit integer types
typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;

//////////////////////// Helper functions /////////////////////////////////////////

// min, max functions
inline int min(int a, int b) {return a<b?a:b;}
inline U64 min(U64 a, U64 b) {return a<b?a:b;}
inline int max(int a, int b) {return a<b?b:a;}

// A basic exception class to let catch() in main() know
// that the exception was thrown intentionally.
class IntentionalException : public std::exception {};

// Error handler: print message if any, and exit
void quit(const char* const message=0) {
  if(message)
    printf("\n%s",message);
  printf("\n");
  throw IntentionalException();
}

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

//////////////////////// Program Checker /////////////////////

// Track time and memory used
// Remark: only Array<T> reports its memory usage, we don't know about other types
class ProgramChecker {
private:
  U64 memused;  // Bytes currently in use (all allocated-all freed)
  U64 maxmem;   // Most bytes allocated ever
  clock_t start_time;  // in ticks
public:
  void alloc(U64 n) {
    memused+=n;
    if (memused>maxmem) maxmem=memused;
  }
  void free(U64 n) {
    assert(memused>=n);
    memused-=n;
  }
  ProgramChecker(): memused(0), maxmem(0) {
    start_time=clock();
    assert(sizeof(U8)==1);
    assert(sizeof(U16)==2);
    assert(sizeof(U32)==4);
    assert(sizeof(U64)==8);
    assert(sizeof(short)==2);
    assert(sizeof(int)==4);
  }
  double get_runtime() const {
    return double(clock()-start_time)/CLOCKS_PER_SEC;
  }
  void print() const {  // Print elapsed time and used memory
      double runtime=get_runtime();
      printf("Time %1.2f sec, used %" PRIu64 " MB (%" PRIu64 " bytes) of memory\n",runtime,maxmem>>20,maxmem);
  }
  ~ProgramChecker() {
    assert(memused==0); // We expect that all reserved memory is already properly freed
  }

} programChecker;

//////////////////////////// Array ////////////////////////////

// Array<T,Align> a(n); allocates memory for n elements of T.
// The base address is aligned if the "alignment" parameter is given.
// Constructors for T are not called, the allocated memory is initialized to 0s.
// It's the caller's responsibility to populate the array with elements.
// Parameters are checked and indexing is bounds checked if assertions are on.
// Use of copy and assignment constructors are not supported.
//
// a.size(): returns the number of T elements currently in the array.
// a.resize(newsize): grows or shrinks the array.
// a.append(x): appends x to the end of the array and reserving space for more elements if needed.
// a.pop_back(): removes the last element by reducing the size by one (but does not free memory).
#ifndef NDEBUG
static void chkindex(U64 index, U64 upper_bound)
{
  if (index>=upper_bound) {
    fprintf(stderr, "%" PRIu64 " out of upper bound %" PRIu64 "\n", index, upper_bound);
    BACKTRACE();
    quit();
  }
}
#endif

template <class T, const int Align=16> class Array {
private:
  U64 used_size;
  U64 reserved_size;
  char *ptr; // Address of allocated memory (may not be aligned)
  T* data;   // Aligned base address of the elements, (ptr <= T)
  void create(U64 requested_size);
  inline U64 padding() const {return Align-1;}
  inline U64 allocated_bytes() const {return (reserved_size==0)?0:reserved_size*sizeof(T)+padding();}
public:
  explicit Array(U64 requested_size) {create(requested_size);}
  ~Array();
  T& operator[](U64 i) {
    #ifndef NDEBUG
    chkindex(i,used_size);
    #endif
    return data[i];
  }
  const T& operator[](U64 i) const {
    #ifndef NDEBUG
    chkindex(i,used_size);
    #endif
    return data[i];
  }
  U64 size() const {return used_size;}
  void resize(U64 new_size);
  void pop_back() {assert(used_size>0); --used_size; }  // decrement size
  void push_back(const T& x);  // increment size, append x
  Array(const Array&) { assert(false); } //prevent copying - this method must be public (gcc must see it but actually won't use it)
private:
  Array& operator=(const Array&); //prevent assignment
};

template<class T, const int Align> void Array<T,Align>::create(U64 requested_size) {
  assert((Align&(Align-1))==0);
  used_size=reserved_size=requested_size;
  if (requested_size==0) {
    data=0;ptr=0;
    return;
  }
  U64 bytes_to_allocate=allocated_bytes();
  ptr=(char*)calloc(bytes_to_allocate,1);
  if(!ptr)quit("Out of memory.");
  U64 pad=padding();
  data=(T*)(((uintptr_t)ptr+pad) & ~(uintptr_t)pad);
  assert(ptr<=(char*)data && (char*)data<=ptr+Align);
  assert(((uintptr_t)data & (Align-1))==0); //aligned as expected?
  programChecker.alloc(bytes_to_allocate);
}

template<class T, const int Align> void Array<T,Align>::resize(U64 new_size) {
  if (new_size<=reserved_size) {
    used_size=new_size;
    return;
  }
  char *old_ptr=ptr;
  T *old_data=data;
  U64 old_size=used_size;
  programChecker.free(allocated_bytes());
  create(new_size);
  if(old_size>0) {
    assert(old_ptr && old_data);
    memcpy(data, old_data, sizeof(T)*old_size);
  }
  if(old_ptr){free(old_ptr);old_ptr=0;}
}

template<class T, const int Align> void Array<T,Align>::push_back(const T& x) {
  if(used_size==reserved_size) {
    U64 old_size=used_size;
    U64 new_size=used_size*2+16;
    resize(new_size);
    used_size=old_size;
  }
  data[used_size++]=x;
}

template<class T, const int Align> Array<T, Align>::~Array() {
  programChecker.free(allocated_bytes());
  free(ptr);
  used_size=reserved_size=0;
  data=0;ptr=0;
}


/////////////////// String and FileName /////////////////////

// A specialized string class
// size() includes NUL terminator.
// strsize() does not include NUL terminator.

class String: public Array<char> {
private:
  void append_int_recursive(U64 x) {
    if(x<=9)
      push_back('0'+char(x));
    else {
      U64 rem= x % 10;
      x = x / 10;
      if(x!=0)append_int_recursive(x);
      push_back('0'+char(rem));
    }
  }
protected:
  #ifdef NDEBUG
  void chk_consistency() const {}
  #else
  void chk_consistency() const {
    for(int i=0;i<size()-1;i++)
      if((*this)[i]==0)quit("Internal error - string consistency check failed (1).");
    if(((*this)[size()-1])!=0)quit("Internal error - string consistency check failed (2).");
  }
  #endif
public:
  const char* c_str() const {return &(*this)[0];}
  int strsize() const {chk_consistency();assert(size()>0);return (int)size()-1;}
  void operator=(const char* s) {
    resize((int)strlen(s)+1);
    memcpy(&(*this)[0], s, size());
    chk_consistency();
  }
  void operator+=(const char* s) {
    U64 pos=size();
    int len=(int)strlen(s);
    resize(pos+len);
    memcpy(&(*this)[pos-1], s, len+1);
    chk_consistency();
  }
  void operator+=(char c) {
    pop_back(); //Remove NUL
    push_back(c);
    push_back(0); //Append NUL
    chk_consistency();
  }
  void operator+=(U64 x) {
    pop_back(); //Remove NUL
    if(x==0)
      push_back('0');
    else
      append_int_recursive(x);
    push_back(0); //Append NUL
    chk_consistency();
  }
  bool endswith(const char * ending) const {
    int endingsize=int(strlen(ending));
    if(endingsize>strsize())return false;
    int cmp=memcmp(ending,&(*this)[strsize()-endingsize],endingsize);
    return(cmp==0);
  }
  void stripend(int count) {
    int newsize=strsize()-count;
    assert(newsize>=0);
    resize(newsize);
    push_back(0); //Append NUL
    chk_consistency();
  }
  bool beginswith(const char * beginning) const {
    int beginningsize=(int)strlen(beginning);
    if(beginningsize>strsize())return false;
    int cmp=memcmp(beginning,&(*this)[0],beginningsize);
    return(cmp==0);
  }
  void stripstart(int count) {
    int newsize=strsize()-count;
    assert(newsize>=0);
    memmove(&(*this)[0],&(*this)[count],newsize);
    resize(newsize);
    push_back(0); //Append NUL
    chk_consistency();
  }
  int findlast (char c) const {
    for(int i=strsize()-1;i>=0;i--)if((*this)[i]==c)return i;
    return -1; //not found
  }
  String(const char* s=""): Array<char>(U64(strlen(s)+1)) {
    memcpy(&(*this)[0], s, size());
    chk_consistency();
  }
};

//The preferred slash for displaying
//We will change the BADSLASH to GOODSLASH before displaying a path string to the user
#ifdef WINDOWS
#define BADSLASH '/'
#define GOODSLASH '\\'
#else
#define BADSLASH '\\'
#define GOODSLASH '/'
#endif

// A class to represent a filename
class FileName: public String {
public:
  FileName(const char* s=""):String(s){};
  int lastslashpos() const {
    int pos=findlast('/');
    if(pos<0)pos=findlast('\\');
    return pos; //has no path when -1
  }
  void keepfilename() {
    int pos=lastslashpos();
    stripstart(pos+1); //don't keep last slash
  }
  void keeppath() {
    int pos=lastslashpos();
    stripend(strsize()-pos-1); //keep last slash
  }
  void replaceslashes() { //prepare path string for screen output
    for(int i=strsize()-1;i>=0;i--)
      if((*this)[i]==BADSLASH)
        (*this)[i]=GOODSLASH;
    chk_consistency();
  }
};


//////////////////// IO functions and classes ///////////////////
// These functions/classes are responsible for all the file
// and directory operations.

//////////////////// Folder operations ///////////////////////////

//examines given "path" and returns:
//0: on error
//1: when exists and is a file
//2: when exists and is a directory
//3: when does not exist, but looks like a file       /abcd/efgh
//4: when does not exist, but looks like a directory  /abcd/efgh/
static int examinepath(const char* path) {
  struct stat status;
  bool success = stat(path, &status)==0;
  if(!success) {
    if(errno==ENOENT){ //no such file or directory
      int len=(int)strlen(path);
      if(len==0)return 0; //error: path is an empty string
      char lastchar=path[len-1];
      if(lastchar!='/' && lastchar!='\\')return 3; //looks like a file
      else return 4; //looks like a directory
    }
    return 0; //error
  }
  if((status.st_mode & S_IFREG)!=0) return 1; //existing file
  if((status.st_mode & S_IFDIR)!=0) return 2; //existing directory
  return 0; //error: "path" may be a socket, symlink, named pipe, etc.
};

//creates a directory if it does not exist
static int makedir(const char* dir) {
  if(examinepath(dir)==2)//existing directory
    return 2; //2: directory already exists, no need to create
  #ifdef WINDOWS
    bool created = (CreateDirectory(dir, 0) == TRUE);
  #else
  #ifdef UNIX
    bool created = (mkdir(dir, 0777) == 0);
  #else
    #error Unknown target system
  #endif
  #endif
  return created?1:0; //0: failed, 1: created successfully
};

//creates directories recusively if they don't exist
static void makedirectories(const char* filename) {
  String path(filename);
  int start = 0;
  if(path[1]==':')start=2; //skip drive letter (c:)
  if(path[start] == '/' || path[start] == '\\')start++; //skip leading slashes (root dir)
  for (int i = start; path[i]; ++i) {
    if (path[i] == '/' || path[i] == '\\') {
      char savechar = path[i];
      path[i] = 0;
      const char* dirname = path.c_str();
      int created = makedir(dirname);
      if (created==0) {
        printf("Unable to create directory %s", dirname);
        quit();
      }
      if(created==1)
        printf("Created directory %s\n", dirname);
      path[i] = savechar;
    }
  }
};


/////////////////////////// File /////////////////////////////
// The main purpose of these classes is to keep temporary files in
// RAM as mush as possible. The default behaviour is to simply pass
// function calls to the operating system - except in case of temporary
// files.

// Helper function: create a temporary file
//
// On Windows when using tmpfile() the temporary file may be created
// in the root directory causing access denied error when User Account Control (UAC) is on.
// To avoid this issue with tmpfile() we simply use fopen() instead.
// We create the temporary file in the directory where the executable is launched from.
// Luckily the MS C runtime library provides two (MS specific) fopen() flags: "T"emporary and "D"elete.

FILE* maketmpfile(void) {
#if defined(WINDOWS)
  char szTempFileName[MAX_PATH];
  UINT uRetVal = GetTempFileName(TEXT("."), TEXT("tmp"), 0, szTempFileName);
  if(uRetVal==0)return 0;
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
  virtual bool open(const char* filename, bool must_succeed) = 0;
  virtual void create(const char* filename) = 0;
  virtual void close() = 0;
  virtual int getchar() = 0;
  virtual void putchar(U8 c) = 0;
  void append(const char* s) {for (int i = 0; s[i]; i++)putchar(s[i]);}
  virtual U64 blockread(U8 *ptr, U64 count) = 0;
  virtual void blockwrite(U8 *ptr, U64 count) = 0;
  U32 get32(){return (getchar() << 24) | (getchar() << 16) | (getchar() << 8) | (getchar());}
  void put32(U32 x){putchar((x >> 24) & 255); putchar((x >> 16) & 255); putchar((x >> 8) & 255); putchar(x & 255);}
  virtual void setpos(U64 newpos) = 0;
  virtual void setend() = 0;
  virtual U64 curpos() = 0;
  virtual bool eof() = 0;
};

// This class is responsible for files on disk
// It simply passes function calls to stdio

class FileDisk :public File {
protected:
  FILE *file;
public:
  FileDisk() {file=0;}
  ~FileDisk() {close();}
  bool open(const char *filename, bool must_succeed) {
    assert(file==0);
    file = fopen(filename, "rb");
    bool success=(file!=0);
    if(!success && must_succeed){printf("Unable to open file %s (%s)", filename, strerror(errno));quit();}
    return success;
  }
  void create(const char *filename) {
    assert(file==0);
    makedirectories(filename);
    file=fopen(filename, "wb+");
    if (!file) {printf("Unable to create file %s (%s)", filename, strerror(errno));quit();}
  }
  void createtmp() {
    assert(file==0);
    file = maketmpfile();
    if (!file) {printf("Unable to create temporary file (%s)", strerror(errno));quit();}
  }
  void close() { if(file) fclose(file); file=0;}
  int getchar() { return fgetc(file); }
  void putchar(U8 c) { fputc(c, file); }
  U64 blockread(U8 *ptr, U64 count) {return fread(ptr,1,count,file);}
  void blockwrite(U8 *ptr, U64 count) {fwrite(ptr,1,count,file);}
  void setpos(U64 newpos) { fseeko(file, newpos, SEEK_SET); }
  void setend() { fseeko(file, 0, SEEK_END); }
  U64 curpos() { return ftello(file); }
  bool eof() { return feof(file)!=0; }
};

// This class is responsible for temporary files in RAM or on disk
// Initially it uses RAM for temporary file content.
// In case of the content size in RAM grows too large, it is written to disk,
// the RAM is freed and all subsequent file operations will use the file on disk.

class FileTmp :public File {
private:
  //file content in ram
  Array<U8> *content_in_ram; //content of file
  U64 filepos;
  U64 filesize;
  void forget_content_in_ram() {
    if (content_in_ram) {
      delete content_in_ram;
      content_in_ram = 0;
      filepos = 0;
      filesize = 0;
    }
  }
  //file on disk
  FileDisk *file_on_disk;
  void forget_file_on_disk() {
    if (file_on_disk) {
      file_on_disk->close();
      delete file_on_disk;
      file_on_disk = 0;
    }
  }
  //switch: ram->disk
  #ifdef NDEBUG
  static const U32 MAX_RAM_FOR_TMP_CONTENT = 64 * 1024 * 1024; //64 MB (per file)
  #else
  static const U32 MAX_RAM_FOR_TMP_CONTENT = 64; // to trigger switching to disk earlier - for testing
  #endif
  void ram_to_disk() {
    assert(file_on_disk==0);
    file_on_disk = new FileDisk();
    file_on_disk->createtmp();
    if(filesize>0)
      file_on_disk->blockwrite(&((*content_in_ram)[0]), filesize);
    file_on_disk->setpos(filepos);
    forget_content_in_ram();
  }
public:
  FileTmp() {content_in_ram=new Array<U8>(0); filepos=0; filesize=0; file_on_disk = 0;}
  ~FileTmp() {close();}
  bool open(const char *filename, bool must_succeed) { assert(false); return false; } //this method is forbidden for temporary files
  void create(const char *filename) { assert(false); } //this method is forbidden for temporary files
  void close() {
    forget_content_in_ram();
    forget_file_on_disk();
  }
  int getchar() {
    if(content_in_ram) {
      if (filepos >= filesize)
        return EOF;
      else {
        U8 c = (*content_in_ram)[(U32)filepos];
        filepos++;
        return c;
      }
    }
    else return file_on_disk->getchar();
  }
  void putchar(U8 c) {
    if(content_in_ram) {
      if (filepos < MAX_RAM_FOR_TMP_CONTENT) {
        if (filepos == filesize) { content_in_ram->push_back(c); filesize++; }
        else (*content_in_ram)[(U32)filepos] = c;
        filepos++;
        return;
      }
      else ram_to_disk();
    }
    file_on_disk->putchar(c);
  }
  U64 blockread(U8 *ptr, U64 count) {
    if(content_in_ram) {
      U64 available = filesize - filepos;
      if (available<count)count = available;
      if(count>0)memcpy(ptr, &((*content_in_ram)[(U32)filepos]), count);
      filepos += count;
      return count;
    }
    else return file_on_disk->blockread(ptr,count);
  }
  void blockwrite(U8 *ptr, U64 count) {
    if(content_in_ram) {
      if (filepos+count <= MAX_RAM_FOR_TMP_CONTENT) {
        content_in_ram->resize((U32)(filepos + count));
        if(count>0)memcpy(&((*content_in_ram)[(U32)filepos]), ptr, count);
        filesize += count;
        filepos += count;
        return;
      }
      else ram_to_disk();
    }
    file_on_disk->blockwrite(ptr,count);
  }
  void setpos(U64 newpos) {
    if(content_in_ram) {
      if(newpos>filesize)ram_to_disk(); //panic: we don't support seeking past end of file (but stdio does) - let's switch to disk
      else {filepos = newpos; return;}
    }
    file_on_disk->setpos(newpos);
  }
  void setend() {
    if(content_in_ram) filepos = filesize;
    else file_on_disk->setend();
  }
  U64 curpos() {
    if(content_in_ram) return filepos;
    else return file_on_disk->curpos();
  }
  bool eof() {
    if(content_in_ram)return filepos >= filesize;
    else return file_on_disk->eof();
  }
};

// Helper class: open a file from the executable's folder
//

static char mypatherror[]="Can't determine my path.";
class OpenFromMyFolder {
private:
public:
  //this static method will open the executable itself
  static void myself(FileDisk *f) {
  #ifdef WINDOWS
    int i;
    Array<char> myfilename(MAX_PATH+1);
    if((i=GetModuleFileName(NULL, &myfilename[0], MAX_PATH)) && i<=MAX_PATH)
  #endif
  #ifdef UNIX
    Array<char> myfilename(PATH_MAX+1);
    if(readlink("/proc/self/exe", &myfilename[0], PATH_MAX)!=-1)
  #endif
      f->open(&myfilename[0],true);
    else
      quit(mypatherror);
  }

  //this static method will open a file from the executable's folder
  static void anotherfile(FileDisk *f, const char* filename) {
    int flength=(int)strlen(filename)+1;
    #ifdef WINDOWS
      int i;
      Array<char> myfilename(MAX_PATH+flength);
      if((i=GetModuleFileName(NULL, &myfilename[0], MAX_PATH)) && i<=MAX_PATH) {
        char *endofpath=strrchr(&myfilename[0], '\\');
    #endif
    #ifdef UNIX
      char myfilename[PATH_MAX+flength];
      if(readlink("/proc/self/exe", myfilename, PATH_MAX)!=-1) {
        char *endofpath=strrchr(&myfilename[0], '/');
    #endif
        if (endofpath==0)quit(mypatherror);
        endofpath++;
        strcpy(endofpath, filename); //append filename to my path
        f->open(&myfilename[0],true);
      }
      else
        quit(mypatherror);
  }
};

// Verify that the specified file exists and is readable, determine file size
static U64 getfilesize(const char * filename) {
  FileDisk f;
  f.open(filename,true);
  f.setend();
  U64 filesize=f.curpos();
  f.close();
  if((filesize>>31)!=0)quit("Large files not supported.");
  return filesize;
}

static void appendtofile(const char *filename, const char* s) {
  FILE *f=fopen(filename,"a");
  if (f==nullptr)
    printf("Warning: could not log compression results to %s\n",filename);
  else {
    fprintf(f,"%s",s);
    fclose(f);
  }
}

//determine if output is redirected
static bool isoutputdirected()
{
#ifdef WINDOWS
  DWORD FileType=GetFileType(GetStdHandle(STD_OUTPUT_HANDLE));
  return (FileType==FILE_TYPE_PIPE) || (FileType==FILE_TYPE_DISK);
#endif
#ifdef UNIX
  return !isatty(fileno(stdout));
#endif
}
static bool to_screen=!isoutputdirected();

//////////////////////////// rnd ///////////////////////////////

// 32-bit pseudo random number generator
class Random{
  Array<U32> table;
  int i;
public:
  Random(): table(64) {
    table[0]=123456789;
    table[1]=987654321;
    for (int j=0; j<62; j++) table[j+2]=table[j+1]*11+table[j]*23/16;
    i=0;
  }
  U32 operator()() {
    return ++i, table[i&63]=table[(i-24)&63]^table[(i-55)&63];
  }
} rnd;

////////////////////////////// Buf /////////////////////////////

// Buf(n) buf; creates an array of n bytes (must be a power of 2).
// buf[i] returns a reference to the i'th byte with wrap (no out of bounds).
// buf(i) returns i'th byte back from pos (i>0) with wrap (no out of bounds)
// buf.size() returns n.

int pos;  // Number of input bytes in buf (not wrapped), must be masked for indexing

class Buf {
  Array<U8> b;
public:
  Buf(U64 size=0): b(size) {}
  void setsize(U64 newsize) {
    if (newsize==0) return;
    assert(newsize>0 && (newsize&(newsize-1))==0); //power of 2?
    b.resize(newsize);
  }
  U8& operator[](U64 i) {
    return b[i&(b.size()-1)];
  }
  U8 operator()(U64 i) const {
    return (i>0)?b[(pos-i)&(b.size()-1)]:0;
  }
  U64 size() const {
    return b.size();
  }
};

// IntBuf(n) is a buffer of n ints (n must be a power of 2).
// intBuf[i] returns a reference to i'th element with wrap.

class IntBuf {
  Array<int> b;
public:
  IntBuf(int size): b(size) {}
  int& operator[](int i) {
    return b[i&(b.size()-1)];
  }
};

/////////////////////// Global context /////////////////////////

typedef enum {DEFAULT=0, FILECONTAINER, JPEG, HDR, IMAGE1, IMAGE4, IMAGE8, IMAGE8GRAY, IMAGE24, IMAGE32, AUDIO, EXE, CD, ZLIB, BASE64, GIF, PNG8, PNG8GRAY, PNG24, PNG32, TEXT, TEXT_EOL} Blocktype;

inline bool hasRecursion(Blocktype ft) { return ft==CD || ft==ZLIB || ft==BASE64 || ft==GIF || ft== FILECONTAINER; }
inline bool hasInfo(Blocktype ft) { return ft==IMAGE1 || ft==IMAGE4 || ft==IMAGE8 || ft==IMAGE8GRAY || ft==IMAGE24 || ft==IMAGE32 || ft==AUDIO || ft==PNG8 || ft==PNG8GRAY || ft==PNG24 || ft==PNG32; }
inline bool hasTransform(Blocktype ft) { return ft==IMAGE24 || ft==IMAGE32 || ft==EXE || ft==CD || ft==ZLIB || ft==BASE64 || ft==GIF || ft==TEXT_EOL; }

int level=0; //this value will be overwritten at the beginning of compression/decompression
#define MEM (U64(65536)<<level)

int y=0;  // Last bit, 0 or 1, set by encoder

U8 options=0;
#define OPTION_MULTIPLE_FILE_MODE 1
#define OPTION_BRUTE 2
#define OPTION_TRAINEXE 4
#define OPTION_TRAINTXT 8
#define OPTION_ADAPTIVE 16
#define OPTION_SKIPRGB 32
#define OPTION_FASTMODE 64

struct ModelStats{
  U32 XML, x86_64, Record;
  Blocktype blockType;
};

// Global context set by Predictor and available to all models.

int c0=1; // Last 0-7 bits of the partial byte with a leading 1 bit (1-255)
U32 c4=0, f4=0, b2=0; // Last 4 whole bytes, packed.  Last byte is bits 0-7.
int bpos=0; // bits in c0 (0 to 7)
Buf buf;  // Rotating input queue set by Predictor
int blpos=0; // Relative position in block

#define CacheSize 32

#if (CacheSize&(CacheSize-1)) || (CacheSize<8)
  #error Cache size must be a power of 2 bigger than 4
#endif


///////////////////////////// ilog //////////////////////////////

// ilog(x) = round(log2(x) * 16), 0 <= x < 64K
class Ilog {
  Array<U8> t;
public:
  int operator()(U16 x) const {return t[x];}
  Ilog();
} ilog;

// Compute lookup table by numerical integration of 1/x
Ilog::Ilog(): t(65536) {
  U32 x=14155776;
  for (int i=2; i<65536; ++i) {
    x+=774541002/(i*2-1);  // numerator is 2^29/ln 2
    t[i]=x>>24;
  }
}

// llog(x) accepts 32 bits
inline int llog(U32 x) {
  if (x>=0x1000000)
    return 256+ilog(U16(x>>16));
  else if (x>=0x10000)
    return 128+ilog(U16(x>>8));
  else
    return ilog(U16(x));
}

inline U32 BitCount(U32 v) {
  v -= ((v >> 1) & 0x55555555);
  v = ((v >> 2) & 0x33333333) + (v & 0x33333333);
  v = ((v >> 4) + v) & 0x0f0f0f0f;
  v = ((v >> 8) + v) & 0x00ff00ff;
  v = ((v >> 16) + v) & 0x0000ffff;
  return v;
}

// ilog2
// returns floor(log2(x)), e.g. 30->4  31->4  32->5,  33->5
#ifdef _MSC_VER
#include <intrin.h>
inline U32 ilog2(U32 x) {
  DWORD tmp=0;
  if(x!=0)_BitScanReverse(&tmp,x);
  return tmp;
}
#elif __GNUC__
inline U32 ilog2(U32 x) {
  if(x!=0)x=31-__builtin_clz(x);
  return x;
}
#else
inline U32 ilog2(U32 x) {
  //copy the leading "1" bit to its left (0x03000000 -> 0x03ffffff)
  x |= (x >> 1);
  x |= (x >> 2);
  x |= (x >> 4);
  x |= (x >> 8);
  x |= (x >>16);
  //how many trailing bits do we have (except the first)?
  return BitCount(x >> 1);
}
#endif

///////////////////////// state table ////////////////////////

// State table:
//   nex(state, 0) = next state if bit y is 0, 0 <= state < 256
//   nex(state, 1) = next state if bit y is 1
//   nex(state, 2) = number of zeros in bit history represented by state
//   nex(state, 3) = number of ones represented
//
// States represent a bit history within some context.
// State 0 is the starting state (no bits seen).
// States 1-30 represent all possible sequences of 1-4 bits.
// States 31-252 represent a pair of counts, (n0,n1), the number
//   of 0 and 1 bits respectively.  If n0+n1 < 16 then there are
//   two states for each pair, depending on if a 0 or 1 was the last
//   bit seen.
// If n0 and n1 are too large, then there is no state to represent this
// pair, so another state with about the same ratio of n0/n1 is substituted.
// Also, when a bit is observed and the count of the opposite bit is large,
// then part of this count is discarded to favor newer data over old.

#if 1 // change to #if 0 to generate this table at run time (4% slower)
static const U8 State_table[256][4]={
  {  1,  2, 0, 0},{  3,  5, 1, 0},{  4,  6, 0, 1},{  7, 10, 2, 0}, // 0-3
  {  8, 12, 1, 1},{  9, 13, 1, 1},{ 11, 14, 0, 2},{ 15, 19, 3, 0}, // 4-7
  { 16, 23, 2, 1},{ 17, 24, 2, 1},{ 18, 25, 2, 1},{ 20, 27, 1, 2}, // 8-11
  { 21, 28, 1, 2},{ 22, 29, 1, 2},{ 26, 30, 0, 3},{ 31, 33, 4, 0}, // 12-15
  { 32, 35, 3, 1},{ 32, 35, 3, 1},{ 32, 35, 3, 1},{ 32, 35, 3, 1}, // 16-19
  { 34, 37, 2, 2},{ 34, 37, 2, 2},{ 34, 37, 2, 2},{ 34, 37, 2, 2}, // 20-23
  { 34, 37, 2, 2},{ 34, 37, 2, 2},{ 36, 39, 1, 3},{ 36, 39, 1, 3}, // 24-27
  { 36, 39, 1, 3},{ 36, 39, 1, 3},{ 38, 40, 0, 4},{ 41, 43, 5, 0}, // 28-31
  { 42, 45, 4, 1},{ 42, 45, 4, 1},{ 44, 47, 3, 2},{ 44, 47, 3, 2}, // 32-35
  { 46, 49, 2, 3},{ 46, 49, 2, 3},{ 48, 51, 1, 4},{ 48, 51, 1, 4}, // 36-39
  { 50, 52, 0, 5},{ 53, 43, 6, 0},{ 54, 57, 5, 1},{ 54, 57, 5, 1}, // 40-43
  { 56, 59, 4, 2},{ 56, 59, 4, 2},{ 58, 61, 3, 3},{ 58, 61, 3, 3}, // 44-47
  { 60, 63, 2, 4},{ 60, 63, 2, 4},{ 62, 65, 1, 5},{ 62, 65, 1, 5}, // 48-51
  { 50, 66, 0, 6},{ 67, 55, 7, 0},{ 68, 57, 6, 1},{ 68, 57, 6, 1}, // 52-55
  { 70, 73, 5, 2},{ 70, 73, 5, 2},{ 72, 75, 4, 3},{ 72, 75, 4, 3}, // 56-59
  { 74, 77, 3, 4},{ 74, 77, 3, 4},{ 76, 79, 2, 5},{ 76, 79, 2, 5}, // 60-63
  { 62, 81, 1, 6},{ 62, 81, 1, 6},{ 64, 82, 0, 7},{ 83, 69, 8, 0}, // 64-67
  { 84, 71, 7, 1},{ 84, 71, 7, 1},{ 86, 73, 6, 2},{ 86, 73, 6, 2}, // 68-71
  { 44, 59, 5, 3},{ 44, 59, 5, 3},{ 58, 61, 4, 4},{ 58, 61, 4, 4}, // 72-75
  { 60, 49, 3, 5},{ 60, 49, 3, 5},{ 76, 89, 2, 6},{ 76, 89, 2, 6}, // 76-79
  { 78, 91, 1, 7},{ 78, 91, 1, 7},{ 80, 92, 0, 8},{ 93, 69, 9, 0}, // 80-83
  { 94, 87, 8, 1},{ 94, 87, 8, 1},{ 96, 45, 7, 2},{ 96, 45, 7, 2}, // 84-87
  { 48, 99, 2, 7},{ 48, 99, 2, 7},{ 88,101, 1, 8},{ 88,101, 1, 8}, // 88-91
  { 80,102, 0, 9},{103, 69,10, 0},{104, 87, 9, 1},{104, 87, 9, 1}, // 92-95
  {106, 57, 8, 2},{106, 57, 8, 2},{ 62,109, 2, 8},{ 62,109, 2, 8}, // 96-99
  { 88,111, 1, 9},{ 88,111, 1, 9},{ 80,112, 0,10},{113, 85,11, 0}, // 100-103
  {114, 87,10, 1},{114, 87,10, 1},{116, 57, 9, 2},{116, 57, 9, 2}, // 104-107
  { 62,119, 2, 9},{ 62,119, 2, 9},{ 88,121, 1,10},{ 88,121, 1,10}, // 108-111
  { 90,122, 0,11},{123, 85,12, 0},{124, 97,11, 1},{124, 97,11, 1}, // 112-115
  {126, 57,10, 2},{126, 57,10, 2},{ 62,129, 2,10},{ 62,129, 2,10}, // 116-119
  { 98,131, 1,11},{ 98,131, 1,11},{ 90,132, 0,12},{133, 85,13, 0}, // 120-123
  {134, 97,12, 1},{134, 97,12, 1},{136, 57,11, 2},{136, 57,11, 2}, // 124-127
  { 62,139, 2,11},{ 62,139, 2,11},{ 98,141, 1,12},{ 98,141, 1,12}, // 128-131
  { 90,142, 0,13},{143, 95,14, 0},{144, 97,13, 1},{144, 97,13, 1}, // 132-135
  { 68, 57,12, 2},{ 68, 57,12, 2},{ 62, 81, 2,12},{ 62, 81, 2,12}, // 136-139
  { 98,147, 1,13},{ 98,147, 1,13},{100,148, 0,14},{149, 95,15, 0}, // 140-143
  {150,107,14, 1},{150,107,14, 1},{108,151, 1,14},{108,151, 1,14}, // 144-147
  {100,152, 0,15},{153, 95,16, 0},{154,107,15, 1},{108,155, 1,15}, // 148-151
  {100,156, 0,16},{157, 95,17, 0},{158,107,16, 1},{108,159, 1,16}, // 152-155
  {100,160, 0,17},{161,105,18, 0},{162,107,17, 1},{108,163, 1,17}, // 156-159
  {110,164, 0,18},{165,105,19, 0},{166,117,18, 1},{118,167, 1,18}, // 160-163
  {110,168, 0,19},{169,105,20, 0},{170,117,19, 1},{118,171, 1,19}, // 164-167
  {110,172, 0,20},{173,105,21, 0},{174,117,20, 1},{118,175, 1,20}, // 168-171
  {110,176, 0,21},{177,105,22, 0},{178,117,21, 1},{118,179, 1,21}, // 172-175
  {110,180, 0,22},{181,115,23, 0},{182,117,22, 1},{118,183, 1,22}, // 176-179
  {120,184, 0,23},{185,115,24, 0},{186,127,23, 1},{128,187, 1,23}, // 180-183
  {120,188, 0,24},{189,115,25, 0},{190,127,24, 1},{128,191, 1,24}, // 184-187
  {120,192, 0,25},{193,115,26, 0},{194,127,25, 1},{128,195, 1,25}, // 188-191
  {120,196, 0,26},{197,115,27, 0},{198,127,26, 1},{128,199, 1,26}, // 192-195
  {120,200, 0,27},{201,115,28, 0},{202,127,27, 1},{128,203, 1,27}, // 196-199
  {120,204, 0,28},{205,115,29, 0},{206,127,28, 1},{128,207, 1,28}, // 200-203
  {120,208, 0,29},{209,125,30, 0},{210,127,29, 1},{128,211, 1,29}, // 204-207
  {130,212, 0,30},{213,125,31, 0},{214,137,30, 1},{138,215, 1,30}, // 208-211
  {130,216, 0,31},{217,125,32, 0},{218,137,31, 1},{138,219, 1,31}, // 212-215
  {130,220, 0,32},{221,125,33, 0},{222,137,32, 1},{138,223, 1,32}, // 216-219
  {130,224, 0,33},{225,125,34, 0},{226,137,33, 1},{138,227, 1,33}, // 220-223
  {130,228, 0,34},{229,125,35, 0},{230,137,34, 1},{138,231, 1,34}, // 224-227
  {130,232, 0,35},{233,125,36, 0},{234,137,35, 1},{138,235, 1,35}, // 228-231
  {130,236, 0,36},{237,125,37, 0},{238,137,36, 1},{138,239, 1,36}, // 232-235
  {130,240, 0,37},{241,125,38, 0},{242,137,37, 1},{138,243, 1,37}, // 236-239
  {130,244, 0,38},{245,135,39, 0},{246,137,38, 1},{138,247, 1,38}, // 240-243
  {140,248, 0,39},{249,135,40, 0},{250, 69,39, 1},{ 80,251, 1,39}, // 244-247
  {140,252, 0,40},{249,135,41, 0},{250, 69,40, 1},{ 80,251, 1,40}, // 248-251
  {140,252, 0,41}};  // 252, 253-255 are reserved

#define nex(state,sel) State_table[state][sel]

// The code used to generate the above table at run time (4% slower).
// To print the table, uncomment the 4 lines of print statements below.
// In this code x,y = n0,n1 is the number of 0,1 bits represented by a state.
#else

class StateTable {
  Array<U8> ns;  // state*4 -> next state if 0, if 1, n0, n1
  enum {B=5, N=64}; // sizes of b, t
  static const int b[B];  // x -> max y, y -> max x
  static U8 t[N][N][2];  // x,y -> state number, number of states
  int num_states(int x, int y);  // compute t[x][y][1]
  void discount(int& x);  // set new value of x after 1 or y after 0
  void next_state(int& x, int& y, int b);  // new (x,y) after bit b
public:
  int operator()(int state, int sel) {return ns[state*4+sel];}
  StateTable();
} nex;

const int StateTable::b[B]={42,41,13,6,5};  // x -> max y, y -> max x
U8 StateTable::t[N][N][2];

int StateTable::num_states(int x, int y) {
  if (x<y) return num_states(y, x);
  if (x<0 || y<0 || x>=N || y>=N || y>=B || x>=b[y]) return 0;

  // States 0-30 are a history of the last 0-4 bits
  if (x+y<=4) {  // x+y choose x = (x+y)!/x!y!
    int r=1;
    for (int i=x+1; i<=x+y; ++i) r*=i;
    for (int i=2; i<=y; ++i) r/=i;
    return r;
  }

  // States 31-255 represent a 0,1 count and possibly the last bit
  // if the state is reachable by either a 0 or 1.
  else
    return 1+(y>0 && x+y<16);
}

// New value of count x if the opposite bit is observed
void StateTable::discount(int& x) {
  if (x>2) x=ilog(x)/6-1;
}

// compute next x,y (0 to N) given input b (0 or 1)
void StateTable::next_state(int& x, int& y, int b) {
  if (x<y)
    next_state(y, x, 1-b);
  else {
    if (b) {
      ++y;
      discount(x);
    }
    else {
      ++x;
      discount(y);
    }
    while (!t[x][y][1]) {
      if (y<2) --x;
      else {
        x=(x*(y-1)+(y/2))/y;
        --y;
      }
    }
  }
}

// Initialize next state table ns[state*4] -> next if 0, next if 1, x, y
StateTable::StateTable(): ns(1024) {

  // Assign states
  int state=0;
  for (int i=0; i<256; ++i) {
    for (int y=0; y<=i; ++y) {
      int x=i-y;
      int n=num_states(x, y);
      if (n) {
        t[x][y][0]=state;
        t[x][y][1]=n;
        state+=n;
      }
    }
  }

  // Print/generate next state table
  state=0;
  for (int i=0; i<N; ++i) {
    for (int y=0; y<=i; ++y) {
      int x=i-y;
      for (int k=0; k<t[x][y][1]; ++k) {
        int x0=x, y0=y, x1=x, y1=y;  // next x,y for input 0,1
        int ns0=0, ns1=0;
        if (state<15) {
          ++x0;
          ++y1;
          ns0=t[x0][y0][0]+state-t[x][y][0];
          ns1=t[x1][y1][0]+state-t[x][y][0];
          if (x>0) ns1+=t[x-1][y+1][1];
          ns[state*4]=ns0;
          ns[state*4+1]=ns1;
          ns[state*4+2]=x;
          ns[state*4+3]=y;
        }
        else if (t[x][y][1]) {
          next_state(x0, y0, 0);
          next_state(x1, y1, 1);
          ns[state*4]=ns0=t[x0][y0][0];
          ns[state*4+1]=ns1=t[x1][y1][0]+(t[x1][y1][1]>1);
          ns[state*4+2]=x;
          ns[state*4+3]=y;
        }
          // uncomment to print table above
//        printf("{%3d,%3d,%2d,%2d},", ns[state*4], ns[state*4+1],
//          ns[state*4+2], ns[state*4+3]);
//        if (state%4==3) printf(" // %d-%d\n  ", state-3, state);
        assert(state>=0 && state<256);
        assert(t[x][y][1]>0);
        assert(t[x][y][0]<=state);
        assert(t[x][y][0]+t[x][y][1]>state);
        assert(t[x][y][1]<=6);
        assert(t[x0][y0][1]>0);
        assert(t[x1][y1][1]>0);
        assert(ns0-t[x0][y0][0]<t[x0][y0][1]);
        assert(ns0-t[x0][y0][0]>=0);
        assert(ns1-t[x1][y1][0]<t[x1][y1][1]);
        assert(ns1-t[x1][y1][0]>=0);
        ++state;
      }
    }
  }
//  printf("%d states\n", state); exit(0);  // uncomment to print table above
}

#endif

///////////////////////////// Squash //////////////////////////////

// return p = 1/(1 + exp(-d)), d scaled by 8 bits, p scaled by 12 bits
int squash(int d) {
  static const int t[33]={
    1,2,3,6,10,16,27,45,73,120,194,310,488,747,1101,
    1546,2047,2549,2994,3348,3607,3785,3901,3975,4022,
    4050,4068,4079,4085,4089,4092,4093,4094};
  if (d>2047) return 4095;
  if (d<-2047) return 0;
  int w=d&127;
  d=(d>>7)+16;
  return (t[d]*(128-w)+t[(d+1)]*w+64) >> 7;
}

//////////////////////////// Stretch ///////////////////////////////

// Inverse of squash. d = ln(p/(1-p)), d scaled by 8 bits, p by 12 bits.
// d has range -2047 to 2047 representing -8 to 8.  p has range 0 to 4095.

class Stretch {
  Array<short> t;
public:
  Stretch();
  int operator()(int p) const {
    assert(p>=0 && p<4096);
    return t[p];
  }
} stretch;

Stretch::Stretch(): t(4096) {
  int pi=0;
  for (int x=-2047; x<=2047; ++x) {  // invert squash()
    int i=squash(x);
    for (int j=pi; j<=i; ++j)
      t[j]=(short)x;
    pi=i+1;
  }
  t[4095]=2047;
}

//////////////////////////// Mixer /////////////////////////////

// Mixer m(N, M, S=1, w=0) combines models using M neural networks with
//   N inputs each, of which up to S may be selected.  If S > 1 then
//   the outputs of these neural networks are combined using another
//   neural network (with parameters S, 1, 1).  If S = 1 then the
//   output is direct.  The weights are initially w (+-32K).
//   It is used as follows:
// m.update() trains the network where the expected output is the
//   last bit (in the global variable y).
// m.add(stretch(p)) inputs prediction from one of N models.  The
//   prediction should be positive to predict a 1 bit, negative for 0,
//   nominally +-256 to +-2K.  The maximum allowed value is +-32K but
//   using such large values may cause overflow if N is large.
// m.set(cxt, range) selects cxt as one of 'range' neural networks to
//   use.  0 <= cxt < range.  Should be called up to S times such
//   that the total of the ranges is <= M.
// m.p() returns the output prediction that the next bit is 1 as a
//   12 bit number (0 to 4095).

#ifdef __GNUC__
__attribute__ ((target ("avx2")))
#endif
static inline int dot_product_simd_avx2 (const short* const t, const short* const w, int n) {
  __m256i sum = _mm256_setzero_si256();

  while ((n-=16)>=0) {
    __m256i tmp = _mm256_madd_epi16(*( __m256i* ) &t[n], *( __m256i* ) &w[n]);
    tmp = _mm256_srai_epi32(tmp, 8);
    sum = _mm256_add_epi32(sum, tmp);
  }

  __m128i lo = _mm256_extractf128_si256(sum, 0);
  __m128i hi = _mm256_extractf128_si256(sum, 1);

  __m128i newsum = _mm_hadd_epi32(lo, hi);
  newsum = _mm_add_epi32(newsum, _mm_srli_si128(newsum, 8));
  newsum = _mm_add_epi32(newsum, _mm_srli_si128(newsum, 4));
  return _mm_cvtsi128_si32(newsum);
}

#ifdef __GNUC__
__attribute__ ((target ("avx2")))
#endif
static inline void train_simd_avx2 (const short* const t, short* const w, int n, const int e)  {
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i err = _mm256_set1_epi16(short(e));

  while ((n-=16)>=0) {
    __m256i tmp = _mm256_adds_epi16(*( __m256i* ) &t[n], *( __m256i* ) &t[n]);
    tmp = _mm256_mulhi_epi16(tmp, err);
    tmp = _mm256_adds_epi16(tmp, one);
    tmp = _mm256_srai_epi16(tmp, 1);
    tmp = _mm256_adds_epi16(tmp, *( __m256i* ) &w[n]);
    *( __m256i* ) &w[n] = tmp;
  }
}

#ifdef __GNUC__
__attribute__ ((target ("sse2")))
#endif
static inline int dot_product_simd_sse2(const short* const t, const short* const w, int n) {
  __m128i sum = _mm_setzero_si128();

  while ((n-=8)>=0) {
    __m128i tmp = _mm_madd_epi16(*(__m128i *) &t[n], *(__m128i *) &w[n]);
    tmp = _mm_srai_epi32(tmp, 8);
    sum = _mm_add_epi32(sum, tmp);
  }

  sum = _mm_add_epi32(sum, _mm_srli_si128 (sum, 8));
  sum = _mm_add_epi32(sum, _mm_srli_si128 (sum, 4));
  return _mm_cvtsi128_si32(sum);
}

#ifdef __GNUC__
__attribute__ ((target ("sse2")))
#endif
static inline void train_simd_sse2(const short* const t, short* const w, int n, const int e) {
  const __m128i one = _mm_set1_epi16(1);
  const __m128i err = _mm_set1_epi16(short(e));

  while ((n-=8)>=0) {
    __m128i tmp = _mm_adds_epi16(*(__m128i *) &t[n], *(__m128i *) &t[n]);
    tmp = _mm_mulhi_epi16(tmp, err);
    tmp = _mm_adds_epi16(tmp, one);
    tmp = _mm_srai_epi16(tmp, 1);
    tmp = _mm_adds_epi16(tmp, *(__m128i *) &w[n]);
    *(__m128i *) &w[n] = tmp;
  }
}

static inline int dot_product_simd_none(const short* const t, const short* const w, int n) {
  int sum = 0;
  while ((n -= 2) >= 0) {
    sum += (t[n] * w[n] + t[n + 1] * w[n + 1]) >> 8;
  }
  return sum;
}

static inline void train_simd_none(const short* const t, short* const w, int n, const int err) {
  while ((n -= 1) >= 0) {
    int wt = w[n] + ((((t[n] * err * 2) >> 16) + 1) >> 1);
    if (wt < -32768) wt = -32768;
    else if (wt > 32767) wt = 32767;
    w[n] = (short)wt;
  }
}

struct ErrorInfo {
  U32 Data[2], Sum, Mask, Collected;
};

inline U32 SQR(U32 x) {
  return x*x;
}

#define DEFAULT_LEARNING_RATE 7

typedef enum {SIMD_NONE, SIMD_SSE2, SIMD_AVX2} SIMD;
class Mixer {
public:
  virtual ~Mixer() {};
  virtual void update()=0;
  virtual void add(int x)=0;
  virtual void set(int cx, int range)=0;
  virtual void reset()=0;
  virtual int p(const int shift0=0, const int shift1=0)=0;
};

template <SIMD simd> class SIMDMixer: public Mixer {
private:
  //define padding requirements
  const inline int simd_width() const {
    if(simd==SIMD_AVX2)return 32/sizeof(short); //256 bit (32 byte) data size
    if(simd==SIMD_SSE2)return 16/sizeof(short); //128 bit (16 byte) data size
    if(simd==SIMD_NONE)return  4/sizeof(short); //processes 2 shorts at once -> width is 4 bytes
    assert(false);
  }
  const int N, M, S;   // max inputs, max contexts, max context sets
  Array<short, 32> tx; // N inputs from add()
  Array<short, 32> wx; // N*M weights
  Array<int> cxt;      // S contexts
  Array<ErrorInfo> info; // stats for the adaptive learning rates
  Array<int> rates; // learning rates
  int ncxt;        // number of contexts (0 to S)
  int base;        // offset of next context
  int nx;          // Number of inputs in tx, 0 to N
  Array<int> pr;   // last result (scaled 12 bits)
  SIMDMixer* mp;       // points to a SIMDMixer to combine results
public:
  SIMDMixer(int n, int m, int s=1, int w=0);

  // Adjust weights to minimize coding cost of last prediction
  void update() {
    int target=y<<12;
    if(nx>0)
    for (int i=0; i<ncxt; ++i) {
      int err=target-pr[i];
      if(simd==SIMD_NONE)
        train_simd_none(&tx[0], &wx[cxt[i]*N], nx, err*rates[i]);
      if(simd==SIMD_SSE2)
        train_simd_sse2(&tx[0], &wx[cxt[i]*N], nx, err*rates[i]);
      if(simd==SIMD_AVX2)
        train_simd_avx2(&tx[0], &wx[cxt[i]*N], nx, err*rates[i]);
      if (options&OPTION_ADAPTIVE){
        U32 logErr=min(0xF,ilog2(abs(err)));
        info[i].Sum-=SQR(info[i].Data[1]>>28);
        info[i].Data[1]<<=4; info[i].Data[1]|=info[i].Data[0]>>28;
        info[i].Data[0]<<=4; info[i].Data[0]|=logErr;
        info[i].Sum+=SQR(logErr);
        info[i].Collected+=info[i].Collected<4096;
        info[i].Mask<<=1; info[i].Mask|=(logErr<=((info[i].Data[0]>>4)&0xF));
        U32 count=BitCount(info[i].Mask);
        if (info[i].Collected>=64 && (info[i].Sum>1500+U32(rates[i])*64 || count<9 || (info[i].Mask&0xFF)==0)){
          rates[i]=DEFAULT_LEARNING_RATE;
          memset(&info[i], 0, sizeof(ErrorInfo));
        }
        else if (info[i].Collected==4096 && info[i].Sum>=56 && info[i].Sum<=144 && count>28-U32(rates[i]) && ((info[i].Mask&0xFF)==0xFF)){
          rates[i]-=rates[i]>2;
          memset(&info[i], 0, sizeof(ErrorInfo));
        }
      }
    }
    reset();
  }

  // Input x (call up to N times)
  void add(int x) {
    assert(nx<N);
    assert(x==short(x));
    tx[nx++]=(short)x;
  }

  // Set a context (call S times, sum of ranges <= M)
  void set(int cx, int range) {
    assert(range>=0);
    assert(ncxt<S);
    assert(0<=cx && cx<range);
    assert(base+range<=M);
    cxt[ncxt++]=base+cx;
    base+=range;
  }

  void reset() {
    nx=base=ncxt=0;
  }

  // predict next bit
  int p(const int shift0=0, const int shift1=0) {
    while (nx&(simd_width()-1)) tx[nx++]=0;  // pad
    if (mp) {  // combine outputs
      mp->update();
      for (int i=0; i<ncxt; ++i) {
        int dp;
        if(simd==SIMD_NONE)
          dp=dot_product_simd_none(&tx[0], &wx[cxt[i]*N], nx)>>(5+shift0);
        if(simd==SIMD_SSE2)
          dp=dot_product_simd_sse2(&tx[0], &wx[cxt[i]*N], nx)>>(5+shift0);
        if(simd==SIMD_AVX2)
          dp=dot_product_simd_avx2(&tx[0], &wx[cxt[i]*N], nx)>>(5+shift0);
        if(dp<-2047)dp=-2047;else if(dp>2047)dp=2047;
        mp->add(dp);
        pr[i]=squash(dp);
      }
      mp->set(0, 1);
      return mp->p(shift0, shift1);
    }
    else {  // S=1 context
      int dp;
      if(simd==SIMD_NONE)
        dp=dot_product_simd_none(&tx[0], &wx[0], nx)>>(8+shift1);
      if(simd==SIMD_SSE2)
        dp=dot_product_simd_sse2(&tx[0], &wx[0], nx)>>(8+shift1);
      if(simd==SIMD_AVX2)
        dp=dot_product_simd_avx2(&tx[0], &wx[0], nx)>>(8+shift1);
      return pr[0]=squash(dp);
    }
  }
  ~SIMDMixer();
};

template<SIMD simd> SIMDMixer<simd>::~SIMDMixer() {
  delete mp;
}

template<SIMD simd> SIMDMixer<simd>::SIMDMixer(int n, int m, int s, int w):
    N((n+(simd_width()-1))&-(simd_width())), M(m), S(s), tx(N), wx(N*M), cxt(S), info(S), rates(S),
    ncxt(0), base(0), nx(0), pr(S), mp(0) {
  assert(n>0 && N>0 && (N&(simd_width()-1))==0 && M>0);
  int i;
  for (i=0; i<S; ++i){
    pr[i]=2048; //initial p=0.5
    rates[i] = DEFAULT_LEARNING_RATE;
    memset(&info[i], 0, sizeof(ErrorInfo));
  }
  for (i=0; i<N*M; ++i)
    wx[i]=w;
  if (S>1) mp=new SIMDMixer<simd>(S, 1, 1);
}

static SIMD chosen_simd=SIMD_NONE; //default value, will be overriden by the CPU dispatcher, and may be overriden from the command line
class MixerFactory {
public:
  static void set_simd(SIMD simd) {
    chosen_simd=simd;
  }
  static Mixer* CreateMixer(int n, int m, int s=1, int w=0) {
    if(chosen_simd==SIMD_NONE)return new SIMDMixer<SIMD_NONE>(n, m, s, w);
    if(chosen_simd==SIMD_SSE2)return new SIMDMixer<SIMD_SSE2>(n, m, s, w);
    if(chosen_simd==SIMD_AVX2)return new SIMDMixer<SIMD_AVX2>(n, m, s, w);
    assert(false);
    return nullptr;
  }
};

//////////////////////////// APM1 //////////////////////////////

// APM1 maps a probability and a context into a new probability
// that bit y will next be 1.  After each guess it updates
// its state to improve future guesses.  Methods:
//
// APM1 a(N) creates with N contexts, uses 66*N bytes memory.
// a.p(pr, cx, rate=7) returned adjusted probability in context cx (0 to
//   N-1).  rate determines the learning rate (smaller = faster, default 7).
//   Probabilities are scaled 12 bits (0-4095).

class APM1 {
  int index;     // last p, context
  const int N;   // number of contexts
  Array<U16> t;  // [N][33]:  p, context -> p
public:
  APM1(int n);
  int p(int pr=2048, int cxt=0, int rate=7) {
    assert(pr>=0 && pr<4096 && cxt>=0 && cxt<N && rate>0 && rate<32);
    //adapt (update prediction from previous pass)
    int g=(y<<16)+(y<<rate)-y-y;
    t[index] += (g-t[index]) >> rate;
    t[index+1] += (g-t[index+1]) >> rate;
    //predict
    pr = stretch(pr);
    const int w=pr&127;  // interpolation weight (33 points)
    index=((pr+2048)>>7)+cxt*33;
    return (t[index]*(128-w)+t[index+1]*w) >> 11;
  }
};

// maps p, cxt -> p initially
APM1::APM1(int n): index(0), N(n), t(n*33) {
  for (int i=0; i<N; ++i)
    for (int j=0; j<33; ++j)
      t[i*33+j] = i==0 ? squash((j-16)*128)*16 : t[j];
}

//////////////////////////// StateMap, APM //////////////////////////

// A StateMap maps a context to a probability.  Methods:
//
// Statemap sm(n) creates a StateMap with n contexts using 4*n bytes memory.
// sm.p(y, cx, limit) converts state cx (0..n-1) to a probability (0..4095).
//     that the next y=1, updating the previous prediction with y (0..1).
//     limit (1..1023, default 1023) is the maximum count for computing a
//     prediction.  Larger values are better for stationary sources.

static int dt[1024];  // i -> 16K/(i+i+3)

class StateMap {
protected:
  const int N;  // Number of contexts
  int cxt;      // Context of last prediction
  Array<U32> t;       // cxt -> prediction in high 22 bits, count in low 10 bits
  inline void update(int limit) {
    assert(cxt>=0 && cxt<N);
    U32 *p=&t[cxt], p0=p[0];
    int n=p0&1023;  //count
    int pr=p0>>10;  //prediction
    if (n<limit) ++p0;
    else p0=(p0&0xfffffc00)|limit;
    int target=y<<22;
    int delta=((target-pr)>>3)*dt[n]; //the larger the count (n) the less it should adapt
    p0+=delta&0xfffffc00;
    p[0]=p0;
  }

public:
  StateMap(int n=256);
  void Reset(int Rate=0){
    for (int i=0; i<N; ++i)
      t[i]=(t[i]&0xfffffc00)|min(Rate, t[i]&0x3FF);
  }
  // update bit y (0..1), predict next bit in context cx
  int p(int cx, int limit=1023) {
    assert(cx>=0 && cx<N);
    assert(limit>0 && limit<1024);
    update(limit);
    return t[cxt=cx]>>20;
  }
};

StateMap::StateMap(int n): N(n), cxt(0), t(n) {
  for (int i=0; i<N; ++i)
    t[i]=(1u<<31)+0;  //initial p=0.5, initial count=0
}

// An APM maps a probability and a context to a new probability.  Methods:
//
// APM a(n) creates with n contexts using 4*STEPS*n bytes memory.
// a.pp(y, pr, cx, limit) updates and returns a new probability (0..4095)
//     like with StateMap.  pr (0..4095) is considered part of the context.
//     The output is computed by interpolating pr into STEPS ranges nonlinearly
//     with smaller ranges near the ends.  The initial output is pr.
//     y=(0..1) is the last bit.  cx=(0..n-1) is the other context.
//     limit=(0..1023) defaults to 255.

template <int steps> class APM : public StateMap {
  static_assert(steps>4, "APM: number of steps must be a positive integer bigger than 4");
public:
  APM(int n) : StateMap(n*steps) {
    for (int i=0; i<N; ++i) {
      int p = ((i%steps*2+1)*4096)/(steps*2)-2048;
      t[i] = (U32(squash(p))<<20)+6; //initial count: 6
    }
  }
  int p(int pr, int cx, const int limit=0xFF) {
    assert(pr>=0 && pr<4096);
    assert(cx>=0 && cx<N/steps);
    assert(limit>0 && limit<1024);
    //adapt (update prediction from previous pass)
    update(limit);
    //predict
    pr = (stretch(pr)+2048)*(steps-1);
    int wt = pr&0xfff;  // interpolation weight (0..4095)
    cx = cx*steps+(pr>>12);
    assert(cx>=0 && cx<N-1);
    cxt = cx+(wt>>11);
    pr = ((t[cx]>>13)*(4096-wt)+(t[cx+1]>>13)*wt)>>19;
    return pr;
  }
};


//////////////////////////// hash //////////////////////////////

// Hash 2-5 ints.
inline U32 hash(U32 a, U32 b, U32 c=0xffffffff, U32 d=0xffffffff,
    U32 e=0xffffffff) {
  U32 h=a*200002979u+b*30005491u+c*50004239u+d*70004807u+e*110002499u;
  return h^h>>9^a>>2^b>>3^c>>4^d>>5^e>>6;
}

///////////////////////////// BH ////////////////////////////////

// A BH maps a 32 bit hash to an array of B bytes (checksum and B-2 values)
//
// BH bh(N); creates N element table with B bytes each.
//   N must be a power of 2.  The first byte of each element is
//   reserved for a checksum to detect collisions.  The remaining
//   B-1 bytes are values, prioritized by the first value.  This
//   byte is 0 to mark an unused element.
//
// bh[i] returns a pointer to the i'th element, such that
//   bh[i][0] is a checksum of i, bh[i][1] is the priority, and
//   bh[i][2..B-1] are other values (0-255).
//   The low lg(n) bits as an index into the table.
//   If a collision is detected, up to M nearby locations in the same
//   cache line are tested and the first matching checksum or
//   empty element is returned.
//   If no match or empty element is found, then the lowest priority
//   element is replaced.

// 2 byte checksum with LRU replacement (except last 2 by priority)
template <int B> class BH {
  enum {M=8};  // search limit
  Array<U8> t; // elements
  U32 n; // size-1
public:
  BH(int i): t(i*B), n(i-1) {
    assert(B>=2 && i>0 && (i&(i-1))==0); // size a power of 2?
  }
  U8* operator[](U32 i);
};

template <int B>
inline  U8* BH<B>::operator[](U32 i) {
  U16 chk=(i>>16^i)&0xffff;
  i=i*M&n;
  U8 *p;
  U16 *cp;
  int j;
  for (j=0; j<M; ++j) {
    p=&t[(i+j)*B];
    cp=(U16*)p;
    if (p[2]==0) {*cp=chk;break;}
    if (*cp==chk) break;  // found
  }
  if (j==0) return p+1;  // front
  static U8 tmp[B];  // element to move to front
  if (j==M) {
    --j;
    memset(tmp, 0, B);
    memmove(tmp, &chk, 2);
    if (M>2 && t[(i+j)*B+2]>t[(i+j-1)*B+2]) --j;
  }
  else memcpy(tmp, cp, B);
  memmove(&t[(i+1)*B], &t[i*B], j*B);
  memcpy(&t[i*B], tmp, B);
  return &t[i*B+1];
}

//////////////////////////// HashTable /////////////////////////

// A HashTable is an array of n items representing n contexts.
// Each item is a storage area of B bytes. In every slot the first
// byte is a checksum using the upper 8 bits of the context
// selector. The second byte is a priority (0 = empty) for hash
// replacement. Priorities must be set by the caller (0: lowest,
// 255: highest). Items with lower priorities will be replaced in
// case of a collision. Only 2 additional items are probed for the
// given checksum.
// The caller can store any information about the given context
// in bytes [2..B-1].
// Checksums must not be modified by the caller.
// The index need not be a hash.

// HashTable<B> h(n) - creates a hashtable with n slots where
//     n and B must be powers of 2 with n >= B*4, and B >= 2.
// h[i] returns a pointer to the storage area starting from
//     position 1 (e.g. without the checksum)

template <int B>
class HashTable {
private:
  Array<U8,64> t;  // storage area for n items (1 item = B bytes): 0:checksum 1:priority 2:data 3:data  ... B-1:data
  const int N;     // number of items in table
public:
  HashTable(int n): t(n), N(n) {
    assert(B>=2 && (B&(B-1))==0);
    assert(N>=B*4 && (N&(N-1))==0);
  }
  U8* operator[](U32 i);
};

template <int B>
inline U8* HashTable<B>::operator[](U32 i) { //i: context selector
  i*=123456791;
  i=i<<16|i>>16;
  i*=234567891;
  U8 chk=U8(i>>24); //8-bit checksum
  i=(i*B)&(N-B); //force bounds
  //search for the checksum in t
  U8 *p = &t[0];
  if (p[i]==chk) return p+i+1;
  if (p[i^B]==chk) return p+(i^B)+1;
  if (p[i^(B*2)]==chk) return p+(i^(B*2))+1;
  //not found, let's overwrite the lowest priority element
  if (p[i+1]>p[(i+1)^B] || p[i+1]>p[(i+1)^(B*2)]) i^=B;
  if (p[i+1]>p[(i+1)^B^(B*2)]) i^=B^(B*2);
  memset(p+i, 0, B);
  p[i]=chk;
  return p+i+1;
}

/////////////////////////// ContextMap /////////////////////////
//
// A ContextMap maps contexts to a bit histories and makes predictions
// to a Mixer.  Methods common to all classes:
//
// ContextMap cm(M, C); creates using about M bytes of memory (a power
//   of 2) for C contexts.
// cm.set(cx);  sets the next context to cx, called up to C times
//   cx is an arbitrary 32 bit value that identifies the context.
//   It should be called before predicting the first bit of each byte.
// cm.mix(m) updates Mixer m with the next prediction.  Returns 1
//   if context cx is found, else 0.  Then it extends all the contexts with
//   global bit y.  It should be called for every bit:
//
//     if (bpos==0)
//       for (int i=0; i<C; ++i) cm.set(cxt[i]);
//     cm.mix(m);
//
// The different types are as follows:
//
// - RunContextMap.  The bit history is a count of 0-255 consecutive
//     zeros or ones.  Uses 4 bytes per whole byte context.  C=1.
//     The context should be a hash.
// - SmallStationaryContextMap.  0 <= cx*256 < M.
//     The state is a 16-bit probability that is adjusted after each
//     prediction.  C=1.
// - ContextMap.  For large contexts, C >= 1.  Context need not be hashed.

// Predict to mixer m from bit history state s, using sm to map s to
// a probability.
inline void mix2(Mixer& m, int s, StateMap& sm) {
  int p1=sm.p(s);
  int n0=-!nex(s,2);
  int n1=-!nex(s,3);
  int st=stretch(p1)>>2;
  m.add(st);
  p1>>=4;
  int p0=255-p1;
  m.add(p1-p0);
  m.add(st*abs(n1-n0));
  m.add((p1&n0)-(p0&n1));
  m.add((p1&n1)-(p0&n0));
}

// A RunContextMap maps a context into the next byte and a repeat
// count up to M.  Size should be a power of 2.  Memory usage is 3M/4.
class RunContextMap {
  BH<4> t;
  U8* cp;
public:
  RunContextMap(int m): t(m/4) {cp=t[0]+1;}
  void set(U32 cx) {  // update count
    if (cp[0]==0 || cp[1]!=buf(1)) cp[0]=1, cp[1]=buf(1);
    else if (cp[0]<255) ++cp[0];
    cp=t[cx]+1;
  }
  int stretched_p() {  // predict next bit
    if ((cp[1]+256)>>(8-bpos)==c0) {
      int sign=(cp[1]>>(7-bpos)&1)*2-1;
      return sign*ilog(cp[0]+1)*8;
    }
    else
      return 0; //p=0.5
  }
  int mix(Mixer& m) {  // return run length
    m.add(stretched_p());
    return cp[0]!=0;
  }
};

/*
Map for modelling contexts of (nearly-)stationary data.
The context is looked up directly. For each bit modelled, a 16bit prediction is stored.
The adaptation rate is controlled by the caller, see mix().

- BitsOfContext: How many bits to use for each context. Higher bits are discarded.
- BitsPerContext: How many bits [1..8] of input are to be modelled for each context.
New contexts must be set at those intervals.

Uses 2^(BitsOfContext+BitsPerContext+1) bytes of memory.
*/
class SmallStationaryContextMap {
  Array<U16> Data;
  int Context, Mask, bCount, B;
  U16 *cp;
public:
  SmallStationaryContextMap(int BitsOfContext, int BitsPerContext = 8) : Data(1ull<<(BitsOfContext+BitsPerContext)), Context(0), Mask(1<<BitsPerContext), bCount(1), B(1) {
    assert(BitsOfContext<=16);
    assert(BitsPerContext && BitsPerContext<=8);
    Reset();
    cp=&Data[0];
  }
  void set(U32 ctx) {
    Context = (ctx*Mask)&(Data.size() - Mask);
    bCount=B=1;
  }
  void Reset() {
    for (U32 i=0; i<Data.size(); ++i)
      Data[i]=0x7FFF;
  }
  void mix(Mixer& m, const int rate = 7, const int Multiplier = 1, const int Divisor = 4) {
    *cp+=((y<<16)-(*cp)+(1<<(rate-1)))>>rate;
    B+=(y && B>1);
    cp = &Data[Context+B];
    int Prediction = (*cp)>>4;
    m.add((stretch(Prediction)*Multiplier)/Divisor);
    m.add(((Prediction-2048)*Multiplier)/(Divisor*2));
    bCount<<=1; B<<=1;
    if (bCount==Mask) {
      bCount=1;
      B=1;
    }
  }
};

/*
  Map for modelling contexts of (nearly-)stationary data.
  The context is looked up directly. For each bit modelled, a 32bit element stores
  a 22 bit prediction and a 10 bit adaptation rate offset.

  - BitsOfContext: How many bits to use for each context. Higher bits are discarded.
  - BitsPerContext: How many bits [1..8] of input are to be modelled for each context.
    New contexts must be set at those intervals.
  - Rate: Initial adaptation rate offset [0..1023]. Lower offsets mean faster adaptation.
    Will be increased on every occurrence until the higher bound is reached.

  Uses 2^(BitsOfContext+BitsPerContext+2) bytes of memory.
*/

class StationaryMap {
  Array<U32> Data;
  int Context, Mask, bCount, B;
  U32 *cp;
public:
  StationaryMap(int BitsOfContext, int BitsPerContext = 8, int Rate = 0): Data(1ull<<(BitsOfContext+BitsPerContext)), Context(0), Mask(1<<BitsPerContext), bCount(1), B(1) {
    assert(BitsOfContext<=16);
    assert(BitsPerContext && BitsPerContext<=8);
    Reset(Rate);
    cp=&Data[0];
  }
  void set(U32 ctx) {
    Context = (ctx*Mask)&(Data.size()-Mask);
    bCount=B=1;
  }
  void Reset( int Rate = 0 ){
    for (U32 i=0; i<Data.size(); ++i)
      Data[i]=(0x7FF<<20)|min(0x3FF,Rate);
  }
  void mix(Mixer& m, const int Multiplier = 1, const int Divisor = 4, const U16 Limit = 0x3FF) {
    U32 Count = min(min(Limit,0x3FF), ((*cp)&0x3FF)+1);
    int Prediction = (*cp)>>10, Error = (y<<22)-Prediction;
    Error = ((Error/8)*dt[Count])/1024;
    Prediction = min(0x3FFFFF,max(0,Prediction+Error));
    *cp = (Prediction<<10)|Count;
    B+=(y && B>1);
    cp=&Data[Context+B];
    Prediction = (*cp)>>20;
    m.add((stretch(Prediction)*Multiplier)/Divisor);
    m.add(((Prediction-2048)*Multiplier)/(Divisor*2));
    bCount<<=1; B<<=1;
    if (bCount==Mask){
      bCount=1;
      B=1;
    }
  }
};

// Context map for large contexts.  Most modeling uses this type of context
// map.  It includes a built in RunContextMap to predict the last byte seen
// in the same context, and also bit-level contexts that map to a bit
// history state.
//
// Bit histories are stored in a hash table.  The table is organized into
// 64-byte buckets aligned on cache page boundaries.  Each bucket contains
// a hash chain of 7 elements, plus a 2 element queue (packed into 1 byte)
// of the last 2 elements accessed for LRU replacement.  Each element has
// a 2 byte checksum for detecting collisions, and an array of 7 bit history
// states indexed by the last 0 to 2 bits of context.  The buckets are indexed
// by a context ending after 0, 2, or 5 bits of the current byte.  Thus, each
// byte modeled results in 3 main memory accesses per context, with all other
// accesses to cache.
//
// On bits 0, 2 and 5, the context is updated and a new bucket is selected.
// The most recently accessed element is tried first, by comparing the
// 16 bit checksum, then the 7 elements are searched linearly.  If no match
// is found, then the element with the lowest priority among the 5 elements
// not in the LRU queue is replaced.  After a replacement, the queue is
// emptied (so that consecutive misses favor a LFU replacement policy).
// In all cases, the found/replaced element is put in the front of the queue.
//
// The priority is the state number of the first element (the one with 0
// additional bits of context).  The states are sorted by increasing n0+n1
// (number of bits seen), implementing a LFU replacement policy.
//
// When the context ends on a byte boundary (bit 0), only 3 of the 7 bit
// history states are used.  The remaining 4 bytes implement a run model
// as follows: <count:7,d:1> <b1> <unused> <unused> where <b1> is the last byte
// seen, possibly repeated.  <count:7,d:1> is a 7 bit count and a 1 bit
// flag (represented by count * 2 + d).  If d=0 then <count> = 1..127 is the
// number of repeats of <b1> and no other bytes have been seen.  If d is 1 then
// other byte values have been seen in this context prior to the last <count>
// copies of <b1>.
//
// As an optimization, the last two hash elements of each byte (representing
// contexts with 2-7 bits) are not updated until a context is seen for
// a second time.  This is indicated by <count,d> = <1,0> (2).  After update,
// <count,d> is updated to <2,0> or <1,1> (4 or 3).

class ContextMap {
  const int C;  // max number of contexts
  class E {  // hash element, 64 bytes
    U16 chk[7];  // byte context checksums
    U8 last;     // last 2 accesses (0-6) in low, high nibble
  public:
    U8 bh[7][7]; // byte context, 3-bit context -> bit history state
      // bh[][0] = 1st bit, bh[][1,2] = 2nd bit, bh[][3..6] = 3rd bit
      // bh[][0] is also a replacement priority, 0 = empty
    U8* get(U16 chk);  // Find element (0-6) matching checksum.
      // If not found, insert or replace lowest priority (not last).
  };
  Array<E,64> t;  // bit histories for bits 0-1, 2-4, 5-7
    // For 0-1, also contains a run count in bh[][4] and value in bh[][5]
    // and pending update count in bh[7]
  Array<U8*> cp;   // C pointers to current bit history
  Array<U8*> cp0;  // First element of 7 element array containing cp[i]
  Array<U32> cxt;  // C whole byte contexts (hashes)
  Array<U8*> runp; // C [0..3] = count, value, unused, unused
  StateMap *sm;    // C maps of state -> p
  int cn;          // Next context to set by set()
  U8 bit, lastByte;
public:
  ContextMap(int m, int c=1);  // m = memory in bytes, a power of 2, C = c
  ~ContextMap();
  void Train(U8 B);
  void set(U32 cx, int next=-1);   // set next whole byte context to cx; if "next" is 0 then set order does not matter
  int mix(Mixer& m);
};

// Find or create hash element matching checksum ch
inline U8* ContextMap::E::get(U16 ch) {
  if (chk[last&15]==ch) return &bh[last&15][0];
  int b=0xffff, bi=0;
  for (int i=0; i<7; ++i) {
    if (chk[i]==ch) return last=last<<4|i, (U8*)&bh[i][0];
    int pri=bh[i][0];
    if (pri<b && (last&15)!=i && last>>4!=i) b=pri, bi=i;
  }
  return last=0xf0|bi, chk[bi]=ch, (U8*)memset(&bh[bi][0], 0, 7);
}

// Construct using m bytes of memory for c contexts
ContextMap::ContextMap(int m, int c): C(c), t(m>>6), cp(c), cp0(c),
    cxt(c), runp(c), cn(0) {
  assert(m>=64 && (m&(m-1))==0);  // power of 2?
  assert(sizeof(E)==64);
  sm=new StateMap[C];
  for (int i=0; i<C; ++i) {
    cp0[i]=cp[i]=&t[0].bh[0][0];
    runp[i]=cp[i]+3;
  }
  bit=lastByte=0;
}

ContextMap::~ContextMap() {
  delete[] sm;
}

// Set the i'th context to cx
inline void ContextMap::set(U32 cx, int next) {
  int i=cn++;
  i&=next;
  assert(i>=0 && i<C);
  cx=cx*987654323+i;  // permute (don't hash) cx to spread the distribution
  cx=cx<<16|cx>>16;
  cxt[i]=cx*123456791+i;
}

void ContextMap::Train(U8 B){
  U8 bits = 1;
  for (int k=0;k<8;k++){
    for (int i=0; i<cn; ++i){
      if (cp[i])
        *cp[i]=nex(*cp[i], bit);

      // Update context pointers
      if (k>1 && runp[i][0]==0)
        cp[i]=0;
      else
      {
       U16 chksum=cxt[i]>>16;
       U64 tmask=t.size()-1;
       switch(k)
       {
        case 1: case 3: case 6: cp[i]=cp0[i]+1+(bits&1); break;
        case 4: case 7: cp[i]=cp0[i]+3+(bits&3); break;
        case 2: case 5: cp0[i]=cp[i]=t[(cxt[i]+bits)&tmask].get(chksum); break;
        default:
        {
         cp0[i]=cp[i]=t[(cxt[i]+bits)&tmask].get(chksum);
         // Update pending bit histories for bits 2-7
         if (cp0[i][3]==2) {
           const int c=cp0[i][4]+256;
           U8 *p=t[(cxt[i]+(c>>6))&tmask].get(chksum);
           p[0]=1+((c>>5)&1);
           p[1+((c>>5)&1)]=1+((c>>4)&1);
           p[3+((c>>4)&3)]=1+((c>>3)&1);
           p=t[(cxt[i]+(c>>3))&tmask].get(chksum);
           p[0]=1+((c>>2)&1);
           p[1+((c>>2)&1)]=1+((c>>1)&1);
           p[3+((c>>1)&3)]=1+(c&1);
           cp0[i][6]=0;
         }
         // Update run count of previous context
         if (runp[i][0]==0)  // new context
           runp[i][0]=2, runp[i][1]=lastByte;
         else if (runp[i][1]!=lastByte)  // different byte in context
           runp[i][0]=1, runp[i][1]=lastByte;
         else if (runp[i][0]<254)  // same byte in context
           runp[i][0]+=2;
         else if (runp[i][0]==255)
           runp[i][0]=128;
         runp[i]=cp0[i]+3;
        } break;
       }
      }
    }
    bit = (B>>(7-k))&1;
    bits+=bits + bit;
  }
  cn=0;
  lastByte=B;
}

// Update the model with bit y, and predict next bit to mixer m.
int ContextMap::mix(Mixer& m) {
  // Update model with y
  U8 c1=buf(1);
  int result=0;
  for (int i=0; i<cn; ++i) {
    if (cp[i]) {
      assert(cp[i]>=&t[0].bh[0][0] && cp[i]<=&t[t.size()-1].bh[6][6]);
      assert((uintptr_t(cp[i])&63)>=15);
      int ns=nex(*cp[i], y);
      if (ns>=204 && rnd() << ((452-ns)>>3)) ns-=4;  // probabilistic increment
      *cp[i]=ns;
    }

    // Update context pointers
    if (bpos>1 && runp[i][0]==0)
      cp[i]=0;
    else
    {
      U16 chksum=cxt[i]>>16;
      U64 tmask=t.size()-1;
      switch(bpos)
      {
        case 1: case 3: case 6: cp[i]=cp0[i]+1+(c0&1); break;
        case 4: case 7: cp[i]=cp0[i]+3+(c0&3); break;
        case 2: case 5: cp0[i]=cp[i]=t[(cxt[i]+c0)&tmask].get(chksum); break;
        default:
        {
          cp0[i]=cp[i]=t[(cxt[i]+c0)&tmask].get(chksum);
          // Update pending bit histories for bits 2-7
          if (cp0[i][3]==2) {
            const int c=cp0[i][4]+256;
            U8 *p=t[(cxt[i]+(c>>6))&tmask].get(chksum);
            p[0]=1+((c>>5)&1);
            p[1+((c>>5)&1)]=1+((c>>4)&1);
            p[3+((c>>4)&3)]=1+((c>>3)&1);
            p=t[(cxt[i]+(c>>3))&tmask].get(chksum);
            p[0]=1+((c>>2)&1);
            p[1+((c>>2)&1)]=1+((c>>1)&1);
            p[3+((c>>1)&3)]=1+(c&1);
            cp0[i][6]=0;
          }
          // Update run count of previous context
          if (runp[i][0]==0)  // new context
            runp[i][0]=2, runp[i][1]=c1;
          else if (runp[i][1]!=c1)  // different byte in context
            runp[i][0]=1, runp[i][1]=c1;
          else if (runp[i][0]<254)  // same byte in context
            runp[i][0]+=2;
          else if (runp[i][0]==255)
            runp[i][0]=128;
          runp[i]=cp0[i]+3;
        } break;
      }
    }

    // predict from last byte in context
    if ((runp[i][1]+256)>>(8-bpos)==c0) {
      int rc=runp[i][0];  // count*2, +1 if 2 different bytes seen
      int sign=(runp[i][1]>>(7-bpos)&1)*2-1;  // predicted bit + for 1, - for 0
      int c=ilog(rc+1)<<(2+(~rc&1));
      m.add(sign*c);
    }
    else
      m.add(0); //p=0.5

    // predict from bit context
    int s = 0;
    if (cp[i]) s = *cp[i];
    mix2(m,s,sm[i]);

    if(s>0)result++;

  }
  if (bpos==7) cn=0;
  return result;
}

/*
Context map for large contexts (32bits).
Maps to a bit history state, a 3 MRU byte history, and 1 byte RunStats.

Bit and byte histories are stored in a hash table with 64 byte buckets.
The buckets are indexed by a context ending after 0, 2 or 5 bits of the
current byte. Thus, each byte modeled results in 3 main memory accesses
per context, with all other accesses to cache.

On a byte boundary (bit 0), only 3 of the 7 bit history states are used.
Of the remaining 4 bytes, 3 are then used to store the last bytes seen
in this context, 7 bits to store the length of consecutive occurrences of
the previously seen byte, and 1 bit to signal if more than 1 byte as been
seen in this context. The byte history is then combined with the bit history
states to provide additional states that are then mapped to predictions.
*/

class ContextMap2 {
  const U32 C; // max number of contexts
  class Bucket { // hash bucket, 64 bytes
    U16 Checksums[7]; // byte context checksums
    U8 MRU; // last 2 accesses (0-6) in low, high nibble
  public:
    U8 BitState[7][7]; // byte context, 3-bit context -> bit history state
                       // BitState[][0] = 1st bit, BitState[][1,2] = 2nd bit, BitState[][3..6] = 3rd bit
                       // BitState[][0] is also a replacement priority, 0 = empty
    inline U8* Find(U16 Checksum) { // Find or create hash element matching checksum.
                                    // If not found, insert or replace lowest priority (skipping 2 most recent).
      if (Checksums[MRU&15]==Checksum)
        return &BitState[MRU&15][0];
      int worst=0xFFFF, idx=0;
      for (int i=0; i<7; ++i) {
        if (Checksums[i]==Checksum)
          return MRU=MRU<<4|i, (U8*)&BitState[i][0];
        if (BitState[i][0]<worst && (MRU&15)!=i && MRU>>4!=i) {
          worst = BitState[i][0];
          idx=i;
        }
      }
      MRU = 0xF0|idx;
      Checksums[idx] = Checksum;
      return (U8*)memset(&BitState[idx][0], 0, 7);
    }
  };
  Array<Bucket, 64> Table; // bit histories for bits 0-1, 2-4, 5-7
                           // For 0-1, also contains run stats in BitState[][3] and byte history in BitState[][4..6]
  Array<U8*> BitState; // C pointers to current bit history states
  Array<U8*> BitState0; // First element of 7 element array containing BitState[i]
  Array<U8*> ByteHistory; // C pointers to run stats plus byte history, 4 bytes, [RunStats,1..3]
  Array<U32> Contexts; // C whole byte contexts (hashes)
  Array<bool> HasHistory; // True if context has a full valid byte history (i.e., seen at least 3 times)
  StateMap **Maps6b, **Maps8b, **Maps12b;
  U32 index; // Next context to set by set()
  U32 bits;
  U8 lastByte, lastBit, bitPos;
  inline void Update() {
    U64 mask = Table.size()-1;
    for (U32 i=0; i<index; i++) {
      if (BitState[i])
        *BitState[i] = nex(*BitState[i], lastBit);

      if (bitPos>1 && ByteHistory[i][0]==0)
        BitState[i] = nullptr;
      else {
        U16 chksum = Contexts[i]>>16;
        switch (bitPos) {
          case 0: {
            BitState[i] = BitState0[i] = Table[(Contexts[i]+bits)&mask].Find(chksum);
            // Update pending bit histories for bits 2-7
            if (BitState0[i][3]==2) {
              const int c = BitState0[i][4]+256;
              U8 *p = Table[(Contexts[i]+(c>>6))&mask].Find(chksum);
              p[0] = 1+((c>>5)&1);
              p[1+((c>>5)&1)] = 1+((c>>4)&1);
              p[3+((c>>4)&3)] = 1+((c>>3)&1);
              p = Table[(Contexts[i]+(c>>3))&mask].Find(chksum);
              p[0] = 1+((c>>2)&1);
              p[1+((c>>2)&1)] = 1+((c>>1)&1);
              p[3+((c>>1)&3)] = 1+(c&1);
              BitState0[i][6] = 0;
            }
            // Update byte history of previous context
            ByteHistory[i][3] = ByteHistory[i][2];
            ByteHistory[i][2] = ByteHistory[i][1];
            if (ByteHistory[i][0]==0)  // new context
              ByteHistory[i][0]=2, ByteHistory[i][1]=lastByte;
            else if (ByteHistory[i][1]!=lastByte)  // different byte in context
              ByteHistory[i][0]=1, ByteHistory[i][1]=lastByte;
            else if (ByteHistory[i][0]<254)  // same byte in context
              ByteHistory[i][0]+=2;
            else if (ByteHistory[i][0]==255) // more than one byte seen, but long run of current byte, reset to single byte seen
              ByteHistory[i][0] = 128;

            ByteHistory[i] = BitState0[i]+3;
            HasHistory[i] = *BitState0[i]>15;
            break;
          }
          case 2: case 5: {
            BitState[i] = BitState0[i] = Table[(Contexts[i]+bits)&mask].Find(chksum);
            break;
          }
          case 1: case 3: case 6: BitState[i] = BitState0[i]+1+lastBit; break;
          case 4: case 7: BitState[i] = BitState0[i]+3+(bits&3); break;
        }
      }
    }
  }
public:
  // Construct using Size bytes of memory for Count contexts
  ContextMap2(const U64 Size, const U32 Count) : C(Count), Table(Size>>6), BitState(Count), BitState0(Count), ByteHistory(Count), Contexts(Count), HasHistory(Count){
    assert(Size>=64 && (Size&(Size-1))==0);  // power of 2?
    assert(sizeof(Bucket)==64);
    Maps6b = new StateMap*[C];
    Maps8b = new StateMap*[C];
    Maps12b = new StateMap*[C];
    for (U32 i=0; i<C; i++) {
      Maps6b[i] = new StateMap((1<<6)+8);
      Maps8b[i] = new StateMap(1<<8);
      Maps12b[i] = new StateMap((1<<12)+(1<<9));
      BitState[i] = BitState0[i] = &Table[i].BitState[0][0];
      ByteHistory[i] = BitState[i]+3;
    }
    index = 0;
    lastByte = lastBit = 0;
    bits = 1;  bitPos = 0;
  }
  ~ContextMap2() {
    for (U32 i=0; i<C; i++) {
      delete Maps6b[i];
      delete Maps8b[i];
      delete Maps12b[i];
    }
    delete[] Maps6b;
    delete[] Maps8b;
    delete[] Maps12b;
  }
  inline void set(U32 ctx) { // set next whole byte context to ctx
    ctx = ctx*987654323+index; // permute (don't hash) ctx to spread the distribution
    ctx = ctx<<16|ctx>>16;
    Contexts[index] = ctx*123456791+index;
    index++;
    assert(index>0 && index<=C);
  }
  void Train(const U8 B){
    for (bitPos=0; bitPos<8; bitPos++){
      Update();
      lastBit = (B>>(7-bitPos))&1;
      bits += bits+lastBit;
    }
    index = 0;
    bits = 1; bitPos = 0;
    lastByte = B;
  }
  int mix(Mixer& m) {
    int result = 0;
    lastBit = y;
    bitPos = bpos;
    bits+=bits+lastBit;
    lastByte = bits&0xFF;
    if (bitPos==0)
      bits = 1;
    Update();

    for (U32 i=0; i<index; i++) {
      // predict from bit context
      int state = (BitState[i])?*BitState[i]:0;
      result+=(state>0);
      int p1 = Maps8b[i]->p(state);
      int n0=nex(state, 2), n1=nex(state, 3), k=-~n1;
      k = (k*64)/(k-~n0);
      n0=-!n0, n1=-!n1;
      // predict from last byte in context
      if ((U32)((ByteHistory[i][1]+256)>>(8-bitPos))==bits){
        int RunStats = ByteHistory[i][0]; // count*2, +1 if 2 different bytes seen
        int sign=(ByteHistory[i][1]>>(7-bitPos)&1)*2-1;  // predicted bit + for 1, - for 0
        int value = ilog(RunStats+1)<<(3-(RunStats&1));
        m.add(sign*value);
      }
      else if (bitPos>0 && (ByteHistory[i][0]&1)>0) {
        if ((U32)((ByteHistory[i][2]+256)>>(8-bitPos))==bits)
          m.add((((ByteHistory[i][2]>>(7-bitPos))&1)*2-1)*128);
        else if (HasHistory[i] && (U32)((ByteHistory[i][3]+256)>>(8-bitPos))==bits)
          m.add((((ByteHistory[i][3]>>(7-bitPos))&1)*2-1)*128);
        else
          m.add(0);
      }
      else
        m.add(0);

      if (HasHistory[i]) {
        state  = (ByteHistory[i][1]>>(7-bitPos))&1;
        state |= ((ByteHistory[i][2]>>(7-bitPos))&1)*2;
        state |= ((ByteHistory[i][3]>>(7-bitPos))&1)*4;
      }
      else
        state = 8;

      int st = stretch(p1)>>2;
      m.add(st);
      m.add((p1-2047)>>3);
      p1>>=4;
      int p0 = 255-p1;
      m.add(st*abs(n1-n0));
      m.add((p1&n0)-(p0&n1));
      m.add((p1&n1)-(p0&n0));
      m.add(stretch(Maps12b[i]->p((state<<9)|(bitPos<<6)|k))>>2);
      m.add(stretch(Maps6b[i]->p((state<<3)|bitPos))>>2);
    }
    if (bitPos==7) index = 0;
    return result;
  }
};

//////////////////////////// Text modelling /////////////////////////

#define TAB 0x09
#define NEW_LINE 0x0A
#define CARRIAGE_RETURN 0x0D
#define SPACE 0x20

#ifdef USE_TEXTMODEL

inline bool CharInArray(const char c, const char a[], const int len) {
  if (a==nullptr)
    return false;
  int i=0;
  for (; i<len && c!=a[i]; i++);
  return i<len;
}

#define MAX_WORD_SIZE 64

class Word {
public:
  U8 Letters[MAX_WORD_SIZE];
  U8 Start, End;
  U32 Hash[4], Type, Language;
  Word() : Start(0), End(0), Hash{0,0,0,0}, Type(0), Language(0) {
    memset(&Letters[0], 0, sizeof(U8)*MAX_WORD_SIZE);
  }
  bool operator==(const char *s) const {
    size_t len=strlen(s);
    return ((size_t)(End-Start+(Letters[Start]!=0))==len && memcmp(&Letters[Start], s, len)==0);
  }
  bool operator!=(const char *s) const {
    return !operator==(s);
  }
  void operator+=(const char c) {
    if (End<MAX_WORD_SIZE-1) {
      End+=(Letters[End]>0);
      Letters[End]=tolower(c);
    }
  }
  U8 operator[](U8 i) const {
    return (End-Start>=i)?Letters[Start+i]:0;
  }
  U8 operator()(U8 i) const {
    return (End-Start>=i)?Letters[End-i]:0;
  }
  U32 Length() const {
    if (Letters[Start]!=0)
      return End-Start+1;
    return 0;
  }
  void GetHashes() {
    Hash[0] = 0xc01dflu, Hash[1] = ~Hash[0];
    for (int i=Start; i<=End; i++) {
      U8 l = Letters[i];
      Hash[0]^=hash(Hash[0], l, i);
      Hash[1]^=hash(Hash[1],
        ((l&0x80)==0)?l&0x5F:
        ((l&0xC0)==0x80)?l&0x3F:
        ((l&0xE0)==0xC0)?l&0x1F:
        ((l&0xF0)==0xE0)?l&0xF:l&0x7
      );
    }
    Hash[2] = (~Hash[0])^Hash[1];
    Hash[3] = (~Hash[1])^Hash[0];
  }
  bool ChangeSuffix(const char *OldSuffix, const char *NewSuffix) {
    size_t len=strlen(OldSuffix);
    if (Length()>len && memcmp(&Letters[End-len+1], OldSuffix, len)==0) {
      size_t n=strlen(NewSuffix);
      if (n>0) {
        memcpy(&Letters[End-int(len)+1], NewSuffix, min(MAX_WORD_SIZE-1,End+int(n))-End);
        End=min(MAX_WORD_SIZE-1, End-int(len)+int(n));
      }
      else
        End-=U8(len);
      return true;
    }
    return false;
  }
  bool MatchesAny(const char* a[], const int count) {
    int i=0;
    size_t len = (size_t)Length();
    for (; i<count && (len!=strlen(a[i]) || memcmp(&Letters[Start], a[i], len)!=0); i++);
    return i<count;
  }
  bool EndsWith(const char *Suffix) const {
    size_t len=strlen(Suffix);
    return (Length()>len && memcmp(&Letters[End-len+1], Suffix, len)==0);
  }
  bool StartsWith(const char *Prefix) const {
    size_t len=strlen(Prefix);
    return (Length()>len && memcmp(&Letters[Start], Prefix, len)==0);
  }
  void print() const {
    for(int r=Start;r<=End;r++)
      printf("%c",(char)Letters[r]);
    printf("\n");
  }
};

class Segment {
public:
  Word FirstWord; // useful following questions
  U32 WordCount;
  U32 NumCount;
};

class Sentence : public Segment {
public:
  enum Types { // possible sentence types, excluding Imperative
    Declarative,
    Interrogative,
    Exclamative,
    Count
  };
  Types Type;
  U32 SegmentCount;
  U32 VerbIndex; // relative position of last detected verb
  U32 NounIndex; // relative position of last detected noun
  U32 CapitalIndex; // relative position of last capitalized word, excluding the initial word of this sentence
  Word lastVerb, lastNoun, lastCapital;
};

class Paragraph {
public:
  U32 SentenceCount, TypeCount[Sentence::Types::Count], TypeMask;
};

class Language {
public:
  enum Flags {
    Verb                   = (1<<0),
    Noun                   = (1<<1)
  };
  enum Ids {
    Unknown,
    English,
    French,
    German,
    Count
  };
  virtual ~Language() {};
  virtual bool IsAbbreviation(Word *W) = 0;
};

class English: public Language {
private:
  static const int NUM_ABBREV = 6;
  const char *Abbreviations[NUM_ABBREV]={ "mr","mrs","ms","dr","st","jr" };
public:
  enum Flags {
    Adjective              = (1<<2),
    Plural                 = (1<<3),
    Male                   = (1<<4),
    Female                 = (1<<5),
    Negation               = (1<<6),
    PastTense              = (1<<7)|Verb,
    PresentParticiple      = (1<<8)|Verb,
    AdjectiveSuperlative   = (1<<9)|Adjective,
    AdjectiveWithout       = (1<<10)|Adjective,
    AdjectiveFull          = (1<<11)|Adjective,
    AdverbOfManner         = (1<<12),
    SuffixNESS             = (1<<13),
    SuffixITY              = (1<<14)|Noun,
    SuffixCapable          = (1<<15),
    SuffixNCE              = (1<<16),
    SuffixNT               = (1<<17),
    SuffixION              = (1<<18),
    SuffixAL               = (1<<19)|Adjective,
    SuffixIC               = (1<<20)|Adjective,
    SuffixIVE              = (1<<21),
    SuffixOUS              = (1<<22)|Adjective,
    PrefixOver             = (1<<23),
    PrefixUnder            = (1<<24)
  };
  bool IsAbbreviation(Word *W) { return W->MatchesAny(Abbreviations, NUM_ABBREV); };
};

class French: public Language {
private:
  static const int NUM_ABBREV = 2;
  const char *Abbreviations[NUM_ABBREV]={ "m","mm" };
public:
  enum Flags {
    Adjective              = (1<<2),
    Plural                 = (1<<3)
  };
  bool IsAbbreviation(Word *W) { return W->MatchesAny(Abbreviations, NUM_ABBREV); };
};

class German : public Language {
private:
  static const int NUM_ABBREV = 3;
  const char *Abbreviations[NUM_ABBREV]={ "fr","hr","hrn" };
public:
  enum Flags {
    Adjective              = (1<<2),
    Plural                 = (1<<3),
    Female                 = (1<<4)
  };
  bool IsAbbreviation(Word *W) { return W->MatchesAny(Abbreviations, NUM_ABBREV); };
};

//////////////////////////// Stemming routines /////////////////////////

class Stemmer {
protected:
  U32 GetRegion(const Word *W, const U32 From) {
    bool hasVowel = false;
    for (int i=W->Start+From; i<=W->End; i++) {
      if (IsVowel(W->Letters[i])) {
        hasVowel = true;
        continue;
      }
      else if (hasVowel)
        return i-W->Start+1;
    }
    return W->Start+W->Length();
  }
  bool SuffixInRn(const Word *W, const U32 Rn, const char *Suffix) {
    return (W->Start!=W->End && Rn<=W->Length()-strlen(Suffix));
  }
public:
  virtual ~Stemmer() {};
  virtual bool IsVowel(const char c) = 0;
  virtual void Hash(Word *W) = 0;
  virtual bool Stem(Word *W) = 0;
};

/*
  English affix stemmer, based on the Porter2 stemmer.

  Changelog:
  (29/12/2017) v127: Initial release by Márcio Pais
  (02/01/2018) v128: Small changes to allow for compilation with MSVC
  Fix buffer overflow (thank you Jan Ondrus)
  (28/01/2018) v133: Refactoring, added processing of "non/non-" prefixes
  (04/02/2018) v135: Refactoring, added processing of gender-specific words and common words
*/

class EnglishStemmer: public Stemmer {
private:
  static const int NUM_VOWELS = 6;
  const char Vowels[NUM_VOWELS]={'a','e','i','o','u','y'};
  static const int NUM_DOUBLES = 9;
  const char Doubles[NUM_DOUBLES]={'b','d','f','g','m','n','p','r','t'};
  static const int NUM_LI_ENDINGS = 10;
  const char LiEndings[NUM_LI_ENDINGS]={'c','d','e','g','h','k','m','n','r','t'};
  static const int NUM_NON_SHORT_CONSONANTS = 3;
  const char NonShortConsonants[NUM_NON_SHORT_CONSONANTS]={'w','x','Y'};
  static const int NUM_MALE_WORDS = 9;
  const char *MaleWords[NUM_MALE_WORDS]={"he","him","his","himself","man","men","boy","husband","actor"};
  static const int NUM_FEMALE_WORDS = 8;
  const char *FemaleWords[NUM_FEMALE_WORDS]={"she","her","herself","woman","women","girl","wife","actress"};
  static const int NUM_COMMON_WORDS = 12;
  const char *CommonWords[NUM_COMMON_WORDS]={"the","be","to","of","and","in","that","you","have","with","from","but"};
  static const int NUM_SUFFIXES_STEP0 = 3;
  const char *SuffixesStep0[NUM_SUFFIXES_STEP0]={"'s'","'s","'"};
  static const int NUM_SUFFIXES_STEP1b = 6;
  const char *SuffixesStep1b[NUM_SUFFIXES_STEP1b]={"eedly","eed","ed","edly","ing","ingly"};
  const U32 TypesStep1b[NUM_SUFFIXES_STEP1b]={English::AdverbOfManner,0,English::PastTense,English::AdverbOfManner|English::PastTense,English::PresentParticiple,English::AdverbOfManner|English::PresentParticiple};
  static const int NUM_SUFFIXES_STEP2 = 22;
  const char *(SuffixesStep2[NUM_SUFFIXES_STEP2])[2]={
    {"ization", "ize"},
    {"ational", "ate"},
    {"ousness", "ous"},
    {"iveness", "ive"},
    {"fulness", "ful"},
    {"tional", "tion"},
    {"lessli", "less"},
    {"biliti", "ble"},
    {"entli", "ent"},
    {"ation", "ate"},
    {"alism", "al"},
    {"aliti", "al"},
    {"fulli", "ful"},
    {"ousli", "ous"},
    {"iviti", "ive"},
    {"enci", "ence"},
    {"anci", "ance"},
    {"abli", "able"},
    {"izer", "ize"},
    {"ator", "ate"},
    {"alli", "al"},
    {"bli", "ble"}
  };
  const U32 TypesStep2[NUM_SUFFIXES_STEP2]={
    English::SuffixION,
    English::SuffixION|English::SuffixAL,
    English::SuffixNESS,
    English::SuffixNESS,
    English::SuffixNESS,
    English::SuffixION|English::SuffixAL,
    English::AdverbOfManner,
    English::AdverbOfManner|English::SuffixITY,
    English::AdverbOfManner,
    English::SuffixION,
    0,
    English::SuffixITY,
    English::AdverbOfManner,
    English::AdverbOfManner,
    English::SuffixITY,
    0,
    0,
    English::AdverbOfManner,
    0,
    0,
    English::AdverbOfManner,
    English::AdverbOfManner
  };
  static const int NUM_SUFFIXES_STEP3 = 8;
  const char *(SuffixesStep3[NUM_SUFFIXES_STEP3])[2]={
    {"ational", "ate"},
    {"tional", "tion"},
    {"alize", "al"},
    {"icate", "ic"},
    {"iciti", "ic"},
    {"ical", "ic"},
    {"ful", ""},
    {"ness", ""}
  };
  const U32 TypesStep3[NUM_SUFFIXES_STEP3]={English::SuffixION|English::SuffixAL,English::SuffixION|English::SuffixAL,0,0,English::SuffixITY,English::SuffixAL,English::AdjectiveFull,English::SuffixNESS};
  static const int NUM_SUFFIXES_STEP4 = 20;
  const char *SuffixesStep4[NUM_SUFFIXES_STEP4]={"al","ance","ence","er","ic","able","ible","ant","ement","ment","ent","ou","ism","ate","iti","ous","ive","ize","sion","tion"};
  const U32 TypesStep4[NUM_SUFFIXES_STEP4]={
    English::SuffixAL,
    English::SuffixNCE,
    English::SuffixNCE,
    0,
    English::SuffixIC,
    English::SuffixCapable,
    English::SuffixCapable,
    English::SuffixNT,
    0,
    0,
    English::SuffixNT,
    0,
    0,
    0,
    English::SuffixITY,
    English::SuffixOUS,
    English::SuffixIVE,
    0,
    English::SuffixION,
    English::SuffixION
  };
  static const int NUM_EXCEPTION_REGION1 = 3;
  const char *ExceptionsRegion1[NUM_EXCEPTION_REGION1]={"gener","arsen","commun"};
  static const int NUM_EXCEPTIONS1 = 19;
  const char *(Exceptions1[NUM_EXCEPTIONS1])[2]={
    {"skis", "ski"},
    {"skies", "sky"},
    {"dying", "die"},
    {"lying", "lie"},
    {"tying", "tie"},
    {"idly", "idl"},
    {"gently", "gentl"},
    {"ugly", "ugli"},
    {"early", "earli"},
    {"only", "onli"},
    {"singly", "singl"},
    {"sky", "sky"},
    {"news", "news"},
    {"howe", "howe"},
    {"atlas", "atlas"},
    {"cosmos", "cosmos"},
    {"bias", "bias"},
    {"andes", "andes"},
    {"texas", "texas"}
  };
  const U32 TypesExceptions1[NUM_EXCEPTIONS1]={
    English::Noun|English::Plural,
    English::Noun|English::Plural,
    English::PresentParticiple,
    English::PresentParticiple,
    English::PresentParticiple,
    English::AdverbOfManner,
    English::AdverbOfManner,
    English::Adjective,
    English::Adjective|English::AdverbOfManner,
    0,
    English::AdverbOfManner,
    English::Noun,
    English::Noun,
    0,
    English::Noun,
    English::Noun,
    English::Noun,
    English::Noun|English::Plural,
    English::Noun
  };
  static const int NUM_EXCEPTIONS2 = 8;
  const char *Exceptions2[NUM_EXCEPTIONS2]={"inning","outing","canning","herring","earring","proceed","exceed","succeed"};
  const U32 TypesExceptions2[NUM_EXCEPTIONS2]={English::Noun,English::Noun,English::Noun,English::Noun,English::Noun,English::Verb,English::Verb,English::Verb};
  inline bool IsConsonant(const char c) {
    return !IsVowel(c);
  }
  inline bool IsShortConsonant(const char c) {
    return !CharInArray(c, NonShortConsonants, NUM_NON_SHORT_CONSONANTS);
  }
  inline bool IsDouble(const char c) {
    return CharInArray(c, Doubles, NUM_DOUBLES);
  }
  inline bool IsLiEnding(const char c) {
    return CharInArray(c, LiEndings, NUM_LI_ENDINGS);
  }
  U32 GetRegion1(const Word *W) {
    for (int i=0; i<NUM_EXCEPTION_REGION1; i++) {
      if (W->StartsWith(ExceptionsRegion1[i]))
        return U32(strlen(ExceptionsRegion1[i]));
    }
    return GetRegion(W, 0);
  }
  bool EndsInShortSyllable(const Word *W) {
    if (W->End==W->Start)
      return false;
    else if (W->End==W->Start+1)
      return IsVowel((*W)(1)) && IsConsonant((*W)(0));
    else
      return (IsConsonant((*W)(2)) && IsVowel((*W)(1)) && IsConsonant((*W)(0)) && IsShortConsonant((*W)(0)));
  }
  bool IsShortWord(const Word *W) {
    return (EndsInShortSyllable(W) && GetRegion1(W)==W->Length());
  }
  inline bool HasVowels(const Word *W) {
    for (int i=W->Start; i<=W->End; i++) {
      if (IsVowel(W->Letters[i]))
        return true;
    }
    return false;
  }
  bool TrimApostrophes(Word *W) {
    bool result=false;
    //trim all apostrophes from the beginning
    int cnt=0;
    while(W->Start!=W->End && (*W)[0]=='\'') {
      result=true;
      W->Start++;
      cnt++;
    }
    //trim the same number of apostrophes from the end (if there are)
    while(W->Start!=W->End && (*W)(0)=='\'') {
      if(cnt==0)break;
      W->End--;
      cnt--;
    }
    return result;
  }
  void MarkYsAsConsonants(Word *W) {
    if ((*W)[0]=='y')
      W->Letters[W->Start]='Y';
    for (int i=W->Start+1; i<=W->End; i++) {
      if (IsVowel(W->Letters[i-1]) && W->Letters[i]=='y')
        W->Letters[i]='Y';
    }
  }
  bool ProcessPrefixes(Word *W) {
    if (W->StartsWith("irr") && W->Length()>5 && ((*W)[3]=='a' || (*W)[3]=='e'))
      W->Start+=2, W->Type|=English::Negation;
    else if (W->StartsWith("over") && W->Length()>5)
      W->Start+=4, W->Type|=English::PrefixOver;
    else if (W->StartsWith("under") && W->Length()>6)
      W->Start+=5, W->Type|=English::PrefixUnder;
    else if (W->StartsWith("unn") && W->Length()>5)
      W->Start+=2, W->Type|=English::Negation;
    else if (W->StartsWith("non") && W->Length()>(U32)(5+((*W)[3]=='-')))
      W->Start+=2+((*W)[3]=='-'), W->Type|=English::Negation;
    else
      return false;
    return true;
  }
  bool ProcessSuperlatives(Word *W) {
    if (W->EndsWith("est") && W->Length()>4) {
      U8 i=W->End;
      W->End-=3;
      W->Type|=English::AdjectiveSuperlative;

      if ((*W)(0)==(*W)(1) && (*W)(0)!='r' && !(W->Length()>=4 && memcmp("sugg",&W->Letters[W->End-3],4)==0)) {
        W->End-= ( ((*W)(0)!='f' && (*W)(0)!='l' && (*W)(0)!='s') ||
                   (W->Length()>4 && (*W)(1)=='l' && ((*W)(2)=='u' || (*W)(3)=='u' || (*W)(3)=='v'))) &&
                   (!(W->Length()==3 && (*W)(1)=='d' && (*W)(2)=='o'));
        if (W->Length()==2 && ((*W)[0]!='i' || (*W)[1]!='n'))
          W->End = i, W->Type&=~English::AdjectiveSuperlative;
      }
      else {
        switch((*W)(0)) {
          case 'd': case 'k': case 'm': case 'y': break;
          case 'g': {
            if (!( W->Length()>3 && ((*W)(1)=='n' || (*W)(1)=='r') && memcmp("cong",&W->Letters[W->End-3],4)!=0 ))
              W->End = i, W->Type&=~English::AdjectiveSuperlative;
            else
              W->End+=((*W)(2)=='a');
            break;
          }
          case 'i': { W->Letters[W->End]='y'; break; }
          case 'l': {
            if (W->End==W->Start+1 || memcmp("mo",&W->Letters[W->End-2],2)==0)
              W->End = i, W->Type&=~English::AdjectiveSuperlative;
            else
              W->End+=IsConsonant((*W)(1));
            break;
          }
          case 'n': {
            if (W->Length()<3 || IsConsonant((*W)(1)) || IsConsonant((*W)(2)))
              W->End = i, W->Type&=~English::AdjectiveSuperlative;
            break;
          }
          case 'r': {
            if (W->Length()>3 && IsVowel((*W)(1)) && IsVowel((*W)(2)))
              W->End+=((*W)(2)=='u') && ((*W)(1)=='a' || (*W)(1)=='i');
            else
              W->End = i, W->Type&=~English::AdjectiveSuperlative;
            break;
          }
          case 's': { W->End++; break; }
          case 'w': {
            if (!(W->Length()>2 && IsVowel((*W)(1))))
              W->End = i, W->Type&=~English::AdjectiveSuperlative;
            break;
          }
          case 'h': {
            if (!(W->Length()>2 && IsConsonant((*W)(1))))
              W->End = i, W->Type&=~English::AdjectiveSuperlative;
            break;
          }
          default: {
            W->End+=3;
            W->Type&=~English::AdjectiveSuperlative;
          }
        }
      }
    }
    return (W->Type&English::AdjectiveSuperlative)>0;
  }
  //Search for the longest among the suffixes, 's' or 's or ' and remove if found.
  //Exaples: Children’s toys / Vice presidents' duties
  bool Step0(Word *W) {
    for (int i=0; i<NUM_SUFFIXES_STEP0; i++) {
      if (W->EndsWith(SuffixesStep0[i])) {
        W->End-=U8(strlen(SuffixesStep0[i]));
        W->Type|=English::Plural;
        return true;
      }
    }
    return false;
  }

  //Search for the longest among the following suffixes, and perform the action indicated.
  bool Step1a(Word *W) {

    //sses -> replace by ss
    if (W->EndsWith("sses")) {
      W->End-=2;
      W->Type|=English::Plural;
      return true;
    }

    //ied+ ies* -> replace by i if preceded by more than one letter, otherwise by ie (so ties -> tie, cries -> cri) 
    if (W->EndsWith("ied") || W->EndsWith("ies")) {
      W->Type |= ((*W)(0)=='d') ? English::PastTense : English::Plural;
      W->End-= W->Length()>4 ? 2 : 1;
      return true;
    }

    //us+ ss -> do nothing
    if (W->EndsWith("us") || W->EndsWith("ss"))
      return false;

    //s -> delete if the preceding word part contains a vowel not immediately before the s (so gas and this retain the s, gaps and kiwis lose it) 
    if ((*W)(0)=='s' && W->Length()>2) {
      for (int i=W->Start;i<=W->End-2;i++) {
        if (IsVowel(W->Letters[i])) {
          W->End--;
          W->Type|=English::Plural;
          return true;
        }
      }
    }

    if (W->EndsWith("n't") && W->Length()>4) {
      switch ((*W)(3)) {
        case 'a': {
          if ((*W)(4)=='c')
            W->End-=2; // can't -> can
          else
            W->ChangeSuffix("n't","ll"); // shan't -> shall
          break;
        }
        case 'i': { W->ChangeSuffix("in't","m"); break; } // ain't -> am
        case 'o': {
          if ((*W)(4)=='w')
            W->ChangeSuffix("on't","ill"); // won't -> will
          else
            W->End-=3; //don't -> do
          break;
        }
        default: W->End-=3; // couldn't -> could, shouldn't -> should, needn't -> need, hasn't -> has
      }
      W->Type|=English::Negation;	
      return true;
    }

    if (W->EndsWith("hood") && W->Length()>7) {
      W->End-=4;  //brotherhood -> brother
      return true;
    }
    return false;
  }
  //Search for the longest among the following suffixes, and perform the action indicated. 
  bool Step1b(Word *W, const U32 R1) {
    for (int i=0; i<NUM_SUFFIXES_STEP1b; i++) {
      if (W->EndsWith(SuffixesStep1b[i])) {
        switch(i) {
          case 0: case 1: {
            if (SuffixInRn(W, R1, SuffixesStep1b[i]))
              W->End-=1+i*2;
            break;
          }
          default: {
            U8 j=W->End;
            W->End-=U8(strlen(SuffixesStep1b[i]));
            if (HasVowels(W)) {
              if (W->EndsWith("at") || W->EndsWith("bl") || W->EndsWith("iz") || IsShortWord(W))
                (*W)+='e';
              else if (W->Length()>2) {
                if ((*W)(0)==(*W)(1) && IsDouble((*W)(0)))
                  W->End--;
                else if (i==2 || i==3) {
                  switch((*W)(0)) {
                    case 'c': case 's': case 'v': { W->End+=!(W->EndsWith("ss") || W->EndsWith("ias")); break; }
                    case 'd': {
                      static const char nAllowed[4] = {'a','e','i','o'};
                      W->End+=IsVowel((*W)(1)) && (!CharInArray((*W)(2), nAllowed, 4)); break;
                    }
                    case 'k': { W->End+=W->EndsWith("uak"); break; }
                    case 'l': {
                      static const char Allowed1[10] = {'b','c','d','f','g','k','p','t','y','z'};
                      static const char Allowed2[4] = {'a','i','o','u'};
                      W->End+= CharInArray((*W)(1), Allowed1, 10) ||
                                (CharInArray((*W)(1), Allowed2, 4) && IsConsonant((*W)(2)));
                      break;
                    }
                  }
                }
                else if (i>=4) {
                  switch((*W)(0)) {
                    case 'd': {
                      if (IsVowel((*W)(1)) && (*W)(2)!='a' && (*W)(2)!='e' && (*W)(2)!='o')
                        (*W)+='e';
                      break;
                    }
                    case 'g': {
                      static const char Allowed[7] = {'a','d','e','i','l','r','u'};
                      if (
                        CharInArray((*W)(1), Allowed, 7) || (
                         (*W)(1)=='n' && (
                          (*W)(2)=='e' ||
                          ((*W)(2)=='u' && (*W)(3)!='b' && (*W)(3)!='d') ||
                          ((*W)(2)=='a' && ((*W)(3)=='r' || ((*W)(3)=='h' && (*W)(4)=='c'))) ||
                          (W->EndsWith("ring") && ((*W)(4)=='c' || (*W)(4)=='f'))
                         )
                        )
                      )
                        (*W)+='e';
                      break;
                    }
                    case 'l': {
                      if (!((*W)(1)=='l' || (*W)(1)=='r' || (*W)(1)=='w' || (IsVowel((*W)(1)) && IsVowel((*W)(2)))))
                        (*W)+='e';
                      if (W->EndsWith("uell") && W->Length()>4 && (*W)(4)!='q')
                        W->End--;
                      break;
                    }
                    case 'r': {
                      if ((
                        ((*W)(1)=='i' && (*W)(2)!='a' && (*W)(2)!='e' && (*W)(2)!='o') ||
                        ((*W)(1)=='a' && (!((*W)(2)=='e' || (*W)(2)=='o' || ((*W)(2)=='l' && (*W)(3)=='l')))) ||
                        ((*W)(1)=='o' && (!((*W)(2)=='o' || ((*W)(2)=='t' && (*W)(3)!='s')))) ||
                        (*W)(1)=='c' || (*W)(1)=='t') && (!W->EndsWith("str"))
                      )
                        (*W)+='e';
                      break;
                    }
                    case 't': {
                      if ((*W)(1)=='o' && (*W)(2)!='g' && (*W)(2)!='l' && (*W)(2)!='i' && (*W)(2)!='o')
                        (*W)+='e';
                      break;
                    }
                    case 'u': {
                      if (!(W->Length()>3 && IsVowel((*W)(1)) && IsVowel((*W)(2))))
                        (*W)+='e';
                      break;
                    }
                    case 'z': {
                      if (W->EndsWith("izz") && W->Length()>3 && ((*W)(3)=='h' || (*W)(3)=='u'))
                        W->End--;
                      else if ((*W)(1)!='t' && (*W)(1)!='z')
                        (*W)+='e';
                      break;
                    }
                    case 'k': {
                      if (W->EndsWith("uak"))
                        (*W)+='e';
                      break;
                    }
                    case 'b': case 'c': case 's': case 'v': {
                      if (!(
                        ((*W)(0)=='b' && ((*W)(1)=='m' || (*W)(1)=='r')) ||
                        W->EndsWith("ss") || W->EndsWith("ias") || (*W)=="zinc"
                      ))
                        (*W)+='e';
                      break;
                    }
                  }
                }
              }
            }
            else {
              W->End=j;
              return false;
            }
          }
        }
        W->Type|=TypesStep1b[i];
        return true;
      }
    }
    return false;
  }
  bool Step1c(Word *W) {
    if (W->Length()>2 && tolower((*W)(0))=='y' && IsConsonant((*W)(1))) {
      W->Letters[W->End]='i';
      return true;
    }
    return false;
  }
  bool Step2(Word *W, const U32 R1) {
    for (int i=0; i<NUM_SUFFIXES_STEP2; i++) {
      if (W->EndsWith(SuffixesStep2[i][0]) && SuffixInRn(W, R1, SuffixesStep2[i][0])) {
        W->ChangeSuffix(SuffixesStep2[i][0], SuffixesStep2[i][1]);
        W->Type|=TypesStep2[i];
        return true;
      }
    }
    if (W->EndsWith("logi") && SuffixInRn(W, R1, "ogi")) {
      W->End--;
      return true;
    }
    else if (W->EndsWith("li")) {
      if (SuffixInRn(W, R1, "li") && IsLiEnding((*W)(2))) {
        W->End-=2;
        W->Type|=English::AdverbOfManner;
        return true;
      }
      else if (W->Length()>3) {
        switch((*W)(2)) {
            case 'b': {
              W->Letters[W->End]='e';
              W->Type|=English::AdverbOfManner;
              return true;
            }
            case 'i': {
              if (W->Length()>4) {
                W->End-=2;
                W->Type|=English::AdverbOfManner;
                return true;
              }
              break;
            }
            case 'l': {
              if (W->Length()>5 && ((*W)(3)=='a' || (*W)(3)=='u')) {
                W->End-=2;
                W->Type|=English::AdverbOfManner;
                return true;
              }
              break;
            }
            case 's': {
              W->End-=2;
              W->Type|=English::AdverbOfManner;
              return true;
            }
            case 'e': case 'g': case 'm': case 'n': case 'r': case 'w': {
              if (W->Length()>(U32)(4+((*W)(2)=='r'))) {
                W->End-=2;
                W->Type|=English::AdverbOfManner;
                return true;
              }
            }
        }
      }
    }
    return false;
  }
  bool Step3(Word *W, const U32 R1, const U32 R2) {
    bool res=false;
    for (int i=0; i<NUM_SUFFIXES_STEP3; i++) {
      if (W->EndsWith(SuffixesStep3[i][0]) && SuffixInRn(W, R1, SuffixesStep3[i][0])) {
        W->ChangeSuffix(SuffixesStep3[i][0], SuffixesStep3[i][1]);
        W->Type|=TypesStep3[i];
        res=true;
        break;
      }
    }
    if (W->EndsWith("ative") && SuffixInRn(W, R2, "ative")) {
      W->End-=5;
      W->Type|=English::SuffixIVE;
      return true;
    }
    if (W->Length()>5 && W->EndsWith("less")) {
      W->End-=4;
      W->Type|=English::AdjectiveWithout;
      return true;
    }
    return res;
  }

  //Search for the longest among the following suffixes, and, if found and in R2, perform the action indicated.
  bool Step4(Word *W, const U32 R2) {
    bool res=false;
    for (int i=0; i<NUM_SUFFIXES_STEP4; i++) {
      if (W->EndsWith(SuffixesStep4[i]) && SuffixInRn(W, R2, SuffixesStep4[i])) {
        W->End-=U8(strlen(SuffixesStep4[i])-(i>17) /*sion, tion*/); // remove: al   ance   ence   er   ic   able   ible   ant   ement   ment   ent   ism   ate   iti   ous   ive   ize ; ion -> delete if preceded by s or t
        if (!(i==10 /* ent */ && (*W)(0)=='m')) //exception: no TypesStep4 should be assigned for 'agreement'
          W->Type|=TypesStep4[i]; 
        if (i==0 && W->EndsWith("nti")) { 
          W->End--; // ntial -> nt
          res=true; // process ant+ial, ent+ial
          continue;
        }
        return true;
      }
    }
    return res;
  }

  //Search for the the following suffixes, and, if found, perform the action indicated. 
  bool Step5(Word *W, const U32 R1, const U32 R2) {
    if ((*W)(0)=='e' && (*W)!="here") {
      if (SuffixInRn(W, R2, "e"))
        W->End--; //e -> delete if in R2, or in R1 and not preceded by a short syllable 
      else if (SuffixInRn(W, R1, "e")) {
        W->End--; //e -> delete if in R1 and not preceded by a short syllable 
        W->End+=EndsInShortSyllable(W);
      }
      else
        return false;
      return true;
    }
    else if (W->Length()>1 && (*W)(0)=='l' && SuffixInRn(W, R2, "l") && (*W)(1)=='l') {
      W->End--; //l -> delete if in R2 and preceded by l 
      return true;
    }
    return false;
  }
public:
  inline bool IsVowel(const char c) final {
    return CharInArray(c, Vowels, NUM_VOWELS);
  }
  inline void Hash(Word *W) final {
    W->Hash[2] = W->Hash[3] = 0xb0a710ad;
    for (int i=W->Start; i<=W->End; i++) {
      U8 l = W->Letters[i];
      W->Hash[2]=W->Hash[2]*263*32 + l;
      if (IsVowel(l))
        W->Hash[3]=W->Hash[3]*997*8 + (l/4-22);
      else if (l>='b' && l<='z')
        W->Hash[3]=W->Hash[3]*271*32 + (l-97);
      else
        W->Hash[3]=W->Hash[3]*11*32 + l;
    }
  }
  bool Stem(Word *W) {
    if (W->Length()<2) {
      Hash(W);
      //W->print(); //for debugging
      return false;
    }
    bool res = TrimApostrophes(W);
    res|=ProcessPrefixes(W);
    res|=ProcessSuperlatives(W);
    for (int i=0; i<NUM_EXCEPTIONS1; i++) {
      if ((*W)==Exceptions1[i][0]) {
        if (i<11) {
          size_t len=strlen(Exceptions1[i][1]);
          memcpy(&W->Letters[W->Start], Exceptions1[i][1], len);
          W->End=W->Start+U8(len-1);
        }
        Hash(W);
        W->Type|=TypesExceptions1[i];
        W->Language = Language::English;
        //W->print(); //for debugging
        return (i<11);
      }
    }

    // Start of modified Porter2 Stemmer
    MarkYsAsConsonants(W);
    U32 R1=GetRegion1(W), R2=GetRegion(W,R1);
    res|=Step0(W);
    res|=Step1a(W);
    for (int i=0; i<NUM_EXCEPTIONS2; i++) {
      if ((*W)==Exceptions2[i]) {
        Hash(W);
        W->Type|=TypesExceptions2[i];
        W->Language = Language::English;
        //W->print(); //for debugging
        return res;
      }
    }
    res|=Step1b(W, R1);
    res|=Step1c(W);
    res|=Step2(W, R1);
    res|=Step3(W, R1, R2);
    res|=Step4(W, R2);
    res|=Step5(W, R1, R2);

    for (U8 i=W->Start; i<=W->End; i++) {
      if (W->Letters[i]=='Y')
        W->Letters[i]='y';
    }
    if (!W->Type || W->Type==English::Plural) {
      if (W->MatchesAny(MaleWords, NUM_MALE_WORDS))
        res = true, W->Type|=English::Male;
      else if (W->MatchesAny(FemaleWords, NUM_FEMALE_WORDS))
        res = true, W->Type|=English::Female;
    }
    if (!res)
      res=W->MatchesAny(CommonWords, NUM_COMMON_WORDS);
    Hash(W);
    if (res)
      W->Language = Language::English;
    //W->print(); //for debugging
    return res;
  }
};

/*
  French suffix stemmer, based on the Porter stemmer.

  Changelog:
  (28/01/2018) v133: Initial release by Márcio Pais
  (04/02/2018) v135: Added processing of common words
  (25/02/2018) v139: Added UTF8 conversion
*/

class FrenchStemmer: public Stemmer {
private:
  static const int NUM_VOWELS = 17;
  const char Vowels[NUM_VOWELS]={'a','e','i','o','u','y','\xE2','\xE0','\xEB','\xE9','\xEA','\xE8','\xEF','\xEE','\xF4','\xFB','\xF9'};
  static const int NUM_COMMON_WORDS = 10;
  const char *CommonWords[NUM_COMMON_WORDS]={"de","la","le","et","en","un","une","du","que","pas"};
  static const int NUM_EXCEPTIONS = 3;
  const char *(Exceptions[NUM_EXCEPTIONS])[2]={
    {"monument", "monument"},
    {"yeux", "oeil"},
    {"travaux", "travail"},
  };
  const U32 TypesExceptions[NUM_EXCEPTIONS]={
    French::Noun,
    French::Noun|French::Plural,
    French::Noun|French::Plural
  };
  static const int NUM_SUFFIXES_STEP1 = 39;
  const char *SuffixesStep1[NUM_SUFFIXES_STEP1]={
    "ance","iqUe","isme","able","iste","eux","ances","iqUes","ismes","ables","istes", //11
    "atrice","ateur","ation","atrices","ateurs","ations", //6
    "logie","logies", //2
    "usion","ution","usions","utions", //4
    "ence","ences", //2
    "issement","issements", //2
    "ement","ements", //2
    "it\xE9","it\xE9s", //2
    "if","ive","ifs","ives", //4
    "euse","euses", //2
    "ment","ments" //2
  };
  static const int NUM_SUFFIXES_STEP2a = 35;
  const char *SuffixesStep2a[NUM_SUFFIXES_STEP2a]={
    "issaIent", "issantes", "iraIent", "issante",
    "issants", "issions", "irions", "issais",
    "issait", "issant", "issent", "issiez", "issons",
    "irais", "irait", "irent", "iriez", "irons",
    "iront", "isses", "issez", "\xEEmes",
    "\xEEtes", "irai", "iras", "irez", "isse",
    "ies", "ira", "\xEEt", "ie", "ir", "is",
    "it", "i"
  };
  static const int NUM_SUFFIXES_STEP2b = 38;
  const char *SuffixesStep2b[NUM_SUFFIXES_STEP2b]={
    "eraIent", "assions", "erions", "assent",
    "assiez", "\xE8rent", "erais", "erait",
    "eriez", "erons", "eront", "aIent", "antes",
    "asses", "ions", "erai", "eras", "erez",
    "\xE2mes", "\xE2tes", "ante", "ants",
    "asse", "\xE9""es", "era", "iez", "ais",
    "ait", "ant", "\xE9""e", "\xE9s", "er",
    "ez", "\xE2t", "ai", "as", "\xE9", "a"
  };
  static const int NUM_SET_STEP4 = 6;
  const char SetStep4[NUM_SET_STEP4]={'a','i','o','u','\xE8','s'};
  static const int NUM_SUFFIXES_STEP4 = 7;
  const char *SuffixesStep4[NUM_SUFFIXES_STEP4]={"i\xE8re","I\xE8re","ion","ier","Ier","e","\xEB"};
  static const int NUM_SUFFIXES_STEP5 = 5;
  const char *SuffixesStep5[NUM_SUFFIXES_STEP5]={"enn","onn","ett","ell","eill"};
  inline bool IsConsonant(const char c) {
    return !IsVowel(c);
  }
  void ConvertUTF8(Word *W) {
    for (int i=W->Start; i<W->End; i++) {
      U8 c = W->Letters[i+1]+((W->Letters[i+1]<0xA0)?0x60:0x40);
      if (W->Letters[i]==0xC3 && (IsVowel(c) || (W->Letters[i+1]&0xDF)==0x87)) {
        W->Letters[i] = c;
        if (i+1<W->End)
          memmove(&W->Letters[i+1], &W->Letters[i+2], W->End-i-1);
        W->End--;
      }
    }
  }
  void MarkVowelsAsConsonants(Word *W) {
    for (int i=W->Start; i<=W->End; i++) {
      switch (W->Letters[i]) {
        case 'i': case 'u': {
          if (i>W->Start && i<W->End && (IsVowel(W->Letters[i-1]) || (W->Letters[i-1]=='q' && W->Letters[i]=='u')) && IsVowel(W->Letters[i+1]))
            W->Letters[i] = toupper(W->Letters[i]);
          break;
        }
        case 'y': {
          if ((i>W->Start && IsVowel(W->Letters[i-1])) || (i<W->End && IsVowel(W->Letters[i+1])))
            W->Letters[i] = toupper(W->Letters[i]);
        }
      }
    }
  }
  U32 GetRV(Word *W) {
    U32 len = W->Length(), res = W->Start+len;
    if (len>=3 && ((IsVowel(W->Letters[W->Start]) && IsVowel(W->Letters[W->Start+1])) || W->StartsWith("par") || W->StartsWith("col") || W->StartsWith("tap") ))
      return W->Start+3;
    else {
      for (int i=W->Start+1;i<=W->End;i++) {
        if (IsVowel(W->Letters[i]))
          return i+1;
      }
    }
    return res;
  }
  bool Step1(Word *W, const U32 RV, const U32 R1, const U32 R2, bool *ForceStep2a) {
    int i = 0;
    for (; i<11; i++) {
      if (W->EndsWith(SuffixesStep1[i]) && SuffixInRn(W, R2, SuffixesStep1[i])) {
        W->End-=U8(strlen(SuffixesStep1[i]));
        if (i==3 /*able*/)
          W->Type|=French::Adjective;
        return true;
      }
    }
    for (; i<17; i++) {
      if (W->EndsWith(SuffixesStep1[i]) && SuffixInRn(W, R2, SuffixesStep1[i])) {
        W->End-=U8(strlen(SuffixesStep1[i]));
        if (W->EndsWith("ic"))
          W->ChangeSuffix("c", "qU");
        return true;
      }
    }
    for (; i<25;i++) {
      if (W->EndsWith(SuffixesStep1[i]) && SuffixInRn(W, R2, SuffixesStep1[i])) {
        W->End-=U8(strlen(SuffixesStep1[i]))-1-(i<19)*2;
        if (i>22) {
          W->End+=2;
          W->Letters[W->End]='t';
        }
        return true;
      }
    }
    for (; i<27; i++) {
      if (W->EndsWith(SuffixesStep1[i]) && SuffixInRn(W, R1, SuffixesStep1[i]) && IsConsonant((*W)((U8)strlen(SuffixesStep1[i])))) {
        W->End-=U8(strlen(SuffixesStep1[i]));
        return true;
      }
    }
    for (; i<29; i++) {
      if (W->EndsWith(SuffixesStep1[i]) && SuffixInRn(W, RV, SuffixesStep1[i])) {
        W->End-=U8(strlen(SuffixesStep1[i]));
        if (W->EndsWith("iv") && SuffixInRn(W, R2, "iv")) {
          W->End-=2;
          if (W->EndsWith("at") && SuffixInRn(W, R2, "at"))
            W->End-=2;
        }
        else if (W->EndsWith("eus")) {
          if (SuffixInRn(W, R2, "eus"))
            W->End-=3;
          else if (SuffixInRn(W, R1, "eus"))
            W->Letters[W->End]='x';
        }
        else if ((W->EndsWith("abl") && SuffixInRn(W, R2, "abl")) || (W->EndsWith("iqU") && SuffixInRn(W, R2, "iqU")))
          W->End-=3;
        else if ((W->EndsWith("i\xE8r") && SuffixInRn(W, RV, "i\xE8r")) || (W->EndsWith("I\xE8r") && SuffixInRn(W, RV, "I\xE8r"))) {
          W->End-=2;
          W->Letters[W->End]='i';
        }
        return true;
      }
    }
    for (; i<31; i++) {
      if (W->EndsWith(SuffixesStep1[i]) && SuffixInRn(W, R2, SuffixesStep1[i])) {
        W->End-=U8(strlen(SuffixesStep1[i]));
        if (W->EndsWith("abil")) {
          if (SuffixInRn(W, R2, "abil"))
            W->End-=4;
          else
            W->End--, W->Letters[W->End]='l';
        }
        else if (W->EndsWith("ic")) {
          if (SuffixInRn(W, R2, "ic"))
            W->End-=2;
          else
            W->ChangeSuffix("c", "qU");
        }
        else if (W->EndsWith("iv") && SuffixInRn(W, R2, "iv"))
          W->End-=2;
        return true;
      }
    }
    for (; i<35; i++) {
      if (W->EndsWith(SuffixesStep1[i]) && SuffixInRn(W, R2, SuffixesStep1[i])) {
        W->End-=U8(strlen(SuffixesStep1[i]));
        if (W->EndsWith("at") && SuffixInRn(W, R2, "at")) {
          W->End-=2;
          if (W->EndsWith("ic")) {
            if (SuffixInRn(W, R2, "ic"))
              W->End-=2;
            else
              W->ChangeSuffix("c", "qU");
          }
        }
        return true;
      }
    }
    for (; i<37; i++) {
      if (W->EndsWith(SuffixesStep1[i])) {
        if (SuffixInRn(W, R2, SuffixesStep1[i])) {
          W->End-=U8(strlen(SuffixesStep1[i]));
          return true;
        }
        else if (SuffixInRn(W, R1, SuffixesStep1[i])) {
          W->ChangeSuffix(SuffixesStep1[i], "eux");
          return true;
        }
      }
    }
    for (; i<NUM_SUFFIXES_STEP1; i++) {
      if (W->EndsWith(SuffixesStep1[i]) && SuffixInRn(W, RV+1, SuffixesStep1[i]) && IsVowel((*W)((U8)strlen(SuffixesStep1[i])))) {
        W->End-=U8(strlen(SuffixesStep1[i]));
        (*ForceStep2a) = true;
        return true;
      }
    }
    if (W->EndsWith("eaux") || (*W)=="eaux") {
      W->End--;
      W->Type|=French::Plural;
      return true;
    }
    else if (W->EndsWith("aux") && SuffixInRn(W, R1, "aux")) {
      W->End--, W->Letters[W->End] = 'l';
      W->Type|=French::Plural;
      return true;
    }
    else if (W->EndsWith("amment") && SuffixInRn(W, RV, "amment")) {
      W->ChangeSuffix("amment", "ant");
      (*ForceStep2a) = true;
      return true;
    }
    else if (W->EndsWith("emment") && SuffixInRn(W, RV, "emment")) {
      W->ChangeSuffix("emment", "ent");
      (*ForceStep2a) = true;
      return true;
    }
    return false;
  }
  bool Step2a(Word *W, const U32 RV) {
    for (int i=0; i<NUM_SUFFIXES_STEP2a; i++) {
      if (W->EndsWith(SuffixesStep2a[i]) && SuffixInRn(W, RV+1, SuffixesStep2a[i]) && IsConsonant((*W)((U8)strlen(SuffixesStep2a[i])))) {
        W->End-=U8(strlen(SuffixesStep2a[i]));
        if (i==31 /*ir*/)
          W->Type|=French::Verb;
        return true;
      }
    }
    return false;
  }
  bool Step2b(Word *W, const U32 RV, const U32 R2) {
    for (int i=0; i<NUM_SUFFIXES_STEP2b; i++) {
      if (W->EndsWith(SuffixesStep2b[i]) && SuffixInRn(W, RV, SuffixesStep2b[i])) {
        switch (SuffixesStep2b[i][0]) {
          case 'a': case '\xE2': {
            W->End-=U8(strlen(SuffixesStep2b[i]));
            if (W->EndsWith("e") && SuffixInRn(W, RV, "e"))
              W->End--;
            return true;
          }
          default: {
            if (i!=14 || SuffixInRn(W, R2, SuffixesStep2b[i])) {
              W->End-=U8(strlen(SuffixesStep2b[i]));
              return true;
            }
          }
        }
      }
    }
    return false;
  }
  void Step3(Word *W) {
    char *final = (char *)&W->Letters[W->End];
    if ((*final)=='Y')
      (*final) = 'i';
    else if ((*final)=='\xE7')
      (*final) = 'c';
  }
  bool Step4(Word *W, const U32 RV, const U32 R2) {
    bool res = false;
    if (W->Length()>=2 && W->Letters[W->End]=='s' && !CharInArray((*W)(1), SetStep4, NUM_SET_STEP4)) {
      W->End--;
      res = true;
    }
    for (int i=0; i<NUM_SUFFIXES_STEP4; i++) {
      if (W->EndsWith(SuffixesStep4[i]) && SuffixInRn(W, RV, SuffixesStep4[i])) {
        switch (i) {
          case 2: { //ion
            char prec = (*W)(3);
            if (SuffixInRn(W, R2, SuffixesStep4[i]) && SuffixInRn(W, RV+1, SuffixesStep4[i]) && (prec=='s' || prec=='t')) {
              W->End-=3;
              return true;
            }
            break;
          }
          case 5: { //e
            W->End--;
            return true;
          }
          case 6: { //\xEB
            if (W->EndsWith("gu\xEB")) {
              W->End--;
              return true;
            }
            break;
          }
          default: {
            W->ChangeSuffix(SuffixesStep4[i], "i");
            return true;
          }
        }
      }
    }
    return res;
  }
  bool Step5(Word *W) {
    for (int i=0; i<NUM_SUFFIXES_STEP5; i++) {
      if (W->EndsWith(SuffixesStep5[i])) {
        W->End--;
        return true;
      }
    }
    return false;
  }
  bool Step6(Word *W) {
    for (int i=W->End; i>=W->Start; i--) {
      if (IsVowel(W->Letters[i])) {
        if (i<W->End && (W->Letters[i]&0xFE)==0xE8) {
          W->Letters[i] = 'e';
          return true;
        }
        return false;
      }
    }
    return false;
  }
public:
  inline bool IsVowel(const char c) final {
    return CharInArray(c, Vowels, NUM_VOWELS);
  }
  inline void Hash(Word *W) final {
    W->Hash[2] = W->Hash[3] = ~0xeff1cace;
    for (int i=W->Start; i<=W->End; i++) {
      U8 l = W->Letters[i];
      W->Hash[2]=W->Hash[2]*251*32 + l;
      if (IsVowel(l))
        W->Hash[3]=W->Hash[3]*997*16 + l;
      else if (l>='b' && l<='z')
        W->Hash[3]=W->Hash[3]*271*32 + (l-97);
      else
        W->Hash[3]=W->Hash[3]*11*32 + l;
    }
  }
  bool Stem(Word *W) {
    ConvertUTF8(W);
    if (W->Length()<2) {
      Hash(W);
      return false;
    }
    for (int i=0; i<NUM_EXCEPTIONS; i++) {
      if ((*W)==Exceptions[i][0]) {
        size_t len=strlen(Exceptions[i][1]);
        memcpy(&W->Letters[W->Start], Exceptions[i][1], len);
        W->End=W->Start+U8(len-1);
        Hash(W);
        W->Type|=TypesExceptions[i];
        W->Language = Language::French;
        return true;
      }
    }
    MarkVowelsAsConsonants(W);
    U32 RV=GetRV(W), R1=GetRegion(W, 0), R2=GetRegion(W, R1);
    bool DoNextStep=false, res=Step1(W, RV, R1, R2, &DoNextStep);
    DoNextStep|=!res;
    if (DoNextStep) {
      DoNextStep = !Step2a(W, RV);
      res|=!DoNextStep;
      if (DoNextStep)
        res|=Step2b(W, RV, R2);
    }
    if (res)
      Step3(W);
    else
      res|=Step4(W, RV, R2);
    res|=Step5(W);
    res|=Step6(W);
    for (int i=W->Start; i<=W->End; i++)
      W->Letters[i] = tolower(W->Letters[i]);
    if (!res)
      res=W->MatchesAny(CommonWords, NUM_COMMON_WORDS);
    Hash(W);
    if (res)
      W->Language = Language::French;
    return res;
  }
};

/*
  German suffix stemmer, based on the Porter stemmer.

  Changelog:
  (27/02/2018) v140: Initial release by Márcio Pais
*/

class GermanStemmer : public Stemmer {
private:
  static const int NUM_VOWELS = 9;
  const char Vowels[NUM_VOWELS]={'a','e','i','o','u','y','\xE4','\xF6','\xFC'};
  static const int NUM_COMMON_WORDS = 10;
  const char *CommonWords[NUM_COMMON_WORDS]={"der","die","das","und","sie","ich","mit","sich","auf","nicht"};
  static const int NUM_ENDINGS = 10;
  const char Endings[NUM_ENDINGS]={'b','d','f','g','h','k','l','m','n','t'}; //plus 'r' for words ending in 's'
  static const int NUM_SUFFIXES_STEP1 = 6;
  const char *SuffixesStep1[NUM_SUFFIXES_STEP1]={"em","ern","er","e","en","es"};
  static const int NUM_SUFFIXES_STEP2 = 3;
  const char *SuffixesStep2[NUM_SUFFIXES_STEP2]={"en","er","est"};
  static const int NUM_SUFFIXES_STEP3 = 7;
  const char *SuffixesStep3[NUM_SUFFIXES_STEP3]={"end","ung","ik","ig","isch","lich","heit"};
  void ConvertUTF8(Word *W) {
    for (int i=W->Start; i<W->End; i++) {
      U8 c = W->Letters[i+1]+((W->Letters[i+1]<0x9F)?0x60:0x40);
      if (W->Letters[i]==0xC3 && (IsVowel(c) || c==0xDF)) {
        W->Letters[i] = c;
        if (i+1<W->End)
          memmove(&W->Letters[i+1], &W->Letters[i+2], W->End-i-1);
        W->End--;
      }
    }
  }
  void ReplaceSharpS(Word *W) {
    for (int i=W->Start; i<=W->End; i++) {
      if (W->Letters[i]==0xDF) {
        W->Letters[i]='s';
        if (i+1<MAX_WORD_SIZE) {
          memmove(&W->Letters[i+2], &W->Letters[i+1], MAX_WORD_SIZE-i-2);
          W->Letters[i+1]='s';
          W->End+=(W->End<MAX_WORD_SIZE-1);
        }
      }
    }
  }
  void MarkVowelsAsConsonants(Word *W) {
    for (int i=W->Start+1; i<W->End; i++) {
      U8 c = W->Letters[i];
      if ((c=='u' || c=='y') && IsVowel(W->Letters[i-1]) && IsVowel(W->Letters[i+1]))
        W->Letters[i] = toupper(c);
    }
  }
  inline bool IsValidEnding(const char c, const bool IncludeR = false) {
    return CharInArray(c, Endings, NUM_ENDINGS) || (IncludeR && c=='r');
  }
  bool Step1(Word *W, const U32 R1) {
    int i = 0;
    for (; i<3; i++) {
      if (W->EndsWith(SuffixesStep1[i]) && SuffixInRn(W, R1, SuffixesStep1[i])) {
        W->End-=U8(strlen(SuffixesStep1[i]));
        return true;
      }
    }
    for (; i<NUM_SUFFIXES_STEP1; i++) {
      if (W->EndsWith(SuffixesStep1[i]) && SuffixInRn(W, R1, SuffixesStep1[i])) {
        W->End-=U8(strlen(SuffixesStep1[i]));
        W->End-=U8(W->EndsWith("niss"));
        return true;
      }
    }
    if (W->EndsWith("s") && SuffixInRn(W, R1, "s") && IsValidEnding((*W)(1), true)) {
      W->End--;
      return true;
    }
    return false;
  }
  bool Step2(Word *W, const U32 R1) {
    for (int i=0; i<NUM_SUFFIXES_STEP2; i++) {
      if (W->EndsWith(SuffixesStep2[i]) && SuffixInRn(W, R1, SuffixesStep2[i])) {
        W->End-=U8(strlen(SuffixesStep2[i]));
        return true;
      }
    }
    if (W->EndsWith("st") && SuffixInRn(W, R1, "st") && W->Length()>5 && IsValidEnding((*W)(2))) {
      W->End-=2;
      return true;
    }
    return false;
  }
  bool Step3(Word *W, const U32 R1, const U32 R2) {
    int i = 0;
    for (; i<2; i++) {
      if (W->EndsWith(SuffixesStep3[i]) && SuffixInRn(W, R2, SuffixesStep3[i])) {
        W->End-=U8(strlen(SuffixesStep3[i]));
        if (W->EndsWith("ig") && (*W)(2)!='e' && SuffixInRn(W, R2, "ig"))
          W->End-=2;
        if (i)
          W->Type|=German::Noun;
        return true;
      }
    }
    for (; i<5; i++) {
      if (W->EndsWith(SuffixesStep3[i]) && SuffixInRn(W, R2, SuffixesStep3[i]) && (*W)((U8)strlen(SuffixesStep3[i]))!='e') {
        W->End-=U8(strlen(SuffixesStep3[i]));
        if (i>2)
          W->Type|=German::Adjective;
        return true;
      }
    }
    for (; i<NUM_SUFFIXES_STEP3; i++) {
      if (W->EndsWith(SuffixesStep3[i]) && SuffixInRn(W, R2, SuffixesStep3[i])) {
        W->End-=U8(strlen(SuffixesStep3[i]));
        if ((W->EndsWith("er") || W->EndsWith("en")) && SuffixInRn(W, R1, "e?"))
          W->End-=2;
        if (i>5)
          W->Type|=German::Noun|German::Female;
        return true;
      }
    }
    if (W->EndsWith("keit") && SuffixInRn(W, R2, "keit")) {
      W->End-=4;
      if (W->EndsWith("lich") && SuffixInRn(W, R2, "lich"))
        W->End-=4;
      else if (W->EndsWith("ig") && SuffixInRn(W, R2, "ig"))
        W->End-=2;
      W->Type|=German::Noun|German::Female;
      return true;
    }
    return false;
  }
public:
  inline bool IsVowel(const char c) final {
    return CharInArray(c, Vowels, NUM_VOWELS);
  }
  inline void Hash(Word *W) final {
    W->Hash[2] = W->Hash[3] = ~0xbea7ab1e;
    for (int i=W->Start; i<=W->End; i++) {
      U8 l = W->Letters[i];
      W->Hash[2]=W->Hash[2]*263*32 + l;
      if (IsVowel(l))
        W->Hash[3]=W->Hash[3]*997*16 + l;
      else if (l>='b' && l<='z')
        W->Hash[3]=W->Hash[3]*251*32 + (l-97);
      else
        W->Hash[3]=W->Hash[3]*11*32 + l;
    }
  }
  bool Stem(Word *W) {
    ConvertUTF8(W);
    if (W->Length()<2) {
      Hash(W);
      return false;
    }
    ReplaceSharpS(W);
    MarkVowelsAsConsonants(W);
    U32 R1=GetRegion(W, 0), R2=GetRegion(W, R1);
    R1 = min(3, R1);
    bool res = Step1(W, R1);
    res|=Step2(W, R1);
    res|=Step3(W, R1, R2);
    for (int i=W->Start; i<=W->End; i++) {
      switch (W->Letters[i]) {
        case 0xE4: { W->Letters[i] = 'a'; break; }
        case 0xF6: case 0xFC: { W->Letters[i]-=0x87; break; }
        default: W->Letters[i] = tolower(W->Letters[i]);
      }
    }
    if (!res)
      res=W->MatchesAny(CommonWords, NUM_COMMON_WORDS);
    Hash(W);
    if (res)
      W->Language = Language::German;
    return res;
  }
};

#endif //USE_TEXTMODEL

//////////////////////////// Models //////////////////////////////

// All of the models below take a Mixer as a parameter and write
// predictions to it.


//////////////////////////// TextModel ///////////////////////////

#ifdef USE_TEXTMODEL

template <class T, const U32 Size> class Cache {
  static_assert(Size>1 && (Size&(Size-1))==0, "Cache size must be a power of 2 bigger than 1");
private:
  Array<T> Data;
  U32 Index;
public:
  explicit Cache() : Data(Size) { Index=0; }
  T& operator()(U32 i) {
    return Data[(Index-i)&(Size-1)];
  }
  void operator++(int) {
    Index++;
  }
  void operator--(int) {
    Index--;
  }
  T& Next() {
    return Index++, *((T*)memset(&Data[Index&(Size-1)], 0, sizeof(T)));
  }
};

/*
  Text model

  Changelog:
  (04/02/2018) v135: Initial release by Márcio Pais
  (11/02/2018) v136: Uses 16 contexts, sets 3 mixer contexts
  (15/02/2018) v138: Uses 21 contexts, sets 4 mixer contexts
  (25/02/2018) v139: Uses 26 contexts
  (27/02/2018) v140: Sets 6 mixer contexts
  (12/05/2018) v142: Sets 7 mixer contexts
*/

class TextModel {
private:
  const U32 MIN_RECOGNIZED_WORDS = 4;
  ContextMap2 Map;
  Array<Stemmer*> Stemmers;
  Array<Language*> Languages;
  Cache<Word, 8> Words[Language::Count];
  Cache<Segment, 4> Segments;
  Cache<Sentence, 4> Sentences;
  Cache<Paragraph, 2> Paragraphs;
  Array<U32> WordPos;
  U32 BytePos[256];
  Word *cWord, *pWord; // current word, previous word
  Segment *cSegment; // current segment
  Sentence *cSentence; // current sentence
  Paragraph *cParagraph; // current paragraph
  enum Parse {
    Unknown,
    ReadingWord,
    PossibleHyphenation,
    WasAbbreviation,
    AfterComma,
    AfterQuote,
    AfterAbbreviation,
    ExpectDigit
  } State, pState;
  struct {
    U32 Count[Language::Count-1]; // number of recognized words of each language in the last 64 words seen
    U64 Mask[Language::Count-1];  // binary mask with the recognition status of the last 64 words for each language
    int Id;  // current language detected
    int pId; // detected language of the previous word
  } Lang;
  struct {
    U64 numbers[2];   // last 2 numbers seen
    U32 numHashes[2]; // hashes of the last 2 numbers seen
    U8  numLength[2]; // digit length of last 2 numbers seen
    U32 numMask;      // binary mask of the results of the arithmetic comparisons between the numbers seen
    U32 numDiff;      // log2 of the consecutive differences between the last 16 numbers seen, clipped to 2 bits per difference
    U32 lastUpper;    // distance to last uppercase letter
    U32 maskUpper;    // binary mask of uppercase letters seen (in the last 32 bytes)
    U32 lastLetter;   // distance to last letter
    U32 lastDigit;    // distance to last digit
    U32 lastPunct;    // distance to last punctuation character
    U32 lastNewLine;  // distance to last new line character
    U32 prevNewLine;  // distance to penultimate new line character
    U32 wordGap;      // distance between the last words
    U32 spaces;       // binary mask of whitespace characters seen (in the last 32 bytes)
    U32 spaceCount;   // count of whitespace characters seen (in the last 32 bytes)
    U32 commas;       // number of commas seen in this line (not this segment/sentence)
    U32 quoteLength;  // length (in words) of current quote
    U32 maskPunct;    // mask of relative position of last comma related to other punctuation
    U32 nestHash;     // hash representing current nesting state
    U32 lastNest;     // distance to last nesting character
    U32 masks[4],
        wordLength[2];
    int UTF8Remaining;// remaining bytes for current UTF8-encoded Unicode code point (-1 if invalid byte found)
    U8 firstLetter;   // first letter of current word
    U8 firstChar;     // first character of current line
    U8 expectedDigit; // next expected digit of detected numerical sequence
    U8 prevPunct;     // most recent punctuation character seen
    Word TopicDescriptor; // last word before ':'
  } Info;
  U32 ParseCtx;
  void Update(Buf& buffer, ModelStats *Stats = nullptr);
  void SetContexts(Buf& buffer, ModelStats *Stats = nullptr);
public:
  TextModel(const U32 Size) : Map(Size, 26), Stemmers(Language::Count-1), Languages(Language::Count-1), WordPos(0x10000), State(Parse::Unknown), pState(State), Lang{ {0}, {0}, Language::Unknown, Language::Unknown }, Info{}, ParseCtx(0) {
    Stemmers[Language::English-1] = new EnglishStemmer();
    Stemmers[Language::French-1] = new FrenchStemmer();
    Stemmers[Language::German-1] = new GermanStemmer();
    Languages[Language::English-1] = new English();
    Languages[Language::French-1] = new French();
    Languages[Language::German-1] = new German();
    cWord = &Words[Lang.Id](0);
    pWord = &Words[Lang.Id](1);
    cSegment = &Segments(0);
    cSentence = &Sentences(0);
    cParagraph = &Paragraphs(0);
    memset(&BytePos[0], 0, sizeof(BytePos));
  }
  ~TextModel() {
    for (int i=0; i<Language::Count-1; i++) {
      delete Stemmers[i];
      delete Languages[i];
    }
  }
  void Predict(Mixer& mixer, Buf& buffer, ModelStats *Stats = nullptr) {
    if (bpos==0) {
      Update(buffer, Stats);
      SetContexts(buffer, Stats);
    }
    Map.mix(mixer);
    mixer.set(hash((Lang.Id!=Language::Unknown)?1+Stemmers[Lang.Id-1]->IsVowel(buffer(1)):0, Info.masks[1]&0xFF, c0)&0x7FF, 2048);
    mixer.set(hash(ilog2(Info.wordLength[0]+1), c0,
      (Info.lastDigit<Info.wordLength[0]+Info.wordGap)|
      ((Info.lastUpper<Info.lastLetter+Info.wordLength[1])<<1)|
      ((Info.lastPunct<Info.wordLength[0]+Info.wordGap)<<2)|
      ((Info.lastUpper<Info.wordLength[0])<<3)
    )&0x7FF, 2048);
    mixer.set(hash(Info.masks[1]&0x3FF, Info.lastUpper<Info.wordLength[0], Info.lastUpper<Info.lastLetter+Info.wordLength[1])&0x7FF, 2048);
    mixer.set(hash(Info.spaces&0x1FF,
      (Info.lastUpper<Info.wordLength[0])|
      ((Info.lastUpper<Info.lastLetter+Info.wordLength[1])<<1)|
      ((Info.lastPunct<Info.lastLetter)<<2)|
      ((Info.lastPunct<Info.wordLength[0]+Info.wordGap)<<3)|
      ((Info.lastPunct<Info.lastLetter+Info.wordLength[1]+Info.wordGap)<<4)
    )&0x7FF, 2048);
    mixer.set(hash(Info.firstLetter*(Info.wordLength[0]<4), min(6, Info.wordLength[0]), c0)&0x7FF, 2048);
    mixer.set(hash((*pWord)[0], (*pWord)(0), min(4, Info.wordLength[0]), Info.lastPunct<Info.lastLetter)&0x7FF, 2048);
    mixer.set(hash(min(4, Info.wordLength[0]), c0,
      Info.lastUpper<Info.wordLength[0],
      (Info.nestHash>0)?Info.nestHash&0xFF:0x100|(Info.firstLetter*(Info.wordLength[0]>0 && Info.wordLength[0]<4))
    )&0xFFF, 4096);
  }
};

void TextModel::Update(Buf& buffer, ModelStats *Stats) {
  Info.lastUpper  = min(0xFF, Info.lastUpper+1), Info.maskUpper<<=1;
  Info.lastLetter = min(0x1F, Info.lastLetter+1);
  Info.lastDigit  = min(0xFF, Info.lastDigit+1);
  Info.lastPunct  = min(0x3F, Info.lastPunct+1);
  Info.lastNewLine++; Info.prevNewLine++; Info.lastNest++;
  Info.spaceCount-=(Info.spaces>>31); Info.spaces<<=1;
  Info.masks[0]<<=2; Info.masks[1]<<=2; Info.masks[2]<<=4; Info.masks[3]<<=3;
  pState = State;

  U8 c = buffer(1), pC=tolower(c);
  BytePos[c] = pos;
  if (c!=pC) {
    c = pC;
    Info.lastUpper = 0, Info.maskUpper|=1;
  }
  pC = buffer(2);
  ParseCtx = hash(State=Parse::Unknown, pWord->Hash[1], c, (ilog2(Info.lastNewLine)+1)*(Info.lastNewLine*3>Info.prevNewLine), Info.masks[1]&0xFC);

  if ((c>='a' && c<='z') || c=='\'' || c=='-' || c>0x7F) {
    if (Info.wordLength[0]==0) {
      // check for hyphenation with "+"
      if (pC==NEW_LINE && ((Info.lastLetter==3 && buffer(3)=='+') || (Info.lastLetter==4 && buffer(3)==CARRIAGE_RETURN && buffer(4)=='+'))) {
        Info.wordLength[0] = Info.wordLength[1];
        for (int i=Language::Unknown; i<Language::Count; i++)
          Words[i]--;
        cWord = pWord, pWord = &Words[Lang.pId](1);
        memset(cWord, 0, sizeof(Word));
        for (U32 i=0; i<Info.wordLength[0]; i++)
          (*cWord)+=buffer(Info.wordLength[0]-i+Info.lastLetter);
        Info.wordLength[1] = (*pWord).Length();
        cSegment->WordCount--;
        cSentence->WordCount--;
      }
      else {
        Info.wordGap = Info.lastLetter;
        Info.firstLetter = c;
      }
    }
    Info.lastLetter = 0;
    Info.wordLength[0]++;
    Info.masks[0]+=(Lang.Id!=Language::Unknown)?1+Stemmers[Lang.Id-1]->IsVowel(c):1, Info.masks[1]++, Info.masks[3]+=Info.masks[0]&3;
    if (c=='\'') {
      Info.masks[2]+=12;
      if (Info.wordLength[0]==1) {
        if (Info.quoteLength==0 && pC==SPACE)
          Info.quoteLength = 1;
        else if (Info.quoteLength>0 && Info.lastPunct==1) {
          Info.quoteLength = 0;
          ParseCtx = hash(State=Parse::AfterQuote, pC);
        }
      }
    }
    (*cWord)+=c;
    cWord->GetHashes();
    ParseCtx = hash(State=Parse::ReadingWord, cWord->Hash[1]);
  }
  else {
    if (cWord->Length()>0) {
      if (Lang.Id!=Language::Unknown)
        memcpy(&Words[Language::Unknown](0), cWord, sizeof(Word));

      for (int i=Language::Count-1; i>Language::Unknown; i--) {
        Lang.Count[i-1]-=(Lang.Mask[i-1]>>63), Lang.Mask[i-1]<<=1;
        if (i!=Lang.Id)
          memcpy(&Words[i](0), cWord, sizeof(Word));
        if (Stemmers[i-1]->Stem(&Words[i](0)))
          Lang.Count[i-1]++, Lang.Mask[i-1]|=1;
      }
      Lang.Id = Language::Unknown;
      U32 best = MIN_RECOGNIZED_WORDS;
      for (int i=Language::Count-1; i>Language::Unknown; i--) {
        if (Lang.Count[i-1]>=best) {
          best = Lang.Count[i-1] + (i==Lang.pId); //bias to prefer the previously detected language
          Lang.Id = i;
        }
        Words[i]++;
      }
      Words[Language::Unknown]++;
      #ifndef NVERBOSE
      if (Lang.Id!=Lang.pId) {
        if (to_screen) printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
        switch (Lang.Id) {
          case Language::Unknown: { printf("[Language: Unknown, blpos: %d]\n",blpos); break; };
          case Language::English: { printf("[Language: English, blpos: %d]\n",blpos); break; };
          case Language::French : { printf("[Language: French,  blpos: %d]\n",blpos); break; };
          case Language::German : { printf("[Language: German,  blpos: %d]\n",blpos); break; };
        }
      }
      #endif
      Lang.pId = Lang.Id;
      pWord = &Words[Lang.Id](1), cWord = &Words[Lang.Id](0);
      memset(cWord, 0, sizeof(Word));
      WordPos[pWord->Hash[1]&(WordPos.size()-1)] = pos;
      if (cSegment->WordCount==0)
        memcpy(&cSegment->FirstWord, pWord, sizeof(Word));
      cSegment->WordCount++;
      if (cSentence->WordCount==0)
        memcpy(&cSentence->FirstWord, pWord, sizeof(Word));
      cSentence->WordCount++;
      Info.wordLength[1] = Info.wordLength[0], Info.wordLength[0] = 0;
      Info.quoteLength+=(Info.quoteLength>0);
      if (Info.quoteLength>0x1F)
        Info.quoteLength = 0;
      cSentence->VerbIndex++, cSentence->NounIndex++, cSentence->CapitalIndex++;
      if ((pWord->Type&Language::Verb)!=0) {
        cSentence->VerbIndex = 0;
        memcpy(&cSentence->lastVerb, pWord, sizeof(Word));
      }
      if ((pWord->Type&Language::Noun)!=0) {
        cSentence->NounIndex = 0;
        memcpy(&cSentence->lastNoun, pWord, sizeof(Word));
      }
      if (cSentence->WordCount>1 && Info.lastUpper<Info.wordLength[1]) {
        cSentence->CapitalIndex = 0;
        memcpy(&cSentence->lastCapital, pWord, sizeof(Word));
      }
    }
    bool skip = false;
    switch (c) {
      case '.': {
        if (Lang.Id!=Language::Unknown && Info.lastUpper==Info.wordLength[1] && Languages[Lang.Id-1]->IsAbbreviation(pWord)) {
          ParseCtx = hash(State=Parse::WasAbbreviation, pWord->Hash[1]);
          break;
        }
      }
      case '?': case '!': {
        cSentence->Type = (c=='.')?Sentence::Types::Declarative:(c=='?')?Sentence::Types::Interrogative:Sentence::Types::Exclamative;
        cSentence->SegmentCount++;
        cParagraph->SentenceCount++;
        cParagraph->TypeCount[cSentence->Type]++;
        cParagraph->TypeMask<<=2, cParagraph->TypeMask|=cSentence->Type;
        cSentence = &Sentences.Next();
        Info.masks[3]+=3;
        skip = true;
      }
      case ',': case ';': case ':': {
        if (c==',') {
          Info.commas++;
          ParseCtx = hash(State=Parse::AfterComma, ilog2(Info.quoteLength+1), ilog2(Info.lastNewLine), Info.lastUpper<Info.lastLetter+Info.wordLength[1]);
        }
        else if (c==':')
          memcpy(&Info.TopicDescriptor, pWord, sizeof(Word));
        if (!skip) {
          cSentence->SegmentCount++;
          Info.masks[3]+=4;
        }
        Info.lastPunct = 0, Info.prevPunct = c;
        Info.masks[0]+=3, Info.masks[1]+=2, Info.masks[2]+=15;
        cSegment = &Segments.Next();
        break;
      }
      case NEW_LINE: {
        Info.prevNewLine = Info.lastNewLine, Info.lastNewLine = 0;
        Info.commas = 0;
        if (Info.prevNewLine==1 || (Info.prevNewLine==2 && pC==CARRIAGE_RETURN))
          cParagraph = &Paragraphs.Next();
        else if ((Info.lastLetter==2 && pC=='+') || (Info.lastLetter==3 && pC==CARRIAGE_RETURN && buffer(3)=='+'))
          ParseCtx = hash(Parse::ReadingWord, pWord->Hash[1]), State=Parse::PossibleHyphenation;
      }
      case TAB: case CARRIAGE_RETURN: case SPACE: {
        Info.spaceCount++, Info.spaces|=1;
        Info.masks[1]+=3, Info.masks[3]+=5;
        if (c==SPACE && pState==Parse::WasAbbreviation) {
          ParseCtx = hash(State=Parse::AfterAbbreviation, pWord->Hash[1]);
        }
        break;
      }
      case '(' : Info.masks[2]+=1; Info.masks[3]+=6; Info.nestHash+=31; Info.lastNest=0; break;
      case '[' : Info.masks[2]+=2; Info.nestHash+=11; Info.lastNest=0; break;
      case '{' : Info.masks[2]+=3; Info.nestHash+=17; Info.lastNest=0; break;
      case '<' : Info.masks[2]+=4; Info.nestHash+=23; Info.lastNest=0; break;
      case 0xAB: Info.masks[2]+=5; break;
      case ')' : Info.masks[2]+=6; Info.nestHash-=31; Info.lastNest=0; break;
      case ']' : Info.masks[2]+=7; Info.nestHash-=11; Info.lastNest=0; break;
      case '}' : Info.masks[2]+=8; Info.nestHash-=17; Info.lastNest=0; break;
      case '>' : Info.masks[2]+=9; Info.nestHash-=23; Info.lastNest=0; break;
      case 0xBB: Info.masks[2]+=10; break;
      case '"': {
        Info.masks[2]+=11;
        // start/stop counting
        if (Info.quoteLength==0)
          Info.quoteLength = 1;
        else {
          Info.quoteLength = 0;
          ParseCtx = hash(State=Parse::AfterQuote, 0x100|pC);
        }
        break;
      }
      case '/' : case '-': case '+': case '*': case '=': case '%': Info.masks[2]+=13; break;
      case '\\': case '|': case '_': case '@': case '&': case '^': Info.masks[2]+=14; break;
    }
    if (c>='0' && c<='9') {
      Info.numbers[0] = Info.numbers[0]*10 + (c&0xF), Info.numLength[0] = min(19, Info.numLength[0]+1);
      Info.numHashes[0] = hash(Info.numHashes[0], c, Info.numLength[0]);
      Info.expectedDigit = -1;
      if (Info.numLength[0]<Info.numLength[1] && (pState==Parse::ExpectDigit || ((Info.numDiff&3)==0 && Info.numLength[0]<=1))) {
        U64 ExpectedNum = Info.numbers[1]+(Info.numMask&3)-2, PlaceDivisor=1;
        for (int i=0; i<Info.numLength[1]-Info.numLength[0]; i++, PlaceDivisor*=10);
        if (ExpectedNum/PlaceDivisor==Info.numbers[0]) {
          PlaceDivisor/=10;
          Info.expectedDigit = (ExpectedNum/PlaceDivisor)%10;
          State = Parse::ExpectDigit;
        }
      }
      else {
        U8 d = buffer(Info.numLength[0]+2);
        if (Info.numLength[0]<3 && buffer(Info.numLength[0]+1)==',' && d>='0' && d<='9')
          State = Parse::ExpectDigit;
      }
      Info.lastDigit = 0;
      Info.masks[3]+=7;
    }
    else if (Info.numbers[0]>0) {
      Info.numMask<<=2, Info.numMask|=1+(Info.numbers[0]>=Info.numbers[1])+(Info.numbers[0]>Info.numbers[1]);
      Info.numDiff<<=2, Info.numDiff|=min(3,ilog2(abs((int)(Info.numbers[0]-Info.numbers[1]))));
      Info.numbers[1] = Info.numbers[0], Info.numbers[0] = 0;
      Info.numHashes[1] = Info.numHashes[0], Info.numHashes[0] = 0;
      Info.numLength[1] = Info.numLength[0], Info.numLength[0] = 0;
      cSegment->NumCount++, cSentence->NumCount++;
    }
  }
  if (Info.lastNewLine==1)
    Info.firstChar = (Lang.Id!=Language::Unknown)?c:min(c,96);
  if (Info.lastNest>512)
    Info.nestHash = 0;
  int leadingBitsSet = 0;
  while (((c>>(7-leadingBitsSet))&1)!=0) leadingBitsSet++;

  if (Info.UTF8Remaining>0 && leadingBitsSet==1)
    Info.UTF8Remaining--;
  else
    Info.UTF8Remaining = (leadingBitsSet!=1)?(c!=0xC0 && c!=0xC1 && c<0xF5)?(leadingBitsSet-(leadingBitsSet>0)):-1:0;
  Info.maskPunct = (BytePos[',']>BytePos['.'])|((BytePos[',']>BytePos['!'])<<1)|((BytePos[',']>BytePos['?'])<<2)|((BytePos[',']>BytePos[':'])<<3)|((BytePos[',']>BytePos[';'])<<4);
}

void TextModel::SetContexts(Buf& buffer, ModelStats *Stats) {
  U8 c = buffer(1), lc = tolower(c), m2 = Info.masks[2]&0xF, column = min(0xFF, Info.lastNewLine);;
  U16 w = ((State==Parse::ReadingWord)?cWord->Hash[1]:pWord->Hash[1])&0xFFFF;
  U32 h = ((State==Parse::ReadingWord)?cWord->Hash[1]:pWord->Hash[2])*271+c;
  int i = State<<6;

  Map.set(ParseCtx);
  Map.set(hash(i++, cWord->Hash[0], pWord->Hash[0],
    (Info.lastUpper<Info.wordLength[0])|
    ((Info.lastDigit<Info.wordLength[0]+Info.wordGap)<<1)
  ));
  Map.set(hash(i++, cWord->Hash[1], Words[Lang.pId](2).Hash[1], min(10,ilog2((U32)Info.numbers[0])),
    (Info.lastUpper<Info.lastLetter+Info.wordLength[1])|
    ((Info.lastLetter>3)<<1)|
    ((Info.lastLetter>0 && Info.wordLength[1]<3)<<2)
  ));
  Map.set(hash(i++, cWord->Hash[1]&0xFFF, Info.masks[1]&0x3FF, Words[Lang.pId](3).Hash[2],
    (Info.lastDigit<Info.wordLength[0]+Info.wordGap)|
    ((Info.lastUpper<Info.lastLetter+Info.wordLength[1])<<1)|
    ((Info.spaces&0x7F)<<2)
  ));
  Map.set(hash(i++, cWord->Hash[1], pWord->Hash[3], Words[Lang.pId](2).Hash[3]));
  Map.set(hash(i++, h&0x7FFF, Words[Lang.pId](2).Hash[1]&0xFFF, Words[Lang.pId](3).Hash[1]&0xFFF));
  Map.set(hash(i++, cWord->Hash[1], c, (cSentence->VerbIndex<cSentence->WordCount)?cSentence->lastVerb.Hash[1]:0));
  Map.set(hash(i++, pWord->Hash[2], Info.masks[1]&0xFC, lc, Info.wordGap));
  Map.set(hash(i++, (Info.lastLetter==0)?cWord->Hash[1]:pWord->Hash[1], c, cSegment->FirstWord.Hash[2], min(3,ilog2(cSegment->WordCount+1))));
  Map.set(hash(i++, cWord->Hash[1], c, Segments(1).FirstWord.Hash[3]));
  Map.set(hash(i++, max(31,lc), Info.masks[1]&0xFFC, (Info.spaces&0xFE)|(Info.lastPunct<Info.lastLetter), (Info.maskUpper&0xFF)|(((0x100|Info.firstLetter)*(Info.wordLength[0]>1))<<8)));
  Map.set(hash(i++, column, min(7,ilog2(Info.lastUpper+1)), ilog2(Info.lastPunct+1)));
  Map.set(
    (column&0xF8)|(Info.masks[1]&3)|((Info.prevNewLine-Info.lastNewLine>63)<<2)|
    (min(3, Info.lastLetter)<<8)|
    (Info.firstChar<<10)|
    ((Info.commas>4)<<18)|
    ((m2>=1 && m2<=5)<<19)|
    ((m2>=6 && m2<=10)<<20)|
    ((m2==11 || m2==12)<<21)|
    ((Info.lastUpper<column)<<22)|
    ((Info.lastDigit<column)<<23)|
    ((column<Info.prevNewLine-Info.lastNewLine)<<24)
  );
  Map.set(hash(
    (2*column)/3,
    min(13, Info.lastPunct)+(Info.lastPunct>16)+(Info.lastPunct>32)+Info.maskPunct*16,
    ilog2(Info.lastUpper+1),
    ilog2(Info.prevNewLine-Info.lastNewLine),
    ((Info.masks[1]&3)==0)|
    ((m2<6)<<1)|
    ((m2<11)<<2)
  ));
  Map.set(hash(i++, column>>1, Info.spaces&0xF));
  Map.set(hash(
    Info.masks[3]&0x3F,
    min((max(Info.wordLength[0],3)-2)*(Info.wordLength[0]<8),3),
    Info.firstLetter*(Info.wordLength[0]<5),
    w&0x3FF,
    (c==buffer(2))|
    ((Info.masks[2]>0)<<1)|
    ((Info.lastPunct<Info.wordLength[0]+Info.wordGap)<<2)|
    ((Info.lastUpper<Info.wordLength[0])<<3)|
    ((Info.lastDigit<Info.wordLength[0]+Info.wordGap)<<4)|
    ((Info.lastPunct<2+Info.wordLength[0]+Info.wordGap+Info.wordLength[1])<<5)
  ));
  Map.set(hash(i++, w, c, Info.numHashes[1]));
  Map.set(hash(i++, w, c, llog(pos-WordPos[w])>>1));
  Map.set(hash(i++, w, c, Info.TopicDescriptor.Hash[1]&0x7FFF));
  Map.set(hash(i++, Info.numLength[0], c, Info.TopicDescriptor.Hash[1]&0x7FFF));
  Map.set(hash(i++, (Info.lastLetter>0)?c:0x100, Info.masks[1]&0xFFC, Info.nestHash&0x7FF));
  Map.set(hash(i++, w*17+c, Info.masks[3]&0x1FF,
    ((cSentence->VerbIndex==0 && cSentence->lastVerb.Length()>0)<<6)|
    ((Info.wordLength[1]>3)<<5)|
    ((cSegment->WordCount==0)<<4)|
    ((cSentence->SegmentCount==0 && cSentence->WordCount<2)<<3)|
    ((Info.lastPunct>=Info.lastLetter+Info.wordLength[1]+Info.wordGap)<<2)|
    ((Info.lastUpper<Info.lastLetter+Info.wordLength[1])<<1)|
    (Info.lastUpper<Info.wordLength[0]+Info.wordGap+Info.wordLength[1])
  ));
  Map.set(hash(i++, c, pWord->Hash[2], Info.firstLetter*(Info.wordLength[0]<6),
    ((Info.lastPunct<Info.wordLength[0]+Info.wordGap)<<1)|
    (Info.lastPunct>=Info.lastLetter+Info.wordLength[1]+Info.wordGap)
  ));
  Map.set(hash(i++, w*23+c, Words[Lang.pId](1+(Info.wordLength[0]==0)).Letters[Words[Lang.pId](1+(Info.wordLength[0]==0)).Start], Info.firstLetter*(Info.wordLength[0]<7)));
  Map.set(hash(i++, column, Info.spaces&7, Info.nestHash&0x7FF));
  Map.set(hash(i++, cWord->Hash[1], (Info.lastUpper<column)|((Info.lastUpper<Info.wordLength[0])<<1), min(5, Info.wordLength[0])));
}

#endif //USE_TEXTMODEL

//////////////////////////// matchModel ///////////////////////////

// matchModel() finds the most recent matching context and returns its max length

class MatchModel {
private:
  enum Parameters : U32{
    MaxLen = 0xFFFF,    // longest allowed match
    MinLen = 2,         // minimum required match
    DeltaLen = 5,       // minimum length to switch to delta mode
    NumCtxs = 5,        // number of contexts used
    NumHashes = 2       // number of hashes used
  };
  Array<U32> Table;
  StateMap StateMaps[NumCtxs];
  SmallStationaryContextMap SCM[2];
  StationaryMap Map;
  U32 hashes[NumHashes];
  U32 ctx[NumCtxs];
  U32 length;    // length of match, or 0 if no match
  U32 index;     // points to next byte of match, if any
  U32 mask;
  bool delta;
  bool canBypass;
  void Update(Buf& buffer, ModelStats *Stats = nullptr) {
    delta = false;
    if (length==0 && Bypass)
      Bypass = false; // can only quit bypass mode on byte boundary
    // update hashes
    hashes[0] = (hashes[0]*(3<<3)+buffer(1))&mask;
    hashes[1] = (hashes[1]*(5<<5)+buffer(1))&mask;
    // extend current match, if available
    if (length) {
      index++;
      if (length<MaxLen)
        length++;
    }
    // or find a new match
    else {
      for (U32 i=0; i<NumHashes && length<MinLen; i++){
        index = Table[hashes[i]];
        if (index && index!=(U32)pos && (U64)pos<(index+buffer.size())) {
          while (length<MaxLen && (index-length-1)!=(U32)pos && buffer(length+1)==buffer[index-length-1])
            length++;
        }
      }
    }
    for (U32 i=0; i<NumHashes; i++)
      Table[hashes[i]] = pos;
    SCM[0].set(buffer[index]);
    SCM[1].set(pos);
    Map.set((buffer[index]<<8)|buffer(1));
  }
public:
  bool Bypass;
  U16 BypassPrediction; // prediction for bypass mode
  MatchModel(const U32 Size, const bool AllowBypass = false) :
    Table(Size/sizeof(U32)),
    StateMaps{ 56<<8, 0x80800, 0x10100, 0x10100, 0x10100 },
    SCM{ {8,8},{8,8} },
    Map{ 16 },
    hashes{ 0 },
    ctx{ 0 },
    length(0),
    mask((Size-1)/sizeof(U32)),
    delta(false),
    canBypass(AllowBypass),
    Bypass(false),
    BypassPrediction(2047)
  {
    assert((Size&(Size-1))==0);
  }
  int Predict(Mixer& mixer, Buf& buffer, ModelStats *Stats = nullptr) {
    if (bpos==0)
      Update(buffer, Stats);
    int bit = (buffer[index]>>(7-bpos))&1;
    if (canBypass && Bypass) {
      if (length>0 && ((buffer[index]+256)>>(8-bpos))!=c0)
        length = 0;
    }
    else {
      ctx[0] = ctx[1] = ctx[2] = ctx[3] = ctx[4] = c0;
      ctx[2]+= (buffer(1)+1)<<8;
      if (!delta) {
        if (length)
        {
          if (buffer(1)==buffer[index-1] && c0==((buffer[index] + 256)>>(8-bpos))) { // bits match?
            if (length<16)
              ctx[0] = length*2 + bit;
            else
              ctx[0] = (std::min<U32>(length, 63)>>2)*2 + bit + 24;
            ctx[0] = (ctx[0]<<8) + buffer(1);
            ctx[1] = ((buffer[index]+1)<<11)|(bpos<<8)|buffer(1);
            mixer.add((std::min<U32>(length, 32)<<5)*(2*bit-1));
            mixer.add((ilog(length)<<2)*(2*bit-1));
          }
          else { // we have a mismatch
            delta = length>DeltaLen;
            length = 0;
            mixer.add(0);
            mixer.add(0);
          }
        }
        else {
          mixer.add(0);
          mixer.add(0);
        }
      }
      else { //delta mode
        ctx[3] = ctx[2];
        ctx[4]+= (buffer[index]+1)<<8;
        mixer.add(0);
        mixer.add(0);
      }
      for (U32 i=0; i<NumCtxs; i++)
        mixer.add(stretch(StateMaps[i].p(ctx[i]))/2);
      SCM[0].mix(mixer, 7, 1, 4);
      SCM[1].mix(mixer, 5);
      Map.mix(mixer, 1, 4, 0xFF);
    }
    BypassPrediction = (length)?bit*0xFFF:0x7FF;
    return length;
  }
};

//////////////////////////// wordModel /////////////////////////

// Model English text (words and columns/end of line)

#ifdef USE_WORDMODEL

static U32 frstchar=0, spafdo=0, spaces=0, spacecount=0, words=0, wordcount=0,wordlen=0,wordlen1=0;
void wordModel(Mixer& m, ModelStats *Stats = nullptr) {
  static U32 word0=0, word1=0, word2=0, word3=0, word4=0, word5=0;  // hashes
  static U32 xword0=0,xword1=0,xword2=0,cword0=0,ccword=0;
  static U32 number0=0, number1=0;  // hashes
  static U32 text0=0;  // hash stream of letters
  static U32 wrdhsh=0, lastLetter=0, firstLetter=0, lastUpper=0, lastDigit=0, wordGap=0;
  static ContextMap cm(MEM*16, 47+1);
  static int nl1=-3, nl=-2, w=0;  // previous, current newline position
  static U32 mask=0, mask2=0;
  static Array<int> wpos(0x10000);  // last position of word
  static Array<Word> StemWords(4);
  static Word *cWord=&StemWords[0], *pWord=&StemWords[3];
  static EnglishStemmer StemmerEN;
  static int StemIndex=0;

  // Update word hashes
  if (bpos==0) {
    int c=c4&255,pC=(U8)(c4>>8),f=0;
    if (spaces&0x80000000) --spacecount;
    if (words&0x80000000) --wordcount;
    spaces=spaces*2;
    words=words*2;
    lastUpper=min(lastUpper+1,63);
    lastLetter=min(lastLetter+1,63);
    mask2<<=2;
    if (c>='A' && c<='Z') c+='a'-'A', lastUpper=0;
    if ((c>='a' && c<='z') || c=='\'' || c=='-' || c>127)
      (*cWord)+=c;
    else if ((*cWord).Length()>0){
      StemmerEN.Stem(cWord);
      cWord->GetHashes();
      StemIndex=(StemIndex+1)&3;
      pWord=cWord;
      cWord=&StemWords[StemIndex];
      memset(cWord, 0, sizeof(Word));
    }
    if ((c>='a' && c<='z') || c==1 || c==2 ||(c>=128 &&(b2!=3))) {
      if (!wordlen){
        // model hyphenation with "+"
        if ((lastLetter==3 && (c4&0xFFFF00)==0x2B0A00) || (lastLetter==4 && (c4&0xFFFFFF00)==0x2B0D0A00)){
          word0 = word1;
          wordlen = wordlen1;
          if (c<128){
            StemIndex=(StemIndex-1)&3;
            cWord=pWord;
            pWord=&StemWords[(StemIndex-1)&3];
            memset(cWord, 0, sizeof(Word));
            for (U32 i=0;i<=wordlen;i++)
              (*cWord)+=buf(wordlen-i+1+(lastLetter-1)*(i!=wordlen));
          }
        }
        else{
          wordGap = lastLetter;
          firstLetter = c;
          wrdhsh = 0;
        }
      }
      lastLetter=0;
      ++words, ++wordcount;
      word0^=hash(word0, c,0);
      text0=text0*997*16+c;
      wordlen++;
      wordlen=min(wordlen,45);
      f=0;
      w=word0&(wpos.size()-1);
      if ((c=='a' || c=='e' || c=='i' || c=='o' || c=='u') || (c=='y' && (wordlen>0 && pC!='a' && pC!='e' && pC!='i' && pC!='o' && pC!='u'))){
        mask2++;
        wrdhsh = wrdhsh*997*8+(c/4-22);
      }
      else if (c>='b' && c<='z'){
        mask2+=2;
        wrdhsh = wrdhsh*271*32+(c-97);
      }
      else
        wrdhsh = wrdhsh*11*32+c;
    }
    else {
      if (word0) {
        word5=word4;
        word4=word3;
        word3=word2;
        word2=word1;
        word1=word0;
        wordlen1=wordlen;
        wpos[w]=blpos;
        if (c==':'|| c=='=') cword0=word0;
        if (c==']'&& (frstchar==':' || frstchar=='*')) xword0=word0;
        ccword=0;
        word0=wordlen=0;
        if((c=='.'||c=='!'||c=='?' ||c=='}' ||c==')') && buf(2)!=10 && (Stats && Stats->blockType!=EXE)) f=1;
      }
      if ((c4&0xFFFF)==0x3D3D) xword1=word1,xword2=word2; // ==
      if ((c4&0xFFFF)==0x2727) xword1=word1,xword2=word2; // ''
      if (c==32 || c==10) { ++spaces, ++spacecount; if (c==10 ) nl1=nl, nl=pos-1;}
      else if (c=='.' || c=='!' || c=='?' || c==',' || c==';' || c==':') spafdo=0,ccword=c,mask2+=3;
      else { ++spafdo; spafdo=min(63,spafdo); }
    }
    lastDigit=min(0xFF,lastDigit+1);
    if (c>='0' && c<='9') {
        number0^=hash(number0, c,1);
        lastDigit = 0;
    }
    else if (number0) {
      number1=number0;
      number0=0,ccword=0;
    }

    U32 col=min(255, pos-nl);
    int above=buf[nl1+col];
    if (col<=2) frstchar=(col==2?min(c,96):0);
    if (frstchar=='[' && c==32)    {if(buf(3)==']' || buf(4)==']' ) frstchar=96,xword0=0;}
    cm.set(hash(513,spafdo, spaces,ccword));
    cm.set(hash(514,frstchar, c));
    cm.set(hash(515,col, frstchar, (lastUpper<col)*4+(mask2&3)));
    cm.set(hash(516,spaces, (words&255)));
    cm.set(hash(256,number0, word2));
    cm.set(hash(257,number0, word1));
    cm.set(hash(258,number1, c,ccword));
    cm.set(hash(259,number0, number1));
    cm.set(hash(260,word0, number1, lastDigit<wordGap+wordlen));
    cm.set(hash(518,wordlen1,col));
    cm.set(hash(519,c,spacecount/2));
    U32 h=wordcount*64+spacecount;
    cm.set(hash(520,c,h,ccword));
    cm.set(hash(517,frstchar,h));
    cm.set(hash(521,h,spafdo));

    U32 d=c4&0xf0ff;
    cm.set(hash(522,d,frstchar,ccword));
    h=word0*271;
    h=h+buf(1);
    cm.set(hash(262,h, 0));
    cm.set(hash(263,word0, 0));
    cm.set(hash(264,h, word1));
    cm.set(hash(265,word0, word1));
    cm.set(hash(266,h, word1,word2,lastUpper<wordlen));
    cm.set(hash(268,text0&0xfffff, 0));
    cm.set(hash(269,word0, xword0));
    cm.set(hash(270,word0, xword1));
    cm.set(hash(271,word0, xword2));
    cm.set(hash(272,frstchar, xword2));
    cm.set(hash(273,word0, cword0));
    cm.set(hash(274,number0, cword0));
    cm.set(hash(275,h, word2));
    cm.set(hash(276,h, word3));
    cm.set(hash(277,h, word4));
    cm.set(hash(278,h, word5));
    cm.set(hash(279,h, word1,word3));
    cm.set(hash(280,h, word2,word3));
    if (f) {
      word5=word4;
      word4=word3;
      word3=word2;
      word2=word1;
      word1='.';
    }
    cm.set(hash(523,col,buf(1),above));
    cm.set(hash(524,buf(1),above));
    cm.set(hash(525,col,buf(1)));
    cm.set(hash(526,col,c==32));
    cm.set(hash(281, w, llog(blpos-wpos[w])>>4));
    cm.set(hash(282,buf(1),llog(blpos-wpos[w])>>2));

    int fl = 0;
    if ((c4&0xff) != 0) {
      if (isalpha(c4&0xff)) fl = 1;
      else if (ispunct(c4&0xff)) fl = 2;
      else if (isspace(c4&0xff)) fl = 3;
      else if ((c4&0xff) == 0xff) fl = 4;
      else if ((c4&0xff) < 16) fl = 5;
      else if ((c4&0xff) < 64) fl = 6;
      else fl = 7;
    }
    mask = (mask<<3)|fl;
    cm.set(hash(528,mask,0));
    cm.set(hash(529,mask,buf(1)));
    cm.set(hash(530,mask&0xff,col));
    cm.set(hash(531,mask,buf(2),buf(3)));
    cm.set(hash(532,mask&0x1ff,f4&0x00fff0));

    cm.set(hash(h, llog(wordGap), mask&0x1FF,
      ((wordlen1 > 3)<<6)|
      ((wordlen > 0)<<5)|
      ((spafdo == wordlen + 2)<<4)|
      ((spafdo == wordlen + wordlen1 + 3)<<3)|
      ((spafdo >= lastLetter + wordlen1 + wordGap)<<2)|
      ((lastUpper < lastLetter + wordlen1)<<1)|
      (lastUpper < wordlen + wordlen1 + wordGap)
    ));
    cm.set(hash(col,wordlen1,above&0x5F,c4&0x5F));
    cm.set(hash( mask2&0x3F, wrdhsh&0xFFF, (0x100|firstLetter)*(wordlen<6),(wordGap>4)*2+(wordlen1>5)) );
    cm.set(hash((*pWord).Hash[2], h));
  }
  cm.mix(m);
}

#endif //USE_WORDMODEL

//////////////////////////// recordModel ///////////////////////

// Model 2-D data with fixed record length.  Also order 1-2 models
// that include the distance to the last match.

inline U8 Clip(int const Px){
  if(Px>255)return 255;
  if(Px<0)return 0;
  return Px;
}
inline U8 Clamp4(const int Px, const U8 n1, const U8 n2, const U8 n3, const U8 n4){
  int maximum=n1;if(maximum<n2)maximum=n2;if(maximum<n3)maximum=n3;if(maximum<n4)maximum=n4;
  int minimum=n1;if(minimum>n2)minimum=n2;if(minimum>n3)minimum=n3;if(minimum>n4)minimum=n4;
  if(Px<minimum)return minimum;
  if(Px>maximum)return maximum;
  return Px;
}
inline U8 LogMeanDiffQt(const U8 a, const U8 b, const U8 limit = 7){
  if (a==b) return 0;
  U8 sign=a>b?8:0;
  return sign | min(limit, ilog2((a+b)/max(2,abs(a-b)*2)+1));
}
inline U32 LogQt(const U8 Px, const U8 bits){
  return (U32(0x100|Px))>>max(0,(int)(ilog2(Px)-bits));
}

struct dBASE {
  U8 Version;
  U32 nRecords;
  U16 RecordLength, HeaderLength;
  int Start, End;
};

void recordModel(Mixer& m, ModelStats *Stats = nullptr) {
  static Array<int> cpos1(256) , cpos2(256), cpos3(256), cpos4(256);
  static Array<int> wpos1(0x10000); // buf(1..2) -> last position
  static int rlen[3] = {2,3,4}; // run length and 2 candidates
  static int rcount[2] = {0,0}; // candidate counts
  static U8 padding = 0; // detected padding byte
  static int prevTransition = 0, nTransition = 0; // position of the last padding transition
  static int col = 0, mxCtx = 0;
  static ContextMap cm(32768, 3), cn(32768/2, 3), co(32768*2, 3), cp(MEM, 7);
  static StationaryMap Map0(10), Map1(10);
  static bool MayBeImg24b = false;
  static dBASE dbase;

  // Find record length
  if (bpos==0) {
    int w=c4&0xffff, c=w&255, d=w>>8;
    if (Stats && (*Stats).Record && ((*Stats).Record>>16)>2 &&
          ((*Stats).Record>>16) != (U32)rlen[0]) {
      rlen[0] = (*Stats).Record>>16;
      rcount[0]=rcount[1]=0;
    }
    else{
      // detect dBASE tables
      if (blpos==0 || (dbase.Version>0 && blpos>=dbase.End))
        dbase.Version = 0;
      else if (dbase.Version==0 && (Stats && Stats->blockType==DEFAULT) && blpos>=31){
        U8 b = buf(32);
        if ( ((b&7)==3 || (b&7)==4 || (b>>4)==3 || b==0xF5) &&
             ((b=buf(30))>0 && b<13) &&
             ((b=buf(29))>0 && b<32) &&
             ((dbase.nRecords = buf(28)|(buf(27)<<8)|(buf(26)<<16)|(buf(25)<<24)) > 0 && dbase.nRecords<0xFFFFF) &&
             ((dbase.HeaderLength = buf(24)|(buf(23)<<8)) > 32 && ( ((dbase.HeaderLength-32-1)%32)==0 || (dbase.HeaderLength>255+8 && (((dbase.HeaderLength-=255+8)-32-1)%32)==0) )) &&
             ((dbase.RecordLength = buf(22)|(buf(21)<<8)) > 8) &&
             (buf(20)==0 && buf(19)==0 && buf(17)<=1 && buf(16)<=1)
        ){
          dbase.Version = (((b=buf(32))>>4)==3)?3:b&7;
          dbase.Start = blpos - 32 + dbase.HeaderLength;
          dbase.End = dbase.Start + dbase.nRecords * dbase.RecordLength;
          if (dbase.Version==3){
            rlen[0] = 32;
            rcount[0]=rcount[1]=0;
          }
        }
      }
      else if (dbase.Version>0 && blpos==dbase.Start){
        rlen[0] = dbase.RecordLength;
        rcount[0]=rcount[1]=0;
      }

      int r=pos-cpos1[c];
      if (r>1 && r==cpos1[c]-cpos2[c]
          && r==cpos2[c]-cpos3[c] && (r>32 || r==cpos3[c]-cpos4[c])
          && (r>10 || ((c==buf(r*5+1)) && c==buf(r*6+1)))) {
        if (r==rlen[1]) ++rcount[0];
        else if (r==rlen[2]) ++rcount[1];
        else if (rcount[0]>rcount[1]) rlen[2]=r, rcount[1]=1;
        else rlen[1]=r, rcount[0]=1;
      }

      // check candidate lengths
      for (int i=0; i < 2; i++) {
        if (rcount[i] > max(0,12-(int)ilog2(rlen[i+1]))){
          if (rlen[0] != rlen[i+1]){
            if (MayBeImg24b && rlen[i+1]==3){
              rcount[0]>>=1;
              rcount[1]>>=1;
              continue;
            }
            else if ( (rlen[i+1] > rlen[0]) && (rlen[i+1] % rlen[0] == 0) ){
              // maybe we found a multiple of the real record size..?
              // in that case, it is probably an immediate multiple (2x).
              // that is probably more likely the bigger the length, so
              // check for small lengths too
              if ((rlen[0] > 32) && (rlen[i+1] == rlen[0]*2)){
                rcount[0]>>=1;
                rcount[1]>>=1;
                continue;
              }
            }
            rlen[0] = rlen[i+1];
            rcount[i] = 0;
            MayBeImg24b = (rlen[0]>30 && (rlen[0]%3)==0);
            nTransition = 0;
          }
          else
            // we found the same length again, that's positive reinforcement that
            // this really is the correct record size, so give it a little boost
            rcount[i]>>=2;

          // if the other candidate record length is orders of
          // magnitude larger, it will probably never have enough time
          // to increase its counter before it's reset again. and if
          // this length is not a multiple of the other, than it might
          // really be worthwhile to investigate it, so we won't set its
          // counter to 0
          if (rlen[i+1]<<4 > rlen[1+(i^1)])
            rcount[i^1] = 0;
        }
      }
    }
    // Set 2 dimensional contexts
    assert(rlen[0]>0);
    cm.set(c<<8| (min(255, pos-cpos1[c])/4));
    cm.set(w<<9| llog(pos-wpos1[w])>>2);
    cm.set(rlen[0]|buf(rlen[0])<<10|buf(rlen[0]*2)<<18);

    cn.set(w|rlen[0]<<8);
    cn.set(d|rlen[0]<<16);
    cn.set(c|rlen[0]<<8);

    co.set(buf(1)<<8|min(255, pos-cpos1[buf(1)]));
    co.set(buf(1)<<17|buf(2)<<9|llog(pos-wpos1[w])>>2);
    co.set(buf(1)<<8|buf(rlen[0]));

    col=pos%rlen[0];
    if (!col)
      nTransition = 0;
    cp.set(rlen[0]|buf(rlen[0])<<10|col<<18);
    cp.set(rlen[0]|buf(1)<<10|col<<18);
    cp.set(col|rlen[0]<<12);

    /*
    Consider record structures that include fixed-length strings.
    These usually contain the text followed by either spaces or 0's,
    depending on whether they're to be trimmed or they're null-terminated.
    That means we can guess the length of the string field by looking
    for small repetitions of one of these padding bytes followed by a
    different byte. By storing the last position where this transition
    ocurred, and what was the padding byte, we are able to model these
    runs of padding bytes.
    Special care is taken to skip record structures of less than 9 bytes,
    since those may be little-endian 64 bit integers. If they contain
    relatively low values (<2^40), we may consistently get runs of 3 or
    even more 0's at the end of each record, and so we could assume that
    to be the general case. But with integers, we can't be reasonably sure
    that a number won't have 3 or more 0's just before a final non-zero MSB.
    And with such simple structures, there's probably no need to be fancy
    anyway
    */

    if ((((c4>>8) == SPACE*0x010101) && (c != SPACE)) || (!(c4>>8) && c && ((padding != SPACE) || (pos-prevTransition > rlen[0])))){
      prevTransition = pos;
      nTransition+=(nTransition<31);
      padding = (U8)d;
    }
    if (rlen[0]>8){
      cp.set( hash( min(min(0xFF,rlen[0]),pos-prevTransition), min(0x3FF,col), (w&0xF0F0)|(w==((padding<<8)|padding)), nTransition ) );
      cp.set( hash( w, (buf(rlen[0]+1)==padding && buf(rlen[0])==padding), col/max(1,rlen[0]/32) ) );
    }
    else
      cp.set(0), cp.set(0);

    int last4 = (buf(rlen[0]*4)<<24)|(buf(rlen[0]*3)<<16)|(buf(rlen[0]*2)<<8)|buf(rlen[0]);
    cp.set( (last4&0xFF)|((last4&0xF000)>>4)|((last4&0xE00000)>>9)|((last4&0xE0000000)>>14)|((col/max(1,rlen[0]/16))<<18) );
    cp.set( (last4&0xF8F8)|(col<<16) );

    int i=0x300;
    if (MayBeImg24b)
      i = (col%3)<<8, Map0.set(Clip(((U8)(c4>>16))+c-(c4>>24))|i);
    else
      Map0.set(Clip(c*2-d)|i);
    Map1.set(Clip(c+buf(rlen[0])-buf(rlen[0]+1))|i);

    // update last context positions
    cpos4[c]=cpos3[c];
    cpos3[c]=cpos2[c];
    cpos2[c]=cpos1[c];
    cpos1[c]=pos;
    wpos1[w]=pos;

    mxCtx = (rlen[0]>128)?(min(0x7F,col/max(1,rlen[0]/128))):col;
  }
  cm.mix(m);
  cn.mix(m);
  co.mix(m);
  cp.mix(m);
  Map0.mix(m);
  Map1.mix(m);

  m.set( (rlen[0]>2)*( (bpos<<7)|mxCtx ), 1024 );
  m.set( ((buf(rlen[0])^((U8)(c0<<(8-bpos))))>>4)|(min(0x1F,col/max(1,rlen[0]/32))<<4), 512 );
  if (Stats)
    (*Stats).Record = (min(0xFFFF,rlen[0])<<16)|min(0xFFFF,col);
}


//////////////////////////// sparseModel ///////////////////////

// Model order 1-2 contexts with gaps.

void sparseModel(Mixer& m, int seenbefore, int howmany) {
  static ContextMap cm(MEM*2, 42);
  if (bpos==0) {
    cm.set(seenbefore);
    cm.set(howmany);
    cm.set(buf(1)|buf(5)<<8);
    cm.set(buf(1)|buf(6)<<8);
    cm.set(buf(3)|buf(6)<<8);
    cm.set(buf(4)|buf(8)<<8);
    cm.set(buf(1)|buf(3)<<8|buf(5)<<16);
    cm.set(buf(2)|buf(4)<<8|buf(6)<<16);
    cm.set(c4&0x00f0f0ff);
    cm.set(c4&0x00ff00ff);
    cm.set(c4&0xff0000ff);
    cm.set(c4&0x00f8f8f8);
    cm.set(c4&0xf8f8f8f8);
    cm.set(f4&0x00000fff);
    cm.set(f4);
    cm.set(c4&0x00e0e0e0);
    cm.set(c4&0xe0e0e0e0);
    cm.set(c4&0x810000c1);
    cm.set(c4&0xC3CCC38C);
    cm.set(c4&0x0081CC81);
    cm.set(c4&0x00c10081);
    for (int i=1; i<8; ++i) {
      cm.set(seenbefore|buf(i)<<8);
      cm.set((buf(i+2)<<8)|buf(i+1));
      cm.set((buf(i+3)<<8)|buf(i+1));
    }
  }
  cm.mix(m);
}

//////////////////////////// distanceModel ///////////////////////

// Model for modelling distances between symbols

void distanceModel(Mixer& m) {
  static ContextMap cr(MEM, 3);
  if (bpos == 0) {
    static int pos00=0,pos20=0,posnl=0;
    int c=c4&0xff;
    if (c==0x00) pos00=pos;
    if (c==0x20) pos20=pos;
    if (c==0xff||c=='\r'||c=='\n') posnl=pos;
    cr.set(min(pos-pos00,255)|(c<<8));
    cr.set(min(pos-pos20,255)|(c<<8));
    cr.set(min(pos-posnl,255)|((c<<8)+234567));
  }
  cr.mix(m);
}


//////////////////////////// im24bitModel /////////////////////////////////

// Square buf(i)
inline int sqrbuf(int i) {
  assert(i>0);
  return buf(i)*buf(i);
}

class RingBuffer {
  Array<U8> b;
  U32 offset;
public:
  RingBuffer(const int i=0): b(i), offset(0) {}
  void Fill(const U8 B) {
    memset(&b[0], B, b.size());
  }
  void Add(const U8 B){
    b[offset&(b.size()-1)] = B;
    offset++;
  }
  int operator()(const int i) const {
    return b[(offset-i)&(b.size()-1)];
  }
};

inline U8 Paeth(U8 W, U8 N, U8 NW){
  int p = W+N-NW;
  int pW=abs(p-(int)W), pN=abs(p-(int)N), pNW=abs(p-(int)NW);
  if (pW<=pN && pW<=pNW) return W;
  else if (pN<=pNW) return N;
  return NW;
}

// Model for filtered (PNG) or unfiltered 24/32-bit image data

void im24bitModel(Mixer& m, int info, int alpha=0, int isPNG=0) {
  static const int nMaps = 94;
  static const int nSCMaps = 59;
  static ContextMap cm(MEM*4, 45);
  static SmallStationaryContextMap SCMap[nSCMaps] = { {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4},
                                                      {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4},
                                                      {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4},
                                                      {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4},
                                                      {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4},
                                                      {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4},
                                                      {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4}, {9,4},
                                                      {9,4}, {9,4}, {0,8}};
  static StationaryMap Map[nMaps] ={      8,      8,      8,      2,      0, {13,4}, {13,4}, {13,4}, {13,4}, {13,4},
                                     {15,4}, {15,4}, {15,4}, {15,4}, {11,4}, {11,4}, {11,4}, {11,4}, { 9,4}, { 9,4},
                                     { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4},
                                     { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4},
                                     { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4},
                                     { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4},
                                     { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4},
                                     { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4},
                                     { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4}, { 9,4},
                                     { 9,4}, { 9,4}, { 9,4}, { 9,4}};
  static RingBuffer buffer(0x100000); // internal rotating buffer for (PNG unfiltered) pixel data
  static U8 WWW, WW, W, NWW, NW, N, NE, NEE, NNWW, NNW, NN, NNE, NNEE, NNN; //pixel neighborhood
  static U8 WWp1, Wp1, p1, NWp1, Np1, NEp1, NNp1;
  static U8 WWp2, Wp2, p2, NWp2, Np2, NEp2, NNp2;
  static U8 px = 0; // current PNG filter prediction
  static int color = -1;
  static int stride = 3;
  static int ctx[2], padding, lastPos=0, lastWasPNG=0, filter=0, x=0, w=0, line=0, R1=0, R2=0;
  static bool filterOn = false;
  static int columns[2] = {1,1}, column[2];

  if (bpos==0) {
    if ((color<0) || (pos-lastPos!=1)) {
      stride = 3+alpha;
      w = info&0xFFFFFF;
      padding = w%stride;
      x = color = line = px = 0;
      filterOn = false;
      columns[0] = max(1, w/max(1, ilog2(w)*3));
      columns[1] = max(1, columns[0]/max(1, ilog2(columns[0])));
      if (lastPos>0 && lastWasPNG!=isPNG) {
        for (int i=0; i<nMaps; i++)
          Map[i].Reset();
      }
      lastWasPNG = isPNG;
      buffer.Fill(0x7F);
    }
    else {
      x++;
      if (x>=w+isPNG) { x=0; line++; }
    }
    lastPos = pos;

    if (x==1 && isPNG)
      filter = c4&0xFF;
    else {
      if (x+padding<w) {
        color++;
        if (color>=stride) color=0;
      }
      else {
        if (padding>0) color=stride+1;
        else color=0;
      }
      if (isPNG) {
        U8 B = c4&0xFF;
        switch (filter) {
          case 1: {
            buffer.Add((U8)(B+buffer(stride)*(x>stride+1 || !x)));
            filterOn = x>stride;
            px = buffer(stride);
            break;
          }
          case 2: {
            buffer.Add((U8)(B+buffer(w)*(filterOn=(line>0))));
            px = buffer(w);
            break;
          }
          case 3: {
            buffer.Add((U8)(B+(buffer(w)*(line>0)+buffer(stride)*(x>stride+1 || !x))/2));
            filterOn = (x>stride || line>0);
            px = (buffer(stride)*(x>stride)+buffer(w)*(line>0))/2;
            break;
          }
          case 4: {
            buffer.Add((U8)(B+Paeth(buffer(stride)*(x>stride+1 || !x), buffer(w)*(line>0), buffer(w+stride)*(line>0 && (x>stride+1 || !x)))));
            filterOn = (x>stride || line>0);
            px = Paeth(buffer(stride)*(x>stride), buffer(w)*(line>0), buffer(w+stride)*(x>stride && line>0));
            break;
          }
          default: buffer.Add(B);
            filterOn = false;
            px = 0;
        }
        if (!filterOn) px=0;
      }
      else
        buffer.Add(c4&0xFF);
    }

    if (x>0 || !isPNG) {
      int i=color<<5;
      column[0]=(x-isPNG)/columns[0];
      column[1]=(x-isPNG)/columns[1];
      WWW=buffer(3*stride), WW=buffer(2*stride), W=buffer(stride), NWW=buffer(w+2*stride), NW=buffer(w+stride), N=buffer(w), NE=buffer(w-stride), NEE=buffer(w-2*stride), NNWW=buffer((w+stride)*2), NNW=buffer(w*2+stride), NN=buffer(w*2), NNE=buffer(w*2-stride), NNEE=buffer((w-stride)*2), NNN=buffer(w*3);
      WWp1=buffer(stride*2+1), Wp1=buffer(stride+1), p1=buffer(1), NWp1=buffer(w+stride+1), Np1=buffer(w+1), NEp1=buffer(w-stride+1), NNp1=buffer(w*2+1);
      WWp2=buffer(stride*2+2), Wp2=buffer(stride+2), p2=buffer(2), NWp2=buffer(w+stride+2), Np2=buffer(w+2), NEp2=buffer(w-stride+2), NNp2=buffer(w*2+2);

      if (!isPNG) {
        int mean=W+NW+N+NE;
        const int var=(W*W+NW*NW+N*N+NE*NE-mean*mean/4)>>2;
        mean>>=2;
        const int logvar=ilog(var);

        cm.set(hash((N+1)>>1, LogMeanDiffQt(N, Clip(NN*2-NNN))));
        cm.set(hash((W+1)>>1, LogMeanDiffQt(W, Clip(WW*2-WWW))));
        cm.set(hash(Clamp4(W+N-NW, W, NW, N, NE), LogMeanDiffQt(Clip(N+NE-NNE), Clip(N+NW-NNW))));
        cm.set(hash((NNN+N+4)/8, Clip(N*3-NN*3+NNN)>>1));
        cm.set(hash((WWW+W+4)/8, Clip(W*3-WW*3+WWW)>>1));
        cm.set(hash(++i, (W+Clip(NE*3-NNE*3+buffer(w*3-stride)))/4, LogMeanDiffQt(N, (NW+NE)/2)));
        cm.set(hash(++i, Clip((-buffer(4*stride)+5*WWW-10*WW+10*W+Clamp4(NE*4-NNE*6+buffer(w*3-stride)*4-buffer(w*4-stride), N, NE, buffer(w-2*stride), buffer(w-3*stride)))/5)/4));
        cm.set(hash(Clip(NEE+N-NNEE), LogMeanDiffQt(W, Clip(NW+NE-NNE))));
        cm.set(hash(Clip(NN+W-NNW), LogMeanDiffQt(W, Clip(NNW+WW-NNWW))));
        cm.set(hash(++i, p1));
        cm.set(hash(++i, p2));
        cm.set(hash(++i, Clip(W+N-NW)/2, Clip(W+p1-Wp1)/2));
        cm.set(hash(Clip(N*2-NN)/2, LogMeanDiffQt(N, Clip(NN*2-NNN))));
        cm.set(hash(Clip(W*2-WW)/2, LogMeanDiffQt(W, Clip(WW*2-WWW))));
        cm.set(Clamp4(N*3-NN*3+NNN, W, NW, N, NE)/2);
        cm.set(Clamp4(W*3-WW*3+WWW, W, N, NE, NEE)/2);
        cm.set(hash(++i, LogMeanDiffQt(W, Wp1), Clamp4((p1*W)/(Wp1<1?1:Wp1), W, N, NE, NEE))); //using max(1,Wp1) results in division by zero in VC2015
        cm.set(hash(++i, Clamp4(N+p2-Np2, W, NW, N, NE)));
        cm.set(hash(++i, Clip(W+N-NW), column[0]));
        cm.set(hash(++i, Clip(N*2-NN), LogMeanDiffQt(W, Clip(NW*2-NNW))));
        cm.set(hash(++i, Clip(W*2-WW), LogMeanDiffQt(N, Clip(NW*2-NWW))));
        cm.set(hash((W+NEE)/2, LogMeanDiffQt(W, (WW+NE)/2)));
        cm.set(Clamp4(Clip(W*2-WW)+Clip(N*2-NN)-Clip(NW*2-NNWW), W, NW, N, NE));
        cm.set(hash(++i, W, p2));
        cm.set(hash(N, NN&0x1F, NNN&0x1F));
        cm.set(hash(W, WW&0x1F, WWW&0x1F));
        cm.set(hash(++i, N, column[0]));
        cm.set(hash(++i, Clip(W+NEE-NE), LogMeanDiffQt(W, Clip(WW+NE-N))));
        cm.set(hash(NN, buffer(w*4)&0x1F, buffer(w*6)&0x1F, column[1]));
        cm.set(hash(WW, buffer(stride*4)&0x1F, buffer(stride*6)&0x1F, column[1]));
        cm.set(hash(NNN, buffer(w*6)&0x1F, buffer(w*9)&0x1F, column[1]));
        cm.set(hash(++i, column[1]));

        cm.set(hash(++i, W, LogMeanDiffQt(W, WW)));
        cm.set(hash(++i, W, p1));
        cm.set(hash(++i, W/4, LogMeanDiffQt(W, p1), LogMeanDiffQt(W, p2)));
        cm.set(hash(++i, N, LogMeanDiffQt(N, NN)));
        cm.set(hash(++i, N, p1));
        cm.set(hash(++i, N/4, LogMeanDiffQt(N, p1), LogMeanDiffQt(N, p2)));
        cm.set(hash(++i, (W+N)>>3, p1>>4, p2>>4));
        cm.set(hash(++i, p1/2, p2/2));
        cm.set(hash(++i, W, p1-Wp1));
        cm.set(hash(++i, W+p1-Wp1));
        cm.set(hash(++i, N, p1-Np1));
        cm.set(hash(++i, N+p1-Np1));
        cm.set(hash(++i, mean, logvar>>4));

        ctx[0] = (min(color, stride-1)<<9)|((abs(W-N)>3)<<8)|((W>N)<<7)|((W>NW)<<6)|((abs(N-NW)>3)<<5)|((N>NW)<<4)|((abs(N-NE)>3)<<3)|((N>NE)<<2)|((W>WW)<<1)|(N>NN);
        ctx[1] = ((LogMeanDiffQt(p1, Clip(Np1+NEp1-buffer(w*2-stride+1)))>>1)<<5)|((LogMeanDiffQt(Clip(N+NE-NNE), Clip(N+NW-NNW))>>1)<<2)|min(color, stride-1);
      }
      else {
        i|=(filterOn)?((0x100|filter)<<8):0;
        int residuals[5] ={ ((int8_t)buf(stride+(x<=stride)))+128,
                             ((int8_t)buf(1+(x<2)))+128,
                             ((int8_t)buf(stride+1+(x<=stride)))+128,
                             ((int8_t)buf(2+(x<3)))+128,
                             ((int8_t)buf(stride+2+(x<=stride)))+128
        };
        R1 = (residuals[1]*residuals[0])/max(1, residuals[2]);
        R2 = (residuals[3]*residuals[0])/max(1, residuals[4]);

        cm.set(hash(++i, Clip(W+N-NW)-px, Clip(W+p1-Wp1)-px, R1));
        cm.set(hash(++i, Clip(W+N-NW)-px, LogMeanDiffQt(p1, Clip(Wp1+Np1-NWp1))));
        cm.set(hash(++i, Clip(W+N-NW)-px, LogMeanDiffQt(p2, Clip(buffer(stride+2)+buffer(w+2)-buffer(w+stride+2))), R2/4));
        cm.set(hash(++i, Clip(W+N-NW)-px, Clip(N+NE-NNE)-Clip(N+NW-NNW)));
        cm.set(hash(++i, Clip(W+N-NW+p1-(Wp1+Np1-NWp1)), px, R1));
        cm.set(hash(++i, Clamp4(W+N-NW, W, NW, N, NE)-px, column[0]));
        cm.set(hash(i>>8, Clamp4(W+N-NW, W, NW, N, NE)/8, px));
        cm.set(hash(++i, N-px, Clip(N+p1-Np1)-px));
        cm.set(hash(++i, Clip(W+p1-Wp1)-px, R1));
        cm.set(hash(++i, Clip(N+p1-Np1)-px));
        cm.set(hash(++i, Clip(N+p1-Np1)-px, Clip(N+p2-buffer(w+2))-px));
        cm.set(hash(++i, Clip(W+p1-Wp1)-px, Clip(W+p2-buffer(stride+2))-px));
        cm.set(hash(++i, Clip(NW+p1-NWp1)-px));
        cm.set(hash(++i, Clip(NW+p1-NWp1)-px, column[0]));
        cm.set(hash(++i, Clip(NE+p1-NEp1)-px, column[0]));
        cm.set(hash(++i, Clip(NE+N-NNE)-px, Clip(NE+p1-NEp1)-px));
        cm.set(hash(i>>8, Clip(N+NE-NNE)-px, column[0]));
        cm.set(hash(++i, Clip(NW+N-NNW)-px, Clip(NW+p1-NWp1)-px));
        cm.set(hash(i>>8, Clip(N+NW-NNW)-px, column[0]));
        cm.set(hash(i>>8, Clip(NN+W-NNW)-px, LogMeanDiffQt(N, Clip(NNN+NW-buffer(w*3+stride)))));
        cm.set(hash(i>>8, Clip(W+NEE-NE)-px, LogMeanDiffQt(W, Clip(WW+NE-N))));
        cm.set(hash(++i, Clip(N+NN-NNN+buffer(1+(!color))-Clip(buffer(w+1+(!color))+buffer(w*2+1+(!color))-buffer(w*3+1+(!color))))-px));
        cm.set(hash(i>>8, Clip(N+NN-NNN)-px, Clip(5*N-10*NN+10*NNN-5*buffer(w*4)+buffer(w*5))-px));
        cm.set(hash(++i, Clip(N*2-NN)-px, LogMeanDiffQt(N, Clip(NN*2-NNN))));
        cm.set(hash(++i, Clip(W*2-WW)-px, LogMeanDiffQt(W, Clip(WW*2-WWW))));
        cm.set(hash(i>>8, Clip(N*3-NN*3+NNN)-px));
        cm.set(hash(++i, Clip(N*3-NN*3+NNN)-px, LogMeanDiffQt(W, Clip(NW*2-NNW))));
        cm.set(hash(i>>8, Clip(W*3-WW*3+WWW)-px));
        cm.set(hash(++i, Clip(W*3-WW*3+WWW)-px, LogMeanDiffQt(N, Clip(NW*2-NWW))));
        cm.set(hash(i>>8, Clip((35*N-35*NNN+21*buffer(w*5)-5*buffer(w*7))/16)-px));
        cm.set(hash(++i, (W+Clip(NE*3-NNE*3+buffer(w*3-stride)))/2-px, R2));
        cm.set(hash(++i, (W+Clamp4(NE*3-NNE*3+buffer(w*3-stride), W, N, NE, NEE))/2-px, LogMeanDiffQt(N, (NW+NE)/2)));
        cm.set(hash(++i, (W+NEE)/2-px, R1/2));
        cm.set(hash(++i, Clamp4(Clip(W*2-WW)+Clip(N*2-NN)-Clip(NW*2-NNWW), W, NW, N, NE)-px));
        cm.set(hash(++i, buf(stride+(x<=stride)), buf(1+(x<2)), buf(2+(x<3))));
        cm.set(hash(++i, buf(1+(x<2)), px));
        cm.set(hash(i>>8, buf(w+1), buf((w+1)*2), buf((w+1)*3), px));
        cm.set(~0x5ca1ab1e);

        ctx[0] = (min(color, stride-1)<<9)|((abs(W-N)>3)<<8)|((W>N)<<7)|((W>NW)<<6)|((abs(N-NW)>3)<<5)|((N>NW)<<4)|((N>NE)<<3)|min(5, filterOn?filter+1:0);
        ctx[1] = ((LogMeanDiffQt(p1, Clip(Np1+NEp1-buffer(w*2-stride+1)))>>1)<<5)|((LogMeanDiffQt(Clip(N+NE-NNE), Clip(N+NW-NNW))>>1)<<2)|min(color, stride-1);
      }

      i=0;
      Map[i++].set((W&0xC0)|((N&0xC0)>>2)|((WW&0xC0)>>4)|(NN>>6));
      Map[i++].set((N&0xC0)|((NN&0xC0)>>2)|((NE&0xC0)>>4)|(NEE>>6));
      Map[i++].set(buf(1+(isPNG && x<2)));
      Map[i++].set(min(color, stride-1));
    }
  }
  if ((bpos==0 || bpos==4) && (x>0 || !isPNG)) {
    U8 B=(c0<<(8-bpos)), lsb=(bpos>0);
    int i=5;

    Map[i++].set((((U8)(Clip(W+N-NW)-px-B))*2+lsb)|(LogMeanDiffQt(Clip(N+NE-NNE),Clip(N+NW-NNW))<<9));
    Map[i++].set((((U8)(Clip(N*2-NN)-px-B))*2+lsb)|(LogMeanDiffQt(W,Clip(NW*2-NNW))<<9));
    Map[i++].set((((U8)(Clip(W*2-WW)-px-B))*2+lsb)|(LogMeanDiffQt(N,Clip(NW*2-NWW))<<9));
    Map[i++].set((((U8)(Clip(W+N-NW)-px-B))*2+lsb)|(LogMeanDiffQt(p1,Clip(Wp1+Np1-NWp1))<<9));
    Map[i++].set((((U8)(Clip(W+N-NW)-px-B))*2+lsb)|(LogMeanDiffQt(p2,Clip(Wp2+Np2-NWp2))<<9));
    Map[i++].set(hash(W-px-B,N-px-B)*2+lsb);
    Map[i++].set(hash(W-px-B,WW-px-B)*2+lsb);
    Map[i++].set(hash(N-px-B,NN-px-B)*2+lsb);
    Map[i++].set(hash(Clip(N+NE-NNE)-px-B, Clip(N+NW-NNW)-px-B)*2+lsb);
    Map[i++].set((min(color,stride-1)<<9)|(((U8)(Clip(N+p1-Np1)-px-B))*2+lsb));
    Map[i++].set((min(color,stride-1)<<9)|(((U8)(Clip(N+p2-Np2)-px-B))*2+lsb));
    Map[i++].set((min(color,stride-1)<<9)|(((U8)(Clip(W+p1-Wp1)-px-B))*2+lsb));
    Map[i++].set((min(color,stride-1)<<9)|(((U8)(Clip(W+p2-Wp2)-px-B))*2+lsb));
    Map[i++].set((Clamp4(N+p1-Np1,W,NW,N,NE)-px-B)*2+lsb);
    Map[i++].set((Clamp4(N+p2-Np2,W,NW,N,NE)-px-B)*2+lsb);

    Map[i++].set(((W+Clamp4(NE*3-NNE*3+buffer(w*3-stride),W,N,NE,NEE))/2-px-B)*2+lsb);
    Map[i++].set((Clamp4((W+Clip(NE*2-NNE))/2,W,NW,N,NE)-px-B)*2+lsb);
    Map[i++].set(((W+NEE)/2-px-B)*2+lsb);
    Map[i++].set((Clip((WWW-4*WW+6*W+Clip(NE*4-NNE*6+buffer(w*3-stride)*4-buffer(w*4-stride)))/4)-px-B)*2+lsb);
    Map[i++].set((Clip((-buffer(4*stride)+5*WWW-10*WW+10*W+Clamp4(NE*4-NNE*6+buffer(w*3-stride)*4-buffer(w*4-stride),N,NE,buffer(w-2*stride),buffer(w-3*stride)))/5)-px-B)*2+lsb);
    Map[i++].set((Clip((-4*WW+15*W+10*Clip(NE*3-NNE*3+buffer(w*3-stride))-Clip(buffer(w-3*stride)*3-buffer(w*2-3*stride)*3+buffer(w*3-3*stride)))/20)-px-B)*2+lsb);
    Map[i++].set((Clip((-3*WW+8*W+Clamp4(NEE*3-NNEE*3+buffer(w*3-stride*2),NE,NEE,buffer(w-3*stride),buffer(w-4*stride)))/6)-px-B)*2+lsb);
    Map[i++].set((Clip((W+Clip(NE*2-NNE))/2+p1-(Wp1+Clip(NEp1*2-buffer(w*2-stride+1)))/2)-px-B)*2+lsb);
    Map[i++].set((Clip((W+Clip(NE*2-NNE))/2+p2-(Wp2+Clip(NEp2*2-buffer(w*2-stride+2)))/2)-px-B)*2+lsb);
    Map[i++].set((Clip((-3*WW+8*W+Clip(NEE*2-NNEE))/6 +p1-(-3*WWp1+8*Wp1+Clip(buffer(w-stride*2+1)*2-buffer(w*2-stride*2+1)))/6)-px-B)*2+lsb);
    Map[i++].set((Clip((-3*WW+8*W+Clip(NEE*2-NNEE))/6 +p2-(-3*WWp2+8*Wp2+Clip(buffer(w-stride*2+2)*2-buffer(w*2-stride*2+2)))/6)-px-B)*2+lsb);
    Map[i++].set((Clip((W+NEE)/2+p1-(Wp1+buffer(w-stride*2+1))/2)-px-B)*2+lsb);
    Map[i++].set((Clip((W+NEE)/2+p2-(Wp2+buffer(w-stride*2+2))/2)-px-B)*2+lsb);
    Map[i++].set((Clip((WW+Clip(NEE*2-NNEE))/2+p1-(WWp1+Clip(buffer(w-stride*2+1)*2-buffer(w*2-stride*2+1)))/2)-px-B)*2+lsb);
    Map[i++].set((Clip((WW+Clip(NEE*2-NNEE))/2+p2-(WWp2+Clip(buffer(w-stride*2+2)*2-buffer(w*2-stride*2+2)))/2)-px-B)*2+lsb);
    Map[i++].set((Clip(WW+NEE-N+p1-Clip(WWp1+buffer(w-stride*2+1)-Np1))-px-B)*2+lsb);
    Map[i++].set((Clip(WW+NEE-N+p2-Clip(WWp2+buffer(w-stride*2+2)-Np2))-px-B)*2+lsb);

    Map[i++].set((Clip(W+N-NW)-px-B)*2+lsb);
    Map[i++].set((Clip(W+N-NW+p1-Clip(Wp1+Np1-NWp1))-px-B)*2+lsb);
    Map[i++].set((Clip(W+N-NW+p2-Clip(Wp2+Np2-NWp2))-px-B)*2+lsb);
    Map[i++].set((Clip(W+NE-N)-px-B)*2+lsb);
    Map[i++].set((Clip(N+NW-NNW)-px-B)*2+lsb);
    Map[i++].set((Clip(N+NW-NNW+p1-Clip(Np1+NWp1-buffer(w*2+stride+1)))-px-B)*2+lsb);
    Map[i++].set((Clip(N+NW-NNW+p2-Clip(Np2+NWp2-buffer(w*2+stride+2)))-px-B)*2+lsb);
    Map[i++].set((Clip(N+NE-NNE)-px-B)*2+lsb);
    Map[i++].set((Clip(N+NE-NNE+p1-Clip(Np1+NEp1-buffer(w*2-stride+1)))-px-B)*2+lsb);
    Map[i++].set((Clip(N+NE-NNE+p2-Clip(Np2+NEp2-buffer(w*2-stride+2)))-px-B)*2+lsb);
    Map[i++].set((Clip(N+NN-NNN)-px-B)*2+lsb);
    Map[i++].set((Clip(N+NN-NNN+p1-Clip(Np1+NNp1-buffer(w*3+1)))-px-B)*2+lsb);
    Map[i++].set((Clip(N+NN-NNN+p2-Clip(Np2+NNp2-buffer(w*3+2)))-px-B)*2+lsb);
    Map[i++].set((Clip(W+WW-WWW)-px-B)*2+lsb);
    Map[i++].set((Clip(W+WW-WWW+p1-Clip(Wp1+WWp1-buffer(stride*3+1)))-px-B)*2+lsb);
    Map[i++].set((Clip(W+WW-WWW+p2-Clip(Wp2+WWp2-buffer(stride*3+2)))-px-B)*2+lsb);
    Map[i++].set((Clip(W+NEE-NE)-px-B)*2+lsb);
    Map[i++].set((Clip(W+NEE-NE+p1-Clip(Wp1+buffer(w-stride*2+1)-NEp1))-px-B)*2+lsb);
    Map[i++].set((Clip(W+NEE-NE+p2-Clip(Wp2+buffer(w-stride*2+2)-NEp2))-px-B)*2+lsb);
    Map[i++].set((Clip(NN+p1-NNp1)-px-B)*2+lsb);
    Map[i++].set((Clip(NN+p2-NNp2)-px-B)*2+lsb);
    Map[i++].set((Clip(NN+W-NNW)-px-B)*2+lsb);
    Map[i++].set((Clip(NN+W-NNW+p1-Clip(NNp1+Wp1-buffer(w*2+stride+1)))-px-B)*2+lsb);
    Map[i++].set((Clip(NN+W-NNW+p2-Clip(NNp2+Wp2-buffer(w*2+stride+2)))-px-B)*2+lsb);
    Map[i++].set((Clip(NN+NW-buffer(w*3+stride))-px-B)*2+lsb);
    Map[i++].set((Clip(NN+NW-buffer(w*3+stride)+p1-Clip(NNp1+NWp1-buffer(w*3+stride+1)))-px-B)*2+lsb);
    Map[i++].set((Clip(NN+NW-buffer(w*3+stride)+p2-Clip(NNp2+NWp2-buffer(w*3+stride+2)))-px-B)*2+lsb);
    Map[i++].set((Clip(NN+NE-buffer(w*3-stride))-px-B)*2+lsb);
    Map[i++].set((Clip(NN+NE-buffer(w*3-stride)+p1-Clip(NNp1+NEp1-buffer(w*3-stride+1)))-px-B)*2+lsb);
    Map[i++].set((Clip(NN+NE-buffer(w*3-stride)+p2-Clip(NNp2+NEp2-buffer(w*3-stride+2)))-px-B)*2+lsb);
    Map[i++].set((Clip(NN+buffer(w*4)-buffer(w*6))-px-B)*2+lsb);
    Map[i++].set((Clip(NN+buffer(w*4)-buffer(w*6)+p1-Clip(NNp1+buffer(w*4+1)-buffer(w*6+1)))-px-B)*2+lsb);
    Map[i++].set((Clip(NN+buffer(w*4)-buffer(w*6)+p2-Clip(NNp2+buffer(w*4+2)-buffer(w*6+2)))-px-B)*2+lsb);
    Map[i++].set((Clip(WW+p1-WWp1)-px-B)*2+lsb);
    Map[i++].set((Clip(WW+p2-WWp2)-px-B)*2+lsb);
    Map[i++].set((Clip(WW+buffer(stride*4)-buffer(stride*6))-px-B)*2+lsb);
    Map[i++].set((Clip(WW+buffer(stride*4)-buffer(stride*6)+p1-Clip(WWp1+buffer(stride*4+1)-buffer(stride*6+1)))-px-B)*2+lsb);
    Map[i++].set((Clip(WW+buffer(stride*4)-buffer(stride*6)+p2-Clip(WWp2+buffer(stride*4+2)-buffer(stride*6+2)))-px-B)*2+lsb);

    Map[i++].set((Clip(N*2-NN+p1-Clip(Np1*2-NNp1))-px-B)*2+lsb);
    Map[i++].set((Clip(N*2-NN+p2-Clip(Np2*2-NNp2))-px-B)*2+lsb);
    Map[i++].set((Clip(W*2-WW+p1-Clip(Wp1*2-WWp1))-px-B)*2+lsb);
    Map[i++].set((Clip(W*2-WW+p2-Clip(Wp2*2-WWp2))-px-B)*2+lsb);
    Map[i++].set((Clip(N*3-NN*3+NNN)-px-B)*2+lsb);
    Map[i++].set((Clamp4(N*3-NN*3+NNN,W,NW,N,NE)-px-B)*2+lsb);
    Map[i++].set((Clamp4(W*3-WW*3+WWW,W,NW,N,NE)-px-B)*2+lsb);
    Map[i++].set((Clamp4(N*2-NN,W,NW,N,NE)-px-B)*2+lsb);
    Map[i++].set((Clip((buffer(w*5)-6*buffer(w*4)+15*NNN-20*NN+15*N+Clamp4(W*4-NWW*6+buffer(w*2+3*stride)*4-buffer(w*3+4*stride),W,NW,N,NN))/6)-px-B)*2+lsb);
    Map[i++].set((Clip((buffer(w*3-3*stride)-4*NNEE+6*NE+Clip(W*4-NW*6+NNW*4-buffer(w*3+stride)))/4)-px-B)*2+lsb);

    Map[i++].set((Clip(((N+3*NW)/4)*3-((NNW+NNWW)/2)*3+(buffer(w*3+2*stride)*3+buffer(w*3+3*stride))/4)-px-B)*2+lsb);
    Map[i++].set((Clip((W*2+NW)-(WW+2*NWW)+buffer(w+3*stride))-px-B)*2+lsb);
    Map[i++].set(((Clip(W*2-NW)+Clip(W*2-NWW)+N+NE)/4-px-B)*2+lsb);

    Map[i++].set((buffer(w*6)-px-B)*2+lsb);
    Map[i++].set(((buffer(w-4*stride)+buffer(w-6*stride))/2-px-B)*2+lsb);
    Map[i++].set(((buffer(stride*6)+buffer(stride*4))/2-px-B)*2+lsb);
    Map[i++].set((((W+N)*3-NW*2)/4-px-B)*2+lsb);
    Map[i++].set((N-px-B)*2+lsb);
    Map[i++].set((NN-px-B)*2+lsb);

    i=0;
    SCMap[i++].set((N+p1-Np1-px-B)*2+lsb);
    SCMap[i++].set((N+p2-Np2-px-B)*2+lsb);
    SCMap[i++].set((W+p1-Wp1-px-B)*2+lsb);
    SCMap[i++].set((W+p2-Wp2-px-B)*2+lsb);
    SCMap[i++].set((NW+p1-NWp1-px-B)*2+lsb);
    SCMap[i++].set((NW+p2-NWp2-px-B)*2+lsb);
    SCMap[i++].set((NE+p1-NEp1-px-B)*2+lsb);
    SCMap[i++].set((NE+p2-NEp2-px-B)*2+lsb);
    SCMap[i++].set((NN+p1-NNp1-px-B)*2+lsb);
    SCMap[i++].set((NN+p2-NNp2-px-B)*2+lsb);
    SCMap[i++].set((WW+p1-WWp1-px-B)*2+lsb);
    SCMap[i++].set((WW+p2-WWp2-px-B)*2+lsb);
    SCMap[i++].set((W+N-NW-px-B)*2+lsb);
    SCMap[i++].set((W+N-NW+p1-Wp1-Np1+NWp1-px-B)*2+lsb);
    SCMap[i++].set((W+N-NW+p2-Wp2-Np2+NWp2-px-B)*2+lsb);
    SCMap[i++].set((W+NE-N-px-B)*2+lsb);
    SCMap[i++].set((W+NE-N+p1-Wp1-NEp1+Np1-px-B)*2+lsb);
    SCMap[i++].set((W+NE-N+p2-Wp2-NEp2+Np2-px-B)*2+lsb);
    SCMap[i++].set((W+NEE-NE-px-B)*2+lsb);
    SCMap[i++].set((W+NEE-NE+p1-Wp1-buffer(w-stride*2+1)+NEp1-px-B)*2+lsb);
    SCMap[i++].set((W+NEE-NE+p2-Wp2-buffer(w-stride*2+2)+NEp2-px-B)*2+lsb);
    SCMap[i++].set((N+NN-NNN-px-B)*2+lsb);
    SCMap[i++].set((N+NN-NNN+p1-Np1-NNp1+buffer(w*3+1)-px-B)*2+lsb);
    SCMap[i++].set((N+NN-NNN+p2-Np2-NNp2+buffer(w*3+2)-px-B)*2+lsb);
    SCMap[i++].set((N+NE-NNE-px-B)*2+lsb);
    SCMap[i++].set((N+NE-NNE+p1-Np1-NEp1+buffer(w*2-stride+1)-px-B)*2+lsb);
    SCMap[i++].set((N+NE-NNE+p2-Np2-NEp2+buffer(w*2-stride+2)-px-B)*2+lsb);
    SCMap[i++].set((N+NW-NNW-px-B)*2+lsb);
    SCMap[i++].set((N+NW-NNW+p1-Np1-NWp1+buffer(w*2+stride+1)-px-B)*2+lsb);
    SCMap[i++].set((N+NW-NNW+p2-Np2-NWp2+buffer(w*2+stride+2)-px-B)*2+lsb);
    SCMap[i++].set((NE+NW-NN-px-B)*2+lsb);
    SCMap[i++].set((NE+NW-NN+p1-NEp1-NWp1+NNp1-px-B)*2+lsb);
    SCMap[i++].set((NE+NW-NN+p2-NEp2-NWp2+NNp2-px-B)*2+lsb);
    SCMap[i++].set((NW+W-NWW-px-B)*2+lsb);
    SCMap[i++].set((NW+W-NWW+p1-NWp1-Wp1+buffer(w+stride*2+1)-px-B)*2+lsb);
    SCMap[i++].set((NW+W-NWW+p2-NWp2-Wp2+buffer(w+stride*2+2)-px-B)*2+lsb);
    SCMap[i++].set((W*2-WW-px-B)*2+lsb);
    SCMap[i++].set((W*2-WW+p1-Wp1*2+WWp1-px-B)*2+lsb);
    SCMap[i++].set((W*2-WW+p2-Wp2*2+WWp2-px-B)*2+lsb);
    SCMap[i++].set((N*2-NN-px-B)*2+lsb);
    SCMap[i++].set((N*2-NN+p1-Np1*2+NNp1-px-B)*2+lsb);
    SCMap[i++].set((N*2-NN+p2-Np2*2+NNp2-px-B)*2+lsb);
    SCMap[i++].set((NW*2-NNWW-px-B)*2+lsb);
    SCMap[i++].set((NW*2-NNWW+p1-NWp1*2+buffer(w*2+stride*2+1)-px-B)*2+lsb);
    SCMap[i++].set((NW*2-NNWW+p2-NWp2*2+buffer(w*2+stride*2+2)-px-B)*2+lsb);
    SCMap[i++].set((NE*2-NNEE-px-B)*2+lsb);
    SCMap[i++].set((NE*2-NNEE+p1-NEp1*2+buffer(w*2-stride*2+1)-px-B)*2+lsb);
    SCMap[i++].set((NE*2-NNEE+p2-NEp2*2+buffer(w*2-stride*2+2)-px-B)*2+lsb);
    SCMap[i++].set((N*3-NN*3+NNN+p1-Np1*3+NNp1*3-buffer(w*3+1)-px-B)*2+lsb);
    SCMap[i++].set((N*3-NN*3+NNN+p2-Np2*3+NNp2*3-buffer(w*3+2)-px-B)*2+lsb);
    SCMap[i++].set((N*3-NN*3+NNN-px-B)*2+lsb);
    SCMap[i++].set(((W+NE*2-NNE)/2-px-B)*2+lsb);
    SCMap[i++].set(((W+NE*3-NNE*3+buffer(w*3-stride))/2-px-B)*2+lsb);
    SCMap[i++].set(((W+NE*2-NNE)/2+p1-(Wp1+NEp1*2-buffer(w*2-stride+1))/2-px-B)*2+lsb);
    SCMap[i++].set(((W+NE*2-NNE)/2+p2-(Wp2+NEp2*2-buffer(w*2-stride+2))/2-px-B)*2+lsb);
    SCMap[i++].set((NNE+NE-buffer(w*3-stride)-px-B)*2+lsb);
    SCMap[i++].set((NNE+W-NN-px-B)*2+lsb);
    SCMap[i++].set((NNW+W-NNWW-px-B)*2+lsb);
  }

  // Predict next bit
  if (x>0 || !isPNG){
    cm.mix(m);
    for (int i=0;i<nMaps;i++)
      Map[i].mix(m);
    for (int i=0;i<nSCMaps;i++)
      SCMap[i].mix(m,9);
    static int col=0;
    if (++col>=stride*8) col=0;
    m.set(5, 6);
    m.set(min(63,column[0])+((ctx[0]>>3)&0xC0), 256);
    m.set(min(127,column[1])+((ctx[0]>>2)&0x180), 512);
    m.set((ctx[0]&0x7FC)|(bpos>>1), 2048);
    m.set(col+(isPNG?(ctx[0]&7)+1:(c0==((0x100|((N+W)/2))>>(8-bpos))))*32, 8*32);
    m.set(((isPNG?p1:0)>>4)*stride+(x%stride) + min(5,filterOn?filter+1:0)*64, 6*64);
    m.set(c0+256*(isPNG && abs(R1-128)>8), 256*2);
    if (level>=4){
      m.set((ctx[1]<<2)|(bpos>>1), 1024);
      m.set(hash(LogMeanDiffQt(W,WW,5), LogMeanDiffQt(N,NN,5), LogMeanDiffQt(W,N,5), ilog2(W), color)&0x1FFF, 8192);
      m.set(hash(ctx[0], column[0]/8)&0x1FFF, 8192);
      m.set(hash(LogQt(N,5), LogMeanDiffQt(N,NN,3), c0)&0x1FFF, 8192);
      m.set(hash(LogQt(W,5), LogMeanDiffQt(W,WW,3), c0)&0x1FFF, 8192);
    }
  }
  else{
    m.add(-2048+((filter>>(7-bpos))&1)*4096);
    m.set(min(4,filter), 6);
  }
}

//////////////////////////// im8bitModel /////////////////////////////////

// Model for 8-bit image data
void im8bitModel(Mixer& m, int w, int gray = 0, int isPNG=0) {
  static const int nMaps = 57;
  static ContextMap cm(MEM*4, 49);
  static StationaryMap Map[nMaps] = { 12, 8, 8, 8, 8, 8, 8, 8,
                                       8, 8, 8, 8, 8, 8, 8, 8,
                                       8, 8, 8, 8, 8, 8, 8, 8,
                                       8, 8, 8, 8, 8, 8, 8, 8,
                                       8, 8, 8, 8, 8, 8, 8, 8,
                                       8, 8, 8, 8, 8, 8, 8, 8,
                                       8, 8, 8, 8, 8, 8, 8, 8,
                                       0 };
  static RingBuffer buffer(0x100000); // internal rotating buffer for (PNG unfiltered) pixel data
  static Array<short> jumps(0x8000);
  static U8 WWW, WW, W, NWW, NW, N, NE, NEE, NNWW, NNW, NN, NNE, NNEE, NNN; //pixel neighborhood
  static U8 px = 0, res = 0; // current PNG filter prediction, expected residual
  static int ctx=0, lastPos=0, lastWasPNG=0, col=0, line=0, x=0, filter=0, jump=0;
  static bool filterOn = false;
  static int columns[2] = {1,1}, column[2];
  // Select nearby pixels as context
  if (bpos==0) {
    if (pos!=lastPos+1){
      x = line = px = jump = 0;
      filterOn = false;
      columns[0] = max(1,w/max(1,ilog2(w)*2));
      columns[1] = max(1,columns[0]/max(1,ilog2(columns[0])));
      if (gray){
        if (lastPos && lastWasPNG!=isPNG){
          for (int i=0;i<nMaps;i++)
            Map[i].Reset();
        }
        lastWasPNG = isPNG;
      }
      buffer.Fill(0x7F);
    }
    else{
      x++;
      if (x>=w+isPNG){x=0; line++;}
    }
    lastPos = pos;

    if (isPNG){
      if (x==1)
        filter = (U8)c4;
      else{
        U8 B = (U8)c4;

        switch (filter){
          case 1: {
            buffer.Add((U8)( B + buffer(1)*(x>2 || !x) ) );
            filterOn = x>1;
            px = buffer(1);
            break;
          }
          case 2: {
            buffer.Add((U8)( B + buffer(w)*(filterOn=(line>0)) ) );
            px = buffer(w);
            break;
          }
          case 3: {
            buffer.Add((U8)( B + (buffer(w)*(line>0) + buffer(1)*(x>2 || !x))/2 ) );
            filterOn = (x>1 || line>0);
            px = (buffer(1)*(x>1)+buffer(w)*(line>0))/2;
            break;
          }
          case 4: {
            buffer.Add((U8)( B + Paeth(buffer(1)*(x>2 || !x), buffer(w)*(line>0), buffer(w+1)*(line>0 && (x>2 || !x))) ) );
            filterOn = (x>1 || line>0);
            px = Paeth(buffer(1)*(x>1),buffer(w)*(line>0),buffer(w+1)*(x>1 && line>0));
            break;
          }
          default: buffer.Add(B);
            filterOn = false;
            px = 0;
        }
        if (!filterOn) px=0;
      }
    }
    else {
      buffer.Add((U8)c4);
      if (x==0) {
        memset(&jumps[0], 0, sizeof(short)*jumps.size());
        if (line>0 && w>8) {
          U8 bMask = 0xFF-((1<<gray)-1);
          U32 pMask = bMask*0x01010101u;
          U32 left=0, right=0;
          int l=min(w, (int)jumps.size()), end=l-4;
          do {
            left = ((buffer(l-x)<<24)|(buffer(l-x-1)<<16)|(buffer(l-x-2)<<8)|buffer(l-x-3))&pMask;
            int i = end;
            while (i>=x+4) {
              right = ((buffer(l-i-3)<<24)|(buffer(l-i-2)<<16)|(buffer(l-i-1)<<8)|buffer(l-i))&pMask;
              if (left==right) {
                int j=(i+3-x-1)/2, k=0;
                for (; k<=j; k++) {
                  if (k<4 || (buffer(l-x-k)&bMask)==(buffer(l-i-3+k)&bMask)) {
                    jumps[x+k] = -(x+(l-i-3)+2*k);
                    jumps[i+3-k] = i+3-x-2*k;
                  }
                  else
                    break;
                }
                x+=k;
                end-=k;
                break;
              }
              i--;
            }
            x++;
            if (x>end)
              break;
          } while (x+4<l);
          x = 0;
        }
      }
    }

    if (x || !isPNG){
      int i= filterOn ? (filter+1)<<6 : 0;
      column[0]=(x-isPNG)/columns[0];
      column[1]=(x-isPNG)/columns[1];
      WWW=buffer(3), WW=buffer(2), W=buffer(1), NWW=buffer(w+2), NW=buffer(w+1), N=buffer(w), NE=buffer(w-1), NEE=buffer(w-2), NNWW=buffer(w*2+2), NNW=buffer(w*2+1), NN=buffer(w*2), NNE=buffer(w*2-1), NNEE=buffer(w*2-2), NNN=buffer(w*3);

      jump = jumps[min(x,(int)jumps.size()-1)];
      cm.set(hash(++i, (jump!=0)?(0x100|buffer(abs(jump)))*(1-2*(jump<0)):N, line&3));
      if (!gray){
        cm.set(hash(++i, W, px));
        cm.set(hash(++i, W, px, column[0]));
        cm.set(hash(++i, N, px));
        cm.set(hash(++i, N, px, column[0]));
        cm.set(hash(++i, NW, px));
        cm.set(hash(++i, NW, px, column[0]));
        cm.set(hash(++i, NE, px));
        cm.set(hash(++i, NE, px, column[0]));
        cm.set(hash(++i, NWW, px));
        cm.set(hash(++i, NEE, px));
        cm.set(hash(++i, WW, px));
        cm.set(hash(++i, NN, px));
        cm.set(hash(++i, W, N, px));
        cm.set(hash(++i, W, NW, px));
        cm.set(hash(++i, W, NE, px));
        cm.set(hash(++i, W, NEE, px));
        cm.set(hash(++i, W, NWW, px));
        cm.set(hash(++i, N, NW, px));
        cm.set(hash(++i, N, NE, px));
        cm.set(hash(++i, NW, NE, px));
        cm.set(hash(++i, W, WW, px));
        cm.set(hash(++i, N, NN, px));
        cm.set(hash(++i, NW, NNWW, px));
        cm.set(hash(++i, NE, NNEE, px));
        cm.set(hash(++i, NW, NWW, px));
        cm.set(hash(++i, NW, NNW, px));
        cm.set(hash(++i, NE, NEE, px));
        cm.set(hash(++i, NE, NNE, px));
        cm.set(hash(++i, N, NNW, px));
        cm.set(hash(++i, N, NNE, px));
        cm.set(hash(++i, N, NNN, px));
        cm.set(hash(++i, W, WWW, px));
        cm.set(hash(++i, WW, NEE, px));
        cm.set(hash(++i, WW, NN, px));
        cm.set(hash(++i, W, buffer(w-3), px));
        cm.set(hash(++i, W, buffer(w-4), px));
        cm.set(hash(++i, W, hash(N,NW)&0x7FF, px));
        cm.set(hash(++i, N, hash(NN,NNN)&0x7FF, px));
        cm.set(hash(++i, W, hash(NE,NEE)&0x7FF, px));
        cm.set(hash(++i, W, hash(NW,N,NE)&0x7FF, px));
        cm.set(hash(++i, N, hash(NE,NN,NNE)&0x7FF, px));
        cm.set(hash(++i, N, hash(NW,NNW,NN)&0x7FF, px));
        cm.set(hash(++i, W, hash(WW,NWW,NW)&0x7FF, px));
        cm.set(hash(++i, W, hash(NW,N)&0xFF, hash(WW,NWW)&0xFF, px));
        cm.set(hash(++i, px, column[0]));
        cm.set(hash(++i, px));
        cm.set(hash(++i, N, px, column[1] ));
        cm.set(hash(++i, W, px, column[1] ));

        ctx = min(0x1F,(x-1)/min(0x20,columns[0]));
        res = W;
      }
      else{
        cm.set(hash(++i, N, px));
        cm.set(hash(++i, N-px));
        cm.set(hash(++i, W, px));
        cm.set(hash(++i, NW, px));
        cm.set(hash(++i, NE, px));
        cm.set(hash(++i, N, NN, px));
        cm.set(hash(++i, W, WW, px));
        cm.set(hash(++i, NE, NNEE, px ));
        cm.set(hash(++i, NW, NNWW, px ));
        cm.set(hash(++i, W, NEE, px));
        cm.set(hash(++i, (Clamp4(W+N-NW,W,NW,N,NE)-px)/2, LogMeanDiffQt(Clip(N+NE-NNE), Clip(N+NW-NNW))));
        cm.set(hash(++i, (W-px)/4, (NE-px)/4, column[0]));
        cm.set(hash(++i, (Clip(W*2-WW)-px)/4, (Clip(N*2-NN)-px)/4));
        cm.set(hash(++i, (Clamp4(N+NE-NNE,W,N,NE,NEE)-px)/4, column[0]));
        cm.set(hash(++i, (Clamp4(N+NW-NNW,W,NW,N,NE)-px)/4, column[0]));
        cm.set(hash(++i, (W+NEE)/4, px, column[0]));
        cm.set(hash(++i, Clip(W+N-NW)-px, column[0]));
        cm.set(hash(++i, Clamp4(N*3-NN*3+NNN,W,N,NN,NE), px, LogMeanDiffQt(W,Clip(NW*2-NNW))));
        cm.set(hash(++i, Clamp4(W*3-WW*3+WWW,W,N,NE,NEE), px, LogMeanDiffQt(N,Clip(NW*2-NWW))));
        cm.set(hash(++i, (W+Clamp4(NE*3-NNE*3+(isPNG?buffer(w*3-1):buf(w*3-1)),W,N,NE,NEE))/2, px, LogMeanDiffQt(N,(NW+NE)/2)));
        cm.set(hash(++i, (N+NNN)/8, Clip(N*3-NN*3+NNN)/4, px));
        cm.set(hash(++i, (W+WWW)/8, Clip(W*3-WW*3+WWW)/4, px));
        cm.set(hash(++i, Clip((-buffer(4)+5*WWW-10*WW+10*W+Clamp4(NE*4-NNE*6+buffer(w*3-1)*4-buffer(w*4-1),N,NE,buffer(w-2),buffer(w-3)))/5)-px));
        cm.set(hash(++i, Clip(N*2-NN)-px, LogMeanDiffQt(N,Clip(NN*2-NNN))));
        cm.set(hash(++i, Clip(W*2-WW)-px, LogMeanDiffQt(NE,Clip(N*2-NW))));
        cm.set(~0xde7ec7ed);

        i=0;
        Map[i++].set(((U8)(Clip(W+N-NW)-px))|(LogMeanDiffQt(Clip(N+NE-NNE),Clip(N+NW-NNW))<<8));
        Map[i++].set(Clamp4(W+N-NW,W,NW,N,NE)-px);
        Map[i++].set(Clip(W+N-NW)-px);
        Map[i++].set(Clamp4(W+NE-N,W,NW,N,NE)-px);
        Map[i++].set(Clip(W+NE-N)-px);
        Map[i++].set(Clamp4(N+NW-NNW,W,NW,N,NE)-px);
        Map[i++].set(Clip(N+NW-NNW)-px);
        Map[i++].set(Clamp4(N+NE-NNE,W,N,NE,NEE)-px);
        Map[i++].set(Clip(N+NE-NNE)-px);
        Map[i++].set((W+NEE)/2-px);
        Map[i++].set(Clip(N*3-NN*3+NNN)-px);
        Map[i++].set(Clip(W*3-WW*3+WWW)-px);
        Map[i++].set((W+Clip(NE*3-NNE*3+buffer(w*3-1)))/2-px);
        Map[i++].set((W+Clip(NEE*3-buffer(w*2-3)*3+buffer(w*3-4)))/2-px);
        Map[i++].set(Clip(NN+buffer(w*4)-buffer(w*6))-px);
        Map[i++].set(Clip(WW+buffer(4)-buffer(6))-px);
        Map[i++].set(Clip((buffer(w*5)-6*buffer(w*4)+15*NNN-20*NN+15*N+Clamp4(W*2-NWW,W,NW,N,NN))/6)-px);
        Map[i++].set(Clip((-3*WW+8*W+Clamp4(NEE*3-NNEE*3+buffer(w*3-2),NE,NEE,buffer(w-3),buffer(w-4)))/6)-px);
        Map[i++].set(Clip(NN+NW-buffer(w*3+1))-px);
        Map[i++].set(Clip(NN+NE-buffer(w*3-1))-px);
        Map[i++].set(Clip((W*2+NW)-(WW+2*NWW)+buffer(w+3))-px);
        Map[i++].set(Clip(((NW+NWW)/2)*3-buffer(w*2+3)*3+(buffer(w*3+4)+buffer(w*3+5))/2)-px);
        Map[i++].set(Clip(NEE+NE-buffer(w*2-3))-px);
        Map[i++].set(Clip(NWW+WW-buffer(w+4))-px);
        Map[i++].set(Clip(((W+NW)*3-NWW*6+buffer(w+3)+buffer(w*2+3))/2)-px);
        Map[i++].set(Clip((NE*2+NNE)-(NNEE+buffer(w*3-2)*2)+buffer(w*4-3))-px);
        Map[i++].set(buffer(w*6)-px);
        Map[i++].set((buffer(w-4)+buffer(w-6))/2-px);
        Map[i++].set((buffer(4)+buffer(6))/2-px);
        Map[i++].set((W+N+buffer(w-5)+buffer(w-7))/4-px);
        Map[i++].set(Clip(buffer(w-3)+W-NEE)-px);
        Map[i++].set(Clip(4*NNN-3*buffer(w*4))-px);
        Map[i++].set(Clip(N+NN-NNN)-px);
        Map[i++].set(Clip(W+WW-WWW)-px);
        Map[i++].set(Clip(W+NEE-NE)-px);
        Map[i++].set(Clip(WW+NEE-N)-px);
        Map[i++].set((Clip(W*2-NW)+Clip(W*2-NWW)+N+NE)/4-px);
        Map[i++].set(Clamp4(N*2-NN,W,N,NE,NEE)-px);
        Map[i++].set((N+NNN)/2-px);
        Map[i++].set(Clip(NN+W-NNW)-px);
        Map[i++].set(Clip(NWW+N-NNWW)-px);
        Map[i++].set(Clip((4*WWW-15*WW+20*W+Clip(NEE*2-NNEE))/10)-px);
        Map[i++].set(Clip((buffer(w*3-3)-4*NNEE+6*NE+Clip(W*3-NW*3+NNW))/4)-px);
        Map[i++].set(Clip((N*2+NE)-(NN+2*NNE)+buffer(w*3-1))-px);
        Map[i++].set(Clip((NW*2+NNW)-(NNWW+buffer(w*3+2)*2)+buffer(w*4+3))-px);
        Map[i++].set(Clip(NNWW+W-buffer(w*2+3))-px);
        Map[i++].set(Clip((-buffer(w*4)+5*NNN-10*NN+10*N+Clip(W*4-NWW*6+buffer(w*2+3)*4-buffer(w*3+4)))/5)-px);
        Map[i++].set(Clip(NEE+Clip(buffer(w-3)*2-buffer(w*2-4))-buffer(w-4))-px);
        Map[i++].set(Clip(NW+W-NWW)-px);
        Map[i++].set(Clip((N*2+NW)-(NN+2*NNW)+buffer(w*3+1))-px);
        Map[i++].set(Clip(NN+Clip(NEE*2-buffer(w*2-3))-NNE)-px);
        Map[i++].set(Clip((-buffer(4)+5*WWW-10*WW+10*W+Clip(NE*2-NNE))/5)-px);
        Map[i++].set(Clip((-buffer(5)+4*buffer(4)-5*WWW+5*W+Clip(NE*2-NNE))/4)-px);
        Map[i++].set(Clip((WWW-4*WW+6*W+Clip(NE*3-NNE*3+buffer(w*3-1)))/4)-px);
        Map[i++].set(Clip((-NNEE+3*NE+Clip(W*4-NW*6+NNW*4-buffer(w*3+1)))/3)-px);
        Map[i++].set(((W+N)*3-NW*2)/4-px);

        if (isPNG)
          ctx = ((abs(W-N)>8)<<10)|((W>N)<<9)|((abs(N-NW)>8)<<8)|((N>NW)<<7)|((abs(N-NE)>8)<<6)|((N>NE)<<5)|((W>WW)<<4)|((N>NN)<<3)|min(5,filterOn?filter+1:0);
        else
          ctx = min(0x1F,x/max(1,w/min(32,columns[0])))|( ( ((abs(W-N)*16>W+N)<<1)|(abs(N-NW)>8) )<<5 )|((W+N)&0x180);

        res = Clamp4(W+N-NW,W,NW,N,NE)-px;
      }
    }
  }

  // Predict next bit
  if (x || !isPNG){
    cm.mix(m);
    if (gray){
      for (int i=0;i<nMaps;i++)
        Map[i].mix(m);
    }
    col=(col+1)&7;
    m.set(5+ctx, 2048+5);
    m.set(col*2+(isPNG && c0==((0x100|res)>>(8-bpos))) + min(5, filterOn?filter+1:0)*16, 6*16);
    m.set(((isPNG?px:N+W)>>4) + min(5, filterOn?filter+1:0)*32, 6*32);
    m.set(c0, 256);
    m.set( ((abs((int)(W-N))>4)<<9)|((abs((int)(N-NE))>4)<<8)|((abs((int)(W-NW))>4)<<7)|((W>N)<<6)|((N>NE)<<5)|((W>NW)<<4)|((W>WW)<<3)|((N>NN)<<2)|((NW>NNWW)<<1)|(NE>NNEE), 1024 );
    m.set(min(63,column[0]), 64);
    m.set(min(127,column[1]), 128);
  }
  else{
    m.add( -2048+((filter>>(7-bpos))&1)*4096 );
    m.set(min(4,filter),5);
  }
}

//////////////////////////// im4bitModel /////////////////////////////////
/*
  Model for 4-bit image data

  Changelog:
  (31/08/2017) v103: Initial release by Márcio Pais
*/
void im4bitModel(Mixer& m, int w) {
  static HashTable<16> t(MEM/2);
  const int S=11; // number of contexts
  static U8* cp[S]; // context pointers
  static StateMap sm[S];
  static U8 WW=0, W=0, NWW=0, NW=0, N=0, NE=0, NEE=0, NNWW = 0, NNW=0, NN=0, NNE=0, NNEE=0;
  static int col=0, line=0, run=0, prevColor=0, px=0;
  int i;
  if (!cp[0]){
    for (i=0;i<S;i++)
      cp[i]=t[263*i]; //set the initial context to an arbitrary slot in the hashtable
  }
  for (i=0;i<S;i++)
    *cp[i]=nex(*cp[i],y); //update hashtable item priorities using predicted counts

  if (bpos==0 || bpos==4){
      WW=W; NWW=NW; NW=N; N=NE; NE=NEE; NNWW=NWW; NNW=NN; NN=NNE; NNE=NNEE;
      if (bpos==0)
        {W=c4&0xF; NEE=buf(w-1)>>4; NNEE=buf(w*2-1)>>4;}
      else
        {W=c0&0xF; NEE=buf(w-1)&0xF; NNEE=buf(w*2-1)&0xF;}
      run=(W!=WW || col==0)?(prevColor=WW,0):min(0xFFF,run+1);
      px=1; //partial pixel (4 bits) with a leading "1"
      i=0;

      cp[i++]=t[hash(W,NW,N)];
      cp[i++]=t[hash(N, min(0xFFF, col/8))];
      cp[i++]=t[hash(W,NW,N,NN,NE)];
      cp[i++]=t[hash(W, N, NE+NNE*16, NEE+NNEE*16)];
      cp[i++]=t[hash(W, N, NW+NNW*16, NWW+NNWW*16)];
      cp[i++]=t[hash(W, ilog2(run+1), prevColor, col/max(1,w/2) )];
      cp[i++]=t[hash(NE, min(0x3FF, (col+line)/max(1,w*8)))];
      cp[i++]=t[hash(NW, (col-line)/max(1,w*8))];
      cp[i++]=t[hash(WW*16+W,NN*16+N,NNWW*16+NW)];
      cp[i++]=t[N+NN*16];
      cp[i++]=t[-1];
      assert(i==S);

      col++;
      if(col==w*2){col=0;line++;}
  }
  else{
    px+=px+y;
    int j=(y+1)<<(bpos&3);
    for (i=0;i<S;i++)
      cp[i]+=j;
  }

  // predict
  for (i=0; i<S; i++)
    m.add(stretch(sm[i].p(*cp[i])));

  m.set((W<<4) | px, 256);
  m.set(min(31,col/max(1,w/16)) | (N<<5), 512);
  m.set((bpos&3) | (W<<2) | (min(7,ilog2(run+1))<<6), 512);
  m.set(W | (NE<<4) | ((bpos&3)<<8), 1024);
  m.set(px, 16);
  m.set(0,1);
}

//////////////////////////// im1bitModel /////////////////////////////////

// Model for 1-bit image data

void im1bitModel(Mixer& m, int w) {
  static U32 r0, r1, r2, r3;  // last 4 rows, bit 8 is over current pixel
  static Array<U8> t(0x23000);  // model: cxt -> state
  const int N=11;  // number of contexts
  static int cxt[N];  // contexts
  static StateMap sm[N];

  // update the model
  int i;
  for (i=0; i<N; ++i)
    t[cxt[i]]=nex(t[cxt[i]],y);

  // update the contexts (pixels surrounding the predicted one)
  r0+=r0+y;
  r1+=r1+((buf(w-1)>>(7-bpos))&1);
  r2+=r2+((buf(w+w-1)>>(7-bpos))&1);
  r3+=r3+((buf(w+w+w-1)>>(7-bpos))&1);
  cxt[0]=(r0&0x7)|(r1>>4&0x38)|(r2>>3&0xc0);
  cxt[1]=0x100+((r0&1)|(r1>>4&0x3e)|(r2>>2&0x40)|(r3>>1&0x80));
  cxt[2]=0x200+((r0&1)|(r1>>4&0x1d)|(r2>>1&0x60)|(r3&0xC0));
  cxt[3]=0x300+(y|((r0<<1)&4)|((r1>>1)&0xF0)|((r2>>3)&0xA));
  cxt[4]=0x400+((r0>>4&0x2AC)|(r1&0xA4)|(r2&0x349)|(!(r3&0x14D)));
  cxt[5]=0x800+(y|((r1>>4)&0xE)|((r2>>1)&0x70)|((r3<<2)&0x380));
  cxt[6]=0xC00+(((r1&0x30)^(r3&0x0c0c))|(r0&3));
  cxt[7]=0x1000+((!(r0&0x444))|(r1&0xC0C)|(r2&0xAE3)|(r3&0x51C));
  cxt[8]=0x2000+((r0&7)|((r1>>1)&0x3F8)|((r2<<5)&0xC00));
  cxt[9]=0x3000+((r0&0x3f)^(r1&0x3ffe)^(r2<<2&0x7f00)^(r3<<5&0xf800));
  cxt[10]=0x13000+((r0&0x3e)^(r1&0x0c0c)^(r2&0xc800));

  // predict
  for (i=0; i<N; ++i) m.add(stretch(sm[i].p(t[cxt[i]])));
}

//////////////////////////// jpegModel /////////////////////////

// Model JPEG. Return 1 if a JPEG file is detected or else 0.
// Only the baseline and 8 bit extended Huffman coded DCT modes are
// supported.  The model partially decodes the JPEG image to provide
// context for the Huffman coded symbols.

// Print a JPEG segment at buf[p...] for debugging
/*
void dump(const char* msg, int p) {
  printf("%s:", msg);
  int len=buf[p+2]*256+buf[p+3];
  for (int i=0; i<len+2; ++i)
    printf(" %02X", buf[p+i]);
  printf("\n");
}
*/

#define finish(success){ \
  int length = pos - images[idx].offset; \
  /*if (success && idx && pos-lastPos==1)*/ \
    /*printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\bEmbedded JPEG at offset %d, size: %d bytes, level %d\nCompressing... ", images[idx].offset-pos+blpos, length, idx), fflush(stdout);*/ \
  memset(&images[idx], 0, sizeof(JPEGImage)); \
  mcusize=0,dqt_state=-1; \
  idx-=(idx>0); \
  images[idx].app-=length; \
  if (images[idx].app < 0) \
    images[idx].app = 0; \
}

// Detect invalid JPEG data.  The proper response is to silently
// fall back to a non-JPEG model.
#define jassert(x) if (!(x)) { \
/*  printf("JPEG error at %d, line %d: %s\n", pos, __LINE__, #x); */ \
  if (idx>0) \
    finish(false) \
  else \
    images[idx].jpeg=0; \
  return images[idx].next_jpeg;}

struct HUF {U32 min, max; int val;}; // Huffman decode tables
  // huf[Tc][Th][m] is the minimum, maximum+1, and pointer to codes for
  // coefficient type Tc (0=DC, 1=AC), table Th (0-3), length m+1 (m=0-15)

struct JPEGImage{
  int offset, // offset of SOI marker
  jpeg, // 1 if JPEG is header detected, 2 if image data
  next_jpeg, // updated with jpeg on next byte boundary
  app, // Bytes remaining to skip in this marker
  sof, sos, data, // pointers to buf
  htsize; // number of pointers in ht
  int ht[8]; // pointers to Huffman table headers
  U8 qtab[256]; // table
  int qmap[10]; // block -> table number
};

class JpegModel {
private:
    // State of parser
    enum {SOF0=0xc0, SOF1, SOF2, SOF3, DHT, RST0=0xd0, SOI=0xd8, EOI, SOS, DQT,
      DNL, DRI, APP0=0xe0, COM=0xfe, FF};  // Second byte of 2 byte codes
    static const int MaxEmbeddedLevel = 3;
    JPEGImage images[MaxEmbeddedLevel]{};
    int idx=-1;
    int lastPos=0;

    // Huffman decode state
    U32 huffcode=0;  // Current Huffman code including extra bits
    int huffbits=0;  // Number of valid bits in huffcode
    int huffsize=0;  // Number of bits without extra bits
    int rs=-1;  // Decoded huffcode without extra bits.  It represents
      // 2 packed 4-bit numbers, r=run of zeros, s=number of extra bits for
      // first nonzero code.  huffcode is complete when rs >= 0.
      // rs is -1 prior to decoding incomplete huffcode.

    int mcupos=0;  // position in MCU (0-639).  The low 6 bits mark
      // the coefficient in zigzag scan order (0=DC, 1-63=AC).  The high
      // bits mark the block within the MCU, used to select Huffman tables.

    // Decoding tables
    Array<HUF> huf{128};  // Tc*64+Th*16+m -> min, max, val
    int mcusize=0;  // number of coefficients in an MCU
    int hufsel[2][10]{{0}};  // DC/AC, mcupos/64 -> huf decode table
    Array<U8> hbuf{2048};  // Tc*1024+Th*256+hufcode -> RS

    // Image state
    Array<int> color{10};  // block -> component (0-3)
    Array<int> pred{4};  // component -> last DC value
    int dc=0;  // DC value of the current block
    int width=0;  // Image width in MCU
    int row=0, column=0;  // in MCU (column 0 to width-1)
    Buf cbuf{0x20000}; // Rotating buffer of coefficients, coded as:
      // DC: level shifted absolute value, low 4 bits discarded, i.e.
      //   [-1023...1024] -> [0...255].
      // AC: as an RS code: a run of R (0-15) zeros followed by an S (0-15)
      //   bit number, or 00 for end of block (in zigzag order).
      //   However if R=0, then the format is ssss11xx where ssss is S,
      //   xx is the first 2 extra bits, and the last 2 bits are 1 (since
      //   this never occurs in a valid RS code).
    int cpos=0;  // position in cbuf
    int rs1;  // last RS code
    int rstpos=0,rstlen=0; // reset position
    int ssum=0, ssum1=0, ssum2=0, ssum3=0;
      // sum of S in RS codes in block and sum of S in first component

    IntBuf cbuf2{0x20000};
    Array<int> adv_pred{4}, sumu{8}, sumv{8}, run_pred{6};
    int prev_coef=0, prev_coef2=0, prev_coef_rs=0;
    Array<int> ls{10};  // block -> distance to previous block
    Array<int> blockW{10}, blockN{10}, SamplingFactors{4};
    Array<int> lcp{7}, zpos{64};

    //for parsing Quantization tables
    int dqt_state = -1, dqt_end = 0, qnum = 0;

    // Context model
    static const int N=32; // size of t, number of contexts
    BH<9> t; // context hash -> bit history
      // As a cache optimization, the context does not include the last 1-2
      // bits of huffcode if the length (huffbits) is not a multiple of 3.
      // The 7 mapped values are for context+{"", 0, 00, 01, 1, 10, 11}.
    Array<U32> cxt{N};  // context hashes
    Array<U8*> cp{N};  // context pointers
    StateMap sm[N]{};
    Mixer *m1;
    APM<24> a1{0x8000}, a2{0x20000};

public:
  JpegModel(): t(MEM) {
    m1=MixerFactory::CreateMixer(N+1, 2050, 3);
  }
  ~JpegModel() {
    delete m1;
  }

  int jpegModel(Mixer& m) {

    const static U8 zzu[64]={  // zigzag coef -> u,v
      0,1,0,0,1,2,3,2,1,0,0,1,2,3,4,5,4,3,2,1,0,0,1,2,3,4,5,6,7,6,5,4,
      3,2,1,0,1,2,3,4,5,6,7,7,6,5,4,3,2,3,4,5,6,7,7,6,5,4,5,6,7,7,6,7};
    const static U8 zzv[64]={
      0,0,1,2,1,0,0,1,2,3,4,3,2,1,0,0,1,2,3,4,5,6,5,4,3,2,1,0,0,1,2,3,
      4,5,6,7,7,6,5,4,3,2,1,2,3,4,5,6,7,7,6,5,4,3,4,5,6,7,7,6,5,6,7,7};

    // Standard Huffman tables (cf. JPEG standard section K.3)
    // IMPORTANT: these are only valid for 8-bit data precision
    const static U8 bits_dc_luminance[16] = {
      0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0
    };
    const static U8 values_dc_luminance[12] = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
    };

    const static U8 bits_dc_chrominance[16] = {
      0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0
    };
    const static U8 values_dc_chrominance[12] = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
    };

    const static U8 bits_ac_luminance[16] = {
      0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d
    };
    const static U8 values_ac_luminance[162] = {
      0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
      0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
      0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
      0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
      0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
      0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
      0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
      0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
      0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
      0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
      0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
      0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
      0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
      0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
      0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
      0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
      0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
      0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
      0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
      0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
      0xf9, 0xfa
    };

    const static U8 bits_ac_chrominance[16] = {
      0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77
    };
    const static U8 values_ac_chrominance[162] = {
      0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
      0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
      0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
      0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
      0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
      0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
      0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
      0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
      0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
      0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
      0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
      0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
      0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
      0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
      0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
      0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
      0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
      0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
      0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
      0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
      0xf9, 0xfa
    };

    if (idx < 0){
      memset(&images[0], 0, sizeof(images));
      idx = 0;
      lastPos = pos;
    }

    // Be sure to quit on a byte boundary
    if (bpos==0) images[idx].next_jpeg=images[idx].jpeg>1;
    if (bpos!=0 && !images[idx].jpeg) return images[idx].next_jpeg;
    if (bpos==0 && images[idx].app>0){
      --images[idx].app;
      if (idx<MaxEmbeddedLevel && buf(4)==FF && buf(3)==SOI && buf(2)==FF && ((buf(1)&0xFE)==0xC0 || buf(1)==0xC4 || (buf(1)>=0xDB && buf(1)<=0xFE)) )
        memset(&images[++idx], 0, sizeof(JPEGImage));
    }
    if (images[idx].app>0) return images[idx].next_jpeg;
    if (bpos==0) {

      // Parse.  Baseline DCT-Huffman JPEG syntax is:
      // SOI APPx... misc... SOF0 DHT... SOS data EOI
      // SOI (= FF D8) start of image.
      // APPx (= FF Ex) len ... where len is always a 2 byte big-endian length
      //   including the length itself but not the 2 byte preceding code.
      //   Application data is ignored.  There may be more than one APPx.
      // misc codes are DQT, DNL, DRI, COM (ignored).
      // SOF0 (= FF C0) len 08 height width Nf [C HV Tq]...
      //   where len, height, width (in pixels) are 2 bytes, Nf is the repeat
      //   count (1 byte) of [C HV Tq], where C is a component identifier
      //   (color, 0-3), HV is the horizontal and vertical dimensions
      //   of the MCU (high, low bits, packed), and Tq is the quantization
      //   table ID (not used).  An MCU (minimum compression unit) consists
      //   of 64*H*V DCT coefficients for each color.
      // DHT (= FF C4) len [TcTh L1...L16 V1,1..V1,L1 ... V16,1..V16,L16]...
      //   defines Huffman table Th (1-4) for Tc (0=DC (first coefficient)
      //   1=AC (next 63 coefficients)).  L1..L16 are the number of codes
      //   of length 1-16 (in ascending order) and Vx,y are the 8-bit values.
      //   A V code of RS means a run of R (0-15) zeros followed by S (0-15)
      //   additional bits to specify the next nonzero value, negative if
      //   the first additional bit is 0 (e.g. code x63 followed by the
      //   3 bits 1,0,1 specify 7 coefficients: 0, 0, 0, 0, 0, 0, 5.
      //   Code 00 means end of block (remainder of 63 AC coefficients is 0).
      // SOS (= FF DA) len Ns [Cs TdTa]... 0 3F 00
      //   Start of scan.  TdTa specifies DC/AC Huffman tables (0-3, packed
      //   into one byte) for component Cs matching C in SOF0, repeated
      //   Ns (1-4) times.
      // EOI (= FF D9) is end of image.
      // Huffman coded data is between SOI and EOI.  Codes may be embedded:
      // RST0-RST7 (= FF D0 to FF D7) mark the start of an independently
      //   compressed region.
      // DNL (= FF DC) 04 00 height
      //   might appear at the end of the scan (ignored).
      // FF 00 is interpreted as FF (to distinguish from RSTx, DNL, EOI).

      // Detect JPEG (SOI followed by a valid marker)
      if (!images[idx].jpeg && buf(4)==FF && buf(3)==SOI && buf(2)==FF && ((buf(1)&0xFE)==0xC0 || buf(1)==0xC4 || (buf(1)>=0xDB && buf(1)<=0xFE)) ){
        images[idx].jpeg=1;
        images[idx].offset = pos-4;
        images[idx].sos=images[idx].sof=images[idx].htsize=images[idx].data=0, images[idx].app=(buf(1)>>4==0xE)*2;
        mcusize=huffcode=huffbits=huffsize=mcupos=cpos=0, rs=-1;
        memset(&huf[0], 0, sizeof(huf));
        memset(&pred[0], 0, pred.size()*sizeof(int));
        rstpos=rstlen=0;
      }

      // Detect end of JPEG when data contains a marker other than RSTx
      // or byte stuff (00), or if we jumped in position since the last byte seen
      if (images[idx].jpeg && images[idx].data && ((buf(2)==FF && buf(1) && (buf(1)&0xf8)!=RST0) || (pos-lastPos>1)) ) {
        jassert((buf(1)==EOI) || (pos-lastPos>1));
        finish(true);
      }
      lastPos = pos;
      if (!images[idx].jpeg) return images[idx].next_jpeg;

      // Detect APPx, COM or other markers, so we can skip them
      if (!images[idx].data && !images[idx].app && buf(4)==FF && (((buf(3)>0xC1) && (buf(3)<=0xCF) && (buf(3)!=DHT)) || ((buf(3)>=0xDC) && (buf(3)<=0xFE)))){
        images[idx].app=buf(2)*256+buf(1)+2;
        if (idx>0)
          jassert( pos + images[idx].app < images[idx].offset + images[idx-1].app );
      }

      // Save pointers to sof, ht, sos, data,
      if (buf(5)==FF && buf(4)==SOS) {
        int len=buf(3)*256+buf(2);
        if (len==6+2*buf(1) && buf(1) && buf(1)<=4)  // buf(1) is Ns
          images[idx].sos=pos-5, images[idx].data=images[idx].sos+len+2, images[idx].jpeg=2;
      }
      if (buf(4)==FF && buf(3)==DHT && images[idx].htsize<8) images[idx].ht[images[idx].htsize++]=pos-4;
      if (buf(4)==FF && (buf(3)&0xFE)==SOF0) images[idx].sof=pos-4;

      // Parse Quantizazion tables
      if (buf(4)==FF && buf(3)==DQT)
        dqt_end=pos+buf(2)*256+buf(1)-1, dqt_state=0;
      else if (dqt_state>=0) {
        if (pos>=dqt_end)
          dqt_state = -1;
        else {
          if (dqt_state%65==0)
            qnum = buf(1);
          else {
            jassert(buf(1)>0);
            jassert(qnum>=0 && qnum<4);
            images[idx].qtab[qnum*64+((dqt_state%65)-1)]=buf(1)-1;
          }
          dqt_state++;
        }
      }

      // Restart
      if (buf(2)==FF && (buf(1)&0xf8)==RST0) {
        huffcode=huffbits=huffsize=mcupos=0, rs=-1;
        memset(&pred[0], 0, pred.size()*sizeof(int));
        rstlen=column+row*width-rstpos;
        rstpos=column+row*width;
      }
    }

    {
      // Build Huffman tables
      // huf[Tc][Th][m] = min, max+1 codes of length m, pointer to byte values
      if (pos==images[idx].data && bpos==1) {
        for (int i=0; i<images[idx].htsize; ++i) {
          int p=images[idx].ht[i]+4;  // pointer to current table after length field
          int end=p+buf[p-2]*256+buf[p-1]-2;  // end of Huffman table
          int count=0;  // sanity check
          while (p<end && end<pos && end<p+2100 && ++count<10) {
            int tc=buf[p]>>4, th=buf[p]&15;
            if (tc>=2 || th>=4) break;
            jassert(tc>=0 && tc<2 && th>=0 && th<4);
            HUF* h=&huf[tc*64+th*16]; // [tc][th][0];
            int val=p+17;  // pointer to values
            int hval=tc*1024+th*256;  // pointer to RS values in hbuf
            int j;
            for (j=0; j<256; ++j) // copy RS codes
              hbuf[hval+j]=buf[val+j];
            int code=0;
            for (j=0; j<16; ++j) {
              h[j].min=code;
              h[j].max=code+=buf[p+j+1];
              h[j].val=hval;
              val+=buf[p+j+1];
              hval+=buf[p+j+1];
              code*=2;
            }
            p=val;
            jassert(hval>=0 && hval<2048);
          }
          jassert(p==end);
        }
        huffcode=huffbits=huffsize=0, rs=-1;

        // load default tables
        if (!images[idx].htsize){
          for (int tc = 0; tc < 2; tc++) {
            for (int th = 0; th < 2; th++) {
              HUF* h = &huf[tc*64+th*16];
              int hval = tc*1024 + th*256;
              int code = 0, c = 0, x = 0;

              for (int i = 0; i < 16; i++) {
                switch (tc*2+th) {
                  case 0: x = bits_dc_luminance[i]; break;
                  case 1: x = bits_dc_chrominance[i]; break;
                  case 2: x = bits_ac_luminance[i]; break;
                  case 3: x = bits_ac_chrominance[i];
                }

                h[i].min = code;
                h[i].max = (code+=x);
                h[i].val = hval;
                hval+=x;
                code+=code;
                c+=x;
              }

              hval = tc*1024 + th*256;
              c--;

              while (c >= 0){
                switch (tc*2+th) {
                  case 0: x = values_dc_luminance[c]; break;
                  case 1: x = values_dc_chrominance[c]; break;
                  case 2: x = values_ac_luminance[c]; break;
                  case 3: x = values_ac_chrominance[c];
                }

                hbuf[hval+c] = x;
                c--;
              }
            }
          }
          images[idx].htsize = 4;
        }

        // Build Huffman table selection table (indexed by mcupos).
        // Get image width.
        if (!images[idx].sof && images[idx].sos) return images[idx].next_jpeg;
        int ns=buf[images[idx].sos+4];
        int nf=buf[images[idx].sof+9];
        jassert(ns<=4 && nf<=4);
        mcusize=0;  // blocks per MCU
        int hmax=0;  // MCU horizontal dimension
        for (int i=0; i<ns; ++i) {
          for (int j=0; j<nf; ++j) {
            if (buf[images[idx].sos+2*i+5]==buf[images[idx].sof+3*j+10]) { // Cs == C ?
              int hv=buf[images[idx].sof+3*j+11];  // packed dimensions H x V
              SamplingFactors[j] = hv;
              if (hv>>4>hmax) hmax=hv>>4;
              hv=(hv&15)*(hv>>4);  // number of blocks in component C
              jassert(hv>=1 && hv+mcusize<=10);
              while (hv) {
                jassert(mcusize<10);
                hufsel[0][mcusize]=buf[images[idx].sos+2*i+6]>>4&15;
                hufsel[1][mcusize]=buf[images[idx].sos+2*i+6]&15;
                jassert (hufsel[0][mcusize]<4 && hufsel[1][mcusize]<4);
                color[mcusize]=i;
                int tq=buf[images[idx].sof+3*j+12];  // quantization table index (0..3)
                jassert(tq>=0 && tq<4);
                images[idx].qmap[mcusize]=tq; // quantizazion table mapping
                --hv;
                ++mcusize;
              }
            }
          }
        }
        jassert(hmax>=1 && hmax<=10);
        int j;
        for (j=0; j<mcusize; ++j) {
          ls[j]=0;
          for (int i=1; i<mcusize; ++i) if (color[(j+i)%mcusize]==color[j]) ls[j]=i;
          ls[j]=(mcusize-ls[j])<<6;
        }
        for (j=0; j<64; ++j) zpos[zzu[j]+8*zzv[j]]=j;
        width=buf[images[idx].sof+7]*256+buf[images[idx].sof+8];  // in pixels
        width=(width-1)/(hmax*8)+1;  // in MCU
        jassert(width>0);
        mcusize*=64;  // coefficients per MCU
        row=column=0;

        // we can have more blocks than components then we have subsampling
        int x=0, y=0;
        for (j = 0; j<(mcusize>>6); j++) {
          int i = color[j];
          int w = SamplingFactors[i]>>4, h = SamplingFactors[i]&0xf;
          blockW[j] = x==0?mcusize-64*(w-1):64;
          blockN[j] = y==0?mcusize*width-64*w*(h-1):w*64;
          x++;
          if (x>=w) { x=0; y++; }
          if (y>=h) { x=0; y=0; }
        }
      }
    }


    // Decode Huffman
    {
      if (mcusize && buf(1+(bpos==0))!=FF) {  // skip stuffed byte
        jassert(huffbits<=32);
        huffcode+=huffcode+y;
        ++huffbits;
        if (rs<0) {
          jassert(huffbits>=1 && huffbits<=16);
          const int ac=(mcupos&63)>0;
          jassert(mcupos>=0 && (mcupos>>6)<10);
          jassert(ac==0 || ac==1);
          const int sel=hufsel[ac][mcupos>>6];
          jassert(sel>=0 && sel<4);
          const int i=huffbits-1;
          jassert(i>=0 && i<16);
          const HUF *h=&huf[ac*64+sel*16]; // [ac][sel];
          jassert(h[i].min<=h[i].max && h[i].val<2048 && huffbits>0);
          if (huffcode<h[i].max) {
            jassert(huffcode>=h[i].min);
            int k=h[i].val+huffcode-h[i].min;
            jassert(k>=0 && k<2048);
            rs=hbuf[k];
            huffsize=huffbits;
          }
        }
        if (rs>=0) {
          if (huffsize+(rs&15)==huffbits) { // done decoding
            rs1=rs;
            int ex=0;  // decoded extra bits
            if (mcupos&63) {  // AC
              if (rs==0) { // EOB
                mcupos=(mcupos+63)&-64;
                jassert(mcupos>=0 && mcupos<=mcusize && mcupos<=640);
                while (cpos&63) {
                  cbuf2[cpos]=0;
                  cbuf[cpos]=(!rs)?0:(63-(cpos&63))<<4; cpos++; rs++;
                }
              }
              else {  // rs = r zeros + s extra bits for the next nonzero value
                      // If first extra bit is 0 then value is negative.
                jassert((rs&15)<=10);
                const int r=rs>>4;
                const int s=rs&15;
                jassert(mcupos>>6==(mcupos+r)>>6);
                mcupos+=r+1;
                ex=huffcode&((1<<s)-1);
                if (s!=0 && (ex>>(s-1))==0) ex-=(1<<s)-1;
                for (int i=r; i>=1; --i) {
                  cbuf2[cpos]=0;
                  cbuf[cpos++]=(i<<4)|s;
                }
                cbuf2[cpos]=ex;
                cbuf[cpos++]=(s<<4)|(huffcode<<2>>s&3)|12;
                ssum+=s;
              }
            }
            else {  // DC: rs = 0S, s<12
              jassert(rs<12);
              ++mcupos;
              ex=huffcode&((1<<rs)-1);
              if (rs!=0 && (ex>>(rs-1))==0) ex-=(1<<rs)-1;
              jassert(mcupos>=0 && mcupos>>6<10);
              const int comp=color[mcupos>>6];
              jassert(comp>=0 && comp<4);
              dc=pred[comp]+=ex;
              jassert((cpos&63)==0);
              cbuf2[cpos]=dc;
              cbuf[cpos++]=(dc+1023)>>3;
              if ((mcupos>>6)==0) {
                ssum1=0;
                ssum2=ssum3;
              } else {
                if (color[(mcupos>>6)-1]==color[0]) ssum1+=(ssum3=ssum);
                ssum2=ssum1;
              }
              ssum=rs;
            }
            jassert(mcupos>=0 && mcupos<=mcusize);
            if (mcupos>=mcusize) {
              mcupos=0;
              if (++column==width) column=0, ++row;
            }
            huffcode=huffsize=huffbits=0, rs=-1;

            // UPDATE_ADV_PRED !!!!
            {
              const int acomp=mcupos>>6, q=64*images[idx].qmap[acomp];
              const int zz=mcupos&63, cpos_dc=cpos-zz;
              const bool norst=rstpos!=column+row*width;
              if (zz==0) {
                for (int i=0; i<8; ++i) sumu[i]=sumv[i]=0;
                // position in the buffer of first (DC) coefficient of the block
                // of this same component that is to the west of this one, not
                // necessarily in this MCU
                int offset_DC_W = cpos_dc - blockW[acomp];
                // position in the buffer of first (DC) coefficient of the block
                // of this same component that is to the north of this one, not
                // necessarily in this MCU
                int offset_DC_N = cpos_dc - blockN[acomp];
                for (int i=0; i<64; ++i) {
                  sumu[zzu[i]]+=(zzv[i]&1?-1:1)*(zzv[i]?16*(16+zzv[i]):185)*(images[idx].qtab[q+i]+1)*cbuf2[offset_DC_N+i];
                  sumv[zzv[i]]+=(zzu[i]&1?-1:1)*(zzu[i]?16*(16+zzu[i]):185)*(images[idx].qtab[q+i]+1)*cbuf2[offset_DC_W+i];
                }
              }
              else {
                sumu[zzu[zz-1]]-=(zzv[zz-1]?16*(16+zzv[zz-1]):185)*(images[idx].qtab[q+zz-1]+1)*cbuf2[cpos-1];
                sumv[zzv[zz-1]]-=(zzu[zz-1]?16*(16+zzu[zz-1]):185)*(images[idx].qtab[q+zz-1]+1)*cbuf2[cpos-1];
              }

              for (int i=0; i<3; ++i)
              {
                run_pred[i]=run_pred[i+3]=0;
                for (int st=0; st<10 && zz+st<64; ++st) {
                  const int zz2=zz+st;
                  int p=sumu[zzu[zz2]]*i+sumv[zzv[zz2]]*(2-i);
                  p/=(images[idx].qtab[q+zz2]+1)*185*(16+zzv[zz2])*(16+zzu[zz2])/128;
                  if (zz2==0 && (norst || ls[acomp]==64)) p-=cbuf2[cpos_dc-ls[acomp]];
                  p=(p<0?-1:+1)*ilog(abs(p)+1);
                  if (st==0) {
                    adv_pred[i]=p;
                  }
                  else if (abs(p)>abs(adv_pred[i])+2 && abs(adv_pred[i]) < 210) {
                    if (run_pred[i]==0) run_pred[i]=st*2+(p>0);
                    if (abs(p)>abs(adv_pred[i])+21 && run_pred[i+3]==0) run_pred[i+3]=st*2+(p>0);
                  }
                }
              }
              ex=0;
              for (int i=0; i<8; ++i) ex+=(zzu[zz]<i)*sumu[i]+(zzv[zz]<i)*sumv[i];
              ex=(sumu[zzu[zz]]*(2+zzu[zz])+sumv[zzv[zz]]*(2+zzv[zz])-ex*2)*4/(zzu[zz]+zzv[zz]+16);
              ex/=(images[idx].qtab[q+zz]+1)*185;
              if (zz==0 && (norst || ls[acomp]==64)) ex-=cbuf2[cpos_dc-ls[acomp]];
              adv_pred[3]=(ex<0?-1:+1)*ilog(abs(ex)+1);

              for (int i=0; i<4; ++i) {
                const int a=(i&1?zzv[zz]:zzu[zz]), b=(i&2?2:1);
                if (a<b) ex=65535;
                else {
                  const int zz2=zpos[zzu[zz]+8*zzv[zz]-(i&1?8:1)*b];
                  ex=(images[idx].qtab[q+zz2]+1)*cbuf2[cpos_dc+zz2]/(images[idx].qtab[q+zz]+1);
                  ex=(ex<0?-1:+1)*(ilog(abs(ex)+1)+(ex!=0?17:0));
                }
                lcp[i]=ex;
              }
              if ((zzu[zz]*zzv[zz])!=0){
                const int zz2=zpos[zzu[zz]+8*zzv[zz]-9];
                ex=(images[idx].qtab[q+zz2]+1)*cbuf2[cpos_dc+zz2]/(images[idx].qtab[q+zz]+1);
                lcp[4]=(ex<0?-1:+1)*(ilog(abs(ex)+1)+(ex!=0?17:0));

                ex=(images[idx].qtab[q+zpos[8*zzv[zz]]]+1)*cbuf2[cpos_dc+zpos[8*zzv[zz]]]/(images[idx].qtab[q+zz]+1);
                lcp[5]=(ex<0?-1:+1)*(ilog(abs(ex)+1)+(ex!=0?17:0));

                ex=(images[idx].qtab[q+zpos[zzu[zz]]]+1)*cbuf2[cpos_dc+zpos[zzu[zz]]]/(images[idx].qtab[q+zz]+1);
                lcp[6]=(ex<0?-1:+1)*(ilog(abs(ex)+1)+(ex!=0?17:0));
              }
              else
                lcp[4]=lcp[5]=lcp[6]=65535;

              int prev1=0,prev2=0,cnt1=0,cnt2=0,r=0,s=0;
              prev_coef_rs = cbuf[cpos-64];
              for (int i=0; i<acomp; i++) {
                ex=0;
                ex+=cbuf2[cpos-(acomp-i)*64];
                if (zz==0 && (norst || ls[i]==64)) ex-=cbuf2[cpos_dc-(acomp-i)*64-ls[i]];
                if (color[i]==color[acomp]-1) { prev1+=ex; cnt1++; r+=cbuf[cpos-(acomp-i)*64]>>4; s+=cbuf[cpos-(acomp-i)*64]&0xF; }
                if (color[acomp]>1 && color[i]==color[0]) { prev2+=ex; cnt2++; }
              }
              if (cnt1>0) prev1/=cnt1, r/=cnt1, s/=cnt1, prev_coef_rs=(r<<4)|s;
              if (cnt2>0) prev2/=cnt2;
              prev_coef=(prev1<0?-1:+1)*ilog(11*abs(prev1)+1)+(cnt1<<20);
              prev_coef2=(prev2<0?-1:+1)*ilog(11*abs(prev2)+1);

              if (column==0 && blockW[acomp]>64*acomp) run_pred[1]=run_pred[2], run_pred[0]=0, adv_pred[1]=adv_pred[2], adv_pred[0]=0;
              if (row==0 && blockN[acomp]>64*acomp) run_pred[1]=run_pred[0], run_pred[2]=0, adv_pred[1]=adv_pred[0], adv_pred[2]=0;
            } // !!!!

          }
        }
      }
    }

    // Estimate next bit probability
    if (!images[idx].jpeg || !images[idx].data) return images[idx].next_jpeg;
    if (buf(1+(bpos==0))==FF) {
      m.add(128); //network bias
      m.set(0, 9);
      m.set(0, 1025);
      m.set(buf(1), 1024);
      return true;
    }
    if (rstlen>0 && rstlen==column+row*width-rstpos && mcupos==0 && (int)huffcode==(1<<huffbits)-1) {
      m.add(2047); //network bias
      m.set(0, 9);
      m.set(0, 1025);
      m.set(buf(1), 1024);
      return true;
    }

    // Update model
    if (cp[N-1]) {
      for (int i=0; i<N; ++i)
        *cp[i]=nex(*cp[i],y);
    }
    m1->update();

    // Update context
    const int comp=color[mcupos>>6];
    const int coef=(mcupos&63)|comp<<6;
    const int hc=(huffcode*4+((mcupos&63)==0)*2+(comp==0))|1<<(huffbits+2);
    const bool firstcol=column==0 && blockW[mcupos>>6]>mcupos;
    static int hbcount=2;
    if (++hbcount>2 || huffbits==0) hbcount=0;
    jassert(coef>=0 && coef<256);
    const int zu=zzu[mcupos&63], zv=zzv[mcupos&63];
    if (hbcount==0) {
      int n=hc*32;
      cxt[0]=hash(++n, coef, adv_pred[2]/12+(run_pred[2]<<8), ssum2>>6, prev_coef/72);
      cxt[1]=hash(++n, coef, adv_pred[0]/12+(run_pred[0]<<8), ssum2>>6, prev_coef/72);
      cxt[2]=hash(++n, coef, adv_pred[1]/11+(run_pred[1]<<8), ssum2>>6);
      cxt[3]=hash(++n, rs1, adv_pred[2]/7, run_pred[5]/2, prev_coef/10);
      cxt[4]=hash(++n, rs1, adv_pred[0]/7, run_pred[3]/2, prev_coef/10);
      cxt[5]=hash(++n, rs1, adv_pred[1]/11, run_pred[4]);
      cxt[6]=hash(++n, adv_pred[2]/14, run_pred[2], adv_pred[0]/14, run_pred[0]);
      cxt[7]=hash(++n, cbuf[cpos-blockN[mcupos>>6]]>>4, adv_pred[3]/17, run_pred[1], run_pred[5]);
      cxt[8]=hash(++n, cbuf[cpos-blockW[mcupos>>6]]>>4, adv_pred[3]/17, run_pred[1], run_pred[3]);
      cxt[9]=hash(++n, lcp[0]/22, lcp[1]/22, adv_pred[1]/7, run_pred[1]);
      cxt[10]=hash(++n, lcp[0]/22, lcp[1]/22, mcupos&63, lcp[4]/30);
      cxt[11]=hash(++n, zu/2, lcp[0]/13, lcp[2]/30, prev_coef/40+((prev_coef2/28)<<20));
      cxt[12]=hash(++n, zv/2, lcp[1]/13, lcp[3]/30, prev_coef/40+((prev_coef2/28)<<20));
      cxt[13]=hash(++n, rs1, prev_coef/42, prev_coef2/34, hash(lcp[0]/60,lcp[2]/14,lcp[1]/60,lcp[3]/14));
      cxt[14]=hash(++n, mcupos&63, column>>1);
      cxt[15]=hash(++n, column>>3, min(5+2*(!comp),zu+zv), hash(lcp[0]/10,lcp[2]/40,lcp[1]/10,lcp[3]/40));
      cxt[16]=hash(++n, ssum>>3, mcupos&63);
      cxt[17]=hash(++n, rs1, mcupos&63, run_pred[1]);
      cxt[18]=hash(++n, coef, ssum2>>5, adv_pred[3]/30, (comp)?hash(prev_coef/22,prev_coef2/50):ssum/((mcupos&0x3F)+1));
      cxt[19]=hash(++n, lcp[0]/40, lcp[1]/40, adv_pred[1]/28, hash( (comp)?prev_coef/40+((prev_coef2/40)<<20):lcp[4]/22, min(7,zu+zv), ssum/(2*(zu+zv)+1) ) );
      cxt[20]=hash(++n, zv, cbuf[cpos-blockN[mcupos>>6]], adv_pred[2]/28, run_pred[2]);
      cxt[21]=hash(++n, zu, cbuf[cpos-blockW[mcupos>>6]], adv_pred[0]/28, run_pred[0]);
      cxt[22]=hash(++n, adv_pred[2]/7, run_pred[2]);
      cxt[23]=hash(n, adv_pred[0]/7, run_pred[0]);
      cxt[24]=hash(n, adv_pred[1]/7, run_pred[1]);
      cxt[25]=hash(++n, zv, lcp[1]/14, adv_pred[2]/16, run_pred[5]);
      cxt[26]=hash(++n, zu, lcp[0]/14, adv_pred[0]/16, run_pred[3]);
      cxt[27]=hash(++n, lcp[0]/14, lcp[1]/14, adv_pred[3]/16);
      cxt[28]=hash(++n, coef, prev_coef/10, prev_coef2/20);
      cxt[29]=hash(++n, coef, ssum>>2, prev_coef_rs);
      cxt[30]=hash(++n, coef, adv_pred[1]/17, hash(lcp[(zu<zv)]/24,lcp[2]/20,lcp[3]/24));
      cxt[31]=hash(++n, coef, adv_pred[3]/11, hash(lcp[(zu<zv)]/50,lcp[2+3*(zu*zv>1)]/50,lcp[3+3*(zu*zv>1)]/50));
    }

    // Predict next bit
    m1->add(128); //network bias
    assert(hbcount<=2);
    int p;
    switch(hbcount) {
      case 0: for (int i=0; i<N; ++i){ cp[i]=t[cxt[i]]+1, p=sm[i].p(*cp[i]); m.add((p-2048)>>3); m1->add(p=stretch(p)); m.add(p>>1);} break;
      case 1: { int hc=1+(huffcode&1)*3; for (int i=0; i<N; ++i){ cp[i]+=hc, p=sm[i].p(*cp[i]); m.add((p-2048)>>3); m1->add(p=stretch(p)); m.add(p>>1); }} break;
      default: { int hc=1+(huffcode&1); for (int i=0; i<N; ++i){ cp[i]+=hc, p=sm[i].p(*cp[i]); m.add((p-2048)>>3); m1->add(p=stretch(p)); m.add(p>>1); }} break;
    }

    m1->set(firstcol, 2);
    m1->set(coef | (min(3,huffbits)<<8), 1024);
    m1->set(((hc&0x1FE)<<1) | min(3,ilog2(zu+zv)), 1024);
    int pr=m1->p(1,1);
    m.add(stretch(pr)>>1);
    m.add((pr>>2)-511);
    pr=a1.p(pr, (hc&511) | (((adv_pred[1]/16)&63)<<9), 1023);
    m.add(stretch(pr)>>1);
    m.add((pr>>2)-511);
    pr=a2.p(pr, (hc&511) | (coef<<9), 1023);
    m.add(stretch(pr)>>1);
    m.add((pr>>2)-511);
    m.set(1 + ((zu+zv<5) | ((huffbits>8)<<1) | (firstcol<<2)), 1+8);
    m.set(1 + ((hc&0xFF) | (min(3,(zu+zv)/3))<<8), 1+1024);
    m.set(coef | (min(3,huffbits/2)<<8), 1024);
    return true;
  }
};

//////////////////////////// wavModel /////////////////////////////////

// Model a 16/8-bit stereo/mono uncompressed .wav file.
// Based on 'An asymptotically Optimal Predictor for Stereo Lossless Audio Compression'
// by Florin Ghido.

#ifdef USE_WAVMODEL

static int S,D;
static int wmode;

inline int s2(int i) { return int(short(buf(i)+256*buf(i-1))); }
inline int t2(int i) { return int(short(buf(i-1)+256*buf(i))); }

inline int X1(int i) {
  switch (wmode) {
    case 0: return buf(i)-128;
    case 1: return buf(i<<1)-128;
    case 2: return s2(i<<1);
    case 3: return s2(i<<2);
    case 4: return (buf(i)^128)-128;
    case 5: return (buf(i<<1)^128)-128;
    case 6: return t2(i<<1);
    case 7: return t2(i<<2);
    default: return 0;
  }
}

inline int X2(int i) {
  switch (wmode) {
    case 0: return buf(i+S)-128;
    case 1: return buf((i<<1)-1)-128;
    case 2: return s2((i+S)<<1);
    case 3: return s2((i<<2)-2);
    case 4: return (buf(i+S)^128)-128;
    case 5: return (buf((i<<1)-1)^128)-128;
    case 6: return t2((i+S)<<1);
    case 7: return t2((i<<2)-2);
    default: return 0;
  }
}

#define pr_(i,chn)(pr[2*(i)+chn])
#define F_(k,l,chn)(F[2*(49*(k)+l)+chn])
#define L_(i,k)(L[49*(i)+k])

void wavModel(Mixer& m, int info, ModelStats *Stats = nullptr) {
  static int col=0;
  static Array<int> pr{3*2}; // [3][2]
  static Array<int> counter{2};
  static Array<double> F{49*49*2}; //[49][49][2]
  static Array<double> L{49*49}; //[49][49]
  static const double a=0.996,a2=1.0/a;
  static SmallStationaryContextMap scm1(8,8), scm2(8,8), scm3(8,8), scm4(8,8), scm5(8,8), scm6(8,8), scm7(8,8);
  static ContextMap cm(MEM*4, 11);
  static int bits, channels, w, ch;
  static int z1, z2, z3, z4, z5, z6, z7;

  int j, k, l, i = 0;
  long double sum;

  if (bpos==0 && blpos==0) {
    bits=((info%4)/2)*8+8;
    channels=info%2+1;
    w=channels*(bits>>3);
    wmode=info;
    z1=z2=z3=z4=z5=z6=z7=0;
    if (channels==1) {S=48;D=0;} else {S=36;D=12;}
    for (k=0; k<=S+D; k++)
      for (l=0; l<=S+D; l++) {
        L_(k,l)=0.0;
        for (int chn=0; chn<channels; chn++)
          F_(k,l,chn)=0.0;
      }
    for (int chn=0; chn<channels; chn++) {
      F_(1,0,chn)=1.0;
      counter[chn]=pr_(2,chn)=pr_(1,chn)=pr_(0,chn)=0;
    }
  }
  // Select previous samples and predicted sample as context
  if (bpos==0 && blpos>=w) {
    ch=blpos%w;
    const int msb=ch%(bits>>3);
    const int chn=ch/(bits>>3);
    if (msb==0) {
      z1=X1(1);z2=X1(2);z3=X1(3);z4=X1(4);z5=X1(5);
      k=X1(1);
      const int mS=min(S,counter[chn]-1);
      const int mD=min(D,counter[chn]);
      for (l=0; l<=mS; l++) { F_(0,l,chn)*=a; F_(0,l,chn)+=X1(l+1)*k; }
      for (l=1; l<=mD; l++) { F_(0,l+S,chn)*=a; F_(0,l+S,chn)+=X2(l+1)*k; }
      if (channels==2) {
        k=X2(2);
        for (l=1; l<=mD; l++) { F_(S+1,l+S,chn)*=a; F_(S+1,l+S,chn)+=X2(l+1)*k; }
        for (l=1; l<=mS; l++) { F_(l,S+1,chn)*=a; F_(l,S+1,chn)+=X1(l+1)*k; }
        z6=X2(1)+X1(1)-X2(2);
        z7=X2(1);
      } else {
        z6=2*X1(1)-X1(2);
        z7=X1(1);
      }

      if (channels==1) for (k=1; k<=S+D; k++) for (l=k; l<=S+D; l++) F_(k,l,chn)=(F_(k-1,l-1,chn)-X1(k)*X1(l))*a2;
      else for (k=1; k<=S+D; k++) if (k!=S+1) for (l=k; l<=S+D; l++) if (l!=S+1) F_(k,l,chn)=(F_(k-1,l-1,chn)-(k-1<=S?X1(k):X2(k-S))*(l-1<=S?X1(l):X2(l-S)))*a2;
      for (i=1; i<=S+D; i++) {
          sum=F_(i,i,chn);
          for (k=1; k<i; k++) sum-=L_(i,k)*L_(i,k);
          sum=floor(sum+0.5);
          sum=1.0/sum;
          if (sum>0.0) {
            L_(i,i)=sqrt(sum);
            for (j=(i+1); j<=S+D; j++) {
              sum=F_(i,j,chn);
              for (k=1; k<i; k++) sum-=L_(j,k)*L_(i,k);
              sum=floor(sum+0.5);
              L_(j,i)=sum*L_(i,i);
            }
          } else break;
      }
      if (i>S+D && counter[chn]>S+1) {
        for (k=1; k<=S+D; k++) {
          F_(k,0,chn)=F_(0,k,chn);
          for (j=1; j<k; j++) F_(k,0,chn)-=L_(k,j)*F_(j,0,chn);
          F_(k,0,chn)*=L_(k,k);
        }
        for (k=S+D; k>0; k--) {
          for (j=k+1; j<=S+D; j++) F_(k,0,chn)-=L_(j,k)*F_(j,0,chn);
          F_(k,0,chn)*=L_(k,k);
        }
      }
      sum=0.0;
      for (l=1; l<=S+D; l++) sum+=F_(l,0,chn)*(l<=S?X1(l):X2(l-S));
      pr_(2,chn)=pr_(1,chn);
      pr_(1,chn)=pr_(0,chn);
      pr_(0,chn)=int(floor(sum));
      counter[chn]++;
    }
    const int y1=pr_(0,chn), y2=pr_(1,chn), y3=pr_(2,chn);
    int x1=buf(1), x2=buf(2), x3=buf(3);
    if (wmode==4 || wmode==5) {x1^=128; x2^=128;}
    if (bits==8) {x1-=128; x2-=128;}
    const int t=((bits==8) || ((!msb)^(wmode<6)));
    i=ch<<4;
    if ((msb)^(wmode<6)) {
      cm.set(hash(++i, y1&0xff));
      cm.set(hash(++i, y1&0xff, ((z1-y2+z2-y3)>>1)&0xff));
      cm.set(hash(++i, x1, y1&0xff));
      cm.set(hash(++i, x1, x2>>3, x3));
      if (bits==8)
        cm.set(hash(++i, y1&0xFE, ilog2(abs((int)(z1-y2)))*2+(z1>y2) ));
      else
        cm.set(hash(++i, (y1+z1-y2)&0xff));
      cm.set(hash(++i, x1));
      cm.set(hash(++i, x1, x2));
      cm.set(hash(++i, z1&0xff));
      cm.set(hash(++i, (z1*2-z2)&0xff));
      cm.set(hash(++i, z6&0xff));
      cm.set(hash(++i, y1&0xFF, ((z1-y2+z2-y3)/(bits>>3))&0xFF ));
    } else {
      cm.set(hash(++i, (y1-x1+z1-y2)>>8));
      cm.set(hash(++i, (y1-x1)>>8));
      cm.set(hash(++i, (y1-x1+z1*2-y2*2-z2+y3)>>8));
      cm.set(hash(++i, (y1-x1)>>8, (z1-y2+z2-y3)>>9));
      cm.set(hash(++i, z1>>12));
      cm.set(hash(++i, x1));
      cm.set(hash(++i, x1>>7, x2, x3>>7));
      cm.set(hash(++i, z1>>8));
      cm.set(hash(++i, (z1*2-z2)>>8));
      cm.set(hash(++i, y1>>8));
      cm.set(hash(++i, (y1-x1)>>6 ));
    }
    scm1.set(t*ch);
    scm2.set(t*((z1-x1+y1)>>9)&0xff);
    scm3.set(t*((z1*2-z2-x1+y1)>>8)&0xff);
    scm4.set(t*((z1*3-z2*3+z3-x1)>>7)&0xff);
    scm5.set(t*((z1+z7-x1+y1*2)>>10)&0xff);
    scm6.set(t*((z1*4-z2*6+z3*4-z4-x1)>>7)&0xff);
    scm7.set(t*((z1*5-z2*10+z3*10-z4*5+z5-x1+y1)>>9)&0xff);
  }

  // Predict next bit
  scm1.mix(m);
  scm2.mix(m);
  scm3.mix(m);
  scm4.mix(m);
  scm5.mix(m);
  scm6.mix(m);
  scm7.mix(m);
  cm.mix(m);
  if (level>=4){
    if (Stats)
      Stats->Record = (w<<16)|(Stats->Record&0xFFFF);
    recordModel(m, Stats);
  }
  col++;
  if(col==w*8)col=0;
  m.set(ch+4*ilog2(col&(bits-1)), 4*8);
  m.set(col%bits<8, 2);
  m.set(col%bits, bits);
  m.set(col, w*8);
  m.set(c0, 256);
}

#endif //USE_WAVMODEL

//////////////////////////// exeModel /////////////////////////
/*
  Model for x86/x64 code.
  Based on the previous paq* exe models and on DisFilter (http://www.farbrausch.de/~fg/code/disfilter/) by Fabian Giesen.

  Attemps to parse the input stream as x86/x64 instructions, and quantizes them into 32-bit representations that are then
  used as context to predict the next bits, and also extracts possible sparse contexts at several previous positions that
  are relevant to parsing (prefixes, opcode, Mod and R/M fields of the ModRM byte, Scale field of the SIB byte)

  Changelog:
  (18/08/2017) v98: Initial release by Márcio Pais
  (19/08/2017) v99: Bug fixes, tables for instruction categorization, other small improvements
  (29/08/2017) v102: Added variable context dependent on parsing break point
  (17/09/2017) v112: Allow pre-training of the model
  (22/10/2017) v116: Fixed a bug in function ProcessMode (missing break, thank you Mauro Vezzosi)
*/

// formats
enum InstructionFormat {
  // encoding mode
  fNM = 0x0,      // no ModRM
  fAM = 0x1,      // no ModRM, "address mode" (jumps or direct addresses)
  fMR = 0x2,      // ModRM present
  fMEXTRA = 0x3,  // ModRM present, includes extra bits for opcode
  fMODE = 0x3,    // bitmask for mode

  // no ModRM: size of immediate operand
  fNI = 0x0,      // no immediate
  fBI = 0x4,      // byte immediate
  fWI = 0x8,      // word immediate
  fDI = 0xc,      // dword immediate
  fTYPE = 0xc,    // type mask

  // address mode: type of address operand
  fAD = 0x0,      // absolute address
  fDA = 0x4,      // dword absolute jump target
  fBR = 0x8,      // byte relative jump target
  fDR = 0xc,      // dword relative jump target

  // others
  fERR = 0xf,     // denotes invalid opcodes
};

// 1 byte opcodes
static const U8 Table1[256] = {
  // 0       1       2       3       4       5       6       7       8       9       a       b       c       d       e       f
  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fNM|fBI,fNM|fDI,fNM|fNI,fNM|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fNM|fBI,fNM|fDI,fNM|fNI,fNM|fNI, // 0
  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fNM|fBI,fNM|fDI,fNM|fNI,fNM|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fNM|fBI,fNM|fDI,fNM|fNI,fNM|fNI, // 1
  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fNM|fBI,fNM|fDI,fNM|fNI,fNM|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fNM|fBI,fNM|fDI,fNM|fNI,fNM|fNI, // 2
  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fNM|fBI,fNM|fDI,fNM|fNI,fNM|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fNM|fBI,fNM|fDI,fNM|fNI,fNM|fNI, // 3

  fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI, // 4
  fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI, // 5
  fNM|fNI,fNM|fNI,fMR|fNI,fMR|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fDI,fMR|fDI,fNM|fBI,fMR|fBI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI, // 6
  fAM|fBR,fAM|fBR,fAM|fBR,fAM|fBR,fAM|fBR,fAM|fBR,fAM|fBR,fAM|fBR,fAM|fBR,fAM|fBR,fAM|fBR,fAM|fBR,fAM|fBR,fAM|fBR,fAM|fBR,fAM|fBR, // 7

  fMR|fBI,fMR|fDI,fMR|fBI,fMR|fBI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI, // 8
  fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fAM|fDA,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI, // 9
  fAM|fAD,fAM|fAD,fAM|fAD,fAM|fAD,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fBI,fNM|fDI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI, // a
  fNM|fBI,fNM|fBI,fNM|fBI,fNM|fBI,fNM|fBI,fNM|fBI,fNM|fBI,fNM|fBI,fNM|fDI,fNM|fDI,fNM|fDI,fNM|fDI,fNM|fDI,fNM|fDI,fNM|fDI,fNM|fDI, // b

  fMR|fBI,fMR|fBI,fNM|fWI,fNM|fNI,fMR|fNI,fMR|fNI,fMR|fBI,fMR|fDI,fNM|fBI,fNM|fNI,fNM|fWI,fNM|fNI,fNM|fNI,fNM|fBI,fERR   ,fNM|fNI, // c
  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fNM|fBI,fNM|fBI,fNM|fNI,fNM|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI, // d
  fAM|fBR,fAM|fBR,fAM|fBR,fAM|fBR,fNM|fBI,fNM|fBI,fNM|fBI,fNM|fBI,fAM|fDR,fAM|fDR,fAM|fAD,fAM|fBR,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI, // e
  fNM|fNI,fERR   ,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fMEXTRA,fMEXTRA,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fMEXTRA,fMEXTRA, // f
};

// 2 byte opcodes
static const U8 Table2[256] = {
  // 0       1       2       3       4       5       6       7       8       9       a       b       c       d       e       f
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fNM|fNI,fERR   ,fNM|fNI,fNM|fNI,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // 0
  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // 1
  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fERR   ,fERR   ,fERR   ,fERR   ,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI, // 2
  fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fERR   ,fNM|fNI,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // 3

  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI, // 4
  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI, // 5
  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI, // 6
  fMR|fBI,fMR|fBI,fMR|fBI,fMR|fBI,fMR|fNI,fMR|fNI,fMR|fNI,fNM|fNI,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fMR|fNI,fMR|fNI, // 7

  fAM|fDR,fAM|fDR,fAM|fDR,fAM|fDR,fAM|fDR,fAM|fDR,fAM|fDR,fAM|fDR,fAM|fDR,fAM|fDR,fAM|fDR,fAM|fDR,fAM|fDR,fAM|fDR,fAM|fDR,fAM|fDR, // 8
  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI, // 9
  fNM|fNI,fNM|fNI,fNM|fNI,fMR|fNI,fMR|fBI,fMR|fNI,fMR|fNI,fMR|fNI,fERR   ,fERR   ,fERR   ,fMR|fNI,fMR|fBI,fMR|fNI,fERR   ,fMR|fNI, // a
  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fERR   ,fERR   ,fERR   ,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI, // b

  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI,fNM|fNI, // c
  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI, // d
  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI, // e
  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fERR   , // f
};

// 3 byte opcodes 0F38XX
static const U8 Table3_38[256] = {
  // 0       1       2       3       4       5       6       7       8       9       a       b       c       d       e       f
  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fERR   ,fERR   ,fERR   ,fERR   , // 0
  fMR|fNI,fERR   ,fERR   ,fERR   ,fMR|fNI,fMR|fNI,fERR   ,fMR|fNI,fERR   ,fERR   ,fERR   ,fERR   ,fMR|fNI,fMR|fNI,fMR|fNI,fERR   , // 1
  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fERR   ,fERR   ,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fERR   ,fERR   ,fERR   ,fERR   , // 2
  fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fERR   ,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI, // 3
  fMR|fNI,fMR|fNI,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // 4
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // 5
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // 6
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // 7
  fMR|fNI,fMR|fNI,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // 8
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // 9
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // a
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // b
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // c
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // d
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // e
  fMR|fNI,fMR|fNI,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // f
};

// 3 byte opcodes 0F3AXX
static const U8 Table3_3A[256] = {
  // 0       1       2       3       4       5       6       7       8       9       a       b       c       d       e       f
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fMR|fBI,fMR|fBI,fMR|fBI,fMR|fBI,fMR|fBI,fMR|fBI,fMR|fBI,fMR|fBI, // 0
  fERR   ,fERR   ,fERR   ,fERR   ,fMR|fBI,fMR|fBI,fMR|fBI,fMR|fBI,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // 1
  fMR|fBI,fMR|fBI,fMR|fBI,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // 2
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // 3
  fMR|fBI,fMR|fBI,fMR|fBI,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // 4
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // 5
  fMR|fBI,fMR|fBI,fMR|fBI,fMR|fBI,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // 6
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // 7
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // 8
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // 9
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // a
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // b
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // c
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // d
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // e
  fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // f
};

// escape opcodes using ModRM byte to get more variants
static const U8 TableX[32] = {
  // 0       1       2       3       4       5       6       7
  fMR|fBI,fERR   ,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI, // escapes for 0xf6
  fMR|fDI,fERR   ,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI,fMR|fNI, // escapes for 0xf7
  fMR|fNI,fMR|fNI,fERR   ,fERR   ,fERR   ,fERR   ,fERR   ,fERR   , // escapes for 0xfe
  fMR|fNI,fMR|fNI,fMR|fNI,fERR   ,fMR|fNI,fERR   ,fMR|fNI,fERR   , // escapes for 0xff
};

static const U8 InvalidX64Ops[19] = {0x06, 0x07, 0x16, 0x17, 0x1E, 0x1F, 0x27, 0x2F, 0x37, 0x3F, 0x60, 0x61, 0x62, 0x82, 0x9A, 0xD4, 0xD5, 0xD6, 0xEA,};
static const U8 X64Prefixes[8] = {0x26, 0x2E, 0x36, 0x3E, 0x9B, 0xF0, 0xF2, 0xF3,};

enum InstructionCategory {
  OP_INVALID              =  0,
  OP_PREFIX_SEGREG        =  1,
  OP_PREFIX               =  2,
  OP_PREFIX_X87FPU        =  3,
  OP_GEN_DATAMOV          =  4,
  OP_GEN_STACK            =  5,
  OP_GEN_CONVERSION       =  6,
  OP_GEN_ARITH_DECIMAL    =  7,
  OP_GEN_ARITH_BINARY     =  8,
  OP_GEN_LOGICAL          =  9,
  OP_GEN_SHF_ROT          = 10,
  OP_GEN_BIT              = 11,
  OP_GEN_BRANCH           = 12,
  OP_GEN_BRANCH_COND      = 13,
  OP_GEN_BREAK            = 14,
  OP_GEN_STRING           = 15,
  OP_GEN_INOUT            = 16,
  OP_GEN_FLAG_CONTROL     = 17,
  OP_GEN_SEGREG           = 18,
  OP_GEN_CONTROL          = 19,
  OP_SYSTEM               = 20,
  OP_X87_DATAMOV          = 21,
  OP_X87_ARITH            = 22,
  OP_X87_COMPARISON       = 23,
  OP_X87_TRANSCENDENTAL   = 24,
  OP_X87_LOAD_CONSTANT    = 25,
  OP_X87_CONTROL          = 26,
  OP_X87_CONVERSION       = 27,
  OP_STATE_MANAGEMENT     = 28,
  OP_MMX                  = 29,
  OP_SSE                  = 30,
  OP_SSE_DATAMOV          = 31,
};

static const U8 TypeOp1[256] = {
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , //03
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_STACK         , OP_GEN_STACK         , //07
  OP_GEN_LOGICAL       , OP_GEN_LOGICAL       , OP_GEN_LOGICAL       , OP_GEN_LOGICAL       , //0B
  OP_GEN_LOGICAL       , OP_GEN_LOGICAL       , OP_GEN_STACK         , OP_PREFIX            , //0F
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , //13
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_STACK         , OP_GEN_STACK         , //17
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , //1B
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_STACK         , OP_GEN_STACK         , //1F
  OP_GEN_LOGICAL       , OP_GEN_LOGICAL       , OP_GEN_LOGICAL       , OP_GEN_LOGICAL       , //23
  OP_GEN_LOGICAL       , OP_GEN_LOGICAL       , OP_PREFIX_SEGREG     , OP_GEN_ARITH_DECIMAL , //27
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , //2B
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_PREFIX_SEGREG     , OP_GEN_ARITH_DECIMAL , //2F
  OP_GEN_LOGICAL       , OP_GEN_LOGICAL       , OP_GEN_LOGICAL       , OP_GEN_LOGICAL       , //33
  OP_GEN_LOGICAL       , OP_GEN_LOGICAL       , OP_PREFIX_SEGREG     , OP_GEN_ARITH_DECIMAL , //37
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , //3B
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_PREFIX_SEGREG     , OP_GEN_ARITH_DECIMAL , //3F
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , //43
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , //47
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , //4B
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , //4F
  OP_GEN_STACK         , OP_GEN_STACK         , OP_GEN_STACK         , OP_GEN_STACK         , //53
  OP_GEN_STACK         , OP_GEN_STACK         , OP_GEN_STACK         , OP_GEN_STACK         , //57
  OP_GEN_STACK         , OP_GEN_STACK         , OP_GEN_STACK         , OP_GEN_STACK         , //5B
  OP_GEN_STACK         , OP_GEN_STACK         , OP_GEN_STACK         , OP_GEN_STACK         , //5F
  OP_GEN_STACK         , OP_GEN_STACK         , OP_GEN_BREAK         , OP_GEN_CONVERSION    , //63
  OP_PREFIX_SEGREG     , OP_PREFIX_SEGREG     , OP_PREFIX            , OP_PREFIX            , //67
  OP_GEN_STACK         , OP_GEN_ARITH_BINARY  , OP_GEN_STACK         , OP_GEN_ARITH_BINARY  , //6B
  OP_GEN_INOUT         , OP_GEN_INOUT         , OP_GEN_INOUT         , OP_GEN_INOUT         , //6F
  OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , //73
  OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , //77
  OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , //7B
  OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , //7F
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , //83
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //87
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //8B
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_STACK         , //8F
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //93
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //97
  OP_GEN_CONVERSION    , OP_GEN_CONVERSION    , OP_GEN_BRANCH        , OP_PREFIX_X87FPU     , //9B
  OP_GEN_STACK         , OP_GEN_STACK         , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //9F
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //A3
  OP_GEN_STRING        , OP_GEN_STRING        , OP_GEN_STRING        , OP_GEN_STRING        , //A7
  OP_GEN_LOGICAL       , OP_GEN_LOGICAL       , OP_GEN_STRING        , OP_GEN_STRING        , //AB
  OP_GEN_STRING        , OP_GEN_STRING        , OP_GEN_STRING        , OP_GEN_STRING        , //AF
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //B3
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //B7
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //BB
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //BF
  OP_GEN_SHF_ROT       , OP_GEN_SHF_ROT       , OP_GEN_BRANCH        , OP_GEN_BRANCH        , //C3
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //C7
  OP_GEN_STACK         , OP_GEN_STACK         , OP_GEN_BRANCH        , OP_GEN_BRANCH        , //CB
  OP_GEN_BREAK         , OP_GEN_BREAK         , OP_GEN_BREAK         , OP_GEN_BREAK         , //CF
  OP_GEN_SHF_ROT       , OP_GEN_SHF_ROT       , OP_GEN_SHF_ROT       , OP_GEN_SHF_ROT       , //D3
  OP_GEN_ARITH_DECIMAL , OP_GEN_ARITH_DECIMAL , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //D7
  OP_X87_ARITH         , OP_X87_DATAMOV       , OP_X87_ARITH         , OP_X87_DATAMOV       , //DB
  OP_X87_ARITH         , OP_X87_DATAMOV       , OP_X87_ARITH         , OP_X87_DATAMOV       , //DF
  OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , //E3
  OP_GEN_INOUT         , OP_GEN_INOUT         , OP_GEN_INOUT         , OP_GEN_INOUT         , //E7
  OP_GEN_BRANCH        , OP_GEN_BRANCH        , OP_GEN_BRANCH        , OP_GEN_BRANCH        , //EB
  OP_GEN_INOUT         , OP_GEN_INOUT         , OP_GEN_INOUT         , OP_GEN_INOUT         , //EF
  OP_PREFIX            , OP_GEN_BREAK         , OP_PREFIX            , OP_PREFIX            , //F3
  OP_SYSTEM            , OP_GEN_FLAG_CONTROL  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , //F7
  OP_GEN_FLAG_CONTROL  , OP_GEN_FLAG_CONTROL  , OP_GEN_FLAG_CONTROL  , OP_GEN_FLAG_CONTROL  , //FB
  OP_GEN_FLAG_CONTROL  , OP_GEN_FLAG_CONTROL  , OP_GEN_ARITH_BINARY  , OP_GEN_BRANCH        , //FF
};

static const U8 TypeOp2[256] = {
  OP_SYSTEM            , OP_SYSTEM            , OP_SYSTEM            , OP_SYSTEM            , //03
  OP_INVALID           , OP_SYSTEM            , OP_SYSTEM            , OP_SYSTEM            , //07
  OP_SYSTEM            , OP_SYSTEM            , OP_INVALID           , OP_GEN_CONTROL       , //0B
  OP_INVALID           , OP_GEN_CONTROL       , OP_INVALID           , OP_INVALID           , //0F
  OP_SSE_DATAMOV       , OP_SSE_DATAMOV       , OP_SSE_DATAMOV       , OP_SSE_DATAMOV       , //13
  OP_SSE               , OP_SSE               , OP_SSE_DATAMOV       , OP_SSE_DATAMOV       , //17
  OP_SSE               , OP_GEN_CONTROL       , OP_GEN_CONTROL       , OP_GEN_CONTROL       , //1B
  OP_GEN_CONTROL       , OP_GEN_CONTROL       , OP_GEN_CONTROL       , OP_GEN_CONTROL       , //1F
  OP_SYSTEM            , OP_SYSTEM            , OP_SYSTEM            , OP_SYSTEM            , //23
  OP_SYSTEM            , OP_INVALID           , OP_SYSTEM            , OP_INVALID           , //27
  OP_SSE_DATAMOV       , OP_SSE_DATAMOV       , OP_SSE               , OP_SSE               , //2B
  OP_SSE               , OP_SSE               , OP_SSE               , OP_SSE               , //2F
  OP_SYSTEM            , OP_SYSTEM            , OP_SYSTEM            , OP_SYSTEM            , //33
  OP_SYSTEM            , OP_SYSTEM            , OP_INVALID           , OP_INVALID           , //37
  OP_PREFIX            , OP_INVALID           , OP_PREFIX            , OP_INVALID           , //3B
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //3F
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //43
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //47
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //4B
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //4F
  OP_SSE_DATAMOV       , OP_SSE               , OP_SSE               , OP_SSE               , //53
  OP_SSE               , OP_SSE               , OP_SSE               , OP_SSE               , //57
  OP_SSE               , OP_SSE               , OP_SSE               , OP_SSE               , //5B
  OP_SSE               , OP_SSE               , OP_SSE               , OP_SSE               , //5F
  OP_MMX               , OP_MMX               , OP_MMX               , OP_MMX               , //63
  OP_MMX               , OP_MMX               , OP_MMX               , OP_MMX               , //67
  OP_MMX               , OP_MMX               , OP_MMX               , OP_MMX               , //6B
  OP_INVALID           , OP_INVALID           , OP_MMX               , OP_MMX               , //6F
  OP_SSE               , OP_MMX               , OP_MMX               , OP_MMX               , //73
  OP_MMX               , OP_MMX               , OP_MMX               , OP_MMX               , //77
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //7B
  OP_INVALID           , OP_INVALID           , OP_MMX               , OP_MMX               , //7F
  OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , //83
  OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , //87
  OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , //8B
  OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , OP_GEN_BRANCH_COND   , //8F
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //93
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //97
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //9B
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //9F
  OP_GEN_STACK         , OP_GEN_STACK         , OP_GEN_CONTROL       , OP_GEN_BIT           , //A3
  OP_GEN_SHF_ROT       , OP_GEN_SHF_ROT       , OP_INVALID           , OP_INVALID           , //A7
  OP_GEN_STACK         , OP_GEN_STACK         , OP_SYSTEM            , OP_GEN_BIT           , //AB
  OP_GEN_SHF_ROT       , OP_GEN_SHF_ROT       , OP_STATE_MANAGEMENT  , OP_GEN_ARITH_BINARY  , //AF
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_BIT           , //B3
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_CONVERSION    , OP_GEN_CONVERSION    , //B7
  OP_INVALID           , OP_GEN_CONTROL       , OP_GEN_BIT           , OP_GEN_BIT           , //BB
  OP_GEN_BIT           , OP_GEN_BIT           , OP_GEN_CONVERSION    , OP_GEN_CONVERSION    , //BF
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_SSE               , OP_SSE               , //C3
  OP_SSE               , OP_SSE               , OP_SSE               , OP_GEN_DATAMOV       , //C7
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //CB
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , //CF
  OP_INVALID           , OP_MMX               , OP_MMX               , OP_MMX               , //D3
  OP_SSE               , OP_MMX               , OP_INVALID           , OP_SSE               , //D7
  OP_MMX               , OP_MMX               , OP_SSE               , OP_MMX               , //DB
  OP_MMX               , OP_MMX               , OP_SSE               , OP_MMX               , //DF
  OP_SSE               , OP_MMX               , OP_SSE               , OP_MMX               , //E3
  OP_SSE               , OP_MMX               , OP_INVALID           , OP_SSE               , //E7
  OP_MMX               , OP_MMX               , OP_SSE               , OP_MMX               , //EB
  OP_MMX               , OP_MMX               , OP_SSE               , OP_MMX               , //EF
  OP_INVALID           , OP_MMX               , OP_MMX               , OP_MMX               , //F3
  OP_SSE               , OP_MMX               , OP_SSE               , OP_SSE               , //F7
  OP_MMX               , OP_MMX               , OP_MMX               , OP_SSE               , //FB
  OP_MMX               , OP_MMX               , OP_MMX               , OP_INVALID           , //FF
};

static const U8 TypeOp3_38[256] = {
  OP_SSE               , OP_SSE               , OP_SSE               , OP_SSE               , //03
  OP_SSE               , OP_SSE               , OP_SSE               , OP_SSE               , //07
  OP_SSE               , OP_SSE               , OP_SSE               , OP_SSE               , //0B
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //0F
  OP_SSE               , OP_INVALID           , OP_INVALID           , OP_INVALID           , //13
  OP_SSE               , OP_SSE               , OP_INVALID           , OP_SSE               , //17
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //1B
  OP_SSE               , OP_SSE               , OP_SSE               , OP_INVALID           , //1F
  OP_SSE               , OP_SSE               , OP_SSE               , OP_SSE               , //23
  OP_SSE               , OP_SSE               , OP_INVALID           , OP_INVALID           , //27
  OP_SSE               , OP_SSE               , OP_SSE               , OP_SSE               , //2B
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //2F
  OP_SSE               , OP_SSE               , OP_SSE               , OP_SSE               , //33
  OP_SSE               , OP_SSE               , OP_INVALID           , OP_SSE               , //37
  OP_SSE               , OP_SSE               , OP_SSE               , OP_SSE               , //3B
  OP_SSE               , OP_SSE               , OP_SSE               , OP_SSE               , //3F
  OP_SSE               , OP_SSE               , OP_INVALID           , OP_INVALID           , //43
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //47
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //4B
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //4F
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //53
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //57
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //5B
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //5F
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //63
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //67
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //6B
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //6F
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //73
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //77
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //7B
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //7F
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //83
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //87
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //8B
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //8F
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //93
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //97
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //9B
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //9F
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //A3
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //A7
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //AB
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //AF
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //B3
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //B7
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //BB
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //BF
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //C3
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //C7
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //CB
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //CF
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //D3
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //D7
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //DB
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //DF
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //E3
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //E7
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //EB
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //EF
  OP_GEN_DATAMOV       , OP_GEN_DATAMOV       , OP_INVALID           , OP_INVALID           , //F3
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //F7
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //FB
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //FF
};

static const U8 TypeOp3_3A[256] = {
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //03
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //07
  OP_SSE               , OP_SSE               , OP_SSE               , OP_SSE               , //0B
  OP_SSE               , OP_SSE               , OP_SSE               , OP_SSE               , //0F
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //13
  OP_SSE               , OP_SSE               , OP_SSE               , OP_SSE               , //17
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //1B
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //1F
  OP_SSE               , OP_SSE               , OP_SSE               , OP_INVALID           , //23
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //27
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //2B
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //2F
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //33
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //37
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //3B
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //3F
  OP_SSE               , OP_SSE               , OP_SSE               , OP_INVALID           , //43
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //47
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //4B
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //4F
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //53
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //57
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //5B
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //5F
  OP_SSE               , OP_SSE               , OP_SSE               , OP_SSE               , //63
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //67
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //6B
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //6F
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //73
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //77
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //7B
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //7F
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //83
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //87
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //8B
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //8F
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //93
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //97
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //9B
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //9F
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //A3
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //A7
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //AB
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //AF
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //B3
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //B7
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //BB
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //BF
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //C3
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //C7
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //CB
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //CF
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //D3
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //D7
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //DB
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //DF
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //E3
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //E7
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //EB
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //EF
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //F3
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //F7
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //FB
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           , //FF
};

static const U8 TypeOpX[32] = {
  // escapes for F6
  OP_GEN_LOGICAL       , OP_GEN_LOGICAL       , OP_GEN_LOGICAL       , OP_GEN_ARITH_BINARY  ,
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  ,
  // escapes for F7
  OP_GEN_LOGICAL       , OP_GEN_LOGICAL       , OP_GEN_LOGICAL       , OP_GEN_ARITH_BINARY  ,
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  ,
  // escapes for FE
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_INVALID           , OP_INVALID           ,
  OP_INVALID           , OP_INVALID           , OP_INVALID           , OP_INVALID           ,
  // escapes for FF
  OP_GEN_ARITH_BINARY  , OP_GEN_ARITH_BINARY  , OP_GEN_BRANCH        , OP_GEN_BRANCH        ,
  OP_GEN_BRANCH        , OP_GEN_BRANCH        , OP_GEN_STACK         , OP_INVALID           ,
};

enum Prefixes {
  ES_OVERRIDE = 0x26,
  CS_OVERRIDE = 0x2E,
  SS_OVERRIDE = 0x36,
  DS_OVERRIDE = 0x3E,
  FS_OVERRIDE = 0x64,
  GS_OVERRIDE = 0x65,
  AD_OVERRIDE = 0x67,
  WAIT_FPU    = 0x9B,
  LOCK        = 0xF0,
  REP_N_STR   = 0xF2,
  REP_STR     = 0xF3,
};

enum Opcodes {
  // 1-byte opcodes of special interest (for one reason or another)
  OP_2BYTE  = 0x0f,     // start of 2-byte opcode
  OP_OSIZE  = 0x66,     // operand size prefix
  OP_CALLF  = 0x9a,
  OP_RETNI  = 0xc2,     // ret near+immediate
  OP_RETN   = 0xc3,
  OP_ENTER  = 0xc8,
  OP_INT3   = 0xcc,
  OP_INTO   = 0xce,
  OP_CALLN  = 0xe8,
  OP_JMPF   = 0xea,
  OP_ICEBP  = 0xf1,
};

enum ExeState {
  Start             =  0,
  Pref_Op_Size      =  1,
  Pref_MultiByte_Op =  2,
  ParseFlags        =  3,
  ExtraFlags        =  4,
  ReadModRM         =  5,
  Read_OP3_38       =  6,
  Read_OP3_3A       =  7,
  ReadSIB           =  8,
  Read8             =  9,
  Read16            = 10,
  Read32            = 11,
  Read8_ModRM       = 12,
  Read16_f          = 13,
  Read32_ModRM      = 14,
  Error             = 15,
};

struct OpCache {
  U32 Op[CacheSize];
  U32 Index;
};

struct Instruction {
  U32 Data;
  U8 Prefix, Code, ModRM, SIB, REX, Flags, BytesRead, Size, Category;
  bool MustCheckREX, Decoding, o16, imm8;
};

#define CodeShift            3
#define CodeMask             (0xFF<<CodeShift)
#define ClearCodeMask        ((-1)^CodeMask)
#define PrefixMask           ((1<<CodeShift)-1)
#define OperandSizeOverride  (0x01<<(8+CodeShift))
#define MultiByteOpcode      (0x02<<(8+CodeShift))
#define PrefixREX            (0x04<<(8+CodeShift))
#define Prefix38             (0x08<<(8+CodeShift))
#define Prefix3A             (0x10<<(8+CodeShift))
#define HasExtraFlags        (0x20<<(8+CodeShift))
#define HasModRM             (0x40<<(8+CodeShift))
#define ModRMShift           (7+8+CodeShift)
#define SIBScaleShift        (ModRMShift+8-6)
#define RegDWordDisplacement (0x01<<(8+SIBScaleShift))
#define AddressMode          (0x02<<(8+SIBScaleShift))
#define TypeShift            (2+8+SIBScaleShift)
#define CategoryShift        5
#define CategoryMask         ((1<<CategoryShift)-1)
#define ModRM_mod            0xC0
#define ModRM_reg            0x38
#define ModRM_rm             0x07
#define SIB_scale            0xC0
#define SIB_index            0x38
#define SIB_base             0x07
#define REX_w                0x08

#define MinRequired          8 // minimum required consecutive valid instructions to be considered as code

class exeModel {
private:
  static const int N1=8, N2=10;
  ContextMap2 cm;
  OpCache Cache;
  U32 StateBH[256];
  ExeState pState, State;
  Instruction Op;
  U32 TotalOps, OpMask, OpCategMask, Context, BrkPoint, BrkCtx;
  bool Valid;
  inline bool IsInvalidX64Op(U8 Op) {
    for (int i=0; i<19; i++) {
      if (Op == InvalidX64Ops[i])
        return true;
    }
    return false;
  }
  inline bool IsValidX64Prefix(U8 Prefix) {
    for (int i=0; i<8; i++) {
      if (Prefix == X64Prefixes[i])
        return true;
    }
    return ((Prefix>=0x40 && Prefix<=0x4F) || (Prefix>=0x64 && Prefix<=0x67));
  }
  void ProcessMode(Instruction &Op, ExeState &State) {
    if ((Op.Flags&fMODE)==fAM) {
      Op.Data|=AddressMode;
      Op.BytesRead = 0;
      switch (Op.Flags&fTYPE) {
        case fDR : Op.Data|=(2<<TypeShift);
        case fDA : Op.Data|=(1<<TypeShift);
        case fAD : {
          State = Read32;
          break;
        }
        case fBR : {
          Op.Data|=(2<<TypeShift);
          State = Read8;
        }
      }
    }
    else{
      switch (Op.Flags&fTYPE) {
        case fBI : State = Read8; break;
        case fWI : {
          State = Read16;
          Op.Data|=(1<<TypeShift);
          Op.BytesRead = 0;
          break;
        }
        case fDI : {
          // x64 Move with 8byte immediate? [REX.W is set, opcodes 0xB8+r]
          Op.imm8=((Op.REX & REX_w)>0 && (Op.Code&0xF8)==0xB8);
          if (!Op.o16 || Op.imm8){
            State = Read32;
            Op.Data|=(2<<TypeShift);
          }
          else{
            State = Read16;
            Op.Data|=(3<<TypeShift);
          }
          Op.BytesRead = 0;
          break;
        }
        default: State = Start; /*no immediate*/
      }
    }
  }
  void ProcessFlags2(Instruction &Op, ExeState &State) {
    //if arriving from state ExtraFlags, we've already read the ModRM byte
    if ((Op.Flags&fMODE)==fMR && State!=ExtraFlags) {
      State = ReadModRM;
      return;
    }
    ProcessMode(Op, State);
  }
  void ProcessFlags(Instruction &Op, ExeState &State) {
    if (Op.Code==OP_CALLF || Op.Code==OP_JMPF || Op.Code==OP_ENTER) {
      Op.BytesRead = 0;
      State = Read16_f;
      return; //must exit, ENTER has ModRM too
    }
    ProcessFlags2(Op, State);
  }
  void CheckFlags(Instruction &Op, ExeState &State) {
    //must peek at ModRM byte to read the REG part, so we can know the opcode
    if (Op.Flags==fMEXTRA)
      State = ExtraFlags;
    else if (Op.Flags==fERR) {
      memset(&Op, 0, sizeof(Instruction));
      State = Error;
    }
    else
      ProcessFlags(Op, State);
  }
  void ReadFlags(Instruction &Op, ExeState &State) {
    Op.Flags = Table1[Op.Code];
    Op.Category = TypeOp1[Op.Code];
    CheckFlags(Op, State);
  }
  void ProcessModRM(Instruction &Op, ExeState &State) {
    if ((Op.ModRM & ModRM_mod)==0x40)
      State = Read8_ModRM; //register+byte displacement
    else if ((Op.ModRM & ModRM_mod)==0x80 || (Op.ModRM & (ModRM_mod|ModRM_rm))==0x05 || (Op.ModRM<0x40 && (Op.SIB & SIB_base)==0x05) ){
      State = Read32_ModRM; //register+dword displacement
      Op.BytesRead = 0;
    }
    else
      ProcessMode(Op, State);
  }
  void ApplyCodeAndSetFlag(Instruction &Op, U32 Flag = 0) {
    Op.Data&=ClearCodeMask; \
      Op.Data|=(Op.Code<<CodeShift)|Flag;
  }
  inline U32 OpN(OpCache &Cache, U32 n) {
    return Cache.Op[ (Cache.Index-n)&(CacheSize-1) ];
  }
  inline U32 OpNCateg(U32 &Mask, U32 n) {
    return ((Mask>>(CategoryShift*(n-1)))&CategoryMask);
  }
  inline int pref(int i) { return (buf(i)==0x0f)+2*(buf(i)==0x66)+3*(buf(i)==0x67); }
  // Get context at buf(i) relevant to parsing 32-bit x86 code
  U32 execxt(int i, int x=0) {
    int prefix=0, opcode=0, modrm=0, sib=0;
    if (i) prefix+=4*pref(i--);
    if (i) prefix+=pref(i--);
    if (i) opcode+=buf(i--);
    if (i) modrm+=buf(i--)&(ModRM_mod|ModRM_rm);
    if (i&&((modrm&ModRM_rm)==4)&&(modrm<ModRM_mod)) sib=buf(i)&SIB_scale;
    return prefix|opcode<<4|modrm<<12|x<<20|sib<<(28-6);
  }
  void Update(U8 B, bool Forced = false);
  void Train();
public:
  exeModel() : cm(MEM*2, N1+N2), pState (Start), State( Start), TotalOps(0), OpMask(0), OpCategMask(0), Context(0), BrkPoint(0), BrkCtx(0), Valid(false) {
    memset(&Cache, 0, sizeof(OpCache));
    memset(&Op, 0, sizeof(Instruction));
    memset(&StateBH, 0, sizeof(StateBH));
    if (options&OPTION_TRAINEXE) Train();
  }
  bool Predict(Mixer& m, bool Forced = false, ModelStats *Stats = nullptr);
};

void exeModel::Train() {
  FileDisk f;
  printf("Pre-training x86/x64 model...");
  OpenFromMyFolder::myself(&f);
  int i=0;
  do {
    Update(buf(1));
    if (Valid)
      cm.Train(i);
    buf[pos++]=i;
    blpos++;
  } while ((i=f.getchar())!=EOF);
  printf(" done [%d bytes]\n",pos-1);
  f.close();
  pos=blpos=0;
  memset(&buf[0], 0, buf.size());
}

void exeModel::Update(U8 B, bool Forced) {
  pState = State;
  Op.Size++;
  switch (State) {
    case Start: case Error: {
      // previous code may have just been a REX prefix
      bool Skip = false;
      if (Op.MustCheckREX) {
        Op.MustCheckREX = false;
        // valid x64 code?
        if (!IsInvalidX64Op(B) && !IsValidX64Prefix(B)) {
          Op.REX = Op.Code;
          Op.Code = B;
          Op.Data = PrefixREX|(Op.Code<<CodeShift)|(Op.Data&PrefixMask);
          Skip = true;
        }
      }

      Op.ModRM = Op.SIB = Op.REX = Op.Flags = Op.BytesRead = 0;
      if (!Skip) {
        Op.Code = B;
        // possible REX prefix?
        Op.MustCheckREX = ((Op.Code&0xF0)==0x40) && (!(Op.Decoding && ((Op.Data&PrefixMask)==1)));

        // check prefixes
        Op.Prefix = (Op.Code==ES_OVERRIDE || Op.Code==CS_OVERRIDE || Op.Code==SS_OVERRIDE || Op.Code==DS_OVERRIDE) + //invalid in x64
                    (Op.Code==FS_OVERRIDE)*2 +
                    (Op.Code==GS_OVERRIDE)*3 +
                    (Op.Code==AD_OVERRIDE)*4 +
                    (Op.Code==WAIT_FPU)*5 +
                    (Op.Code==LOCK)*6 +
                    (Op.Code==REP_N_STR || Op.Code==REP_STR)*7;

        if (!Op.Decoding) {
          TotalOps+=(Op.Data!=0)-(Cache.Index && Cache.Op[ Cache.Index&(CacheSize-1) ]!=0);
          OpMask = (OpMask<<1)|(State!=Error);
          OpCategMask = (OpCategMask<<CategoryShift)|(Op.Category);
          Op.Size = 0;

          Cache.Op[ Cache.Index&(CacheSize-1) ] = Op.Data;
          Cache.Index++;

          if (!Op.Prefix)
            Op.Data = Op.Code<<CodeShift;
          else {
            Op.Data = Op.Prefix;
            Op.Category = TypeOp1[Op.Code];
            Op.Decoding = true;
            BrkCtx = hash(1+(BrkPoint = 0), Op.Prefix, OpCategMask&CategoryMask);
            break;
          }
        }
        else {
          // we only have enough bits for one prefix, so the
          // instruction will be encoded with the last one
          if (!Op.Prefix) {
            Op.Data|=(Op.Code<<CodeShift);
            Op.Decoding = false;
          }
          else {
            Op.Data = Op.Prefix;
            Op.Category = TypeOp1[Op.Code];
            BrkCtx = hash(1+(BrkPoint = 1), Op.Prefix, OpCategMask&CategoryMask);
            break;
          }
        }
      }

      if ((Op.o16=(Op.Code==OP_OSIZE)))
        State = Pref_Op_Size;
      else if (Op.Code==OP_2BYTE)
        State = Pref_MultiByte_Op;
      else
        ReadFlags(Op, State);
      BrkCtx = hash(1+(BrkPoint = 2), State, Op.Code, (OpCategMask&CategoryMask), OpN(Cache,1)&((ModRM_mod|ModRM_reg|ModRM_rm)<<ModRMShift));
      break;
    }
    case Pref_Op_Size : {
      Op.Code = B;
      ApplyCodeAndSetFlag(Op, OperandSizeOverride);
      ReadFlags(Op, State);
      BrkCtx = hash(1+(BrkPoint = 3), State);
      break;
    }
    case Pref_MultiByte_Op : {
      Op.Code = B;
      Op.Data|=MultiByteOpcode;

      if (Op.Code==0x38)
        State = Read_OP3_38;
      else if (Op.Code==0x3A)
        State = Read_OP3_3A;
      else {
        ApplyCodeAndSetFlag(Op);
        Op.Flags = Table2[Op.Code];
        Op.Category = TypeOp2[Op.Code];
        CheckFlags(Op, State);
      }
      BrkCtx = hash(1+(BrkPoint = 4), State);
      break;
    }
    case ParseFlags : {
      ProcessFlags(Op, State);
      BrkCtx = hash(1+(BrkPoint = 5), State);
      break;
    }
    case ExtraFlags : case ReadModRM : {
      Op.ModRM = B;
      Op.Data|=(Op.ModRM<<ModRMShift)|HasModRM;
      Op.SIB = 0;
      if (Op.Flags==fMEXTRA) {
        Op.Data|=HasExtraFlags;
        int i = ((Op.ModRM>>3)&0x07) | ((Op.Code&0x01)<<3) | ((Op.Code&0x08)<<1);
        Op.Flags = TableX[i];
        Op.Category = TypeOpX[i];
        if (Op.Flags==fERR) {
          memset(&Op, 0, sizeof(Instruction));
          State = Error;
          BrkCtx = hash(1+(BrkPoint = 6), State);
          break;
        }
        ProcessFlags(Op, State);
        BrkCtx = hash(1+(BrkPoint = 7), State);
        break;
      }

      if ((Op.ModRM & ModRM_rm)==4 && Op.ModRM<ModRM_mod) {
        State = ReadSIB;
        BrkCtx = hash(1+(BrkPoint = 8), State);
        break;
      }

      ProcessModRM(Op, State);
      BrkCtx = hash(1+(BrkPoint = 9), State, Op.Code );
      break;
    }
    case Read_OP3_38 : case Read_OP3_3A : {
      Op.Code = B;
      ApplyCodeAndSetFlag(Op, Prefix38<<(State-Read_OP3_38));
      if (State==Read_OP3_38) {
        Op.Flags = Table3_38[Op.Code];
        Op.Category = TypeOp3_38[Op.Code];
      }
      else {
        Op.Flags = Table3_3A[Op.Code];
        Op.Category = TypeOp3_3A[Op.Code];
      }
      CheckFlags(Op, State);
      BrkCtx = hash(1+(BrkPoint = 10), State);
      break;
    }
    case ReadSIB : {
      Op.SIB = B;
      Op.Data|=((Op.SIB & SIB_scale)<<SIBScaleShift);
      ProcessModRM(Op, State);
      BrkCtx = hash(1+(BrkPoint = 11), State, Op.SIB&SIB_scale);
      break;
    }
    case Read8 : case Read16 : case Read32 : {
      if (++Op.BytesRead>=((State-Read8)<<int(Op.imm8+1))) {
        Op.BytesRead = 0;
        Op.imm8 = false;
        State = Start;
      }
      BrkCtx = hash(1+(BrkPoint = 12), State, Op.Flags&fMODE, Op.BytesRead, ((Op.BytesRead>1)?(buf(Op.BytesRead)<<8):0)|((Op.BytesRead)?B:0) );
      break;
    }
    case Read8_ModRM : {
      ProcessMode(Op, State);
      BrkCtx = hash(1+(BrkPoint = 13), State);
      break;
    }
    case Read16_f : {
      if (++Op.BytesRead==2) {
        Op.BytesRead = 0;
        ProcessFlags2(Op, State);
      }
      BrkCtx = hash(1+(BrkPoint = 14), State);
      break;
    }
    case Read32_ModRM : {
      Op.Data|=RegDWordDisplacement;
      if (++Op.BytesRead==4) {
        Op.BytesRead = 0;
        ProcessMode(Op, State);
      }
      BrkCtx = hash(1+(BrkPoint = 15), State);
      break;
    }
  }
  Valid = (TotalOps>2*MinRequired) && ((OpMask&((1<<MinRequired)-1))==((1<<MinRequired)-1));
  Context = State+16*Op.BytesRead+16*(Op.REX & REX_w);
  StateBH[Context] = (StateBH[Context]<<8)|B;

  if (Valid || Forced) {
    int mask=0, count0=0;
    for (int i=0, j=0; i<N1; ++i){
      if (i>1) mask=mask*2+(buf(i-1)==0), count0+=mask&1;
      j=(i<4)?i+1:5+(i-4)*(2+(i>6));
      cm.set(hash(execxt(j, buf(1)*(j>6)), ((1<<N1)|mask)*(count0*N1/2>=i), (0x08|(blpos&0x07))*(i<4)));
    }

    cm.set(BrkCtx);
    mask = PrefixMask|(0xF8<<CodeShift)|MultiByteOpcode|Prefix38|Prefix3A;
    cm.set(hash(OpN(Cache, 1)&(mask|RegDWordDisplacement|AddressMode), State+16*Op.BytesRead, Op.Data&mask, Op.REX, Op.Category));

    mask = 0x04|(0xFE<<CodeShift)|MultiByteOpcode|Prefix38|Prefix3A|((ModRM_mod|ModRM_reg)<<ModRMShift);
    cm.set(hash(
      OpN(Cache, 1)&mask, OpN(Cache, 2)&mask, OpN(Cache, 3)&mask,
      Context+256*((Op.ModRM & ModRM_mod)==ModRM_mod),
      Op.Data&((mask|PrefixREX)^(ModRM_mod<<ModRMShift))
    ));

    mask = 0x04|CodeMask;
    cm.set(hash(OpN(Cache, 1)&mask, OpN(Cache, 2)&mask, OpN(Cache, 3)&mask, OpN(Cache, 4)&mask, (Op.Data&mask)|(State<<11)|(Op.BytesRead<<15)));

    mask = 0x04|(0xFC<<CodeShift)|MultiByteOpcode|Prefix38|Prefix3A;
    cm.set(hash(State+16*Op.BytesRead, Op.Data&mask, Op.Category*8 + (OpMask&0x07), Op.Flags, ((Op.SIB & SIB_base)==5)*4+((Op.ModRM & ModRM_reg)==ModRM_reg)*2+((Op.ModRM & ModRM_mod)==0)));

    mask = PrefixMask|CodeMask|OperandSizeOverride|MultiByteOpcode|PrefixREX|Prefix38|Prefix3A|HasExtraFlags|HasModRM|((ModRM_mod|ModRM_rm)<<ModRMShift);
    cm.set(hash(Op.Data&mask, State+16*Op.BytesRead, Op.Flags));

    mask = PrefixMask|CodeMask|OperandSizeOverride|MultiByteOpcode|Prefix38|Prefix3A|HasExtraFlags|HasModRM;
    cm.set(hash(OpN(Cache, 1)&mask, State, Op.BytesRead*2+((Op.REX&REX_w)>0), Op.Data&((U16)(mask^OperandSizeOverride))));

    mask = 0x04|(0xFE<<CodeShift)|MultiByteOpcode|Prefix38|Prefix3A|(ModRM_reg<<ModRMShift);
    cm.set(hash(OpN(Cache, 1)&mask, OpN(Cache, 2)&mask, State+16*Op.BytesRead, Op.Data&(mask|PrefixMask|CodeMask)));

    cm.set(State+16*Op.BytesRead);

    cm.set(hash(
      (0x100|B)*(Op.BytesRead>0),
      State+16*pState+256*Op.BytesRead,
      ((Op.Flags&fMODE)==fAM)*16 + (Op.REX & REX_w) + (Op.o16)*4 + ((Op.Code & 0xFE)==0xE8)*2 + ((Op.Data & MultiByteOpcode)!=0 && (Op.Code & 0xF0)==0x80)
    ));
  }
}

bool exeModel::Predict(Mixer& m, bool Forced, ModelStats *Stats) {
  if (bpos==0)
    Update(buf(1), Forced);

  if (Valid || Forced)
    cm.mix(m);
  else {
      for (int i=0; i<(N1+N2)*8; ++i)
        m.add(0);
  }
  U8 s = ((StateBH[Context]>>(28-bpos))&0x08) |
         ((StateBH[Context]>>(21-bpos))&0x04) |
         ((StateBH[Context]>>(14-bpos))&0x02) |
         ((StateBH[Context]>>( 7-bpos))&0x01) |
         ((Op.Category==OP_GEN_BRANCH)<<4)|
         (((c0&((1<<bpos)-1))==0)<<5);

  m.set(Context*4+(s>>4), 1024);
  m.set(State*64+bpos*8+(Op.BytesRead>0)*4+(s>>4), 1024);
  m.set((BrkCtx&0x1FF)|((s&0x20)<<4), 1024);
  m.set(hash(Op.Code, State, OpN(Cache, 1)&CodeMask)&0x1FFF, 8192);
  m.set(hash(State, bpos, Op.Code, Op.BytesRead)&0x1FFF, 8192);
  m.set(hash(State, (bpos<<2)|(c0&3), OpCategMask&CategoryMask, ((Op.Category==OP_GEN_BRANCH)<<2)|(((Op.Flags&fMODE)==fAM)<<1)|(Op.BytesRead>0))&0x1FFF, 8192);
  if (Stats)
    (*Stats).x86_64 = (Valid?1:0)|(Context<<1)|(s<<9);
  return Valid;
}

//////////////////////////// indirectModel /////////////////////

// The context is a byte string history that occurs within a
// 1 or 2 byte context.

void indirectModel(Mixer& m) {
  static ContextMap cm(MEM, 12);
  static Array<U32> t1(256);
  static Array<U16> t2(0x10000);
  static Array<U16> t3(0x8000);
  static Array<U16> t4(0x8000);

  if (bpos==0) {
    U32 d=c4&0xffff, c=d&255, d2=(buf(1)&31)+32*(buf(2)&31)+1024*(buf(3)&31);
    U32 d3=(buf(1)>>3&31)+32*(buf(3)>>3&31)+1024*(buf(4)>>3&31);
    U32& r1=t1[d>>8];
    r1=r1<<8|c;
    U16& r2=t2[c4>>8&0xffff];
    r2=r2<<8|c;
    U16& r3=t3[(buf(2)&31)+32*(buf(3)&31)+1024*(buf(4)&31)];
    r3=r3<<8|c;
    U16& r4=t4[(buf(2)>>3&31)+32*(buf(4)>>3&31)+1024*(buf(5)>>3&31)];
    r4=r4<<8|c;
    const U32 t=c|t1[c]<<8;
    const U32 t0=d|t2[d]<<16;
    const U32 ta=d2|t3[d2]<<16;
    const U32 tc=d3|t4[d3]<<16;
    cm.set(t);
    cm.set(t0);
    cm.set(ta);
    cm.set(tc);
    cm.set(t&0xff00);
    cm.set(t0&0xff0000);
    cm.set(ta&0xff0000);
    cm.set(tc&0xff0000);
    cm.set(t&0xffff);
    cm.set(t0&0xffffff);
    cm.set(ta&0xffffff);
    cm.set(tc&0xffffff);
  }
  cm.mix(m);
}

//////////////////////////// dmcModel //////////////////////////

// Model using DMC (Dynamic Markov Compression).
//
// The bitwise context is represented by a state graph.
//
// See the original paper: http://webhome.cs.uvic.ca/~nigelh/Publications/DMC.pdf
// See the original DMC implementation: http://maveric0.uwaterloo.ca/ftp/dmc/
//
// Main differences:
// - Instead of floats we use fixed point arithmetic.
// - The threshold for cloning a state increases gradually as memory is used up.
// - For probability estimation each state maintains both a 0,1 count ("c0" and "c1") 
//   and a bit history ("state"). The bit history is mapped to a probability adaptively using 
//   a StateMap. The two computed probabilities are emitted to the Mixer to be combined.
// - All counts are updated adaptively.
// - The "dmcModel" is used in "dmcForest". See below.

struct DMCNode { // 12 bytes
private:
  // c0,c1: adaptive counts of zeroes and ones; 
  //   fixed point numbers with 4 integer and 8 fractional bits, i.e. scaling factor=256;
  //   thus the counts 0.0 .. 15.996 are represented by 0 .. 4095
  // state: bit history state - as in a contextmodel
  U32 state_c0_c1;  // 8 + 12 + 12 = 32 bits
public:
  U32 nx0,nx1;     //indexes of next DMC nodes in the state graph
  U8   get_state() const   {return state_c0_c1>>24;}
  void set_state(U8 state) {state_c0_c1=(state_c0_c1 & 0x00FFFFFF)|(state<<24);}
  U32  get_c0() const      {return (state_c0_c1>>12) & 0xFFF;}
  void set_c0(U32 c0)      {assert(c0<4096);state_c0_c1=(state_c0_c1 &0xFF000FFF) | (c0<<12);}
  U32  get_c1() const      {return state_c0_c1 & 0xFFF;}
  void set_c1(U32 c1)      {assert(c1<4096);state_c0_c1=(state_c0_c1 &0xFFFFF000) | c1;}
};

class dmcModel {
private:
  Array<DMCNode> t;     // state graph
  StateMap sm;
  U32 top, curr;        // index of first unallocated node (i.e. number of allocated nodes); index of current node
  U32 threshold;        // cloning threshold parameter: fixed point number as c0,c1
  U32 threshold_start;
  U32 threshold_end;    

  // helper function: adaptively increment a counter
  U32 increment_counter (const U32 x, const U32 increment) const { // x is a fixed point number as c0,c1 ; "increment"  is 0 or 1
    return (((x<<4)-x)>>4)+(increment<<8); // x * (1-1/16) + increment*256
  }
  
public: 
  dmcModel(U32 mem, U32 th_start, U32 th_end) : t(mem), sm() {resetstategraph(th_start, th_end);}

  // Initialize the state graph to a bytewise order 1 model
  // See an explanation of the initial structure in:
  // http://wing.comp.nus.edu.sg/~junping/docs/njp-icita2005.pdf
  void resetstategraph(U32 th_start, U32 th_end) {
    top=curr=0;
    threshold=threshold_start=th_start;
    threshold_end=th_end;
    for (int j=0; j<256; ++j) { //256 trees
      for (int i=0; i<255; ++i) { //255 nodes in each tree
        if (i<127) { //internal tree nodes
          t[top].nx0=top+i+1; // left node 
          t[top].nx1=top+i+2; // right node
        }
        else { // 128 leaf nodes - they each references a root node of tree(i)
          t[top].nx0=(i-127)*255; // left node -> root of tree 0,1,2,3,... 
          t[top].nx1=(i+1)*255;   // right node -> root of tree 128,129,...
        }
        t[top].set_c0(128); //0.5
        t[top].set_c1(128); //0.5
        t[top].set_state(0);
        top++;
      }
    }
  }

  //update stategraph
  void update() {

    U32 c0=t[curr].get_c0();
    U32 c1=t[curr].get_c1();
    const U32 n = y ==0 ? c0 : c1;

    // update counts, state
    t[curr].set_c0(increment_counter(c0,1-y));
    t[curr].set_c1(increment_counter(c1,y));
    t[curr].set_state(nex(t[curr].get_state(), y));

    // clone next state when threshold is reached
    const U32 next = y==0 ? t[curr].nx0 : t[curr].nx1;
    c0=t[next].get_c0();
    c1=t[next].get_c1();
    const U32 nn=c0+c1;
    if(n>threshold && nn>n+threshold && top<t.size()) {
      U32 c0_top=c0*n/nn;
      U32 c1_top=c1*n/nn;
      assert(c0>=c0_top);
      assert(c1>=c1_top);
      c0-=c0_top;
      c1-=c1_top;

      t[top].set_c0(c0_top);
      t[top].set_c1(c1_top);
      t[next].set_c0(c0);
      t[next].set_c1(c1);

      t[top].nx0=t[next].nx0;
      t[top].nx1=t[next].nx1;
      t[top].set_state(t[next].get_state());
      if(y==0) t[curr].nx0=top;
      else t[curr].nx1=top;

      ++top;

      U32 const target=(t.size()-65280);
      double const rate=double(top-65280)/double(target);
      threshold=threshold_start+(threshold_end-threshold_start)*rate;
    }

    if(y==0) curr=t[curr].nx0;
    else     curr=t[curr].nx1;
  }

  bool isfull() const {return top==t.size() && bpos==1;}
  int pr1() const {
    const U32 n0=t[curr].get_c0()+1;
    const U32 n1=t[curr].get_c1()+1;
    return (n1<<12)/(n0+n1);
  }
  int pr2() {return sm.p(t[curr].get_state(),256);}
};

// This class solves two problems of the DMC model
// 1) The DMC model is a memory hungry algorithm. In theory it works best when it can clone
//    nodes forever. But memory is a limited resource. When the state graph is full you can't
//    clone nodes anymore. You can either i) reset the model (the state graph) and start over
//    or ii) you can keep updating the counts forever in the already fixed state graph. Both
//    choices are troublesome: i) resetting the model degrades the predictive power significantly
//    until the graph becomes large enough again and ii) a fixed structure can't adapt anymore.
//    To solve this issue:
//    Two models with the same parameters work in tandem. Always both models are updated but
//    only one model (the larger, mature one) is active (predicts) at any time. When one model
//    needs resetting the other one feeds the mixer with predictions until the first one
//    becomes mature (nearly full) again.
//    Disadvantages: with the same memory requirements we have just half of the number of nodes
//    in each model. Also keeping two models updated at all times requires 2x as much
//    calculations as updating one model only.
//    Advantage: stable and better compression - even with reduced number of nodes.
// 2) The DMC model is sensitive to the cloning threshold parameter. Some files prefer
//    a smaller threshold other files prefer a larger threshold.
//    The difference in terms of compression is significant.
//    To solve this issue:
//    Three models with different thresholds are used and their predictions are emitted to 
//    the mixer. This way the model with the better threshold will be favored naturally.
//    Disadvantage: same as in 1) just the available number of nodes is 1/3 of the 
//    one-model case.
//    Advantage: same as in 1).

class dmcForest {
private:
  dmcModel dmcmodel1a; // models a and b have the same parameters and work in tandem
  dmcModel dmcmodel1b;
  dmcModel dmcmodel2a; // models 1,2,3 have different threshold parameters
  dmcModel dmcmodel2b;
  dmcModel dmcmodel3a;
  dmcModel dmcmodel3b;
  // states of each model-pair:
  //   0: model (a) and (b) are both active, both are growing (initial state)
  //   1: model (a) is full and active, model (b) is reset and growing
  //   2: model (b) is full and active, model (a) is reset and growing
  int model1_state=0; 
  int model2_state=0; 
  int model3_state=0; 
public:
  dmcForest():dmcmodel1a(MEM*4/9,20,1800),dmcmodel1b(MEM*4/9,1,1800),dmcmodel2a(MEM*4/9,30,1800),dmcmodel2b(MEM*4/9,4,1800),dmcmodel3a(MEM*4/9,256,1800),dmcmodel3b(MEM*4/9,128,1800){}
  void mix(Mixer& m) {

    int pr11=0,pr12=0,pr21=0,pr22=0,pr31=0,pr32=0;

    dmcmodel1a.update();
    dmcmodel1b.update();
    switch(model1_state) {
    case 0:
      pr11=(dmcmodel1a.pr1()+dmcmodel1b.pr1()+1)>>1;
      pr12=(dmcmodel1a.pr2()+dmcmodel1b.pr2()+1)>>1;
      if(dmcmodel1a.isfull()){dmcmodel1b.resetstategraph(256,1800);model1_state++;}
      break;
    case 1:
      pr11=dmcmodel1a.pr1();
      pr12=dmcmodel1a.pr2();
      if(dmcmodel1b.isfull()){dmcmodel1a.resetstategraph(256,1800);model1_state++;}
      break;
    case 2:
      pr11=dmcmodel1b.pr1();
      pr12=dmcmodel1b.pr2();
      if(dmcmodel1a.isfull()){dmcmodel1b.resetstategraph(256,1800);model1_state--;}
      break;
    }

    dmcmodel2a.update();
    dmcmodel2b.update();
    switch(model2_state) {
    case 0:
      pr21=(dmcmodel2a.pr1()+dmcmodel2b.pr1()+1)>>1;
      pr22=(dmcmodel2a.pr2()+dmcmodel2b.pr2()+1)>>1;
      if(dmcmodel2a.isfull()){dmcmodel2b.resetstategraph(512,1800);model2_state++;} 
      break;
    case 1:
      pr21=dmcmodel2a.pr1();
      pr22=dmcmodel2a.pr2();
      if(dmcmodel2b.isfull()){dmcmodel2a.resetstategraph(512,1800);model2_state++;}
      break;
    case 2:
      pr21=dmcmodel2b.pr1();
      pr22=dmcmodel2b.pr2();
      if(dmcmodel2a.isfull()){dmcmodel2b.resetstategraph(512,1800);model2_state--;}
      break;
    }

    dmcmodel3a.update();
    dmcmodel3b.update();
    switch(model3_state) {
    case 0:
      pr31=(dmcmodel3a.pr1()+dmcmodel3b.pr1()+1)>>1;
      pr32=(dmcmodel3a.pr2()+dmcmodel3b.pr2()+1)>>1;
      if(dmcmodel3a.isfull()){dmcmodel3b.resetstategraph(768,1800);model3_state++;}
      break;
    case 1:
      pr31=dmcmodel3a.pr1();
      pr32=dmcmodel3a.pr2();
      if(dmcmodel3b.isfull()){dmcmodel3a.resetstategraph(768,1800);model3_state++;}
      break;
    case 2:
      pr31=dmcmodel3b.pr1();
      pr32=dmcmodel3b.pr2();
      if(dmcmodel3a.isfull()){dmcmodel3b.resetstategraph(768,1800);model3_state--;}
      break;
    }
    m.add(stretch(pr11)>>2);
    m.add(stretch(pr12)>>2);
    m.add(stretch(pr21)>>2);
    m.add(stretch(pr22)>>2);
    m.add(stretch(pr31)>>2);
    m.add(stretch(pr32)>>2);
  }
};


void nestModel(Mixer& m)
{
  static int ic=0, bc=0, pc=0,vc=0, qc=0, lvc=0, wc=0, ac=0, ec=0, uc=0, sense1=0, sense2=0, w=0;
  static ContextMap cm(MEM, 10+2);
  if (bpos==0) {
    int c=c4&255, matched=1, vv;
    w*=((vc&7)>0 && (vc&7)<3);
    if (c&0x80) w = w*11*32 + c;
    const int lc = (c >= 'A' && c <= 'Z'?c+'a'-'A':c);
    if (lc == 'a' || lc == 'e' || lc == 'i' || lc == 'o' || lc == 'u'){ vv = 1; w = w*997*8 + (lc/4-22); } else
    if (lc >= 'a' && lc <= 'z'){ vv = 2; w = w*271*32 + lc-97; } else
    if (lc == ' ' || lc == '.' || lc == ',' || lc == '\n') vv = 3; else
    if (lc >= '0' && lc <= '9') vv = 4; else
    if (lc == 'y') vv = 5; else
    if (lc == '\'') vv = 6; else vv=(c&32)?7:0;
    vc = (vc << 3) | vv;
    if (vv != lvc) {
      wc = (wc << 3) | vv;
      lvc = vv;
    }
    switch(c) {
      case ' ': qc = 0; break;
      case '(': ic += 31; break;
      case ')': ic -= 31; break;
      case '[': ic += 11; break;
      case ']': ic -= 11; break;
      case '<': ic += 23; qc += 34; break;
      case '>': ic -= 23; qc /= 5; break;
      case ':': pc = 20; break;
      case '{': ic += 17; break;
      case '}': ic -= 17; break;
      case '|': pc += 223; break;
      case '"': pc += 0x40; break;
      case '\'': pc += 0x42; if (c!=(U8)(c4>>8)) sense2^=1; else ac+=(2*sense2-1); break;
      case '\n': pc = qc = 0; break;
      case '.': pc = 0; break;
      case '!': pc = 0; break;
      case '?': pc = 0; break;
      case '#': pc += 0x08; break;
      case '%': pc += 0x76; break;
      case '$': pc += 0x45; break;
      case '*': pc += 0x35; break;
      case '-': pc += 0x3; break;
      case '@': pc += 0x72; break;
      case '&': qc += 0x12; break;
      case ';': qc /= 3; break;
      case '\\': pc += 0x29; break;
      case '/': pc += 0x11;
                if (buf.size() > 1 && buf(1) == '<') qc += 74;
                break;
      case '=': pc += 87; if (c!=(U8)(c4>>8)) sense1^=1; else ec+=(2*sense1-1); break;
      default: matched = 0;
    }
    if (c4==0x266C743B) uc=min(7,uc+1);
    else if (c4==0x2667743B) uc-=(uc>0);
    if (matched) bc = 0; else bc += 1;
    if (bc > 300) bc = ic = pc = qc = uc = 0;

    cm.set(hash( (vv>0 && vv<3)?0:(lc|0x100), ic&0x3FF, ec&0x7, ac&0x7, uc ));
    cm.set(hash(ic, w, ilog2(bc+1)));
    cm.set((3*vc+77*pc+373*ic+qc)&0xffff);
    cm.set((31*vc+27*pc+281*qc)&0xffff);
    cm.set((13*vc+271*ic+qc+bc)&0xffff);
    cm.set((17*pc+7*ic)&0xffff);
    cm.set((13*vc+ic)&0xffff);
    cm.set((vc/3+pc)&0xffff);
    cm.set((7*wc+qc)&0xffff);
    cm.set((vc&0xffff)|((c4&0xff)<<16));
    cm.set(((3*pc)&0xffff)|((c4&0xff)<<16));
    cm.set((ic&0xffff)|((c4&0xff)<<16));

  }
  cm.mix(m);
}

/*
  XML Model.
  Attempts to parse the tag structure and detect specific content types.

  Changelog:
  (17/08/2017) v96: Initial release by Márcio Pais
  (17/08/2017) v97: Bug fixes (thank you Mauro Vezzosi) and improvements.
*/

struct XMLAttribute {
  U32 Name, Value, Length;
};

struct XMLContent {
  U32 Data, Length, Type;
};

struct XMLTag {
  U32 Name, Length;
  int Level;
  bool EndTag, Empty;
  XMLContent Content;
  struct XMLAttributes {
    XMLAttribute Items[4];
    U32 Index;
  } Attributes;
};

struct XMLTagCache {
  XMLTag Tags[CacheSize];
  U32 Index;
};

enum ContentFlags {
  Text        = 0x001,
  Number      = 0x002,
  Date        = 0x004,
  Time        = 0x008,
  URL         = 0x010,
  Link        = 0x020,
  Coordinates = 0x040,
  Temperature = 0x080,
  ISBN        = 0x100,
};

enum XMLState {
  None               = 0,
  ReadTagName        = 1,
  ReadTag            = 2,
  ReadAttributeName  = 3,
  ReadAttributeValue = 4,
  ReadContent        = 5,
  ReadCDATA          = 6,
  ReadComment        = 7,
};

#define DetectContent(){ \
  if ((c4&0xF0F0F0F0)==0x30303030){ \
    int i = 0, j = 0; \
    while ((i<4) && ( (j=(c4>>(8*i))&0xFF)>=0x30 && j<=0x39 )) \
      i++; \
\
    if (i==4 && ( ((c8&0xFDF0F0FD)==0x2D30302D && buf(9)>=0x30 && buf(9)<=0x39) || ((c8&0xF0FDF0FD)==0x302D302D) )) \
      (*Content).Type |= Date; \
  } \
  else if (((c8&0xF0F0FDF0)==0x30302D30 || (c8&0xF0F0F0FD)==0x3030302D) && buf(9)>=0x30 && buf(9)<=0x39){ \
    int i = 2, j = 0; \
    while ((i<4) && ( (j=(c8>>(8*i))&0xFF)>=0x30 && j<=0x39 )) \
      i++; \
\
    if (i==4 && (c4&0xF0FDF0F0)==0x302D3030) \
      (*Content).Type |= Date; \
  } \
\
  if ((c4&0xF0FFF0F0)==0x303A3030 && buf(5)>=0x30 && buf(5)<=0x39 && ((buf(6)<0x30 || buf(6)>0x39) || ((c8&0xF0F0FF00)==0x30303A00 && (buf(9)<0x30 || buf(9)>0x39)))) \
    (*Content).Type |= Time; \
\
  if ((*Content).Length>=8 && (c8&0x80808080)==0 && (c4&0x80808080)==0) \
    (*Content).Type |= Text; \
\
  if ((c8&0xF0F0FF)==0x3030C2 && (c4&0xFFF0F0FF)==0xB0303027){ \
    int i = 2; \
    while ((i<7) && buf(i)>=0x30 && buf(i)<=0x39) \
      i+=(i&1)*2+1; \
\
    if (i==10) \
      (*Content).Type |= Coordinates; \
  } \
\
  if ((c4&0xFFFFFA)==0xC2B042 && B!=0x47 && (((c4>>24)>=0x30 && (c4>>24)<=0x39) || ((c4>>24)==0x20 && (buf(5)>=0x30 && buf(5)<=0x39)))) \
    (*Content).Type |= Temperature; \
\
  if (B>=0x30 && B<=0x39) \
    (*Content).Type |= Number; \
\
  if (c4==0x4953424E && buf(5)==0x20) \
    (*Content).Type |= ISBN; \
}

void XMLModel(Mixer& m, ModelStats *Stats = NULL){
  static ContextMap cm(MEM/4, 4);
  static XMLTagCache Cache;
  static U32 StateBH[8];
  static XMLState State = None, pState = None;
  static U32 c8, WhiteSpaceRun = 0, pWSRun = 0, IndentTab = 0, IndentStep = 2, LineEnding = 2;

  if (bpos==0) {
    U8 B = (U8)c4;
    XMLTag *pTag = &Cache.Tags[ (Cache.Index-1)&(CacheSize-1) ], *Tag = &Cache.Tags[ Cache.Index&(CacheSize-1) ];
    XMLAttribute *Attribute = &((*Tag).Attributes.Items[ (*Tag).Attributes.Index&3 ]);
    XMLContent *Content = &(*Tag).Content;
    pState = State;
    c8 = (c8<<8)|buf(5);
    if ((B==TAB || B==SPACE) && (B==(U8)(c4>>8) || !WhiteSpaceRun)){
      WhiteSpaceRun++;
      IndentTab = (B==TAB);
    }
    else{
      if ((State==None || (State==ReadContent && (*Content).Length<=LineEnding+WhiteSpaceRun)) && WhiteSpaceRun>1+IndentTab && WhiteSpaceRun!=pWSRun){
        IndentStep=abs((int)(WhiteSpaceRun-pWSRun));
        pWSRun = WhiteSpaceRun;
      }
      WhiteSpaceRun=0;
    }
    if (B==NEW_LINE)
      LineEnding = 1+((U8)(c4>>8)==CARRIAGE_RETURN);

    switch (State){
      case None : {
        if (B==0x3C){
          State = ReadTagName;
          memset(Tag, 0, sizeof(XMLTag));
          (*Tag).Level = ((*pTag).EndTag || (*pTag).Empty)?(*pTag).Level:(*pTag).Level+1;
        }
        if ((*Tag).Level>1)
          DetectContent();

        cm.set(hash(pState, State, ((*pTag).Level+1)*IndentStep - WhiteSpaceRun));
        break;
      }
      case ReadTagName : {
        if ((*Tag).Length>0 && (B==TAB || B==NEW_LINE || B==CARRIAGE_RETURN || B==SPACE))
          State = ReadTag;
        else if ((B==0x3A || (B>='A' && B<='Z') || B==0x5F || (B>='a' && B<='z')) || ((*Tag).Length>0 && (B==0x2D || B==0x2E || (B>='0' && B<='9')))){
          (*Tag).Length++;
          (*Tag).Name = (*Tag).Name * 263 * 32 + (B&0xDF);
        }
        else if (B == 0x3E){
          if ((*Tag).EndTag){
            State = None;
            Cache.Index++;
          }
          else
            State = ReadContent;
        }
        else if (B!=0x21 && B!=0x2D && B!=0x2F && B!=0x5B){
          State = None;
          Cache.Index++;
        }
        else if ((*Tag).Length==0){
          if (B==0x2F){
            (*Tag).EndTag = true;
            (*Tag).Level = max(0,(*Tag).Level-1);
          }
          else if (c4==0x3C212D2D){
            State = ReadComment;
            (*Tag).Level = max(0,(*Tag).Level-1);
          }
        }

        if ((*Tag).Length==1 && (c4&0xFFFF00)==0x3C2100){
          memset(Tag, 0, sizeof(XMLTag));
          State = None;
        }
        else if ((*Tag).Length==5 && c8==0x215B4344 && c4==0x4154415B){
          State = ReadCDATA;
          (*Tag).Level = max(0,(*Tag).Level-1);
        }

        int i = 1;
        do{
          pTag = &Cache.Tags[ (Cache.Index-i)&(CacheSize-1) ];
          i+=1+((*pTag).EndTag && Cache.Tags[ (Cache.Index-i-1)&(CacheSize-1) ].Name==(*pTag).Name);
        }
        while ( i<CacheSize && ((*pTag).EndTag || (*pTag).Empty) );

        cm.set(hash(pState*8+State, (*Tag).Name, (*Tag).Level, (*pTag).Name, (*pTag).Level!=(*Tag).Level ));
        break;
      }
      case ReadTag : {
        if (B==0x2F)
          (*Tag).Empty = true;
        else if (B==0x3E){
          if ((*Tag).Empty){
            State = None;
            Cache.Index++;
          }
          else
            State = ReadContent;
        }
        else if (B!=TAB && B!=NEW_LINE && B!=CARRIAGE_RETURN && B!=SPACE){
          State = ReadAttributeName;
          (*Attribute).Name = B&0xDF;
        }
        cm.set(hash(pState, State, (*Tag).Name, B, (*Tag).Attributes.Index ));
        break;
      }
      case ReadAttributeName : {
        if ((c4&0xFFF0)==0x3D20 && (B==0x22 || B==0x27)){
          State = ReadAttributeValue;
          if ((c8&0xDFDF)==0x4852 && (c4&0xDFDF0000)==0x45460000)
            (*Content).Type |= Link;
        }
        else if (B!=0x22 && B!=0x27 && B!=0x3D)
          (*Attribute).Name = (*Attribute).Name * 263 * 32 + (B&0xDF);

        cm.set(hash(pState*8+State, (*Attribute).Name, (*Tag).Attributes.Index, (*Tag).Name, (*Content).Type ));
        break;
      }
      case ReadAttributeValue : {
        if (B==0x22 || B==0x27){
          (*Tag).Attributes.Index++;
          State = ReadTag;
        }
        else{
          (*Attribute).Value = (*Attribute).Value* 263 * 32 + (B&0xDF);
          (*Attribute).Length++;
          if ((c8&0xDFDFDFDF)==0x48545450 && ((c4>>8)==0x3A2F2F || c4==0x733A2F2F))
            (*Content).Type |= URL;
        }
        cm.set(hash(pState, State, (*Attribute).Name, (*Content).Type ));
        break;
      }
      case ReadContent : {
        if (B==0x3C){
          State = ReadTagName;
          Cache.Index++;
          memset(&Cache.Tags[ Cache.Index&(CacheSize-1) ], 0, sizeof(XMLTag));
          Cache.Tags[ Cache.Index&(CacheSize-1) ].Level = (*Tag).Level+1;
        }
        else{
          (*Content).Length++;
          (*Content).Data = (*Content).Data * 997*16 + (B&0xDF);

          DetectContent();
        }
        cm.set(hash(pState, State, (*Tag).Name, c4&0xC0FF ));
        break;
      }
      case ReadCDATA : {
        if ((c4&0xFFFFFF)==0x5D5D3E){
          State = None;
          Cache.Index++;
        }
        cm.set(hash(pState, State));
        break;
      }
      case ReadComment : {
        if ((c4&0xFFFFFF)==0x2D2D3E){
          State = None;
          Cache.Index++;
        }
        cm.set(hash(pState, State));
        break;
      }
    }

    StateBH[pState] = (StateBH[pState]<<8)|B;
    pTag = &Cache.Tags[ (Cache.Index-1)&(CacheSize-1) ];
    cm.set(hash(State, (*Tag).Level, pState*2+(*Tag).EndTag, (*Tag).Name));
    cm.set(hash((*pTag).Name, State*2+(*pTag).EndTag, (*pTag).Content.Type, (*Tag).Content.Type));
    cm.set(hash(State*2+(*Tag).EndTag, (*Tag).Name, (*Tag).Content.Type, c4&0xE0FF));
  }
  cm.mix(m);
  U8 s = ((StateBH[State]>>(28-bpos))&0x08) |
         ((StateBH[State]>>(21-bpos))&0x04) |
         ((StateBH[State]>>(14-bpos))&0x02) |
         ((StateBH[State]>>( 7-bpos))&0x01) |
         ((bpos)<<4);
  if (Stats)
    (*Stats).XML = (s<<3)|State;
}

//////////////////////////// contextModel //////////////////////

// This combines all the context models with a Mixer.

class ContextModel{
  ContextMap2 cm;
  RunContextMap rcm7, rcm9, rcm10;
  exeModel exeModel1;
  JpegModel *jpegModel;
  dmcForest *dmcforest;
  #ifdef USE_TEXTMODEL
  TextModel textModel;
  #endif //USE_TEXTMODEL
  MatchModel matchModel;
  Mixer *m;
  U32 cxt[15];
  Blocktype next_blocktype, blocktype;
  int blocksize, blockinfo;
  void UpdateContexts(U8 B);
  void Train(const char* Dictionary, int Iterations = 1);
public:
  bool Bypass;
  ContextModel() : cm(MEM*32, 9), rcm7(MEM), rcm9(MEM), rcm10(MEM), jpegModel(0), dmcforest(0),

    #ifdef USE_TEXTMODEL
      textModel(MEM*16),
    #endif //USE_TEXTMODEL
    matchModel(MEM*4, options&OPTION_FASTMODE),

    next_blocktype(DEFAULT), blocktype(DEFAULT), blocksize(0), blockinfo(0), Bypass(false) {

    if(level>=4)dmcforest=new dmcForest();

    #ifdef USE_WORDMODEL
      m=MixerFactory::CreateMixer(985+288, 4096+(1536/*recordModel*/+27648/*exeModel*/+16384/*textModel*/)*(level>=4), 7+15*(level>=4));
    #else
      m=MixerFactory::CreateMixer(985, 4096+(1536/*recordModel*/+27648/*exeModel*/+16384/*textModel*/)*(level>=4), 7+15*(level>=4));
    #endif //USE_WORD_MODEL

      memset(&cxt[0], 0, sizeof(cxt));
      if (options&OPTION_TRAINTXT) {
        Train("english.dic", 3);
        Train("english.exp");
      }
    }
  ~ContextModel() {
    delete m;
    if(jpegModel) delete jpegModel;
    if(dmcforest) delete dmcforest;
  }
  int Predict(ModelStats *Stats = nullptr);
};

void ContextModel::Train(const char* Dictionary, int Iterations) {
  FileDisk f;
  printf("Pre-training main model...");
  OpenFromMyFolder::anotherfile(&f, Dictionary);
  int i;
  while (Iterations-->0) {
    f.setpos(0);
    i=SPACE, pos=0;
    do {
      if (i==CARRIAGE_RETURN)
        continue;
      else if (i==NEW_LINE){
        i=SPACE;
        memset(&cxt[0], 0, sizeof(cxt));
      }
      cm.Train(i);
      buf[pos++]=i;
      UpdateContexts(i);
    } while ((i=f.getchar())!=EOF);
  }
  printf(" done [%s, %d bytes]\n", Dictionary, pos);
  f.close();
  pos = 0;
  memset(&cxt[0], 0, sizeof(cxt));
  memset(&buf[0], 0, buf.size());
}

void ContextModel::UpdateContexts(const U8 B){
  for (int i=14; i>0; --i)
    cxt[i]=(cxt[i-1]<<8)+cxt[i-1]+B+1;
  for (int i=0; i<7; ++i)
    cm.set(cxt[i]);
  cm.set(cxt[8]);
  cm.set(cxt[14]);
  rcm7.set(cxt[7]);
  rcm9.set(cxt[10]);
  rcm10.set(cxt[12]);
}

int ContextModel::Predict(ModelStats *Stats){
  // Parse block type and block size
  if (bpos==0) {
    --blocksize;
    ++blpos;
    if (blocksize==-1) next_blocktype=(Blocktype)buf(1); //got blocktype but don't switch (we don't have all the info yet)
    if (blocksize==-5 && !hasInfo(next_blocktype)) {
      blocksize=buf(4)<<24|buf(3)<<16|buf(2)<<8|buf(1);
      if (hasRecursion(next_blocktype)) blocksize=0;
      blpos=0;
    }
    if (blocksize==-9) {
      blocksize=buf(8)<<24|buf(7)<<16|buf(6)<<8|buf(5);
      blockinfo=buf(4)<<24|buf(3)<<16|buf(2)<<8|buf(1);
      blpos=0;
    }
    if (blpos==0) blocktype=next_blocktype; //got all the info - switch to next blocktype
    if (blocksize==0) blocktype=DEFAULT;
    if (Stats)
      Stats->blockType = blocktype;
  }

  m->update();
  Bypass=false;
  m->add(256); //network bias
  int matchlength=matchModel.Predict(*m, buf, Stats);
  if (options&OPTION_FASTMODE && (matchlength>0xFFF || matchModel.Bypass)) {
    matchModel.Bypass = Bypass = true;
    m->reset();
    return matchModel.BypassPrediction;
  }
  matchlength=ilog(matchlength); // ilog of length of most recent match (0..255)

  // Test for special block types
  if (blocktype==IMAGE1) im1bitModel(*m, blockinfo);
  if (blocktype==IMAGE4) return im4bitModel(*m, blockinfo), m->p();
  if (blocktype==IMAGE8) return im8bitModel(*m, blockinfo), m->p();
  if (blocktype==IMAGE8GRAY) return im8bitModel(*m, blockinfo, 1), m->p(0,1);
  if (blocktype==IMAGE24) return im24bitModel(*m, blockinfo), m->p(1,1);
  if (blocktype==IMAGE32) return im24bitModel(*m, blockinfo, 1), m->p();
  if (blocktype==PNG8) return im8bitModel(*m, blockinfo, 0, 1), m->p();
  if (blocktype==PNG8GRAY) return im8bitModel(*m, blockinfo, 1, 1), m->p(0,1);
  if (blocktype==PNG24) return im24bitModel(*m, blockinfo, 0, 1), m->p(1,1);
  if (blocktype==PNG32) return im24bitModel(*m, blockinfo, 1, 1), m->p();
  #ifdef USE_WAVMODEL
  if (blocktype==AUDIO) return wavModel(*m, blockinfo, Stats), m->p();
  #endif //USE_WAVMODEL
  if ((blocktype==JPEG || blocktype==HDR)) {
    if(jpegModel==nullptr)jpegModel=new JpegModel();
    if (jpegModel->jpegModel(*m)) return m->p(1,0);
  }

  // Normal model
  if (bpos==0)
    UpdateContexts(buf(1));
  int order=cm.mix(*m);

  rcm7.mix(*m);
  rcm9.mix(*m);
  rcm10.mix(*m);

  if (level>=4 && blocktype!=IMAGE1) {
    sparseModel(*m,matchlength,order);
    distanceModel(*m);
    recordModel(*m, Stats);
    #ifdef USE_WORDMODEL
    wordModel(*m, Stats);
    #endif //USE_WORDMODEL
    indirectModel(*m);
    dmcforest->mix(*m);
    nestModel(*m);
    XMLModel(*m, Stats);
    #ifdef USE_TEXTMODEL
    textModel.Predict(*m, buf, Stats);
    #endif //USE_TEXTMODEL
    if (blocktype!=TEXT && blocktype!=TEXT_EOL)
      exeModel1.Predict(*m, blocktype==EXE, Stats);
  }

  order = order-2;
  if (order<0) order=0;

  U32 c1=buf(1), c2=buf(2), c3=buf(3), c;

  m->set(8+(c1 | (bpos>5)<<8 |  ( ((c0&((1<<bpos)-1))==0) || (c0==((2<<bpos)-1)) )<<9), 8+1024);
  m->set(c0, 256);
  m->set(order | ((c4>>6)&3)<<3 | (bpos==0)<<5 | (c1==c2)<<6 | (blocktype==EXE)<<7, 256);
  m->set(c2, 256);
  m->set(c3, 256);
  m->set(matchlength, 256);

  if (bpos!=0)
  {
    c=c0<<(8-bpos); if (bpos==1)c|=c3>>1;
    c=min(bpos,5)<<8 | c1>>5 | (c2>>5)<<3 | (c&192);
  }
  else c=c3>>7 | (c4>>31)<<1 | (c2>>6)<<2 | (c1&240);
  m->set(c, 1536);
  int pr=m->p(1,1);
  return pr;
}

//////////////////////////// Predictor /////////////////////////

// A Predictor estimates the probability that the next bit of
// uncompressed data is 1.  Methods:
// p() returns P(1) as a 12 bit number (0-4095).
// update(y) trains the predictor with the actual bit (0 or 1).

class Predictor {
  int pr;  // next prediction
  ContextModel contextModel;
  APM<24> APMs[4];
  APM1 APM1s[7];
  ModelStats stats;
public:
  Predictor();
  int p() const {return pr;}
  void update();
};

Predictor::Predictor() : pr(2048), APMs{ 256, 0x10000, 0x10000, 0x10000 }, APM1s{ 256, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000 } {
  memset(&stats, 0, sizeof(ModelStats));
}

void Predictor::update() {
  // Update global context: pos, bpos, c0, c4, buf, f4
  c0+=c0+y;
  if (c0>=256) {
    buf[pos++]=c0;
    c4=(c4<<8)+c0-256;
    c0=1;

    int b1=buf(1);
    b2=b1;
    if(b1=='.' || b1=='!' || b1=='?' || b1=='/'|| b1==')'|| b1=='}') f4=(f4&0xfffffff0)+2;
    if (b1==32) --b1;
    f4=f4*16+(b1>>4);

  }
  bpos=(bpos+1)&7;

  int pr0=contextModel.Predict(&stats);
  if (contextModel.Bypass) {
    pr=pr0;
    return;
  }

  // Filter the context model with APMs
  int pr1, pr2, pr3;
  if (stats.blockType==TEXT || stats.blockType==TEXT_EOL){
    pr =APMs[0].p(pr0, c0, 0x3FF);
    pr1=APMs[1].p(pr0, c0+256*buf(1), 0x3FF);
    pr2=APMs[2].p(pr0, (c0^hash(buf(1), buf(2)))&0xffff, 0x3FF);
    pr3=APMs[3].p(pr0, (c0^hash(buf(1), buf(2), buf(3)))&0xffff, 0x3FF);
  }
  else{
    pr =APM1s[0].p(pr0, c0);
    pr1=APM1s[1].p(pr0, c0+256*buf(1));
    pr2=APM1s[2].p(pr0, (c0^hash(buf(1), buf(2)))&0xffff);
    pr3=APM1s[3].p(pr0, (c0^hash(buf(1), buf(2), buf(3)))&0xffff);
  }
  pr0=(pr0+pr1+pr2+pr3+2)>>2;

  pr1=APM1s[4].p(pr, c0+256*buf(1));
  pr2=APM1s[5].p(pr, (c0^hash(buf(1), buf(2)))&0xffff);
  pr3=APM1s[6].p(pr, (c0^hash(buf(1), buf(2), buf(3)))&0xffff);
  pr=(pr+pr1+pr2+pr3+2)>>2;

  pr=(pr+pr0+1)>>1;
}

//////////////////////////// Encoder ////////////////////////////

// An Encoder does arithmetic encoding.  Methods:
// Encoder(COMPRESS, f) creates encoder for compression to archive f, which
//   must be open past any header for writing in binary mode.
// Encoder(DECOMPRESS, f) creates encoder for decompression from archive f,
//   which must be open past any header for reading in binary mode.
// code(i) in COMPRESS mode compresses bit i (0 or 1) to file f.
// code() in DECOMPRESS mode returns the next decompressed bit from file f.
//   Global y is set to the last bit coded or decoded by code().
// compress(c) in COMPRESS mode compresses one byte.
// decompress() in DECOMPRESS mode decompresses and returns one byte.
// flush() should be called exactly once after compression is done and
//   before closing f.  It does nothing in DECOMPRESS mode.
// size() returns current length of archive
// setFile(f) sets alternate source to File *f for decompress() in COMPRESS
//   mode (for testing transforms).
// If level (global) is 0, then data is stored without arithmetic coding.

typedef enum {COMPRESS, DECOMPRESS} Mode;
class Encoder {
private:
  Predictor predictor;
  const Mode mode;       // Compress or decompress?
  File *archive;         // Compressed data file
  U32 x1, x2;            // Range, initially [0, 1), scaled by 2^32
  U32 x;                 // Decompress mode: last 4 input bytes of archive
  File *alt;             // decompress() source in COMPRESS mode
  float p1, p2;          // percentages for progress indicator: 0.0 .. 1.0

  // Compress bit y or return decompressed bit
  int code(int i=0) {
    int p=predictor.p();
    if(p==0)p++;
    assert(p>0 && p<4096);
    U32 xmid=x1 + ((x2-x1)>>12)*p + (((x2-x1)&0xfff)*p>>12);
    assert(xmid>=x1 && xmid<x2);
    if (mode==DECOMPRESS) y=x<=xmid; else y=i;
    y ? (x2=xmid) : (x1=xmid+1);
    predictor.update();
    while (((x1^x2)&0xff000000)==0) {  // pass equal leading bytes of range
      if (mode==COMPRESS) archive->putchar(x2>>24);
      x1<<=8;
      x2=(x2<<8)+255;
      if (mode==DECOMPRESS) x=(x<<8)+(archive->getchar()&255);  // EOF is OK
    }
    return y;
  }

public:
  Encoder(Mode m, File *f);
  Mode getMode() const {return mode;}
  U64 size() const {return archive->curpos();}  // length of archive so far
  void flush();  // call this when compression is finished
  void setFile(File *f) {alt=f;}

  // Compress one byte
  void compress(int c) {
    assert(mode==COMPRESS);
    if (level==0)
      archive->putchar(c);
    else
      for (int i=7; i>=0; --i)
        code((c>>i)&1);
  }

  // Decompress and return one byte
  int decompress() {
    if (mode==COMPRESS) {
      assert(alt);
      return alt->getchar();
    }
    else if (level==0)
      return archive->getchar();
    else {
      int c=0;
      for (int i=0; i<8; ++i)
        c+=c+code();
      return c;
    }
  }

  void encode_blocksize(U64 blocksize) {
    //TODO: Large file support
    compress((blocksize>>24)&0xFF);
    compress((blocksize>>16)&0xFF);
    compress((blocksize>>8)&0xFF);
    compress((blocksize)&0xFF);
  }

  U64 decode_blocksize() {
    //TODO: Large file support
    U64 blocksize=0;
    blocksize =U64(decompress())<<24;
    blocksize|=U64(decompress())<<16;
    blocksize|=U64(decompress())<<8;
    blocksize|=U64(decompress());
    return blocksize;
  }

  void set_status_range(float perc1, float perc2) { p1=perc1; p2=perc2; }
  void print_status(U64 n, U64 size) {
    if (to_screen)
      printf("%6.2f%%\b\b\b\b\b\b\b", (p1+(p2-p1)*n/(size+1))*100), fflush(stdout);
  }
  void print_status() {
    if (to_screen)
      printf("%6.2f%%\b\b\b\b\b\b\b", float(size())/(p2+1)*100), fflush(stdout);
  }
};

Encoder::Encoder(Mode m, File *f):
    mode(m), archive(f), x1(0), x2(0xffffffff), x(0), alt(0) {
  if (mode==DECOMPRESS) {
    U64 start=size();
    archive->setend();
    U64 end=size();
    if (end>=(1u<<31))quit("Large archives not yet supported.");
    set_status_range(0.0, (float)end);
    archive->setpos(start);
  }
  if (level>0 && mode==DECOMPRESS) {  // x = first 4 bytes of archive
    for (int i=0; i<4; ++i)
      x=(x<<8)+(archive->getchar()&255);
  }
}

void Encoder::flush() {
  if (mode==COMPRESS && level>0)
    archive->putchar(x1>>24);  // Flush first unequal byte of range
}

/////////////////////////// Filters /////////////////////////////////
//
// Before compression, data is encoded in blocks with the following format:
//
//   <type> <size> <encoded-data>
//
// Type is 1 byte (type Blocktype): DEFAULT=0, JPEG, EXE
// Size is 4 bytes in big-endian format.
// Encoded-data decodes to <size> bytes.  The encoded size might be
// different.  Encoded data is designed to be more compressible.
//
//   void encode(File *in, File *out, int n);
//
// Reads n bytes of in (open in "rb" mode) and encodes one or
// more blocks to temporary file out (open in "wb+" mode).
// The file pointer of in is advanced n bytes.  The file pointer of
// out is positioned after the last byte written.
//
//   en.setFile(File *out);
//   int decode(Encoder& en);
//
// Decodes and returns one byte.  Input is from en.decompress(), which
// reads from out if in COMPRESS mode.  During compression, n calls
// to decode() must exactly match n bytes of in, or else it is compressed
// as type 0 without encoding.
//
//   Blocktype detect(File *in, int n, Blocktype type);
//
// Reads n bytes of in, and detects when the type changes to
// something else.  If it does, then the file pointer is repositioned
// to the start of the change and the new type is returned.  If the type
// does not change, then it repositions the file pointer n bytes ahead
// and returns the old type.
//
// For each type X there are the following 2 functions:
//
//   void encode_X(File *in, File *out, int n, ...);
//
// encodes n bytes from in to out.
//
//   int decode_X(Encoder& en);
//
// decodes one byte from en and returns it.  decode() and decode_X()
// maintain state information using static variables.
#define bswap(x) \
+   ((((x) & 0xff000000) >> 24) | \
+    (((x) & 0x00ff0000) >>  8) | \
+    (((x) & 0x0000ff00) <<  8) | \
+    (((x) & 0x000000ff) << 24))

#define IMG_DET(type,start_pos,header_len,width,height) return dett=(type),\
deth=(header_len),detd=(width)*(height),info=(width),\
in->setpos(start+(start_pos)),HDR

#define AUD_DET(type,start_pos,header_len,data_len,wmode) return dett=(type),\
deth=(header_len),detd=(data_len),info=(wmode),\
in->setpos(start+(start_pos)),HDR


// Function ecc_compute(), edc_compute() and eccedc_init() taken from
// ** UNECM - Decoder for ECM (Error Code Modeler) format.
// ** Version 1.0
// ** Copyright (C) 2002 Neill Corlett

/* LUTs used for computing ECC/EDC */
static U8 ecc_f_lut[256];
static U8 ecc_b_lut[256];
static U32 edc_lut[256];
static int luts_init=0;

void eccedc_init(void) {
  if (luts_init) return;
  U32 i, j, edc;
  for(i = 0; i < 256; i++) {
    j = (i << 1) ^ (i & 0x80 ? 0x11D : 0);
    ecc_f_lut[i] = j;
    ecc_b_lut[i ^ j] = i;
    edc = i;
    for(j = 0; j < 8; j++) edc = (edc >> 1) ^ (edc & 1 ? 0xD8018001 : 0);
    edc_lut[i] = edc;
  }
  luts_init=1;
}

void ecc_compute(U8 *src, U32 major_count, U32 minor_count, U32 major_mult, U32 minor_inc, U8 *dest) {
  U32 size = major_count * minor_count;
  U32 major, minor;
  for(major = 0; major < major_count; major++) {
    U32 index = (major >> 1) * major_mult + (major & 1);
    U8 ecc_a = 0;
    U8 ecc_b = 0;
    for(minor = 0; minor < minor_count; minor++) {
      U8 temp = src[index];
      index += minor_inc;
      if(index >= size) index -= size;
      ecc_a ^= temp;
      ecc_b ^= temp;
      ecc_a = ecc_f_lut[ecc_a];
    }
    ecc_a = ecc_b_lut[ecc_f_lut[ecc_a] ^ ecc_b];
    dest[major              ] = ecc_a;
    dest[major + major_count] = ecc_a ^ ecc_b;
  }
}

U32 edc_compute(const U8  *src, int size) {
  U32 edc = 0;
  while(size--) edc = (edc >> 8) ^ edc_lut[(edc ^ (*src++)) & 0xFF];
  return edc;
}

int expand_cd_sector(U8 *data, int address, int test) {
  U8 d2[2352];
  eccedc_init();
  //sync pattern: 00 FF FF FF FF FF FF FF FF FF FF 00
  d2[0]=d2[11]=0;
  for (int i=1; i<11; i++) d2[i]=255;
  //determine Mode and Form
  int mode=(data[15]!=1?2:1);
  int form=(data[15]==3?2:1);
  //address (Minutes, Seconds, Sectors)
  if (address==-1) for (int i=12; i<15; i++) d2[i]=data[i]; else {
    int c1=(address&15)+((address>>4)&15)*10;
    int c2=((address>>8)&15)+((address>>12)&15)*10;
    int c3=((address>>16)&15)+((address>>20)&15)*10;
    c1=(c1+1)%75;
    if (c1==0) {
      c2=(c2+1)%60;
      if (c2==0) c3++;
    }
    d2[12]=(c3%10)+16*(c3/10);
    d2[13]=(c2%10)+16*(c2/10);
    d2[14]=(c1%10)+16*(c1/10);
  }
  d2[15]=mode;
  if (mode==2) for (int i=16; i<24; i++) d2[i]=data[i-4*(i>=20)]; //8 byte subheader
  if (form==1) {
    if (mode==2) {
      d2[1]=d2[12],d2[2]=d2[13],d2[3]=d2[14];
      d2[12]=d2[13]=d2[14]=d2[15]=0;
    } else {
      for(int i=2068; i<2076; i++) d2[i]=0; //Mode1: reserved 8 (zero) bytes
    }
    for (int i=16+8*(mode==2); i<2064+8*(mode==2); i++) d2[i]=data[i]; //data bytes
    U32 edc=edc_compute(d2+16*(mode==2), 2064-8*(mode==2));
    for (int i=0; i<4; i++) d2[2064+8*(mode==2)+i]=(edc>>(8*i))&0xff;
    ecc_compute(d2+12, 86, 24,  2, 86, d2+2076);
    ecc_compute(d2+12, 52, 43, 86, 88, d2+2248);
    if (mode==2) {
      d2[12]=d2[1],d2[13]=d2[2],d2[14]=d2[3],d2[15]=2;
      d2[1]=d2[2]=d2[3]=255;
    }
  }
  for (int i=0; i<2352; i++) if (d2[i]!=data[i] && test) form=2;
  if (form==2) {
    for (int i=24; i<2348; i++) d2[i]=data[i]; //data bytes
    U32 edc=edc_compute(d2+16, 2332);
    for (int i=0; i<4; i++) d2[2348+i]=(edc>>(8*i))&0xff; //EDC
  }
  for (int i=0; i<2352; i++) if (d2[i]!=data[i] && test) return 0; else data[i]=d2[i];
  return mode+form-1;
}

#ifdef USE_ZLIB
int parse_zlib_header(int header) {
    switch (header) {
        case 0x2815 : return 0;  case 0x2853 : return 1;  case 0x2891 : return 2;  case 0x28cf : return 3;
        case 0x3811 : return 4;  case 0x384f : return 5;  case 0x388d : return 6;  case 0x38cb : return 7;
        case 0x480d : return 8;  case 0x484b : return 9;  case 0x4889 : return 10; case 0x48c7 : return 11;
        case 0x5809 : return 12; case 0x5847 : return 13; case 0x5885 : return 14; case 0x58c3 : return 15;
        case 0x6805 : return 16; case 0x6843 : return 17; case 0x6881 : return 18; case 0x68de : return 19;
        case 0x7801 : return 20; case 0x785e : return 21; case 0x789c : return 22; case 0x78da : return 23;
    }
    return -1;
}
int zlib_inflateInit(z_streamp strm, int zh) {
    if (zh==-1) return inflateInit2(strm, -MAX_WBITS); else return inflateInit(strm);
}
#endif //USE_ZLIB

bool IsGrayscalePalette(File *in, int n = 256, int isRGBA = 0){
  U64 offset = in->curpos();
  int stride = 3+isRGBA, res = (n>0)<<8, order=1;
  for (int i = 0; (i < n*stride) && (res>>8); i++) {
    int b = in->getchar();
    if (b==EOF){
      res = 0;
      break;
    }
    if (!i) {
      res = 0x100|b;
      order = 1-2*(b>0);
      continue;
    }

    //"j" is the index of the current byte in this color entry
    int j = i%stride;
    if (!j){
      // load first component of this entry
      res = (res&((b-(res&0xFF)==order)<<8));
      res|=(res)?b:0;
    }
    else if (j==3)
      res&=((!b || (b==0xFF))*0x1FF); // alpha/attribute component must be zero or 0xFF
    else
      res&=((b==(res&0xFF))<<9)-1;
  }
  in->setpos(offset);
  return (res>>8)>0;
}

#define MIN_TEXT_SIZE 0x400 //1KB
#define MAX_TEXT_MISSES 2 //number of misses in last 32 bytes before resetting
struct TextInfo {
  U64 start;
  U32 lastSpace;
  U32 lastNL;
  U32 wordLength;
  U32 misses;
  U32 missCount;
  U32 zeroRun;
  U32 spaceRun;
  bool isLetter, isUTF8, needsEolTransform, seenNL;
};

// Detect blocks
Blocktype detect(File *in, U64 blocksize, Blocktype type, int &info) {
  //TODO: Large file support
  int n = (int)blocksize;
  U32 buf3=0, buf2=0, buf1=0, buf0=0;  // last 16 bytes
  U64 start= in->curpos();

  // For EXE detection
  Array<int> abspos(256),  // CALL/JMP abs. addr. low byte -> last offset
    relpos(256);    // CALL/JMP relative addr. low byte -> last offset
  int e8e9count=0;  // number of consecutive CALL/JMPs
  int e8e9pos=0;    // offset of first CALL or JMP instruction
  int e8e9last=0;   // offset of most recent CALL or JMP

  int soi=0, sof=0, sos=0, app=0;  // For JPEG detection - position where found
#ifdef USE_WAVMODEL
  int wavi=0,wavsize=0,wavch=0,wavbps=0,wavm=0,wavtype=0,wavlen=0,wavlist=0;  // For WAVE detection
  int aiff=0,aiffm=0,aiffs=0;  // For AIFF detection
  int s3mi=0,s3mno=0,s3mni=0;  // For S3M detection
#endif //  USE_WAVMODEL
  int bmp=0,imgbpp=0,bmpx=0,bmpy=0,bmpof=0,bmps=0,hdrless=0;  // For BMP detection
  int rgbi=0,rgbx=0,rgby=0;  // For RGB detection
  int tga=0,tgax=0,tgay=0,tgaz=0,tgat=0,tgaid=0,tgamap=0;  // For TGA detection
  int pgm=0,pgmcomment=0,pgmw=0,pgmh=0,pgm_ptr=0,pgmc=0,pgmn=0,pamatr=0,pamd=0;  // For PBM, PGM, PPM, PAM detection
  char pgm_buf[32];
  int cdi=0,cda=0,cdm=0;  // For CD sectors detection
  U32 cdf=0;
  unsigned char zbuf[256+32] = {0}, zin[1<<16] = {0}, zout[1<<16] = {0}; // For ZLIB stream detection
  int zbufpos=0,zzippos=-1, histogram[256]={0};
  int pdfim=0,pdfimw=0,pdfimh=0,pdfimb=0,pdfgray=0,pdfimp=0;
  int b64s=0,b64i=0,b64line=0,b64nl=0; // For base64 detection
  int gif=0,gifa=0,gifi=0,gifw=0,gifc=0,gifb=0,gifplt=0,gifgray=0; // For GIF detection
  int png=0, pngw=0, pngh=0, pngbps=0, pngtype=0, pnggray=0, lastchunk=0, nextchunk=0; // For PNG detection
  TextInfo text = {0};
  // For image detection
  static int deth=0,detd=0;  // detected header/data size in bytes
  static Blocktype dett;  // detected block type
  if (deth) {in->setpos(start+deth);deth=0;return dett;}
  else if (detd) {in->setpos(start+min(blocksize, (U64)detd));detd=0;return DEFAULT;}

  for (int i=0; i<n; ++i) {
    int c=in->getchar();
    if (c==EOF) return (Blocktype)(-1);
    buf3=buf3<<8|buf2>>24;
    buf2=buf2<<8|buf1>>24;
    buf1=buf1<<8|buf0>>24;
    buf0=buf0<<8|c;

    // detect PNG images
    if (!png && buf3==0x89504E47 /*%PNG*/ && buf2==0x0D0A1A0A && buf1==0x0000000D && buf0==0x49484452) png=i, pngtype=-1, lastchunk=buf3;
    if (png){
      const int p=i-png;
      if (p==12){
        pngw = buf2;
        pngh = buf1;
        pngbps = buf0>>24;
        pngtype = (U8)(buf0>>16);
        pnggray = 0;
        png*=((buf0&0xFFFF)==0 && pngw && pngh && pngbps==8 && (!pngtype || pngtype==2 || pngtype==3 || pngtype==4 || pngtype==6));
      }
      else if (p>12 && pngtype<0)
        png = 0;
      else if (p==17){
        png*=((buf1&0xFF)==0);
        nextchunk =(png)?i+8:0;
      }
      else if (p>17 && i==nextchunk){
        nextchunk+=buf1+4/*CRC*/+8/*Chunk length+Id*/;
        lastchunk = buf0;
        png*=(lastchunk!=0x49454E44/*IEND*/);
        if (lastchunk==0x504C5445/*PLTE*/){
          png*=(buf1%3==0);
          pnggray = (png && IsGrayscalePalette(in, buf1/3));
        }
      }
    }

    // ZLIB stream detection
    #ifdef USE_ZLIB
    histogram[c]++;
    if (i>=256)
      histogram[zbuf[zbufpos]]--;
    zbuf[zbufpos] = c;
    if (zbufpos<32)
      zbuf[zbufpos+256] = c;
    zbufpos=(zbufpos+1)&0xFF;

    int zh=parse_zlib_header(((int)zbuf[(zbufpos-32)&0xFF])*256+(int)zbuf[(zbufpos-32+1)&0xFF]);
    bool valid = (i>=31 && zh!=-1);
    if (!valid && options&OPTION_BRUTE && i>=255){
      U8 BTYPE = (zbuf[zbufpos]&7)>>1;
      if ((valid=(BTYPE==1 || BTYPE==2))){
        int maximum=0, used=0, offset=zbufpos;
        for (int i=0;i<4;i++,offset+=64){
          for (int j=0;j<64;j++){
            int freq = histogram[zbuf[(offset+j)&0xFF]];
            used+=(freq>0);
            maximum+=(freq>maximum);
          }
          if (maximum>=((12+i)<<i) || used*(6-i)<(i+1)*64){
            valid = false;
            break;
          }
        }
      }
    }
    if (valid || zzippos==i) {
      int streamLength=0, ret=0, brute=(zh==-1 && zzippos!=i);

      // Quick check possible stream by decompressing first 32 bytes
      z_stream strm;
      strm.zalloc=Z_NULL; strm.zfree=Z_NULL; strm.opaque=Z_NULL;
      strm.next_in=Z_NULL; strm.avail_in=0;
      if (zlib_inflateInit(&strm,zh)==Z_OK) {
        strm.next_in=&zbuf[(zbufpos-(brute?0:32))&0xFF]; strm.avail_in=32;
        strm.next_out=zout; strm.avail_out=1<<16;
        ret=inflate(&strm, Z_FINISH);
        ret=(inflateEnd(&strm)==Z_OK && (ret==Z_STREAM_END || ret==Z_BUF_ERROR) && strm.total_in>=16);
      }
      if (ret) {
        // Verify valid stream and determine stream length
        U64 savedpos=in->curpos();
        strm.zalloc=Z_NULL; strm.zfree=Z_NULL; strm.opaque=Z_NULL;
        strm.next_in=Z_NULL; strm.avail_in=0; strm.total_in=strm.total_out=0;
        if (zlib_inflateInit(&strm,zh)==Z_OK) {
          for (int j=i-(brute?255:31); j<n; j+=1<<16) {
            unsigned int blsize=min(n-j,1<<16);
            in->setpos(start+j);
            if (in->blockread(zin, blsize)!=blsize) break;
            strm.next_in=zin; strm.avail_in=blsize;
            do {
              strm.next_out=zout; strm.avail_out=1<<16;
              ret=inflate(&strm, Z_FINISH);
            } while (strm.avail_out==0 && ret==Z_BUF_ERROR);
            if (ret==Z_STREAM_END) streamLength=strm.total_in;
            if (ret!=Z_BUF_ERROR) break;
          }
          if (inflateEnd(&strm)!=Z_OK) streamLength=0;
        }
        in->setpos(savedpos);
      }
      if (streamLength>(brute<<7)) {
        info=0;
        if (pdfimw>0 && pdfimw<0x1000000 && pdfimh>0) {
          if (pdfimb==8 && (int)strm.total_out==pdfimw*pdfimh) info=((pdfgray?IMAGE8GRAY:IMAGE8)<<24)|pdfimw;
          if (pdfimb==8 && (int)strm.total_out==pdfimw*pdfimh*3) info=(IMAGE24<<24)|pdfimw*3;
          if (pdfimb==4 && (int)strm.total_out==((pdfimw+1)/2)*pdfimh) info=(IMAGE4<<24)|((pdfimw+1)/2);
          if (pdfimb==1 && (int)strm.total_out==((pdfimw+7)/8)*pdfimh) info=(IMAGE1<<24)|((pdfimw+7)/8);
          pdfgray=0;
        }
        else if (png && pngw<0x1000000 && lastchunk==0x49444154/*IDAT*/){
          if (pngbps==8 && pngtype==2 && (int)strm.total_out==(pngw*3+1)*pngh) info=(PNG24<<24)|(pngw*3), png=0;
          else if (pngbps==8 && pngtype==6 && (int)strm.total_out==(pngw*4+1)*pngh) info=(PNG32<<24)|(pngw*4), png=0;
          else if (pngbps==8 && (!pngtype || pngtype==3) && (int)strm.total_out==(pngw+1)*pngh) info=(((!pngtype || pnggray)?PNG8GRAY:PNG8)<<24)|(pngw), png=0;
        }
        in->setpos(start+i-(brute?255:31));
        detd=streamLength;
        return ZLIB;
      }
    }
    if (zh==-1 && zbuf[(zbufpos-32)&0xFF]=='P' && zbuf[(zbufpos-32+1)&0xFF]=='K' && zbuf[(zbufpos-32+2)&0xFF]=='\x3'
      && zbuf[(zbufpos-32+3)&0xFF]=='\x4' && zbuf[(zbufpos-32+8)&0xFF]=='\x8' && zbuf[(zbufpos-32+9)&0xFF]=='\0') {
        int nlen=(int)zbuf[(zbufpos-32+26)&0xFF]+((int)zbuf[(zbufpos-32+27)&0xFF])*256
                +(int)zbuf[(zbufpos-32+28)&0xFF]+((int)zbuf[(zbufpos-32+29)&0xFF])*256;
        if (nlen<256 && i+30+nlen<n) zzippos=i+30+nlen;
    }
    #endif //USE_ZLIB

    if (i-pdfimp>1024) pdfim=pdfimw=pdfimh=pdfimb=pdfgray=0;
    if (pdfim>1 && !(isspace(c) || isdigit(c))) pdfim=1;
    if (pdfim==2 && isdigit(c)) pdfimw=pdfimw*10+(c-'0');
    if (pdfim==3 && isdigit(c)) pdfimh=pdfimh*10+(c-'0');
    if (pdfim==4 && isdigit(c)) pdfimb=pdfimb*10+(c-'0');
    if ((buf0&0xffff)==0x3c3c) pdfimp=i,pdfim=1; // <<
    if (pdfim && (buf1&0xffff)==0x2f57 && buf0==0x69647468) pdfim=2,pdfimw=0; // /Width
    if (pdfim && (buf1&0xffffff)==0x2f4865 && buf0==0x69676874) pdfim=3,pdfimh=0; // /Height
    if (pdfim && buf3==0x42697473 && buf2==0x50657243 && buf1==0x6f6d706f
       && buf0==0x6e656e74 && zbuf[(zbufpos-32+15)&0xFF]=='/') pdfim=4,pdfimb=0; // /BitsPerComponent
    if (pdfim && (buf2&0xFFFFFF)==0x2F4465 && buf1==0x76696365 && buf0==0x47726179) pdfgray=1; // /DeviceGray

    // CD sectors detection (mode 1 and mode 2 form 1+2 - 2352 bytes)
    if (buf1==0x00ffffff && buf0==0xffffffff && !cdi) cdi=i,cda=-1,cdm=0;
    if (cdi && i>cdi) {
      const int p=(i-cdi)%2352;
      if (p==8 && (buf1!=0xffffff00 || ((buf0&0xff)!=1 && (buf0&0xff)!=2))) cdi=0;
      else if (p==16 && i+2336<n) {
        U8 data[2352];
        U64 savedpos=in->curpos();
        in->setpos(start+i-23);
        in->blockread(data, 2352);
        in->setpos(savedpos);
        int t=expand_cd_sector(data, cda, 1);
        if (t!=cdm) cdm=t*(i-cdi<2352);
        if (cdm && cda!=10 && (cdm==1 || buf0==buf1)) {
          if (type!=CD) return info=cdm, in->setpos(start+cdi-7), CD;
          cda=(data[12]<<16)+(data[13]<<8)+data[14];
          if (cdm!=1 && i-cdi>2352 && buf0!=cdf) cda=10;
          if (cdm!=1) cdf=buf0;
        } else cdi=0;
      }
      if (!cdi && type==CD) {
        in->setpos(start+i-p-7);
        return DEFAULT;
      }
    }
    if (type==CD) continue;

    // Detect JPEG by code SOI APPx (FF D8 FF Ex) followed by
    // SOF0 (FF C0 xx xx 08) and SOS (FF DA) within a reasonable distance.
    // Detect end by any code other than RST0-RST7 (FF D9-D7) or
    // a byte stuff (FF 00).

    if (!soi && i>=3 && (buf0&0xffffff00)==0xffd8ff00 && ((buf0&0xFE)==0xC0 || (U8)buf0==0xC4 || ((U8)buf0>=0xDB && (U8)buf0<=0xFE) )) soi=i, app=i+2, sos=sof=0;
    if (soi) {
      if (app==i && (buf0>>24)==0xff &&
         ((buf0>>16)&0xff)>0xc1 && ((buf0>>16)&0xff)<0xff) app=i+(buf0&0xffff)+2;
      if (app<i && (buf1&0xff)==0xff && (buf0&0xfe0000ff)==0xc0000008) sof=i;
      if (sof && sof>soi && i-sof<0x1000 && (buf0&0xffff)==0xffda) {
        sos=i;
        if (type!=JPEG) return in->setpos(start+soi-3), JPEG;
      }
      if (i-soi>0x40000 && !sos) soi=0;
    }
    if (type==JPEG && sos && i>sos && (buf0&0xff00)==0xff00
        && (buf0&0xff)!=0 && (buf0&0xf8)!=0xd0) return DEFAULT;
#ifdef USE_WAVMODEL
    // Detect .wav file header
    if (buf0==0x52494646) wavi=i,wavm=wavlen=0;
    if (wavi) {
      int p=i-wavi;
      if (p==4) wavsize=bswap(buf0);
      else if (p==8){
        wavtype=(buf0==0x57415645)?1:(buf0==0x7366626B)?2:0;
        if (!wavtype) wavi=0;
      }
      else if (wavtype){
        if (wavtype==1) {
          if (p==16+wavlen && (buf1!=0x666d7420 || bswap(buf0)!=16)) wavlen=((bswap(buf0)+1)&(-2))+8, wavi*=(buf1==0x666d7420 && bswap(buf0)!=16);
          else if (p==22+wavlen) wavch=bswap(buf0)&0xffff;
          else if (p==34+wavlen) wavbps=bswap(buf0)&0xffff;
          else if (p==40+wavlen+wavm && buf1!=0x64617461) wavm+=((bswap(buf0)+1)&(-2))+8,wavi=(wavm>0xfffff?0:wavi);
          else if (p==40+wavlen+wavm) {
            int wavd=bswap(buf0);
            wavlen=0;
            if ((wavch==1 || wavch==2) && (wavbps==8 || wavbps==16) && wavd>0 && wavsize>=wavd+36
               && wavd%((wavbps/8)*wavch)==0) AUD_DET(AUDIO,wavi-3,44+wavm,wavd,wavch+wavbps/4-3);
            wavi=0;
          }
        }
        else{
          if ((p==16 && buf1!=0x4C495354) || (p==20 && buf0!=0x494E464F))
            wavi=0;
          else if (p>20 && buf1==0x4C495354 && (wavi*=(buf0!=0))){
            wavlen = bswap(buf0);
            wavlist = i;
          }
          else if (wavlist){
            p=i-wavlist;
            if (p==8 && (buf1!=0x73647461 || buf0!=0x736D706C))
              wavi=0;
            else if (p==12){
              int wavd = bswap(buf0);
              if (wavd && (wavd+12)==wavlen)
                AUD_DET(AUDIO,wavi-3,(12+wavlist-(wavi-3)+1)&~1,wavd,1+16/4-3);
              wavi=0;
            }
          }
        }
      }
    }

    // Detect .aiff file header
    if (buf0==0x464f524d) aiff=i,aiffs=0; // FORM
    if (aiff) {
      const int p=i-aiff;
      if (p==12 && (buf1!=0x41494646 || buf0!=0x434f4d4d)) aiff=0; // AIFF COMM
      else if (p==24) {
        const int bits=buf0&0xffff, chn=buf1>>16;
        if ((bits==8 || bits==16) && (chn==1 || chn==2)) aiffm=chn+bits/4+1; else aiff=0;
      } else if (p==42+aiffs && buf1!=0x53534e44) aiffs+=(buf0+8)+(buf0&1),aiff=(aiffs>0x400?0:aiff);
      else if (p==42+aiffs) AUD_DET(AUDIO,aiff-3,54+aiffs,buf0-8,aiffm);
    }

    // Detect .mod file header
    if ((buf0==0x4d2e4b2e || buf0==0x3643484e || buf0==0x3843484e  // M.K. 6CHN 8CHN
       || buf0==0x464c5434 || buf0==0x464c5438) && (buf1&0xc0c0c0c0)==0 && i>=1083) {
      U64 savedpos=in->curpos();
      const int chn=((buf0>>24)==0x36?6:(((buf0>>24)==0x38 || (buf0&0xff)==0x38)?8:4));
      int len=0; // total length of samples
      int numpat=1; // number of patterns
      for (int j=0; j<31; j++) {
        in->setpos(start+i-1083+42+j*30);
        const int i1=in->getchar();
        const int i2=in->getchar();
        len+=i1*512+i2*2;
      }
      in->setpos(start+i-131);
      for (int j=0; j<128; j++) {
        int x=in->getchar();
        if (x+1>numpat) numpat=x+1;
      }
      if (numpat<65) AUD_DET(AUDIO,i-1083,1084+numpat*256*chn,len,4);
      in->setpos(savedpos);
    }

    // Detect .s3m file header
    if (buf0==0x1a100000) s3mi=i,s3mno=s3mni=0;
    if (s3mi) {
      const int p=i-s3mi;
      if (p==4) s3mno=bswap(buf0)&0xffff,s3mni=(bswap(buf0)>>16);
      else if (p==16 && (((buf1>>16)&0xff)!=0x13 || buf0!=0x5343524d)) s3mi=0;
      else if (p==16) {
        U64 savedpos=in->curpos();
        int b[31],sam_start=(1<<16),sam_end=0,ok=1;
        for (int j=0;j<s3mni;j++) {
          in->setpos(start+s3mi-31+0x60+s3mno+j*2);
          int i1=in->getchar();
          i1+=in->getchar()*256;
          in->setpos(start+s3mi-31+i1*16);
          i1=in->getchar();
          if (i1==1) { // type: sample
            for (int k=0;k<31;k++) b[k]=in->getchar();
            int len=b[15]+(b[16]<<8);
            int ofs=b[13]+(b[14]<<8);
            if (b[30]>1) ok=0;
            if (ofs*16<sam_start) sam_start=ofs*16;
            if (ofs*16+len>sam_end) sam_end=ofs*16+len;
          }
        }
        if (ok && sam_start<(1<<16)) AUD_DET(AUDIO,s3mi-31,sam_start,sam_end-sam_start,0);
        s3mi=0;
        in->setpos(savedpos);
      }
    }
#endif //  USE_WAVMODEL
    // Detect .bmp image
    if ( !(bmp || hdrless) && (((buf0&0xffff)==16973) || (!(buf0&0xFFFFFF) && ((buf0>>24)==0x28))) ) //possible 'BM' or headerless DIB
      imgbpp=bmpx=bmpy=0,hdrless=!(U8)buf0,bmpof=hdrless*54,bmp=i-hdrless*16;
    if (bmp || hdrless) {
      const int p=i-bmp;
      if (p==12) bmpof=bswap(buf0);
      else if (p==16 && buf0!=0x28000000) bmp=hdrless=0; //BITMAPINFOHEADER (0x28)
      else if (p==20) bmpx=bswap(buf0),bmp=((bmpx==0||bmpx>0x30000)?(hdrless=0):bmp); //width
      else if (p==24) bmpy=abs((int)bswap(buf0)),bmp=((bmpy==0||bmpy>0x10000)?(hdrless=0):bmp); //height
      else if (p==27) imgbpp=c,bmp=((imgbpp!=1 && imgbpp!=4 && imgbpp!=8 && imgbpp!=24 && imgbpp!=32)?(hdrless=0):bmp);
      else if ((p==31) && buf0) bmp=hdrless=0;
      else if (p==36) bmps=bswap(buf0);
      // check number of colors in palette (4 bytes), must be 0 (default) or <= 1<<bpp.
      // also check if image is too small, since it might not be worth it to use the image models
      else if (p==48){
        if ( (!buf0 || ((bswap(buf0)<=(U32)(1<<imgbpp)) && (imgbpp<=8))) && (((bmpx*bmpy*imgbpp)>>3)>64) ) {
          // possible icon/cursor?
          if (hdrless && (bmpx*2==bmpy) && imgbpp>1 &&
             (
              (bmps>0 && bmps==( (bmpx*bmpy*(imgbpp+1))>>4 )) ||
              ((!bmps || bmps<((bmpx*bmpy*imgbpp)>>3)) && (
               (bmpx==8)   || (bmpx==10) || (bmpx==14) || (bmpx==16) || (bmpx==20) ||
               (bmpx==22)  || (bmpx==24) || (bmpx==32) || (bmpx==40) || (bmpx==48) ||
               (bmpx==60)  || (bmpx==64) || (bmpx==72) || (bmpx==80) || (bmpx==96) ||
               (bmpx==128) || (bmpx==256)
              ))
             )
          )
            bmpy=bmpx;

          // if DIB and not 24bpp, we must calculate the data offset based on BPP or num. of entries in color palette
          if (hdrless && (imgbpp<24))
            bmpof+=((buf0)?bswap(buf0)*4:4<<imgbpp);
          bmpof+=(bmp-1)*(bmp<1);

          if (hdrless && bmps && bmps<((bmpx*bmpy*imgbpp)>>3)) { /*Guard against erroneous DIB detections*/ }
          else if (imgbpp==1) IMG_DET(IMAGE1,max(0,bmp-1),bmpof,(((bmpx-1)>>5)+1)*4,bmpy);
          else if (imgbpp==4) IMG_DET(IMAGE4,max(0,bmp-1),bmpof,((bmpx*4+31)>>5)*4,bmpy);
          else if (imgbpp==8){
            in->setpos(start+bmp+53);
            IMG_DET( (IsGrayscalePalette(in, (buf0)?bswap(buf0):1<<imgbpp, 1))?IMAGE8GRAY:IMAGE8,max(0,bmp-1),bmpof,(bmpx+3)&-4,bmpy);
          }
          else if (imgbpp==24) IMG_DET(IMAGE24,max(0,bmp-1),bmpof,((bmpx*3)+3)&-4,bmpy);
          else if (imgbpp==32) IMG_DET(IMAGE32,max(0,bmp-1),bmpof,bmpx*4,bmpy);
        }
        bmp=hdrless=0;
      }
    }

    // Detect .pbm .pgm .ppm .pam image
    if ((buf0&0xfff0ff)==0x50300a) { //"P0"
      pgmn=(buf0&0xf00)>>8;
      if ((pgmn>=4 && pgmn<=6) || pgmn==7) pgm=i,pgm_ptr=pgmw=pgmh=pgmc=pgmcomment=pamatr=pamd=0;
    }
    if (pgm) {
      if (i-pgm==1 && c==0x23) pgmcomment=1; //pgm comment
      if (!pgmcomment && pgm_ptr) {
        int s=0;
        if (pgmn==7) {
           if ((buf1&0xdfdf)==0x5749 && (buf0&0xdfdfdfff)==0x44544820) pgm_ptr=0, pamatr=1; // WIDTH
           if ((buf1&0xdfdfdf)==0x484549 && (buf0&0xdfdfdfff)==0x47485420) pgm_ptr=0, pamatr=2; // HEIGHT
           if ((buf1&0xdfdfdf)==0x4d4158 && (buf0&0xdfdfdfff)==0x56414c20) pgm_ptr=0, pamatr=3; // MAXVAL
           if ((buf1&0xdfdf)==0x4445 && (buf0&0xdfdfdfff)==0x50544820) pgm_ptr=0, pamatr=4; // DEPTH
           if ((buf2&0xdf)==0x54 && (buf1&0xdfdfdfdf)==0x55504c54 && (buf0&0xdfdfdfff)==0x59504520) pgm_ptr=0, pamatr=5; // TUPLTYPE
           if ((buf1&0xdfdfdf)==0x454e44 && (buf0&0xdfdfdfff)==0x4844520a) pgm_ptr=0, pamatr=6; // ENDHDR
           if (c==0x0a) {
             if (pamatr==0) pgm=0;
             else if (pamatr<5) s=pamatr;
             if (pamatr!=6) pamatr=0;
           }
        } else if (c==0x20 && !pgmw) s=1;
        else if (c==0x0a && !pgmh) s=2;
        else if (c==0x0a && !pgmc && pgmn!=4) s=3;
        if (s) {
          pgm_buf[pgm_ptr++]=0;
          int v=atoi(pgm_buf);
          if (s==1) pgmw=v; else if (s==2) pgmh=v; else if (s==3) pgmc=v; else if (s==4) pamd=v;
          if (v==0 || (s==3 && v>255)) pgm=0; else pgm_ptr=0;
        }
      }
      if (!pgmcomment) pgm_buf[pgm_ptr++]=c;
      if (pgm_ptr>=32) pgm=0;
      if (pgmcomment && c==0x0a) pgmcomment=0;
      if (pgmw && pgmh && !pgmc && pgmn==4) IMG_DET(IMAGE1,pgm-2,i-pgm+3,(pgmw+7)/8,pgmh);
      if (pgmw && pgmh && pgmc && (pgmn==5 || (pgmn==7 && pamd==1 && pamatr==6))) IMG_DET(IMAGE8GRAY,pgm-2,i-pgm+3,pgmw,pgmh);
      if (pgmw && pgmh && pgmc && (pgmn==6 || (pgmn==7 && pamd==3 && pamatr==6))) IMG_DET(IMAGE24,pgm-2,i-pgm+3,pgmw*3,pgmh);
      if (pgmw && pgmh && pgmc && (pgmn==7 && pamd==4 && pamatr==6)) IMG_DET(IMAGE32,pgm-2,i-pgm+3,pgmw*4,pgmh);
    }

    // Detect .rgb image
    if ((buf0&0xffff)==0x01da) rgbi=i,rgbx=rgby=0;
    if (rgbi) {
      const int p=i-rgbi;
      if (p==1 && c!=0) rgbi=0;
      else if (p==2 && c!=1) rgbi=0;
      else if (p==4 && (buf0&0xffff)!=1 && (buf0&0xffff)!=2 && (buf0&0xffff)!=3) rgbi=0;
      else if (p==6) rgbx=buf0&0xffff,rgbi=(rgbx==0?0:rgbi);
      else if (p==8) rgby=buf0&0xffff,rgbi=(rgby==0?0:rgbi);
      else if (p==10) {
        int z=buf0&0xffff;
        if (rgbx && rgby && (z==1 || z==3 || z==4)) IMG_DET(IMAGE8,rgbi-1,512,rgbx,rgby*z);
        rgbi=0;
      }
    }

    // Detect .tiff file header (2/8/24 bit color, not compressed).
    if (buf1==0x49492a00 && n>i+(int)bswap(buf0)) {
      U64 savedpos=in->curpos();
      in->setpos(start+i+ (U64)bswap(buf0)-7);

      // read directory
      int dirsize=in->getchar();
      int tifx=0,tify=0,tifz=0,tifzb=0,tifc=0,tifofs=0,tifofval=0,b[12];
      if (in->getchar()==0) {
        for (int i=0; i<dirsize; i++) {
          for (int j=0; j<12; j++) b[j]=in->getchar();
          if (b[11]==EOF) break;
          int tag=b[0]+(b[1]<<8);
          int tagfmt=b[2]+(b[3]<<8);
          int taglen=b[4]+(b[5]<<8)+(b[6]<<16)+(b[7]<<24);
          int tagval=b[8]+(b[9]<<8)+(b[10]<<16)+(b[11]<<24);
          if (tagfmt==3||tagfmt==4) {
            if (tag==256) tifx=tagval;
            else if (tag==257) tify=tagval;
            else if (tag==258) tifzb=taglen==1?tagval:8; // bits per component
            else if (tag==259) tifc=tagval; // 1 = no compression
            else if (tag==273 && tagfmt==4) tifofs=tagval,tifofval=(taglen<=1);
            else if (tag==277) tifz=tagval; // components per pixel
          }
        }
      }
      if (tifx && tify && tifzb && (tifz==1 || tifz==3) && (tifc==1) && (tifofs && tifofs+i<n)) {
        if (!tifofval) {
          in->setpos(start+i+tifofs-7);
          for (int j=0; j<4; j++) b[j]=in->getchar();
          tifofs=b[0]+(b[1]<<8)+(b[2]<<16)+(b[3]<<24);
        }
        if (tifofs && tifofs<(1<<18) && tifofs+i<n) {
          if (tifz==1 && tifzb==1) IMG_DET(IMAGE1,i-7,tifofs,((tifx-1)>>3)+1,tify);
          else if (tifz==1 && tifzb==8) IMG_DET(IMAGE8,i-7,tifofs,tifx,tify);
          else if (tifz==3 && tifzb==8) IMG_DET(IMAGE24,i-7,tifofs,tifx*3,tify);
        }
      }
      in->setpos(savedpos);
    }

    // Detect .tga image (8-bit 256 colors or 24-bit uncompressed)
    if ((buf1&0xFFFFFF)==0x00010100 && (buf0&0xFFFFFFC7)==0x00000100 && (c==16 || c==24 || c==32)) tga=i,tgax=tgay,tgaz=8,tgat=1,tgaid=buf1>>24,tgamap=c/8;
    else if ((buf1&0xFFFFFF)==0x00000200 && buf0==0x00000000) tga=i,tgax=tgay,tgaz=24,tgat=2;
    else if ((buf1&0xFFFFFF)==0x00000300 && buf0==0x00000000) tga=i,tgax=tgay,tgaz=8,tgat=3;
    if (tga) {
      if (i-tga==8) tga=(buf1==0?tga:0),tgax=(bswap(buf0)&0xffff),tgay=(bswap(buf0)>>16);
      else if (i-tga==10) {
        if ((buf0&0xFFF7)==32<<8)
          tgaz=32;
        if ((tgaz<<8)==(int)(buf0&0xFFD7) && tgax && tgay) {
          if (tgat==1){
            in->setpos(start+tga+11);
            IMG_DET( (IsGrayscalePalette(in))?IMAGE8GRAY:IMAGE8,tga-7,18+tgaid+256*tgamap,tgax,tgay);
          }
          else if (tgat==2) IMG_DET((tgaz==24)?IMAGE24:IMAGE32,tga-7,18+tgaid,tgax*(tgaz>>3),tgay);
          else if (tgat==3) IMG_DET(IMAGE8GRAY,tga-7,18+tgaid,tgax,tgay);
        }
        tga=0;
      }
    }

    // Detect .gif
    if (type==DEFAULT && dett==GIF && i==0) {
      dett=DEFAULT;
      if (c==0x2c || c==0x21) gif=2,gifi=2;
    }
    if (!gif && (buf1&0xffff)==0x4749 && (buf0==0x46383961 || buf0==0x46383761)) gif=1,gifi=i+5;
    if (gif) {
      if (gif==1 && i==gifi) gif=2, gifi = i+5+(gifplt=(c&128)?(3*(2<<(c&7))):0);
      if (gif==2 && gifplt && i==gifi-gifplt-2) gifgray = IsGrayscalePalette(in, gifplt/3), gifplt = 0;
      if (gif==2 && i==gifi) {
        if ((buf0&0xff0000)==0x210000) gif=5,gifi=i;
        else if ((buf0&0xff0000)==0x2c0000) gif=3,gifi=i;
        else gif=0;
      }
      if (gif==3 && i==gifi+6) gifw=(bswap(buf0)&0xffff);
      if (gif==3 && i==gifi+7) gif=4,gifc=gifb=0,gifa=gifi=i+2+((c&128)?(3*(2<<(c&7))):0);
      if (gif==4 && i==gifi) {
        if (c>0 && gifb && gifc!=gifb) gifw=0;
        if (c>0) gifb=gifc,gifc=c,gifi+=c+1;
        else if (!gifw) gif=2,gifi=i+3;
        else return in->setpos(start+gifa-1),detd=i-gifa+2,info=((gifgray?IMAGE8GRAY:IMAGE8)<<24)|gifw,dett=GIF;
      }
      if (gif==5 && i==gifi) {
        if (c>0) gifi+=c+1; else gif=2,gifi=i+3;
      }
    }

    // Detect EXE if the low order byte (little-endian) XX is more
    // recently seen (and within 4K) if a relative to absolute address
    // conversion is done in the context CALL/JMP (E8/E9) XX xx xx 00/FF
    // 4 times in a row.  Detect end of EXE at the last
    // place this happens when it does not happen for 64KB.

    if (((buf1&0xfe)==0xe8 || (buf1&0xfff0)==0x0f80) && ((buf0+1)&0xfe)==0) {
      int r=buf0>>24;  // relative address low 8 bits
      int a=((buf0>>24)+i)&0xff;  // absolute address low 8 bits
      int rdist=i-relpos[r];
      int adist=i-abspos[a];
      if (adist<rdist && adist<0x800 && abspos[a]>5) {
        e8e9last=i;
        ++e8e9count;
        if (e8e9pos==0 || e8e9pos>abspos[a]) e8e9pos=abspos[a];
      }
      else e8e9count=0;
      if (type==DEFAULT && e8e9count>=4 && e8e9pos>5)
        return in->setpos(start+e8e9pos-5), EXE;
      abspos[a]=i;
      relpos[r]=i;
    }
    if (i-e8e9last>0x4000) {
      //TODO: Large file support
      if (type==EXE) {
        info=(int)start;
        in->setpos(start+e8e9last);
        return DEFAULT;
      }
      e8e9count=e8e9pos=0;
    }

    // Detect base64 encoded data
    if (b64s==0 && buf0==0x73653634 && ((buf1&0xffffff)==0x206261 || (buf1&0xffffff)==0x204261)) b64s=1,b64i=i-6; //' base64' ' Base64'
    if (b64s==0 && ((buf1==0x3b626173 && buf0==0x6536342c) || (buf1==0x215b4344 && buf0==0x4154415b))) b64s=3,b64i=i+1; // ';base64,' '![CDATA['
    if (b64s>0) {
      if (b64s==1 && buf0==0x0d0a0d0a) b64s=((i-b64i>=128)?0:2),b64i=i+1,b64line=0;
      else if (b64s==2 && (buf0&0xffff)==0x0d0a && b64line==0) b64line=i+1-b64i,b64nl=i;
      else if (b64s==2 && (buf0&0xffff)==0x0d0a && b64line>0 && (buf0&0xffffff)!=0x3d0d0a) {
         if (i-b64nl<b64line && buf0!=0x0d0a0d0a) i-=1,b64s=5;
         else if (buf0==0x0d0a0d0a) i-=3,b64s=5;
         else if (i-b64nl==b64line) b64nl=i;
         else b64s=0;
      }
      else if (b64s==2 && (buf0&0xffffff)==0x3d0d0a) i-=1,b64s=5; // '=' or '=='
      else if (b64s==2 && !(isalnum(c) || c=='+' || c=='/' || c==10 || c==13 || c=='=')) b64s=0;
      if (b64line>0 && (b64line<=4 || b64line>255)) b64s=0;
      if (b64s==3 && i>=b64i && !(isalnum(c) || c=='+' || c=='/' || c=='=')) b64s=4;
      if ((b64s==4 && i-b64i>128) || (b64s==5 && i-b64i>512 && i-b64i<(1<<27))) return in->setpos(start+b64i),detd=i-b64i,BASE64;
      if (b64s>3) b64s=0;
    }

    // detect text
    text.isLetter = tolower(c)!=toupper(c);
    text.isUTF8 = ((c!=0xC0 && c!=0xC1 && c<0xF5) && (
        (c<0x80) ||
        // possible 1st byte of UTF8 sequence
        ((buf0&0xC000)!=0xC000 && ((c&0xE0)==0xC0 || (c&0xF0)==0xE0 || (c&0xF8)==0xF0)) ||
        // possible 2nd byte of UTF8 sequence
        ((buf0&0xE0C0)==0xC080 && (buf0&0xFE00)!=0xC000) || (buf0&0xF0C0)==0xE080 || ((buf0&0xF8C0)==0xF080 && ((buf0>>8)&0xFF)<0xF5) ||
        // possible 3rd byte of UTF8 sequence
        (buf0&0xF0C0C0)==0xE08080 || ((buf0&0xF8C0C0)==0xF08080 && ((buf0>>16)&0xFF)<0xF5) ||
        // possible 4th byte of UTF8 sequence
        ((buf0&0xF8C0C0C0)==0xF0808080 && (buf0>>24)<0xF5)
    ));
    text.lastNL = (c==NEW_LINE || c==CARRIAGE_RETURN)?0:text.lastNL+1;
    if (c==SPACE || c==TAB){
      text.lastSpace = 0;
      text.spaceRun++;
    }
    else{
      text.lastSpace++;
      text.spaceRun = 0;
    }
    text.wordLength = (text.isLetter)?text.wordLength+1:0;
    text.missCount-=text.misses>>31;
    text.misses<<=1;
    text.zeroRun=(!c && text.zeroRun<32)?text.zeroRun+1:0;
    if (c==NEW_LINE){
      if (!text.seenNL)
        text.needsEolTransform = true;
      text.seenNL = true;
      text.needsEolTransform&=U8(buf0>>8)==CARRIAGE_RETURN;
    }
    if ((c<SPACE && c!=TAB && (text.zeroRun<2 || text.zeroRun>8) && text.lastNL!=0) || 
        ((buf0&0xFF00)==(CARRIAGE_RETURN<<8) && c!=NEW_LINE) ||
        (text.spaceRun>8 && text.lastNL>256) ||
        (!text.isLetter && !text.isUTF8 && (
          text.lastNL>128 ||
		      text.lastSpace > std::max<U32>({ text.lastNL, text.wordLength*8, 64u }) ||
          text.wordLength>32)
        )
     ) {
        text.misses|=1;
        text.missCount++;
        int length = i-text.start-1;
        if ((length<MIN_TEXT_SIZE || (png || pdfimw || cdi || soi || pgm || rgbi || tga || gif || b64s))){
          text = {0};
          text.start = i+1;
        }
        else if (text.missCount>MAX_TEXT_MISSES) {
          in->setpos(start + text.start);
          detd = length;
          return (text.needsEolTransform)?TEXT_EOL:TEXT;
        }
    }
  }
  if (n-text.start>=MIN_TEXT_SIZE){
    in->setpos(start + text.start);
    detd = n-text.start;
    return (text.needsEolTransform)?TEXT_EOL:TEXT;
  }
  return type;
}


typedef enum {FDECOMPRESS, FCOMPARE, FDISCARD} FMode;

void encode_cd(File *in, File *out, U64 size, int info) {
  //TODO: Large file support
  const int BLOCK=2352;
  U8 blk[BLOCK];
  U64 blockresidual=size%BLOCK;
  assert(blockresidual<65536);
  out->putchar((blockresidual>>8)&255);
  out->putchar(blockresidual &255);
  for (U64 offset=0; offset<size; offset+=BLOCK) {
    if (offset+BLOCK > size) { //residual
      in->blockread(&blk[0], size-offset);
      out->blockwrite(&blk[0], size-offset);
    } else { //normal sector
      in->blockread(&blk[0], BLOCK);
      if (info==3) blk[15]=3; //indicate Mode2/Form2
      if (offset==0) out->blockwrite(&blk[12], 4+4*(blk[15]!=1)); //4-byte address + 4 bytes from the 8-byte subheader goes only to the first sector
      out->blockwrite(&blk[16+8*(blk[15]!=1)], 2048+276*(info==3)); //user data goes to all sectors
      if (offset+BLOCK*2 > size && blk[15]!=1) out->blockwrite(&blk[16], 4); //in Mode2 4 bytes from the 8-byte subheader goes after the last sector
    }
  }
}

U64 decode_cd(File *in, U64 size, File *out, FMode mode, U64 &diffFound) {
  //TODO: Large file support
  const int BLOCK=2352;
  U8 blk[BLOCK];
  U64 i=0; //*in position
  U64 nextblockpos=0;
  int address=-1;
  int datasize=0;
  U64 residual=(in->getchar() << 8) + in->getchar();
  size -=2;
  while (i<size) {
    if (size-i == residual) { //residual data after last sector
      in->blockread(blk, residual);
      if (mode==FDECOMPRESS) out->blockwrite(blk, residual);
      else if (mode==FCOMPARE) for (int j=0; j<(int)residual; ++j) if (blk[j]!= out->getchar() && !diffFound) diffFound=nextblockpos+j+1;
      return nextblockpos+residual;
    } else if (i==0) { //first sector
      in->blockread(blk+12, 4); //header (4 bytes) consisting of address (Minutes, Seconds, Sectors) and mode (1 = Mode1, 2 = Mode2/Form1, 3 = Mode2/Form2)
      if (blk[15]!=1) in->blockread(blk+16, 4);  //Mode2: 4 bytes from the read 8-byte subheader
      datasize=2048+(blk[15]==3)*276; //user data bytes: Mode1 and Mode2/Form1: 2048 (ECC is present) or Mode2/Form2: 2048+276=2324 bytes (ECC is not present)
      i+=4+4*(blk[15]!=1); //4 byte header + ( Mode2: 4 bytes from the 8-byte subheader )
    } else { //normal sector
      address=(blk[12]<<16)+(blk[13]<<8)+blk[14]; //3-byte address (Minutes, Seconds, Sectors)
    }
    in->blockread(blk+16+(blk[15]!=1)*8, datasize);  //read data bytes, but skip 8-byte subheader in Mode 2 (which we processed alread above)
    i+=datasize;
    if (datasize>2048) blk[15]=3; //indicate Mode2/Form2
    if (blk[15]!=1 && size-residual-i==4) { //Mode 2: we are at the last sector - grab the 4 subheader bytes
      in->blockread(blk+16, 4);
      i+=4;
    }
    expand_cd_sector(blk, address, 0);
    if (mode==FDECOMPRESS) out->blockwrite(blk, BLOCK);
    else if (mode==FCOMPARE) for (int j=0; j<BLOCK; ++j) if (blk[j]!= out->getchar() && !diffFound) diffFound=nextblockpos+j+1;
    nextblockpos+=BLOCK;
  }
  return nextblockpos;
}


// 24-bit image data transforms, controlled by OPTION_SKIPRGB:
// - simple color transform (b, g, r) -> (g, g-r, g-b)
// - channel reorder only (b, g, r) -> (g, r, b)
// Detects RGB565 to RGB888 conversions

#define RGB565_MIN_RUN 63

void encode_bmp(File *in, File *out, U64 len, int width) {
  int r, g, b, total=0;
  bool isPossibleRGB565 = true;
  for (int i=0; i<(int)(len/width); i++) {
    for (int j=0; j<width/3; j++) {
      b=in->getchar();
      g=in->getchar();
      r=in->getchar();
      if (isPossibleRGB565) {
        int pTotal=total;
        total=std::min<int>(total+1, 0xFFFF)*((b&7)==((b&8)-((b>>3)&1)) && (g&3)==((g&4)-((g>>2)&1)) && (r&7)==((r&8)-((r>>3)&1)));
        if (total>RGB565_MIN_RUN || pTotal>=RGB565_MIN_RUN) {
          b^=(b&8)-((b>>3)&1);
          g^=(g&4)-((g>>2)&1);
          r^=(r&8)-((r>>3)&1);
        }
        isPossibleRGB565=total>0;
      }
      out->putchar(g);
      out->putchar(options&OPTION_SKIPRGB?r:g-r);
      out->putchar(options&OPTION_SKIPRGB?b:g-b);
    }
    for (int j=0; j<width%3; j++) out->putchar(in->getchar());
  }
  for (int i=len%width; i>0; i--) out->putchar(in->getchar());
}

U64 decode_bmp(Encoder& en, U64 size, int width, File *out, FMode mode, U64 &diffFound) {
  int r, g, b, p, total=0;
  bool isPossibleRGB565 = true;
  for (int i=0; i<(int)(size/width); i++) {
    p=i*width;
    for (int j=0; j<width/3; j++) {
      g=en.decompress();
      r=en.decompress();
      b=en.decompress();
      if (!(options&OPTION_SKIPRGB))
        r=g-r, b=g-b;
      if (isPossibleRGB565){
        if (total>=RGB565_MIN_RUN) {
          b^=(b&8)-((b>>3)&1);
          g^=(g&4)-((g>>2)&1);
          r^=(r&8)-((r>>3)&1);
        }
        total=std::min<int>(total+1, 0xFFFF)*((b&7)==((b&8)-((b>>3)&1)) && (g&3)==((g&4)-((g>>2)&1)) && (r&7)==((r&8)-((r>>3)&1)));
        isPossibleRGB565=total>0;
      }
      if (mode==FDECOMPRESS) {
        out->putchar(b);
        out->putchar(g);
        out->putchar(r);
        if (!j && !(i&0xf)) en.print_status();
      }
      else if (mode==FCOMPARE) {
        if ((b&255)!=out->getchar() && !diffFound) diffFound=p+1;
        if (g!=out->getchar() && !diffFound) diffFound=p+2;
        if ((r&255)!=out->getchar() && !diffFound) diffFound=p+3;
        p+=3;
      }
    }
    for (int j=0; j<width%3; j++) {
      if (mode==FDECOMPRESS) {
        out->putchar(en.decompress());
      }
      else if (mode==FCOMPARE) {
        if (en.decompress()!= out->getchar() && !diffFound) diffFound=p+j+1;
      }
    }
  }
  for (int i=size%width; i>0; i--) {
    if (mode==FDECOMPRESS) {
      out->putchar(en.decompress());
    }
    else if (mode==FCOMPARE) {
      if (en.decompress()!=out->getchar()&&!diffFound) { diffFound=size-i; break; }
    }
  }
  return size;
}

// 32-bit image
void encode_im32(File *in, File *out, U64 len, int width) {
  int r,g,b,a;
  for (int i=0; i<(int)(len/width); i++) {
    for (int j=0; j<width/4; j++) {
      b=in->getchar();
      g=in->getchar();
      r=in->getchar();
      a=in->getchar();
      out->putchar(g);
      out->putchar(options&OPTION_SKIPRGB?r:g-r);
      out->putchar(options&OPTION_SKIPRGB?b:g-b);
      out->putchar(a);
    }
    for (int j=0; j<width%4; j++) out->putchar(in->getchar());
  }
  for (int i=len%width; i>0; i--) out->putchar(in->getchar());
}

U64 decode_im32(Encoder& en, U64 size, int width, File *out, FMode mode, U64 &diffFound) {
  int r,g,b,a,p;
  bool rgb = (width&(1<<31))>0;
  if (rgb) width^=(1<<31);
  for (int i=0; i<(int)(size/width); i++) {
    p=i*width;
    for (int j=0; j<width/4; j++) {
      b=en.decompress(), g=en.decompress(), r=en.decompress(), a=en.decompress();
      if (mode==FDECOMPRESS) {
        out->putchar(options&OPTION_SKIPRGB?r:b-r); out->putchar(b); out->putchar(options&OPTION_SKIPRGB?g:b-g); out->putchar(a);
        if (!j && !(i&0xf)) en.print_status();
      }
      else if (mode==FCOMPARE) {
        if (((options&OPTION_SKIPRGB?r:b-r)&255)!=out->getchar() && !diffFound) diffFound=p+1;
        if (b!=out->getchar() && !diffFound) diffFound=p+2;
        if (((options&OPTION_SKIPRGB?g:b-g)&255)!=out->getchar() && !diffFound) diffFound=p+3;
        if (((a)&255)!=out->getchar() && !diffFound) diffFound=p+4;
        p+=4;
      }
    }
    for (int j=0; j<width%4; j++) {
      if (mode==FDECOMPRESS) {
        out->putchar(en.decompress());
      }
      else if (mode==FCOMPARE) {
        if (en.decompress()!=out->getchar() && !diffFound) diffFound=p+j+1;
      }
    }
  }
  for (int i=size%width; i>0; i--) {
    if (mode==FDECOMPRESS) {
      out->putchar(en.decompress());
    }
    else if (mode==FCOMPARE) {
      if (en.decompress()!= out->getchar() && !diffFound){ diffFound=size-i; break; }
    }
  }
  return size;
}

// EOL transform

void encode_eol(File *in, File *out, U64 len) {
  U8 B=0, pB=0;
  for (int i=0; i<(int)len; i++){
    B = in->getchar();
    if (pB==CARRIAGE_RETURN && B!=NEW_LINE)
      out->putchar(pB);
    if (B!=CARRIAGE_RETURN)
      out->putchar(B);
    pB = B;
  }
  if (B==CARRIAGE_RETURN)
    out->putchar(B);
}

U64 decode_eol(Encoder& en, U64 size, File *out, FMode mode, U64 &diffFound) {
  U8 B;
  for (int i=0; i<(int)size; i++){
    if ((B=en.decompress())==NEW_LINE){
      if (mode==FDECOMPRESS)
        out->putchar(CARRIAGE_RETURN);
      else if (mode == FCOMPARE){
        if (out->getchar()!=CARRIAGE_RETURN && !diffFound){
          diffFound=size-i;
          break;
        }
      }
    }
    if (mode==FDECOMPRESS)
      out->putchar(B);
    else if (mode==FCOMPARE){
      if (B!=out->getchar() && !diffFound){
        diffFound=size-i;
        break;
      }
    }
    if (mode == FDECOMPRESS && !(i&0xFFF))
      en.print_status();
  }
  return out->curpos();
}

// EXE transform: <encoded-size> <begin> <block>...
// Encoded-size is 4 bytes, MSB first.
// begin is the offset of the start of the input file, 4 bytes, MSB first.
// Each block applies the e8e9 transform to strings falling entirely
// within the block starting from the end and working backwards.
// The 5 byte pattern is E8/E9 xx xx xx 00/FF (x86 CALL/JMP xxxxxxxx)
// where xxxxxxxx is a relative address LSB first.  The address is
// converted to an absolute address by adding the offset mod 2^25
// (in range +-2^24).

void encode_exe(File *in, File *out, U64 len, U64 begin) {
  const int BLOCK=0x10000;
  Array<U8> blk(BLOCK);
  out->put32((U32)begin); //TODO: Large file support

  // Transform
  for (U64 offset=0; offset<len; offset+=BLOCK) {
    U32 size=min(U32(len-offset), BLOCK);
    int bytesRead=(int)in->blockread(&blk[0], size);
    if (bytesRead!=(int)size) quit("encode_exe read error");
    for (int i=bytesRead-1; i>=5; --i) {
      if ((blk[i-4]==0xe8 || blk[i-4]==0xe9 || (blk[i-5]==0x0f && (blk[i-4]&0xf0)==0x80))
         && (blk[i]==0||blk[i]==0xff)) {
        int a=(blk[i-3]|blk[i-2]<<8|blk[i-1]<<16|blk[i]<<24)+(int)(offset+begin)+i+1;
        a<<=7;
        a>>=7;
        blk[i]=a>>24;
        blk[i-1]=a^176;
        blk[i-2]=(a>>8)^176;
        blk[i-3]=(a>>16)^176;
      }
    }
    out->blockwrite(&blk[0], bytesRead);
  }
}

U64 decode_exe(Encoder& en, U64 size, File *out, FMode mode, U64 &diffFound) {
  const int BLOCK=0x10000;  // block size
  int begin, offset=6, a;
  U8 c[6];
  begin=(int)en.decode_blocksize(); //TODO: Large file support
  size-=4;
  for (int i=4; i>=0; i--) c[i]=en.decompress();  // Fill queue

  while (offset<(int)size+6) {
    memmove(c+1, c, 5);
    if (offset<=(int)size) c[0]=en.decompress();
    // E8E9 transform: E8/E9 xx xx xx 00/FF -> subtract location from x
    if ((c[0]==0x00 || c[0]==0xFF) && (c[4]==0xE8 || c[4]==0xE9 || (c[5]==0x0F && (c[4]&0xF0)==0x80))
     && (((offset-1)^(offset-6))&-BLOCK)==0 && offset<=(int)size) { // not crossing block boundary
      a=((c[1]^176)|(c[2]^176)<<8|(c[3]^176)<<16|c[0]<<24)-offset-begin;
      a<<=7;
      a>>=7;
      c[3]=a;
      c[2]=a>>8;
      c[1]=a>>16;
      c[0]=a>>24;
    }
    if (mode==FDECOMPRESS) out->putchar(c[5]);
    else if (mode==FCOMPARE && c[5]!=out->getchar() && !diffFound) diffFound=offset-6+1;
    if (mode==FDECOMPRESS && !(offset&0xfff)) en.print_status();
    offset++;
  }
  return size;
}

#ifdef USE_ZLIB
class zLibMTF{
private:
  int Root, Index;
  Array<int, 16> Previous;
  Array<int, 16> Next;
public:
  zLibMTF(): Root(0), Index(0), Previous(81), Next(81) {
    for (int i=0;i<81;i++) {
      Previous[i] = i-1;
      Next[i] = i+1;
    }
    Next[80] = -1;
  }
  inline int GetFirst(){
    return Index=Root;
  }
  inline int GetNext(){
    if(Index>=0){Index=Next[Index];return Index;}
    return Index; //-1
  }
  inline void MoveToFront(int i){
    assert(i>=0 && i<=80);
    if ((Index=i)==Root) return;
    int p=Previous[Index];
    int n=Next[Index];
    if(p>=0)Next[p] = Next[Index];
    if(n>=0)Previous[n] = Previous[Index];
    Previous[Root] = Index;
    Next[Index] = Root;
    Root=Index;
    Previous[Root]=-1;
  }
} MTF;

int encode_zlib(File *in, File *out, U64 len, int &hdrsize) {
  const int BLOCK=1<<16, LIMIT=128;
  U8 zin[BLOCK*2],zout[BLOCK],zrec[BLOCK*2], diffByte[81*LIMIT];
  U64 diffPos[81*LIMIT];

  // Step 1 - parse offset type form zlib stream header
  U64 pos_backup=in->curpos();
  U32 h1=in->getchar(), h2=in->getchar();
  in->setpos(pos_backup);
  int zh=parse_zlib_header(h1*256+h2);
  int memlevel,clevel,window=zh==-1?0:MAX_WBITS+10+zh/4,ctype=zh%4;
  int minclevel=window==0?1:ctype==3?7:ctype==2?6:ctype==1?2:1;
  int maxclevel=window==0?9:ctype==3?9:ctype==2?6:ctype==1?5:1;
  int index=-1, nTrials=0;
  bool found=false;

  // Step 2 - check recompressiblitiy, determine parameters and save differences
  z_stream main_strm, rec_strm[81];
  int diffCount[81], recpos[81], main_ret=Z_STREAM_END;
  main_strm.zalloc=Z_NULL; main_strm.zfree=Z_NULL; main_strm.opaque=Z_NULL;
  main_strm.next_in=Z_NULL; main_strm.avail_in=0;
  if (zlib_inflateInit(&main_strm,zh)!=Z_OK) return false;
  for (int i=0; i<81; i++) {
    clevel=(i/9)+1;
    // Early skip if invalid parameter
    if (clevel<minclevel || clevel>maxclevel){
      diffCount[i]=LIMIT;
      continue;
    }
    memlevel=(i%9)+1;
    rec_strm[i].zalloc=Z_NULL; rec_strm[i].zfree=Z_NULL; rec_strm[i].opaque=Z_NULL;
    rec_strm[i].next_in=Z_NULL; rec_strm[i].avail_in=0;
    int ret=deflateInit2(&rec_strm[i], clevel, Z_DEFLATED, window-MAX_WBITS, memlevel, Z_DEFAULT_STRATEGY);
    diffCount[i]=(ret==Z_OK)?0:LIMIT;
    recpos[i]=BLOCK*2;
    diffPos[i*LIMIT]=0xFFFFFFFFFFFFFFFF;
    diffByte[i*LIMIT]=0;
  }

  for (U64 i=0; i<len; i+=BLOCK) {
    U32 blsize=min(U32(len-i),BLOCK);
    nTrials=0;
    for (int j=0; j<81; j++) {
      if (diffCount[j]==LIMIT) continue;
      nTrials++;
      if (recpos[j]>=BLOCK)
        recpos[j]-=BLOCK;
    }
    // early break if nothing left to test
    if (nTrials==0)
      break;
    memmove(&zrec[0], &zrec[BLOCK], BLOCK);
    memmove(&zin[0], &zin[BLOCK], BLOCK);
    in->blockread(&zin[BLOCK], blsize); // Read block from input file

    // Decompress/inflate block
    main_strm.next_in=&zin[BLOCK]; main_strm.avail_in=blsize;
    do {
      main_strm.next_out=&zout[0]; main_strm.avail_out=BLOCK;
      main_ret=inflate(&main_strm, Z_FINISH);
      nTrials=0;

      // Recompress/deflate block with all possible parameters
      for (int j=MTF.GetFirst(); j>=0; j=MTF.GetNext()){
        if (diffCount[j]==LIMIT) continue;
        nTrials++;
        rec_strm[j].next_in=&zout[0];  rec_strm[j].avail_in=BLOCK-main_strm.avail_out;
        rec_strm[j].next_out=&zrec[recpos[j]]; rec_strm[j].avail_out=BLOCK*2-recpos[j];
        int ret=deflate(&rec_strm[j], main_strm.total_in == len ? Z_FINISH : Z_NO_FLUSH);
        if (ret!=Z_BUF_ERROR && ret!=Z_STREAM_END && ret!=Z_OK) { diffCount[j]=LIMIT; continue; }

        // Compare
        int end=2*BLOCK-(int)rec_strm[j].avail_out;
        int tail=max(main_ret==Z_STREAM_END ? (int)len-(int)rec_strm[j].total_out : 0,0);
        for (int k=recpos[j]; k<end+tail; k++) {
          if ((k<end && i+k-BLOCK<len && zrec[k]!=zin[k]) || k>=end) {
            if (++diffCount[j]<LIMIT) {
              const int p=j*LIMIT+diffCount[j];
              diffPos[p]=i+k-BLOCK;
              assert(k < int(sizeof(zin)/sizeof(*zin)));
              diffByte[p]=zin[k];
            }
          }
        }
        // Early break on perfect match
        if (main_ret==Z_STREAM_END && diffCount[j]==0){
          index=j;
          found=true;
          break;
        }
        recpos[j]=2*BLOCK-rec_strm[j].avail_out;
      }
    } while (main_strm.avail_out==0 && main_ret==Z_BUF_ERROR && nTrials>0);
    if ((main_ret!=Z_BUF_ERROR && main_ret!=Z_STREAM_END) || nTrials==0) break;
  }
  int minCount=(found)?0:LIMIT;
  for (int i=80; i>=0; i--) {
    clevel=(i/9)+1;
    if (clevel>=minclevel && clevel<=maxclevel)
      deflateEnd(&rec_strm[i]);
    if (!found && diffCount[i]<minCount)
      minCount=diffCount[index=i];
  }
  inflateEnd(&main_strm);
  if (minCount==LIMIT) return false;
  MTF.MoveToFront(index);

  // Step 3 - write parameters, differences and precompressed (inflated) data
  out->putchar(diffCount[index]);
  out->putchar(window);
  out->putchar(index);
  for (int i=0; i<=diffCount[index]; i++) {
    const int v=i==diffCount[index] ? int(len-diffPos[index*LIMIT+i])
                                    : int(diffPos[index*LIMIT+i+1]-diffPos[index*LIMIT+i])-1;
    out->put32(v);
  }
  for (int i=0; i<diffCount[index]; i++) out->putchar(diffByte[index*LIMIT+i+1]);

  in->setpos(pos_backup);
  main_strm.zalloc=Z_NULL; main_strm.zfree=Z_NULL; main_strm.opaque=Z_NULL;
  main_strm.next_in=Z_NULL; main_strm.avail_in=0;
  if (zlib_inflateInit(&main_strm,zh)!=Z_OK) return false;
  for (U64 i=0; i<len; i+=BLOCK) {
    U32 blsize=min(U32(len-i),BLOCK);
    in->blockread(&zin[0], blsize);
    main_strm.next_in=&zin[0]; main_strm.avail_in=blsize;
    do {
      main_strm.next_out=&zout[0]; main_strm.avail_out=BLOCK;
      main_ret=inflate(&main_strm, Z_FINISH);
      out->blockwrite(&zout[0], BLOCK-main_strm.avail_out);
    } while (main_strm.avail_out==0 && main_ret==Z_BUF_ERROR);
    if (main_ret!=Z_BUF_ERROR && main_ret!=Z_STREAM_END) break;
  }
  inflateEnd(&main_strm);
  hdrsize=diffCount[index]*5+7;
  return main_ret==Z_STREAM_END;
}

int decode_zlib(File *in, U64 size, File *out, FMode mode, U64 &diffFound) {
  const int BLOCK=1<<16, LIMIT=128;
  U8 zin[BLOCK],zout[BLOCK];
  int diffCount=min(in->getchar(),LIMIT-1);
  int window=in->getchar()-MAX_WBITS;
  int index=in->getchar();
  int memlevel=(index%9)+1;
  int clevel=(index/9)+1;
  int len=0;
  int diffPos[LIMIT];
  diffPos[0]=-1;
  for (int i=0; i<=diffCount; i++) {
    int v=in->get32();
    if (i==diffCount) len=v+diffPos[i]; else diffPos[i+1]=v+diffPos[i]+1;
  }
  U8 diffByte[LIMIT];
  diffByte[0]=0;
  for (int i=0; i<diffCount; i++) diffByte[i+1]=in->getchar();
  size-=7+5*diffCount;

  z_stream rec_strm;
  int diffIndex=1,recpos=0;
  rec_strm.zalloc=Z_NULL; rec_strm.zfree=Z_NULL; rec_strm.opaque=Z_NULL;
  rec_strm.next_in=Z_NULL; rec_strm.avail_in=0;
  int ret=deflateInit2(&rec_strm, clevel, Z_DEFLATED, window, memlevel, Z_DEFAULT_STRATEGY);
  if (ret!=Z_OK) return 0;
  for (U64 i=0; i<size; i+=BLOCK) {
    U32 blsize=min(U32(size-i),BLOCK);
    in->blockread(&zin[0], blsize);
    rec_strm.next_in=&zin[0];  rec_strm.avail_in=blsize;
    do {
      rec_strm.next_out=&zout[0]; rec_strm.avail_out=BLOCK;
      ret=deflate(&rec_strm, i+blsize==size ? Z_FINISH : Z_NO_FLUSH);
      if (ret!=Z_BUF_ERROR && ret!=Z_STREAM_END && ret!=Z_OK) break;
      const int have=min(BLOCK-rec_strm.avail_out,len-recpos);
      while (diffIndex<=diffCount && diffPos[diffIndex]>=recpos && diffPos[diffIndex]<recpos+have) {
        zout[diffPos[diffIndex]-recpos]=diffByte[diffIndex];
        diffIndex++;
      }
      if (mode==FDECOMPRESS) out->blockwrite(&zout[0], have);
      else if (mode==FCOMPARE) for (int j=0; j<have; j++) if (zout[j]!=out->getchar() && !diffFound) diffFound=recpos+j+1;
      recpos+=have;

    } while (rec_strm.avail_out==0);
  }
  while (diffIndex<=diffCount) {
    if (mode==FDECOMPRESS) out->putchar(diffByte[diffIndex]);
    else if (mode==FCOMPARE) if (diffByte[diffIndex]!=out->getchar() && !diffFound) diffFound=recpos+1;
    diffIndex++;
    recpos++;
  }
  deflateEnd(&rec_strm);
  return recpos==len ? len : 0;
}
#endif //USE_ZLIB

//
// decode/encode base64
//
static const char  table1[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
bool isbase64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/')|| (c == 10) || (c == 13));
}

U64 decode_base64(File *in, File *out, FMode mode, U64 &diffFound){
    U8 inn[3];
    int i, len=0, blocksout = 0;
    int fle=0;
    int linesize=0;
    int outlen=0;
    int tlf=0;
    linesize=in->getchar();
    outlen=in->getchar();
    outlen+=(in->getchar()<<8);
    outlen+=(in->getchar()<<16);
    tlf=(in->getchar());
    outlen+=((tlf&63)<<24);
    Array<U8> ptr((outlen>>2)*4+10);
    tlf=(tlf&192);
    if (tlf==128)
       tlf=10;        // LF: 10
    else if (tlf==64)
       tlf=13;        // LF: 13
    else
       tlf=0;

    while(fle<outlen){
      len=0;
      for(i=0;i<3;i++){
        int c=in->getchar();
        if(c!=EOF) {
          inn[i]=c;
          len++;
        }
        else
          inn[i]=0;
      }
      if(len){
        U8 in0,in1,in2;
        in0=inn[0],in1=inn[1],in2=inn[2];
        ptr[fle++]=(table1[in0>>2]);
        ptr[fle++]=(table1[((in0&0x03)<<4)|((in1&0xf0)>>4)]);
        ptr[fle++]=((len>1?table1[((in1&0x0f)<<2)|((in2&0xc0)>>6)]:'='));
        ptr[fle++]=((len>2?table1[in2&0x3f]:'='));
        blocksout++;
      }
      if(blocksout>=(linesize/4) && linesize!=0){ //no lf if linesize==0
        if( blocksout && !in->eof() && fle<=outlen) { //no lf if eof
          if (tlf) ptr[fle++]=(tlf);
          else ptr[fle++]=13,ptr[fle++]=10;
        }
        blocksout = 0;
      }
    }
    //Write out or compare
    if (mode==FDECOMPRESS){
      out->blockwrite(&ptr[0], outlen);
    }
    else if (mode==FCOMPARE){
      for(i=0;i<outlen;i++){
        U8 b=ptr[i];
        if (b!=out->getchar() && !diffFound) diffFound=(int)out->curpos();
      }
    }
    return outlen;
}

inline char valueb(char c){
  const char *p = strchr(table1, c);
  if (p) return (char)(p - table1);
  return 0;
}

void encode_base64(File *in, File *out, U64 len64) {
  int len=(int)len64;
  int in_len = 0;
  int i = 0;
  int j = 0;
  int b=0;
  int lfp=0;
  int tlf=0;
  char src[4];
  int b64mem=(len>>2)*3+10;
  Array<U8> ptr(b64mem);
  int olen=5;

  while (b=in->getchar(),in_len++ , ( b != '=') && isbase64(b) && in_len<=len) {
    if (b==13 || b==10) {
       if (lfp==0) lfp=in_len ,tlf=b;
       if (tlf!=b) tlf=0;
       continue;
    }
    src[i++] = b;
    if (i ==4){
          for (j = 0; j <4; j++) src[j] = valueb(src[j]);
          src[0] = (src[0] << 2) + ((src[1] & 0x30) >> 4);
          src[1] = ((src[1] & 0xf) << 4) + ((src[2] & 0x3c) >> 2);
          src[2] = ((src[2] & 0x3) << 6) + src[3];

          ptr[olen++]=src[0];
          ptr[olen++]=src[1];
          ptr[olen++]=src[2];
      i = 0;
    }
  }

  if (i){
    for (j=i;j<4;j++)
      src[j] = 0;

    for (j=0;j<4;j++)
      src[j] = valueb(src[j]);

    src[0] = (src[0] << 2) + ((src[1] & 0x30) >> 4);
    src[1] = ((src[1] & 0xf) << 4) + ((src[2] & 0x3c) >> 2);
    src[2] = ((src[2] & 0x3) << 6) + src[3];

    for (j=0;(j<i-1);j++) {
        ptr[olen++]=src[j];
    }
  }
  ptr[0]=lfp&255; //nl length
  ptr[1]=len&255;
  ptr[2]=(len>>8)&255;
  ptr[3]=(len>>16)&255;
  if (tlf!=0) {
    if (tlf==10) ptr[4]=128;
    else ptr[4]=64;
  }
  else
      ptr[4]=(len>>24)&63; //1100 0000
  out->blockwrite(&ptr[0], olen);
}

int encode_gif(File *in, File *out, U64 len, int &hdrsize) {
  int codesize=in->getchar(),diffpos=0,clearpos=0,bsize=0;
  U64 beginin=in->curpos(),beginout=out->curpos();
  Array<U8> output(4096);
  hdrsize=6;
  out->putchar(hdrsize>>8);
  out->putchar(hdrsize&255);
  out->putchar(bsize);
  out->putchar(clearpos>>8);
  out->putchar(clearpos&255);
  out->putchar(codesize);
  for (int phase=0; phase<2; phase++) {
    in->setpos(beginin);
    int bits=codesize+1,shift=0,buffer=0;
    int blocksize=0,maxcode=(1<<codesize)+1,last=-1;
    Array<int> dict(4096);
    bool end=false;
    while ((blocksize=in->getchar())>0 && in->curpos()-beginin<len && !end) {
      for (int i=0; i<blocksize; i++) {
        buffer|=in->getchar()<<shift;
        shift+=8;
        while (shift>=bits && !end) {
          int code=buffer&((1<<bits)-1);
          buffer>>=bits;
          shift-=bits;
          if (!bsize && code!=(1<<codesize)) {
            hdrsize+=4; out->put32(0);
          }
          if (!bsize) bsize=blocksize;
          if (code==(1<<codesize)) {
            if (maxcode>(1<<codesize)+1) {
              if (clearpos && clearpos!=69631-maxcode) return 0;
              clearpos=69631-maxcode;
            }
            bits=codesize+1, maxcode=(1<<codesize)+1, last=-1;
          }
          else if (code==(1<<codesize)+1) end=true;
          else if (code>maxcode+1) return 0;
          else {
            int j=(code<=maxcode?code:last),size=1;
            while (j>=(1<<codesize)) {
              output[4096-(size++)]=dict[j]&255;
              j=dict[j]>>8;
            }
            output[4096-size]=j;
            if (phase==1) out->blockwrite(&output[4096-size], size); else diffpos+=size;
            if (code==maxcode+1) { if (phase==1) out->putchar(j); else diffpos++; }
            if (last!=-1) {
              if (++maxcode>=8191) return 0;
              if (maxcode<=4095)
              {
                dict[maxcode]=(last<<8)+j;
                if (phase==0) {
                  bool diff=false;
                  for (int m=(1<<codesize)+2;m<min(maxcode,4095);m++) if (dict[maxcode]==dict[m]) { diff=true; break; }
                  if (diff) {
                    hdrsize+=4;
                    j=diffpos-size-(code==maxcode);
                    out->put32(j);
                    diffpos=size+(code==maxcode);
                  }
                }
              }
              if (maxcode>=((1<<bits)-1) && bits<12) bits++;
            }
            last=code;
          }
        }
      }
    }
  }
  diffpos=(int)out->curpos();
  out->setpos(beginout);
  out->putchar(hdrsize>>8);
  out->putchar(hdrsize&255);
  out->putchar(255-bsize);
  out->putchar((clearpos>>8)&255);
  out->putchar(clearpos&255);
  out->setpos(diffpos);
  return in->curpos()-beginin==len-1;
}

#define gif_write_block(count) { output[0]=(count);\
if (mode==FDECOMPRESS) out->blockwrite(&output[0], (count)+1);\
else if (mode==FCOMPARE) for (int j=0; j<(count)+1; j++) if (output[j]!=out->getchar() && !diffFound) diffFound=outsize+j+1;\
outsize+=(count)+1; blocksize=0; }

#define gif_write_code(c) { buffer+=(c)<<shift; shift+=bits;\
while (shift>=8) { output[++blocksize]=buffer&255; buffer>>=8;shift-=8;\
if (blocksize==bsize) gif_write_block(bsize); }}

int decode_gif(File *in, U64 size, File *out, FMode mode, U64 &diffFound) {
  int diffcount=in->getchar(), curdiff=0;
  Array<int> diffpos(4096);
  diffcount=((diffcount<<8)+in->getchar()-6)/4;
  int bsize=255-in->getchar();
  int clearpos=in->getchar(); clearpos=(clearpos<<8)+in->getchar();
  clearpos=(69631-clearpos)&0xffff;
  int codesize=in->getchar(),bits=codesize+1,shift=0,buffer=0,blocksize=0;
  if (diffcount>4096 || clearpos<=(1<<codesize)+2) return 1;
  int maxcode=(1<<codesize)+1,input;
  Array<int> dict(4096);
  for (int i=0; i<diffcount; i++) {
    diffpos[i]=in->getchar();
    diffpos[i]=(diffpos[i]<<8)+in->getchar();
    diffpos[i]=(diffpos[i]<<8)+in->getchar();
    diffpos[i]=(diffpos[i]<<8)+in->getchar();
    if (i>0) diffpos[i]+=diffpos[i-1];
  }
  Array<U8> output(256);
  size-=6+diffcount*4;
  int last=in->getchar(),total=(int)size+1,outsize=1;
  if (mode==FDECOMPRESS) out->putchar(codesize);
  else if (mode==FCOMPARE) if (codesize!=out->getchar() && !diffFound) diffFound=1;
  if (diffcount==0 || diffpos[0]!=0) gif_write_code(1<<codesize) else curdiff++;
  while (size!=0 && (input=in->getchar())!=EOF) {
    size--;
    int code=-1, key=(last<<8)+input;
    for (int i=(1<<codesize)+2; i<=min(maxcode,4095); i++) if (dict[i]==key) code=i;
    if (curdiff<diffcount && total-(int)size>diffpos[curdiff]) curdiff++,code=-1;
    if (code==-1) {
      gif_write_code(last);
      if (maxcode==clearpos) { gif_write_code(1<<codesize); bits=codesize+1, maxcode=(1<<codesize)+1; }
      else
      {
        ++maxcode;
        if (maxcode<=4095) dict[maxcode]=key;
        if (maxcode>=(1<<bits) && bits<12) bits++;
      }
      code=input;
    }
    last=code;
  }
  gif_write_code(last);
  gif_write_code((1<<codesize)+1);
  if (shift>0) {
    output[++blocksize]=buffer&255;
    if (blocksize==bsize) gif_write_block(bsize);
  }
  if (blocksize>0) gif_write_block(blocksize);
  if (mode==FDECOMPRESS) out->putchar(0);
  else if (mode==FCOMPARE) if (0!=out->getchar() && !diffFound) diffFound=outsize+1;
  return outsize+1;
}


//////////////////// Compress, Decompress ////////////////////////////

void direct_encode_block(Blocktype type, File *in, U64 len, Encoder &en, int info=-1) {
  //TODO: Large file support
  en.compress(type);
  en.encode_blocksize(len);
  if (info!=-1) {
    en.compress((info>>24)&0xFF);
    en.compress((info>>16)&0xFF);
    en.compress((info>>8)&0xFF);
    en.compress((info)&0xFF);
  }
  if (to_screen)
    printf("Compressing... ");
  for (U64 j=0; j<len; ++j) {
    if ((j&0xfff)==0) en.print_status(j, len);
    en.compress(in->getchar());
  }
  if (to_screen)
    printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
}

void compressRecursive(File *in, U64 blocksize, Encoder &en, String &blstr, int recursion_level, float p1, float p2);

U64 decode_func(Blocktype type, Encoder &en, File *tmp, U64 len, int info, File *out, FMode mode, U64 &diffFound) {
  if (type==IMAGE24) return decode_bmp(en, len, info, out, mode, diffFound);
  else if (type==IMAGE32) return decode_im32(en, len, info, out, mode, diffFound);
  else if (type==EXE) return decode_exe(en, len, out, mode, diffFound);
  else if (type == TEXT_EOL) return decode_eol(en, len, out, mode, diffFound);
  else if (type==CD) return decode_cd(tmp, len, out, mode, diffFound);
  #ifdef USE_ZLIB
  else if (type==ZLIB) return decode_zlib(tmp, len, out, mode, diffFound);
  #endif //USE_ZLIB
  else if (type==BASE64) return decode_base64(tmp, out, mode, diffFound);
  else if (type==GIF) return decode_gif(tmp, len, out, mode, diffFound);
  else assert(false);
  return 0;
}

U64 encode_func(Blocktype type, File *in, File *tmp, U64 len, int info, int &hdrsize) {
  if (type==IMAGE24) encode_bmp(in, tmp, len, info);
  else if (type==IMAGE32) encode_im32(in, tmp, len, info);
  else if (type==EXE) encode_exe(in, tmp, len, info);
  else if (type == TEXT_EOL) encode_eol(in, tmp, len);
  else if (type==CD) encode_cd(in, tmp, len, info);
  #ifdef USE_ZLIB
  else if (type==ZLIB) return encode_zlib(in, tmp, len, hdrsize)?0:1;
  #endif //USE_ZLIB
  else if (type==BASE64) encode_base64(in, tmp, len);
  else if (type==GIF) return encode_gif(in, tmp, len, hdrsize)?0:1;
  else assert(false);
  return 0;
}

void transform_encode_block(Blocktype type, File *in, U64 len, Encoder &en, int info, String &blstr, int recursion_level, float p1, float p2, U64 begin) {
  if (hasTransform(type)) {
    FileTmp tmp;
    int hdrsize=0;
    U64 diffFound=encode_func(type, in, &tmp, len, info, hdrsize);
    const U64 tmpsize=tmp.curpos();
    tmp.setpos(tmpsize); //switch to read mode
    if (diffFound==0) {
      tmp.setpos(0);
      en.setFile(&tmp);
      in->setpos(begin);
      decode_func(type, en, &tmp, tmpsize, info, in, FCOMPARE, diffFound);
    }
    // Test fails, compress without transform
    if (diffFound>0 || tmp.getchar()!=EOF) {
      printf("Transform fails at %" PRIu64 ", skipping...\n", diffFound-1);
      in->setpos(begin);
      direct_encode_block(DEFAULT, in, len, en);
    } else {
      tmp.setpos(0);
      if (hasRecursion(type)) {
        //TODO: Large file support
        en.compress(type);
        en.encode_blocksize(tmpsize);
        Blocktype type2=(Blocktype)((info>>24)&0xFF);
        if (type2!=DEFAULT) {
          String blstr_sub0;blstr_sub0+=blstr.c_str();blstr_sub0+="->";
          String blstr_sub1;blstr_sub1+=blstr.c_str();blstr_sub1+="-->";
          String blstr_sub2;blstr_sub2+=blstr.c_str();blstr_sub2+="-->";
          printf(" %-11s | ->  exploded     |%10d bytes [%d - %d]\n",blstr_sub0.c_str(),int(tmpsize),0,int(tmpsize-1));
          printf(" %-11s | --> added header |%10d bytes [%d - %d]\n",blstr_sub1.c_str(),hdrsize,0,hdrsize-1);
          direct_encode_block(HDR, &tmp, hdrsize, en);
          printf(" %-11s | --> data         |%10d bytes [%d - %d]\n",blstr_sub2.c_str(),int(tmpsize-hdrsize),hdrsize,int(tmpsize-1));
          transform_encode_block(type2, &tmp, tmpsize-hdrsize, en, info&0xffffff, blstr, recursion_level, p1, p2, hdrsize);
        } else {
          compressRecursive(&tmp, tmpsize, en, blstr, recursion_level+1, p1, p2);
        }
      } else {
        direct_encode_block(type, &tmp, tmpsize, en, hasInfo(type)?info:-1);
      }
    }
    tmp.close();
  } else {
    direct_encode_block(type, in, len, en, hasInfo(type)?info:-1);
  }
}

void compressRecursive(File *in, const U64 blocksize, Encoder &en, String &blstr, int recursion_level, float p1, float p2) {
  static const char* typenames[22]={"default", "filecontainer", "jpeg", "hdr", "1b-image", "4b-image", "8b-image", "8b-img-grayscale",
    "24b-image", "32b-image", "audio", "exe", "cd", "zlib", "base64", "gif", "png-8b", "png-8b-grayscale", "png-24b", "png-32b", "text", "text - eol"};
  static const char* audiotypes[4]={"8b-mono", "8b-stereo", "16b-mono","16b-stereo"};
  Blocktype type=DEFAULT;
  int blnum=0, info=0;  // image width or audio type
  U64 begin=in->curpos();
  U64 block_end=begin+blocksize;
  if (recursion_level==5) {
    direct_encode_block(DEFAULT, in, blocksize, en);
    return;
  }
  float pscale=blocksize>0?(p2-p1)/blocksize:0;

  // Transform and test in blocks
  U64 bytes_to_go=blocksize;
  while (bytes_to_go>0) {
    Blocktype nextType=detect(in, bytes_to_go, type, info);
    U64 detected_end=in->curpos();
    in->setpos(begin);
    if (detected_end>block_end) {  // if a detection reports a larger size than the actual block size, fall back
      detected_end=begin+1;
      type=DEFAULT;
    }
    U64 len=detected_end-begin;
    if (len>0) {
      en.set_status_range(p1,p2=p1+pscale*len);

      //Compose block enumeration string
      String blstr_sub;
      blstr_sub+=blstr.c_str();
      if (blstr_sub.strsize()!=0) blstr_sub+="-";
      blstr_sub+=U64(blnum);
      blnum++;

      printf(" %-11s | %-16s |%10" PRIu64 " bytes [%" PRIu64 " - %" PRIu64 "]",blstr_sub.c_str(),typenames[(type==ZLIB && (info>>24)>=PNG8)?info>>24:type],len,begin,detected_end-1);
      if (type==AUDIO) printf(" (%s)", audiotypes[info%4]);
      else if (type==IMAGE1 || type==IMAGE4 || type==IMAGE8 || type==IMAGE8GRAY || type==IMAGE24 || type==IMAGE32 || (type==ZLIB && (info>>24)>=PNG8)) printf(" (width: %d)", (type==ZLIB)?(info&0xFFFFFF):info);
      else if (hasRecursion(type) && (info>>24) > 0) printf(" (%s)",typenames[info>>24]);
      else if (type==CD) printf(" (mode%d/form%d)", info==1?1:2, info!=3?1:2);
      printf("\n");
      transform_encode_block(type, in, len, en, info, blstr_sub, recursion_level, p1, p2, begin);
      p1=p2;
      bytes_to_go-=len;
    }
    type=nextType;
    begin=detected_end;
  }
}

// Compress a file. Split filesize bytes into blocks by type.
// For each block, output
// <type> <size> and call encode_X to convert to type X.
// Test transform and compress.
void compressfile(const char* filename, U64 filesize, Encoder& en, bool verbose) {
  assert(en.getMode()==COMPRESS);
  assert(filename && filename[0]);

  en.compress(FILECONTAINER);
  U64 start=en.size();
  en.encode_blocksize(filesize);

  FileDisk in;
  in.open(filename, true);
  printf("Block segmentation:\n");
  String blstr;
  compressRecursive(&in, filesize, en, blstr, 0, 0.0f, 1.0f);
  in.close();

  if(options & OPTION_MULTIPLE_FILE_MODE) { //multiple file mode
    if(verbose)
    printf("File size to encode   : 4\n"); //This string must be long enough. "Compressing ..." is still on screen, we need to overwrite it.
    printf("File input size       : %" PRIu64 "\n",filesize);
    printf("File compressed size  : %" PRIu64 "\n",en.size()-start);
  }
}

U64 decompressRecursive(File *out, U64 blocksize, Encoder& en, FMode mode, int recursion_level) {
  Blocktype type;
  U64 len, i=0;
  U64 diffFound=0;
  int info=0;
  while (i<blocksize) {
    type=(Blocktype)en.decompress();
    len=en.decode_blocksize();
    if (hasInfo(type)) {
      info=0; for (int i=0; i<4; ++i) { info<<=8; info+=en.decompress(); }
    }
    if (hasRecursion(type)) {
      FileTmp tmp;
      decompressRecursive(&tmp, len, en, FDECOMPRESS, recursion_level+1);
      if (mode!=FDISCARD) {
        tmp.setpos(0);
        if (hasTransform(type)) len=decode_func(type, en, &tmp, len, info, out, mode, diffFound);
      }
      tmp.close();
    } else if (hasTransform(type)) {
      len=decode_func(type, en, NULL, len, info, out, mode, diffFound);
    } else {
      for (U64 j=0; j<len; ++j) {
        if (!(j&0xfff)) en.print_status();
        if (mode==FDECOMPRESS) out->putchar(en.decompress());
        else if (mode==FCOMPARE) {
          if (en.decompress()!=out->getchar() && !diffFound) {
            mode=FDISCARD;
            diffFound=i+j+1;
          }
        } else en.decompress();
      }
    }
    i+=len;
  }
  return diffFound;
}

// Decompress or compare a file
void decompressfile(const char* filename, FMode fmode, Encoder& en) {
  assert(en.getMode()==DECOMPRESS);
  assert(filename && filename[0]);

  Blocktype blocktype=(Blocktype)en.decompress();
  if(blocktype!=FILECONTAINER)quit("Bad archive.");
  U64 filesize=en.decode_blocksize();

  FileDisk f;
  if(fmode==FCOMPARE){
    f.open(filename,true);
    printf("Comparing");
  }
  else { //mode==FDECOMPRESS;
    f.create(filename);
    printf("Extracting");
  }
  printf(" %s %" PRIu64 " bytes -> ", filename, filesize);

  // Decompress/Compare
  U64 r=decompressRecursive(&f, filesize, en, fmode, 0);
  if (fmode==FCOMPARE && !r && f.getchar()!=EOF) printf("file is longer\n");
  else if (fmode==FCOMPARE && r) printf("differ at %" PRIu64 "\n",r-1);
  else if (fmode==FCOMPARE) printf("identical\n");
  else printf("done   \n");
  f.close();
}


//////////////////////////// User Interface ////////////////////////////


class ListOfFiles {
private:
  typedef enum{IN_HEADER,FINISHED_A_FILENAME,FINISHED_A_LINE,PROCESSING_FILENAME} STATE;
  STATE state; //parsing state
  FileName basepath;
  String list_of_files; //path/file list in first column, columns separated by tabs, rows separated by newlines, with header in 1st row
  Array<FileName*> names;  //all filenames parsed from list_of_files
public:
  ListOfFiles():state(IN_HEADER),names(0) {}
  ~ListOfFiles() {for(int i=0;i<(int)names.size();i++)delete names[i];}
  void setbasepath(const char *s) {
    assert(basepath.strsize()==0);
    basepath+=s;
  }
  void addchar(char c) {
    if(c!=EOF)list_of_files+=c;
    if(c==10 || c==13 || c==EOF) //got a newline
      state=FINISHED_A_LINE; //empty lines / extra newlines (cr, crlf or lf) are ignored
    else if(state==IN_HEADER)return; //ignore anything in header
    else if(c==TAB) //got a tab
      state=FINISHED_A_FILENAME; //ignore the rest (other columns)
    else { // got a character
      if(state==FINISHED_A_FILENAME)return; //ignore the rest (other columns)
      if(state==FINISHED_A_LINE) {
        names.push_back(new FileName(basepath.c_str()));
        state=PROCESSING_FILENAME;
        if(c=='/' || c=='\\')
          quit("For security reasons absolute paths are not allowed in the file list.");
        //TODO: prohibit parent folder references in path ('/../')
      }
      if(c==0 || c==':' || c=='?' || c=='*') {printf("\nIllegal character ('%c') in file list.",c);quit();}
      if(c==BADSLASH)c=GOODSLASH;
      (*names[names.size()-1])+=c;
    }
  }
  int getcount() {return (int)names.size();}
  const char * getfilename(int i) {return names[i]->c_str();}
  String * getstring() {return &list_of_files;}
};


int main(int argc, char** argv) {
  try {

    printf(PROGNAME " archiver v" PROGVERSION " (C) " PROGYEAR ", Matt Mahoney et al.\n");

    // Print help message
    if (argc<2) {
      printf(
        "\n"
        "Free under GPL, http://www.gnu.org/licenses/gpl.txt\n\n"
        "To compress:\n"
        "\n"
        "  " PROGNAME " -LEVEL[SWITCHES] INPUTSPEC [OUTPUTSPEC]\n"
        "\n"
        "    -LEVEL:\n"
        "      -0 = store (uses 15 MB)\n"
        "      -1 -2 -3 = faster (uses 44, 52, 68 MB)\n"
        "      -4 -5 -6 -7 -8 -9 = smaller (uses 268, 388, 626, 1103, 2057, 3965 MB)\n"
        "    The listed memory requirements are indicative, actual usage may vary\n"
        "    depending on several factors including need for temporary files,\n"
        "    temporary memory needs of some preprocessing (transformations), etc.\n"
        "\n"
        "    Optional compression SWITCHES:\n"
        "      b = Brute-force detection of DEFLATE streams\n"
        "      e = Pre-train x86/x64 model\n"
        "      t = Pre-train main model with word and expression list\n"
        "          (english.dic, english.exp)\n"
        "      a = Adaptive learning rate\n"
        "      s = Skip the color transform, just reorder the RGB channels\n"
        "      f = Bypass modeling and mixing on long matches\n\n"
        "    INPUTSPEC:\n"
        "    The input may be a FILE or a PATH/FILE or a [PATH/]@FILELIST.\n"
        "    Only file content and the file size is kept in the archive. Filename,\n"
        "    path, date and any file attributes or permissions are not stored.\n"
        "    When a @FILELIST is provided the FILELIST file will be considered\n"
        "    implicitly as the very first input file. It will be compressed and upon\n"
        "    decompression it will be extracted. The FILELIST is a tab separated text\n"
        "    file where the first column contains the names and optionally the relative\n"
        "    paths of the files to be compressed. The paths should be relative to the\n"
        "    FILELIST file. In the other columns you may store any information you wish\n"
        "    to keep about the files (timestamp, owner, attributes or your own remarks).\n"
        "    These extra columns will be ignored by the compressor and the decompressor\n"
        "    but you may restore full file information using them with a 3rd party\n"
        "    utility. The FILELIST file must contain a header but will be ignored.\n"
        "\n"
        "    OUTPUTSPEC:\n"
        "    When omitted: the archive will be created in the same folder where the\n"
        "    input file resides. The archive filename will be constructed from the\n"
        "    input file name by appending ." PROGNAME PROGVERSION " extension\n"
        "    to it.\n"
        "    When OUTPUTSPEC is a filename (with an optional path) it will be\n"
        "    used as the archive filename.\n"
        "    When OUTPUTSPEC is a folder the archive file will be generated from\n"
        "    the input filename and will be created in the specified folder.\n"
        "    If the archive file already exists it will be overwritten.\n"
        "\n"
        "    Examples:\n"
        "      " PROGNAME " -8 enwik8\n"
        "      " PROGNAME " -8ba b64sample.xml\n"
        "      " PROGNAME " -8 @myfolder/myfilelist.txt\n"
        "      " PROGNAME " -8a benchmark/enwik8 results/enwik8_a_" PROGNAME PROGVERSION "\n"
        "\n"
        "To extract (decompress contents):\n"
        "\n"
        "  " PROGNAME " -d [INPUTPATH/]ARCHIVEFILE [[OUTPUTPATH/]OUTPUTFILE]\n"
        "    If an output folder is not provided the output file will go to the input\n"
        "    folder. If an output filename is not provided output filename will be the\n"
        "    same as ARCHIVEFILE without the last extension (e.g. without ." PROGNAME PROGVERSION")\n"
        "    When OUTPUTPATH does not exist it will be created.\n"
        "    When the archive contains multiple files, first the @LISTFILE is extracted\n"
        "    then the rest of the files. Any required folders will be created.\n"
        "\n"
        "To test:\n"
        "\n"
        "  " PROGNAME " -t [INPUTPATH/]ARCHIVEFILE [[OUTPUTPATH/]OUTPUTFILE]\n"
        "    Tests contents of the archive by decompressing it (to memory) and comparing\n"
        "    the result to the original file(s). If a file fails the test, the first\n"
        "    mismatched position will be printed to screen.\n"
        "\n"
        "To list archive contents:\n"
        "\n"
        "  " PROGNAME " -l [INPUTFOLDER/]ARCHIVEFILE\n"
        "    Extracts @FILELIST from archive (to memory) and prints its content\n"
        "    to screen. This command is only applicable to multi-file archives.\n"
        "\n"
        "Additional optional swithes:\n"
        "\n"
        "    -v\n"
        "    Print more detailed (verbose) information to screen.\n"
        "\n"
        "    -log LOGFILE\n"
        "    Logs (appends) compression results in the specified tab separated LOGFILE.\n"
        "    Logging is only applicable for compression.\n"
        "\n"
        "    -simd [NONE|SSE2|AVX2]\n"
        "    Overrides detected SIMD instruction set for neural network operations\n"
        "\n"
        "Remark: the command line parameters may be used in any order except the input\n"
        "and output: always the input comes first then (the optional) output.\n"
        "\n"
        "    Example:\n"
        "      " PROGNAME " -8 enwik8 folder/ -v -log logfile.txt -simd sse2\n"
        "    is equivalent to:\n"
        "      " PROGNAME " -v -simd sse2 enwik8 -log logfile.txt folder/ -8\n"
      );
      quit();
    }

    // Parse command line parameters
    typedef enum {doNone, doCompress, doExtract, doCompare, doList} WHATTODO;
    WHATTODO whattodo=doNone;
    bool verbose=false;
    int c;
    int simd_iset=-1; //simd instruction set to use

    FileName input;
    FileName output;
    FileName inputpath;
    FileName outputpath;
    FileName archiveName;
    FileName logfile;

    for(int i=1;i<argc;i++) {
      int arg_len = (int)strlen(argv[i]);
      if(argv[i][0]=='-') {
        if(arg_len==1)quit("Empty command.");
        if (argv[i][1]>='0' && argv[i][1]<='9') {
          if(whattodo != doNone)quit("Only one command may be specified.");
          whattodo = doCompress;
          level = argv[i][1]-'0';
          //process optional compression switches
          for(int j=2;j<arg_len;j++) {
            switch (argv[i][j]&0xDF) {
              case 'B': options |= OPTION_BRUTE; break;
              case 'E': options |= OPTION_TRAINEXE; break;
              case 'T': options |= OPTION_TRAINTXT; break;
              case 'A': options |= OPTION_ADAPTIVE; break;
              case 'S': options |= OPTION_SKIPRGB; break;
              case 'F': options |= OPTION_FASTMODE; break;
              default: {printf("Invalid compression switch: %c",argv[1][j]); quit();}
            }
          }
        }
        else if (strcasecmp(argv[i],"-d")==0) {
          if(whattodo != doNone)quit("Only one command may be specified.");
          whattodo=doExtract;
        }
        else if (strcasecmp(argv[i],"-t")==0) {
          if(whattodo != doNone)quit("Only one command may be specified.");
          whattodo=doCompare;
        }
        else if (strcasecmp(argv[i],"-l")==0) {
          if(whattodo != doNone)quit("Only one command may be specified.");
          whattodo=doList;
        }
        else if (strcasecmp(argv[i],"-v")==0) {
          verbose=true;
        }
        else if (strcasecmp(argv[i],"-log")==0) {
          if(logfile.strsize()!=0)quit("Only one logfile may be specified.");
          if(++i==argc)quit("The -log switch requires a filename.");
          logfile+=argv[i];
        }
        else if (strcasecmp(argv[i],"-simd")==0) {
          if(++i==argc)quit("The -simd switch requires an instruction set name (NONE,SSE2,AVX2).");
          if(strcasecmp(argv[i],"NONE")==0)simd_iset=0;
          else if(strcasecmp(argv[i],"SSE2")==0)simd_iset=3;
          else if(strcasecmp(argv[i],"AVX2")==0)simd_iset=9;
          else quit("Invalid -simd option. Use -simd NONE, -simd SSE2 or -simd AVX2.");
        }
        else {
          printf("Invalid command: %s",argv[i]);
          quit();
        }
      } else { //this parameter does not begin with a dash ("-") -> it must be a folder/filename
        if(input.strsize()==0) {
          input+=argv[i];
          input.replaceslashes();
        }
        else if(output.strsize()==0) {
          output+=argv[i];
          output.replaceslashes();
        }
        else
          quit("More than two filenames specified. Only an input and an output is needed.");
      }
    }

    if(verbose) {
      //print compiled-in modules
      printf("\n");
      printf("Build: ");
      #ifdef USE_ZLIB
      printf("USE_ZLIB ");
      #else
      printf("NO_ZLIB ");
      #endif

      #ifdef USE_WAVMODEL
      printf("USE_WAVMODEL ");
      #else
      printf("NO_WAVMODEL ");
      #endif

      #ifdef USE_TEXTMODEL
      printf("USE_TEXTMODEL ");
      #else
      printf("NO_ TEXTMODEL ");
      #endif

      #ifdef USE_WORDMODEL
      printf("USE_WORDMODEL ");
      #else
      printf("NO_WORDMODEL ");
      #endif
      printf("\n");
    }

    // Determine CPU's (and OS) support for SIMD vectorization istruction set 
    int detected_simd_iset = simd_detect();
    if(simd_iset==-1)simd_iset=detected_simd_iset;
    if(simd_iset>detected_simd_iset)printf("\nOverriding system highest vectorization support. Expect a crash.");

    // Print anything only if the user wants/needs to know
    if(verbose || simd_iset!=detected_simd_iset) {
      printf("\nHighest SIMD vectorization support on this system: ");
      if(detected_simd_iset<0 || detected_simd_iset>9)quit("Oops, sorry. Unexpected result.");
      static const char* vectorization_string[10]={"None","MMX","SSE","SSE2","SSE3","SSSE3","SSE4.1","SSE4.2","AVX","AVX2"};
      printf("%s.\n", vectorization_string[detected_simd_iset]);

      printf("Using ");
      if(simd_iset>=9) printf("AVX2");
      else if(simd_iset>=3) printf("SSE2");  
      else printf("non-vectorized");  
      printf(" neural network functions.\n");  
    }

    // Set highest or user selected vectorization mode
    if(simd_iset>=9) MixerFactory::set_simd(SIMD_AVX2);
    else if(simd_iset>=3) MixerFactory::set_simd(SIMD_SSE2);
    else MixerFactory::set_simd(SIMD_NONE);

    if(verbose) {
      printf("\n");
      printf(" Command line   =");
      for(int i=0;i<argc;i++)printf(" %s",argv[i]);
      printf("\n");
    }

    // Successfully parsed command line parameters
    // Let's check their validity
    if(whattodo==doNone)
      quit("A command switch is required: -0..-9 to compress, -d to decompress, -t to test, -l to list.");
    if(input.strsize()==0) {
      printf("\nAn %s is required %s.\n",whattodo==doCompress ? "input file or filelist" : "archive filename",
      whattodo==doCompress ? "for compressing" :
      whattodo==doExtract ? "for decompressing" :
      whattodo==doCompare ? "for testing" :
      whattodo==doList ? "to list its contents":""
      );
      quit();
    }
    if(whattodo==doList && output.strsize()!=0)
      quit("The list command needs only one file parameter.");

    // File list supplied?
    if(input.beginswith("@")) {
      if(whattodo==doCompress) {
        options|=OPTION_MULTIPLE_FILE_MODE;
        input.stripstart(1);
      } else
        quit("A file list (a file name prefixed by '@') may only be specified when compressing.");
    }

    int pathtype;

    //Logfile supplied?
    if(logfile.strsize()!=0) {
      if(whattodo!=doCompress)
        quit("A log file may only be specified for compression.");
      pathtype=examinepath(logfile.c_str());
      if(pathtype==2 || pathtype==4)
        quit("Specified log file should be a file not a directory.");
      if(pathtype==0) {
        printf("\nThere is a problem with the log file: %s",logfile.c_str());
        quit();
      }
    }

    // Separate paths from input filename/directory name
    pathtype=examinepath(input.c_str());
    if(pathtype==2 || pathtype==4) {
      printf("\nSpecified input is a directory but should be a file: %s",input.c_str());quit();
    }
    if(pathtype==3) {
      printf("\nSpecified input file does not exist: %s",input.c_str());quit();
    }
    if(pathtype==0) {
      printf("\nThere is a problem with the specified input file: %s",input.c_str());quit();
    }
    if(input.lastslashpos()>=0) {
      inputpath+=input.c_str();
      inputpath.keeppath();
      input.keepfilename();
    }

    // Separate paths from output filename/directory name
    if(output.strsize()>0) {
      pathtype=examinepath(output.c_str());
      if(pathtype==1 || pathtype==3) { //is an existing file, or looks like a file
        if(output.lastslashpos()>=0) {
          outputpath+=output.c_str();
          outputpath.keeppath();
          output.keepfilename();
        }
      }
      else if (pathtype==2 || pathtype==4) {//is an existing directory, or looks like a directory
        outputpath+=output.c_str();
        if(!outputpath.endswith("/") && !outputpath.endswith("\\"))
          outputpath+=GOODSLASH;
        //output file is not specified
        output.resize(0);
        output.push_back(0);
      }
      else {
        printf("\nThere is a problem with the specified output: %s",output.c_str());
        quit();
      }
    }

    //determine archive name
    if(whattodo==doCompress) {
      archiveName+=outputpath.c_str();
      if(output.strsize()==0) { // If no archive name is provided, construct it from input (append PROGNAME extension to input filename)
        archiveName+=input.c_str();
        archiveName+="." PROGNAME PROGVERSION;
      }
      else
        archiveName+=output.c_str();
    } else { // extract/compare/list: archivename is simply the input
      archiveName+=inputpath.c_str();
      archiveName+=input.c_str();
    }
    if(verbose) {
      printf(" Archive        = %s\n",archiveName.c_str());
      printf(" Input folder   = %s\n",inputpath.strsize()==0 ? "." : inputpath.c_str());
      printf(" Output folder  = %s\n",outputpath.strsize()==0 ? "." : outputpath.c_str());
    }

    Mode mode = whattodo==doCompress ? COMPRESS : DECOMPRESS;

    ListOfFiles listoffiles;

    //set basepath for filelist
    listoffiles.setbasepath(whattodo==doCompress ? inputpath.c_str() : outputpath.c_str());

    // Process file list (in multiple file mode)
    if(options & OPTION_MULTIPLE_FILE_MODE) { //multiple file mode
      assert(whattodo==doCompress);
      // Read and parse filelist file
      FileDisk f;
      FileName fn(inputpath.c_str());fn+=input.c_str();
      f.open(fn.c_str(),true);
      while(true) {c=f.getchar();listoffiles.addchar(c);if(c==EOF)break;}
      f.close();
      //Verify input files
      for(int i=0;i<listoffiles.getcount();i++)
        getfilesize(listoffiles.getfilename(i)); // Does file exist? Is it readable? (we don't actually need the filesize now)
    } else { //single file mode or extract/compare/list
      FileName fn(inputpath.c_str());fn+=input.c_str();
      getfilesize(fn.c_str()); // Does file exist? Is it readable? (we don't actually need the filesize now)
    }

    FileDisk archive;  // compressed file

    if(mode==DECOMPRESS) {
      archive.open(archiveName.c_str(),true);
      // Verify archive header, get level and options
      int len=(int)strlen(PROGNAME);
      for(int i=0;i<len;i++)
        if(archive.getchar()!=PROGNAME[i]) {printf("%s: not a valid %s file.", archiveName.c_str(), PROGNAME); quit();}
      level=archive.getchar();
      c=archive.getchar();
      if(c==EOF) printf("Unexpected end of archive file.\n");
      options=(U8)c;
    }

    if(verbose) {
      // Print specified command
      printf(" To do          = ");
      if(whattodo==doNone)printf("-");
      if(whattodo==doCompress)printf("Compress");
      if(whattodo==doExtract)printf("Extract");
      if(whattodo==doCompare)printf("Compare");
      if(whattodo==doList)printf("List");
      printf("\n");
      // Print specified options
      printf(" Level          = %d\n",level);
      printf(" Brute      (b) = %s\n",options&OPTION_BRUTE    ? "On  (Brute-force detection of DEFLATE streams)" : "Off"); //this is a compression-only option, but we put/get it for reproducibility
      printf(" Train exe  (e) = %s\n",options&OPTION_TRAINEXE ? "On  (Pre-train x86/x64 model)" : "Off");
      printf(" Train txt  (t) = %s\n",options&OPTION_TRAINTXT ? "On  (Pre-train main model with word and expression list)" : "Off");
      printf(" Adaptive   (a) = %s\n",options&OPTION_ADAPTIVE ? "On  (Adaptive learning rate)" : "Off");
      printf(" Skip RGB   (s) = %s\n",options&OPTION_SKIPRGB  ? "On  (Skip the color transform, just reorder the RGB channels)" : "Off");
      printf(" Fast mode  (f) = %s\n",options&OPTION_FASTMODE ? "On  (Bypass modeling and mixing on long matches)" : "Off");
      printf(" File mode      = %s\n",options & OPTION_MULTIPLE_FILE_MODE ? "Multiple": "Single");
    }
    printf("\n");

    int number_of_files=1; //default for single file mode

    // Write archive header to archive file
    if (mode==COMPRESS) {
      if(options & OPTION_MULTIPLE_FILE_MODE) { //multiple file mode
        number_of_files=listoffiles.getcount();
        printf("Creating archive %s in multiple file mode with %d file%s...\n",archiveName.c_str(), number_of_files, number_of_files>1?"s":"");
      } else //single file mode
        printf("Creating archive %s in single file mode...\n",archiveName.c_str());
      archive.create(archiveName.c_str());
      archive.append(PROGNAME);
      archive.putchar(level);
      archive.putchar(options);
    }

    // In single file mode with no output filename specified we must construct it from the supplied archive filename
    if((options & OPTION_MULTIPLE_FILE_MODE)==0) { //single file mode
      if((whattodo==doExtract || whattodo==doCompare) && output.strsize()==0) {
        output+=input.c_str();
        const char* file_extension="." PROGNAME PROGVERSION;
        if(output.endswith(file_extension))
          output.stripend((int)strlen(file_extension));
        else {
          printf("Can't construct output filename from archive filename.\nArchive file extension must be: '%s'",file_extension);
          quit();
        }
      }
    }

    // Set globals according to requested compression level
    assert(level>=0 && level<=9);
    buf.setsize(MEM*8);
    for (int i = 0; i<1024; ++i)
      dt[i] = 16384 / (i + i + 3);
    Encoder en(mode, &archive);
    U64 content_size=0;
    U64 total_size=0;

    // Compress list of files
    if (mode==COMPRESS) {
      U64 start=en.size(); //header size (=8)
      if(verbose)
      printf("Writing header : %" PRIu64 " bytes\n", start);
      total_size+=start;
      if ((options & OPTION_MULTIPLE_FILE_MODE)!=0) { //multiple file mode
        U64 len1=input.size(); //with ending zero
        for (U64 i=0; i<len1; i++)
          en.compress(input[i]); //ASCIIZ filename
        const String * const s=listoffiles.getstring();
        U64 len2=s->size(); //with ending zero
        for (U64 i=0; i<len2; i++)
          en.compress((*s)[i]); //ASCIIZ filenames
        printf("1/2 - Filename of listfile : %" PRIu64 " bytes\n", len1);
        printf("2/2 - Content of listfile  : %" PRIu64 " bytes\n", len2);
        printf("----- Compressed to        : %" PRIu64 " bytes\n", en.size()-start);
        total_size+=len1+len2;
      }
    }

    // Decompress list of files
    if (mode==DECOMPRESS && (options & OPTION_MULTIPLE_FILE_MODE)!=0) {
      // name of listfile
      FileName list_filename(outputpath.c_str());
      if(output.strsize()!=0)
        quit("Output filename must not be specified when extracting multiple files.");
      while((c=en.decompress())!=0) {
        if(c==255)quit("Invalid character or unexpected end of archive file.");
        list_filename+=(char)c;
      }
      while((c=en.decompress())!=0) {
        if(c==255)quit("Invalid character or unexpected end of archive file.");
        listoffiles.addchar((char)c);
      }
      if (whattodo==doList)
        printf("File list of %s archive:\n", archiveName.c_str());

      number_of_files=listoffiles.getcount();

      //write filenames to screen or listfile or verify (compare) contents
      if (whattodo==doList)
        printf("%s\n",listoffiles.getstring()->c_str());
      else if (whattodo==doExtract) {
        FileDisk f;
        f.create(list_filename.c_str());
        String *s=listoffiles.getstring();
        f.blockwrite((U8*)(&(*s)[0]),s->strsize());
        f.close();
      }
      else if (whattodo==doCompare) {
        FileDisk f;
        f.open(list_filename.c_str(),true);
        String *s=listoffiles.getstring();
        for(int i=0;i<s->strsize();i++)
          if(f.getchar()!=(*s)[i])
            quit("Mismatch in list of files.");
        if(f.getchar()!=EOF) printf("Filelist on disk is larger than in archive.\n");
        f.close();
      }
    }

    if(whattodo==doList && (options & OPTION_MULTIPLE_FILE_MODE)==0)
      quit("Can't list. Filenames are not stored in single file mode.\n");

    // Compress or decompress files
    if (mode==COMPRESS) {
      if ((options & OPTION_MULTIPLE_FILE_MODE)!=0) { //multiple file mode
        for (int i=0; i<number_of_files; i++) {
          const char* fname=listoffiles.getfilename(i);
          U64 fsize=getfilesize(fname);
          printf("\n%d/%d - Filename: %s (%" PRIu64 " bytes)\n", i+1, number_of_files, fname, fsize);
          compressfile(fname, fsize, en, verbose);
          total_size+=fsize+4; //4: file size information
          content_size+=fsize;
        }
      } else { //single file mode
        FileName fn;fn+=inputpath.c_str();fn+=input.c_str();
        const char * fname=fn.c_str();
        U64 fsize=getfilesize(fname);
        printf("\nFilename: %s (%" PRIu64 " bytes)\n",fname, fsize);
        compressfile(fname, fsize, en, verbose);
        total_size+=fsize+4; //4: file size information
        content_size+=fsize;
      }

      U64 pre_flush=en.size();
      en.flush();
      total_size+=en.size()-pre_flush; //we consider padding bytes as auxiliary bytes
      printf("-----------------------\n");
      printf("Total input size     : %" PRIu64 "\n", content_size);
      if(verbose)
      printf("Total metadata bytes : %" PRIu64 "\n", total_size-content_size);
      printf("Total archive size   : %" PRIu64 "\n", en.size());
      printf("\n");
      // Log compression results
      if(logfile.strsize()!=0) {
        String results;
        pathtype=examinepath(logfile.c_str());
        //Write header if needed
        if(pathtype==3 /*does not exist*/ || (pathtype==1 && getfilesize(logfile.c_str())==0)/*exists but does not contain a header*/)
          results+="PROG_NAME\tPROG_VERSION\tCOMMAND_LINE\tLEVEL\tINPUT_FILENAME\tORIGINAL_SIZE_BYTES\tCOMPRESSED_SIZE_BYTES\tRUNTIME_MS\n";
        //Write results to logfile
        results+=PROGNAME "\t" PROGVERSION "\t";
        for(int i=1;i<argc;i++){if(i!=0)results+=' ';results+=argv[i];}results+="\t";
        results+=U64(level);results+="\t";
        results+=input.c_str();results+="\t";
        results+=content_size;results+="\t";
        results+=en.size();results+="\t";
        results+=U64(programChecker.get_runtime()*1000.0);results+="\t";
        results+="\n";
        appendtofile(logfile.c_str(),results.c_str());
        printf("Results logged to file '%s'\n",logfile.c_str());
        printf("\n");
      }
    } else { //decompress
      if(whattodo==doExtract || whattodo==doCompare) {
        FMode fmode= whattodo==doExtract ? FDECOMPRESS : FCOMPARE;
        if ((options & OPTION_MULTIPLE_FILE_MODE)!=0) { //multiple file mode
          for (int i=0; i<number_of_files; i++) {
            const char* fname=listoffiles.getfilename(i);
            decompressfile(fname, fmode, en);
          }
        } else { //single file mode
          FileName fn;fn+=outputpath.c_str();fn+=output.c_str();
          const char * fname=fn.c_str();
          decompressfile(fname, fmode, en);
        }
      }
    }

    archive.close();
    if (whattodo!=doList) programChecker.print();
  }
  // we catch only the intentional exceptions from quit() to exit gracefully
  // any other exception should result in a crash and must be investigated
  catch(IntentionalException const& e) {}

  return 0;
}
