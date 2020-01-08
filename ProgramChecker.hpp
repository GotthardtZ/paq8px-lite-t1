#ifndef PAQ8PX_PROGRAMCHECKER_HPP
#define PAQ8PX_PROGRAMCHECKER_HPP

#include <chrono>
#include <cstdint>
#include <cinttypes>
#include <cassert>
#include <cstdio>

/**
 * Track time and memory used
 * \remark: only Array<T> reports its memory usage, we don't know about other types
 */
class ProgramChecker {
private:
    uint64_t memUsed {};  // Bytes currently in use (all allocated minus all freed)
    uint64_t maxMem {};   // Most bytes allocated ever
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;

    /**
     * Constructor is private so that it cannot be called
     */
    ProgramChecker() {
      startTime = std::chrono::high_resolution_clock::now();
    }

    /**
     * Copy constructor is private so that it cannot be called
     */
    ProgramChecker(ProgramChecker const &) {};

    /**
     * Assignment operator is private so that it cannot be called
     */
    ProgramChecker &operator=(ProgramChecker const &) {};

    static ProgramChecker *instance;
public:
    static ProgramChecker *getInstance();;
    void alloc(uint64_t n);
    void free(uint64_t n);
    [[nodiscard]] double getRuntime() const;

    /**
     * Print elapsed time and used memory
     */
    void print() const;
    ~ProgramChecker();
};

#endif //PAQ8PX_PROGRAMCHECKER_HPP
