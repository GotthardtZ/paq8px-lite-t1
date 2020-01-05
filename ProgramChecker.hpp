#ifndef PAQ8PX_PROGRAMCHECKER_HPP
#define PAQ8PX_PROGRAMCHECKER_HPP

#include <chrono>
#include <cstdint>
#include <cinttypes>
#include <cassert>
#include <cstdio>

//////////////////////// Program Checker /////////////////////

// Track time and memory used
// Remark: only Array<T> reports its memory usage, we don't know about other types
class ProgramChecker {
private:
    uint64_t memUsed;  // Bytes currently in use (all allocated minus all freed)
    uint64_t maxMem;   // Most bytes allocated ever
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
public:
    void alloc(uint64_t n) {
      memUsed += n;
      if( memUsed > maxMem )
        maxMem = memUsed;
    }

    void free(uint64_t n) {
      assert(memUsed >= n);
      memUsed -= n;
    }

    ProgramChecker() : memUsed(0), maxMem(0) {
      startTime = std::chrono::high_resolution_clock::now();
    }

    [[nodiscard]] double getRuntime() const {
      const std::chrono::time_point<std::chrono::high_resolution_clock> finishTime = std::chrono::high_resolution_clock::now();
      return std::chrono::duration<double>(finishTime - startTime).count();
    }

    void print() const {  // Print elapsed time and used memory
      const double runtime = getRuntime();
      printf("Time %1.2f sec, used %" PRIu64 " MB (%" PRIu64 " bytes) of memory\n", runtime, maxMem >> 20U, maxMem);
    }

    ~ProgramChecker() {
      assert(memUsed == 0); // We expect that all reserved memory is already properly freed
    }

} programChecker;


#endif //PAQ8PX_PROGRAMCHECKER_HPP
