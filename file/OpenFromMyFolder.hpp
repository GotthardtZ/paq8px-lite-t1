#ifndef PAQ8PX_OPENFROMMYFOLDER_HPP
#define PAQ8PX_OPENFROMMYFOLDER_HPP

#include "FileDisk.hpp"

#ifdef __APPLE__

#include <mach-o/dyld.h>

#endif

namespace ofmf {
    static char myPathError[] = "Can't determine my path.";
} // namespace ofmf

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
    static void myself(FileDisk *f);

    /**
     * This static method will open a file from the executable's folder.
     * Only ASCII file names are supported.
     * @param f
     * @param filename
     */
    static void anotherFile(FileDisk *f, const char *filename);
};

#endif //PAQ8PX_OPENFROMMYFOLDER_HPP
