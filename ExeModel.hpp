#ifndef PAQ8PX_EXEMODEL_HPP
#define PAQ8PX_EXEMODEL_HPP

#include "RingBuffer.hpp"
#include <cstdint>

/**
 * Model for x86/x64 code.
 * Based on the previous paq* exe models and on DisFilter (http://www.farbrausch.de/~fg/code/disfilter/) by Fabian Giesen.
 *
 * Attempts to parse the input stream as x86/x64 instructions, and quantizes them into 32-bit representations that are then
 * used as context to predict the next bits, and also extracts possible sparse contexts at several previous positions that
 * are relevant to parsing (prefixes, opcode, Mod and R/M fields of the ModRM byte, Scale field of the SIB byte)
 */
class ExeModel {
    // formats
    enum InstructionFormat {
        // encoding mode
                fNM = 0x0, // no ModRM
        fAM = 0x1, // no ModRM, "address mode" (jumps or direct addresses)
        fMR = 0x2, // ModRM present
        fMEXTRA = 0x3, // ModRM present, includes extra bits for opcode
        fMODE = 0x3, // bitmask for mode

        // no ModRM: size of immediate operand
                fNI = 0x0, // no immediate
        fBI = 0x4, // byte immediate
        fWI = 0x8, // word immediate
        fDI = 0xc, // dword immediate
        fTYPE = 0xc, // type mask

        // address mode: type of address operand
                fAD = 0x0, // absolute address
        fDA = 0x4, // dword absolute jump target
        fBR = 0x8, // byte relative jump target
        fDR = 0xc, // dword relative jump target

        // others
                fERR = 0xf, // denotes invalid opcodes
    };

    // 1 byte opcodes
    static constexpr uint8_t Table1[256] = {
            // 0       1       2       3       4       5       6       7       8       9       a       b       c       d       e       f
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fNM | fBI, fNM | fDI, fNM | fNI, fNM | fNI, fMR | fNI,
            fMR | fNI, fMR | fNI, fMR | fNI, fNM | fBI, fNM | fDI, fNM | fNI, fNM | fNI, // 0
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fNM | fBI, fNM | fDI, fNM | fNI, fNM | fNI, fMR | fNI,
            fMR | fNI, fMR | fNI, fMR | fNI, fNM | fBI, fNM | fDI, fNM | fNI, fNM | fNI, // 1
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fNM | fBI, fNM | fDI, fNM | fNI, fNM | fNI, fMR | fNI,
            fMR | fNI, fMR | fNI, fMR | fNI, fNM | fBI, fNM | fDI, fNM | fNI, fNM | fNI, // 2
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fNM | fBI, fNM | fDI, fNM | fNI, fNM | fNI, fMR | fNI,
            fMR | fNI, fMR | fNI, fMR | fNI, fNM | fBI, fNM | fDI, fNM | fNI, fNM | fNI, // 3

            fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI,
            fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, // 4
            fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI,
            fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, // 5
            fNM | fNI, fNM | fNI, fMR | fNI, fMR | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fDI,
            fMR | fDI, fNM | fBI, fMR | fBI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, // 6
            fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR,
            fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, // 7

            fMR | fBI, fMR | fDI, fMR | fBI, fMR | fBI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // 8
            fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI,
            fNM | fNI, fAM | fDA, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, // 9
            fAM | fAD, fAM | fAD, fAM | fAD, fAM | fAD, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fBI,
            fNM | fDI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, // a
            fNM | fBI, fNM | fBI, fNM | fBI, fNM | fBI, fNM | fBI, fNM | fBI, fNM | fBI, fNM | fBI, fNM | fDI,
            fNM | fDI, fNM | fDI, fNM | fDI, fNM | fDI, fNM | fDI, fNM | fDI, fNM | fDI, // b

            fMR | fBI, fMR | fBI, fNM | fWI, fNM | fNI, fMR | fNI, fMR | fNI, fMR | fBI, fMR | fDI, fNM | fBI,
            fNM | fNI, fNM | fWI, fNM | fNI, fNM | fNI, fNM | fBI, fERR, fNM | fNI, // c
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fNM | fBI, fNM | fBI, fNM | fNI, fNM | fNI, fMR | fNI,
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // d
            fAM | fBR, fAM | fBR, fAM | fBR, fAM | fBR, fNM | fBI, fNM | fBI, fNM | fBI, fNM | fBI, fAM | fDR,
            fAM | fDR, fAM | fAD, fAM | fBR, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, // e
            fNM | fNI, fERR, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fMEXTRA, fMEXTRA, fNM | fNI, fNM | fNI,
            fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fMEXTRA, fMEXTRA, // f
    };

    // 2 byte opcodes
    static constexpr uint8_t Table2[256] = {
            // 0       1       2       3       4       5       6       7       8       9       a       b       c       d       e       f
            fERR, fERR, fERR, fERR, fERR, fERR, fNM | fNI, fERR, fNM | fNI, fNM | fNI, fERR, fERR, fERR, fERR, fERR,
            fERR, // 0
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fERR,
            fERR, fERR, fERR, fERR, fERR, fERR, // 1
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fERR, fERR, fERR, fERR, fMR | fNI, fMR | fNI, fMR | fNI,
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // 2
            fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fERR, fNM | fNI, fERR, fERR, fERR, fERR,
            fERR, fERR, fERR, fERR, // 3

            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // 4
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // 5
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // 6
            fMR | fBI, fMR | fBI, fMR | fBI, fMR | fBI, fMR | fNI, fMR | fNI, fMR | fNI, fNM | fNI, fERR, fERR, fERR,
            fERR, fERR, fERR, fMR | fNI, fMR | fNI, // 7

            fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR,
            fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR, fAM | fDR, // 8
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // 9
            fNM | fNI, fNM | fNI, fNM | fNI, fMR | fNI, fMR | fBI, fMR | fNI, fMR | fNI, fMR | fNI, fERR, fERR, fERR,
            fMR | fNI, fMR | fBI, fMR | fNI, fERR, fMR | fNI, // a
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fERR, fERR, fERR,
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // b

            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fNM | fNI,
            fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, fNM | fNI, // c
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // d
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // e
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fERR, // f
    };

    // 3 byte opcodes 0F38XX
    static constexpr uint8_t Table3_38[256] = {
            // 0       1       2       3       4       5       6       7       8       9       a       b       c       d       e       f
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI,
            fMR | fNI, fMR | fNI, fMR | fNI, fERR, fERR, fERR, fERR, // 0
            fMR | fNI, fERR, fERR, fERR, fMR | fNI, fMR | fNI, fERR, fMR | fNI, fERR, fERR, fERR, fERR, fMR | fNI,
            fMR | fNI, fMR | fNI, fERR, // 1
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fERR, fERR, fMR | fNI, fMR | fNI,
            fMR | fNI, fMR | fNI, fERR, fERR, fERR, fERR, // 2
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fERR, fMR | fNI, fMR | fNI, fMR | fNI,
            fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // 3
            fMR | fNI, fMR | fNI, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR,
            fERR, // 4
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 5
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 6
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 7
            fMR | fNI, fMR | fNI, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR,
            fERR, // 8
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 9
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // a
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // b
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // c
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // d
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // e
            fMR | fNI, fMR | fNI, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR,
            fERR, // f
    };

    // 3 byte opcodes 0F3AXX
    static constexpr uint8_t Table3_3A[256] = {
            // 0       1       2       3       4       5       6       7       8       9       a       b       c       d       e       f
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fMR | fBI, fMR | fBI, fMR | fBI, fMR | fBI, fMR | fBI,
            fMR | fBI, fMR | fBI, fMR | fBI, // 0
            fERR, fERR, fERR, fERR, fMR | fBI, fMR | fBI, fMR | fBI, fMR | fBI, fERR, fERR, fERR, fERR, fERR, fERR,
            fERR, fERR, // 1
            fMR | fBI, fMR | fBI, fMR | fBI, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR,
            fERR, // 2
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 3
            fMR | fBI, fMR | fBI, fMR | fBI, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR,
            fERR, // 4
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 5
            fMR | fBI, fMR | fBI, fMR | fBI, fMR | fBI, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR,
            fERR, fERR, // 6
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 7
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 8
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // 9
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // a
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // b
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // c
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // d
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // e
            fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, fERR, // f
    };

    // escape opcodes using ModRM byte to get more variants
    static constexpr uint8_t TableX[32] = {
            // 0       1       2       3       4       5       6       7
            fMR | fBI, fERR, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // escapes for 0xf6
            fMR | fDI, fERR, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, fMR | fNI, // escapes for 0xf7
            fMR | fNI, fMR | fNI, fERR, fERR, fERR, fERR, fERR, fERR, // escapes for 0xfe
            fMR | fNI, fMR | fNI, fMR | fNI, fERR, fMR | fNI, fERR, fMR | fNI, fERR, // escapes for 0xff
    };

    static constexpr uint8_t InvalidX64Ops[19] = {0x06, 0x07, 0x16, 0x17, 0x1E, 0x1F, 0x27, 0x2F, 0x37, 0x3F, 0x60,
                                                  0x61, 0x62, 0x82, 0x9A, 0xD4, 0xD5, 0xD6, 0xEA,};
    static constexpr uint8_t X64Prefixes[8] = {0x26, 0x2E, 0x36, 0x3E, 0x9B, 0xF0, 0xF2, 0xF3,};

    enum InstructionCategory {
        OP_INVALID = 0,
        OP_PREFIX_SEGREG = 1,
        OP_PREFIX = 2,
        OP_PREFIX_X87FPU = 3,
        OP_GEN_DATAMOV = 4,
        OP_GEN_STACK = 5,
        OP_GEN_CONVERSION = 6,
        OP_GEN_ARITH_DECIMAL = 7,
        OP_GEN_ARITH_BINARY = 8,
        OP_GEN_LOGICAL = 9,
        OP_GEN_SHF_ROT = 10,
        OP_GEN_BIT = 11,
        OP_GEN_BRANCH = 12,
        OP_GEN_BRANCH_COND = 13,
        OP_GEN_BREAK = 14,
        OP_GEN_STRING = 15,
        OP_GEN_INOUT = 16,
        OP_GEN_FLAG_CONTROL = 17,
        OP_GEN_SEGREG = 18,
        OP_GEN_CONTROL = 19,
        OP_SYSTEM = 20,
        OP_X87_DATAMOV = 21,
        OP_X87_ARITH = 22,
        OP_X87_COMPARISON = 23,
        OP_X87_TRANSCENDENTAL = 24,
        OP_X87_LOAD_CONSTANT = 25,
        OP_X87_CONTROL = 26,
        OP_X87_CONVERSION = 27,
        OP_STATE_MANAGEMENT = 28,
        OP_MMX = 29,
        OP_SSE = 30,
        OP_SSE_DATAMOV = 31,
    };

    static constexpr uint8_t TypeOp1[256] = {OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY,
                                             OP_GEN_ARITH_BINARY, //03
                                             OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_STACK, OP_GEN_STACK, //07
                                             OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_LOGICAL, //0B
                                             OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_STACK, OP_PREFIX, //0F
                                             OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY,
                                             OP_GEN_ARITH_BINARY, //13
                                             OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_STACK, OP_GEN_STACK, //17
                                             OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY,
                                             OP_GEN_ARITH_BINARY, //1B
                                             OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_STACK, OP_GEN_STACK, //1F
                                             OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_LOGICAL, //23
                                             OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_PREFIX_SEGREG,
                                             OP_GEN_ARITH_DECIMAL, //27
                                             OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY,
                                             OP_GEN_ARITH_BINARY, //2B
                                             OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_PREFIX_SEGREG,
                                             OP_GEN_ARITH_DECIMAL, //2F
                                             OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_LOGICAL, //33
                                             OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_PREFIX_SEGREG,
                                             OP_GEN_ARITH_DECIMAL, //37
                                             OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY,
                                             OP_GEN_ARITH_BINARY, //3B
                                             OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_PREFIX_SEGREG,
                                             OP_GEN_ARITH_DECIMAL, //3F
                                             OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY,
                                             OP_GEN_ARITH_BINARY, //43
                                             OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY,
                                             OP_GEN_ARITH_BINARY, //47
                                             OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY,
                                             OP_GEN_ARITH_BINARY, //4B
                                             OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY,
                                             OP_GEN_ARITH_BINARY, //4F
                                             OP_GEN_STACK, OP_GEN_STACK, OP_GEN_STACK, OP_GEN_STACK, //53
                                             OP_GEN_STACK, OP_GEN_STACK, OP_GEN_STACK, OP_GEN_STACK, //57
                                             OP_GEN_STACK, OP_GEN_STACK, OP_GEN_STACK, OP_GEN_STACK, //5B
                                             OP_GEN_STACK, OP_GEN_STACK, OP_GEN_STACK, OP_GEN_STACK, //5F
                                             OP_GEN_STACK, OP_GEN_STACK, OP_GEN_BREAK, OP_GEN_CONVERSION, //63
                                             OP_PREFIX_SEGREG, OP_PREFIX_SEGREG, OP_PREFIX, OP_PREFIX, //67
                                             OP_GEN_STACK, OP_GEN_ARITH_BINARY, OP_GEN_STACK, OP_GEN_ARITH_BINARY, //6B
                                             OP_GEN_INOUT, OP_GEN_INOUT, OP_GEN_INOUT, OP_GEN_INOUT, //6F
                                             OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND,
                                             OP_GEN_BRANCH_COND, //73
                                             OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND,
                                             OP_GEN_BRANCH_COND, //77
                                             OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND,
                                             OP_GEN_BRANCH_COND, //7B
                                             OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND,
                                             OP_GEN_BRANCH_COND, //7F
                                             OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY,
                                             OP_GEN_ARITH_BINARY, //83
                                             OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_DATAMOV,
                                             OP_GEN_DATAMOV, //87
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //8B
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_STACK, //8F
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //93
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //97
                                             OP_GEN_CONVERSION, OP_GEN_CONVERSION, OP_GEN_BRANCH, OP_PREFIX_X87FPU, //9B
                                             OP_GEN_STACK, OP_GEN_STACK, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //9F
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //A3
                                             OP_GEN_STRING, OP_GEN_STRING, OP_GEN_STRING, OP_GEN_STRING, //A7
                                             OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_STRING, OP_GEN_STRING, //AB
                                             OP_GEN_STRING, OP_GEN_STRING, OP_GEN_STRING, OP_GEN_STRING, //AF
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //B3
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //B7
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //BB
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //BF
                                             OP_GEN_SHF_ROT, OP_GEN_SHF_ROT, OP_GEN_BRANCH, OP_GEN_BRANCH, //C3
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //C7
                                             OP_GEN_STACK, OP_GEN_STACK, OP_GEN_BRANCH, OP_GEN_BRANCH, //CB
                                             OP_GEN_BREAK, OP_GEN_BREAK, OP_GEN_BREAK, OP_GEN_BREAK, //CF
                                             OP_GEN_SHF_ROT, OP_GEN_SHF_ROT, OP_GEN_SHF_ROT, OP_GEN_SHF_ROT, //D3
                                             OP_GEN_ARITH_DECIMAL, OP_GEN_ARITH_DECIMAL, OP_GEN_DATAMOV,
                                             OP_GEN_DATAMOV, //D7
                                             OP_X87_ARITH, OP_X87_DATAMOV, OP_X87_ARITH, OP_X87_DATAMOV, //DB
                                             OP_X87_ARITH, OP_X87_DATAMOV, OP_X87_ARITH, OP_X87_DATAMOV, //DF
                                             OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND,
                                             OP_GEN_BRANCH_COND, //E3
                                             OP_GEN_INOUT, OP_GEN_INOUT, OP_GEN_INOUT, OP_GEN_INOUT, //E7
                                             OP_GEN_BRANCH, OP_GEN_BRANCH, OP_GEN_BRANCH, OP_GEN_BRANCH, //EB
                                             OP_GEN_INOUT, OP_GEN_INOUT, OP_GEN_INOUT, OP_GEN_INOUT, //EF
                                             OP_PREFIX, OP_GEN_BREAK, OP_PREFIX, OP_PREFIX, //F3
                                             OP_SYSTEM, OP_GEN_FLAG_CONTROL, OP_GEN_ARITH_BINARY,
                                             OP_GEN_ARITH_BINARY, //F7
                                             OP_GEN_FLAG_CONTROL, OP_GEN_FLAG_CONTROL, OP_GEN_FLAG_CONTROL,
                                             OP_GEN_FLAG_CONTROL, //FB
                                             OP_GEN_FLAG_CONTROL, OP_GEN_FLAG_CONTROL, OP_GEN_ARITH_BINARY,
                                             OP_GEN_BRANCH, //FF
    };

    static constexpr uint8_t TypeOp2[256] = {OP_SYSTEM, OP_SYSTEM, OP_SYSTEM, OP_SYSTEM, //03
                                             OP_INVALID, OP_SYSTEM, OP_SYSTEM, OP_SYSTEM, //07
                                             OP_SYSTEM, OP_SYSTEM, OP_INVALID, OP_GEN_CONTROL, //0B
                                             OP_INVALID, OP_GEN_CONTROL, OP_INVALID, OP_INVALID, //0F
                                             OP_SSE_DATAMOV, OP_SSE_DATAMOV, OP_SSE_DATAMOV, OP_SSE_DATAMOV, //13
                                             OP_SSE, OP_SSE, OP_SSE_DATAMOV, OP_SSE_DATAMOV, //17
                                             OP_SSE, OP_GEN_CONTROL, OP_GEN_CONTROL, OP_GEN_CONTROL, //1B
                                             OP_GEN_CONTROL, OP_GEN_CONTROL, OP_GEN_CONTROL, OP_GEN_CONTROL, //1F
                                             OP_SYSTEM, OP_SYSTEM, OP_SYSTEM, OP_SYSTEM, //23
                                             OP_SYSTEM, OP_INVALID, OP_SYSTEM, OP_INVALID, //27
                                             OP_SSE_DATAMOV, OP_SSE_DATAMOV, OP_SSE, OP_SSE, //2B
                                             OP_SSE, OP_SSE, OP_SSE, OP_SSE, //2F
                                             OP_SYSTEM, OP_SYSTEM, OP_SYSTEM, OP_SYSTEM, //33
                                             OP_SYSTEM, OP_SYSTEM, OP_INVALID, OP_INVALID, //37
                                             OP_PREFIX, OP_INVALID, OP_PREFIX, OP_INVALID, //3B
                                             OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //3F
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //43
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //47
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //4B
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //4F
                                             OP_SSE_DATAMOV, OP_SSE, OP_SSE, OP_SSE, //53
                                             OP_SSE, OP_SSE, OP_SSE, OP_SSE, //57
                                             OP_SSE, OP_SSE, OP_SSE, OP_SSE, //5B
                                             OP_SSE, OP_SSE, OP_SSE, OP_SSE, //5F
                                             OP_MMX, OP_MMX, OP_MMX, OP_MMX, //63
                                             OP_MMX, OP_MMX, OP_MMX, OP_MMX, //67
                                             OP_MMX, OP_MMX, OP_MMX, OP_MMX, //6B
                                             OP_INVALID, OP_INVALID, OP_MMX, OP_MMX, //6F
                                             OP_SSE, OP_MMX, OP_MMX, OP_MMX, //73
                                             OP_MMX, OP_MMX, OP_MMX, OP_MMX, //77
                                             OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //7B
                                             OP_INVALID, OP_INVALID, OP_MMX, OP_MMX, //7F
                                             OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND,
                                             OP_GEN_BRANCH_COND, //83
                                             OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND,
                                             OP_GEN_BRANCH_COND, //87
                                             OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND,
                                             OP_GEN_BRANCH_COND, //8B
                                             OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND, OP_GEN_BRANCH_COND,
                                             OP_GEN_BRANCH_COND, //8F
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //93
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //97
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //9B
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //9F
                                             OP_GEN_STACK, OP_GEN_STACK, OP_GEN_CONTROL, OP_GEN_BIT, //A3
                                             OP_GEN_SHF_ROT, OP_GEN_SHF_ROT, OP_INVALID, OP_INVALID, //A7
                                             OP_GEN_STACK, OP_GEN_STACK, OP_SYSTEM, OP_GEN_BIT, //AB
                                             OP_GEN_SHF_ROT, OP_GEN_SHF_ROT, OP_STATE_MANAGEMENT,
                                             OP_GEN_ARITH_BINARY, //AF
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_BIT, //B3
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_CONVERSION, OP_GEN_CONVERSION, //B7
                                             OP_INVALID, OP_GEN_CONTROL, OP_GEN_BIT, OP_GEN_BIT, //BB
                                             OP_GEN_BIT, OP_GEN_BIT, OP_GEN_CONVERSION, OP_GEN_CONVERSION, //BF
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_SSE, OP_SSE, //C3
                                             OP_SSE, OP_SSE, OP_SSE, OP_GEN_DATAMOV, //C7
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //CB
                                             OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_GEN_DATAMOV, //CF
                                             OP_INVALID, OP_MMX, OP_MMX, OP_MMX, //D3
                                             OP_SSE, OP_MMX, OP_INVALID, OP_SSE, //D7
                                             OP_MMX, OP_MMX, OP_SSE, OP_MMX, //DB
                                             OP_MMX, OP_MMX, OP_SSE, OP_MMX, //DF
                                             OP_SSE, OP_MMX, OP_SSE, OP_MMX, //E3
                                             OP_SSE, OP_MMX, OP_INVALID, OP_SSE, //E7
                                             OP_MMX, OP_MMX, OP_SSE, OP_MMX, //EB
                                             OP_MMX, OP_MMX, OP_SSE, OP_MMX, //EF
                                             OP_INVALID, OP_MMX, OP_MMX, OP_MMX, //F3
                                             OP_SSE, OP_MMX, OP_SSE, OP_SSE, //F7
                                             OP_MMX, OP_MMX, OP_MMX, OP_SSE, //FB
                                             OP_MMX, OP_MMX, OP_MMX, OP_INVALID, //FF
    };

    static constexpr uint8_t TypeOp3_38[256] = {OP_SSE, OP_SSE, OP_SSE, OP_SSE, //03
                                                OP_SSE, OP_SSE, OP_SSE, OP_SSE, //07
                                                OP_SSE, OP_SSE, OP_SSE, OP_SSE, //0B
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //0F
                                                OP_SSE, OP_INVALID, OP_INVALID, OP_INVALID, //13
                                                OP_SSE, OP_SSE, OP_INVALID, OP_SSE, //17
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //1B
                                                OP_SSE, OP_SSE, OP_SSE, OP_INVALID, //1F
                                                OP_SSE, OP_SSE, OP_SSE, OP_SSE, //23
                                                OP_SSE, OP_SSE, OP_INVALID, OP_INVALID, //27
                                                OP_SSE, OP_SSE, OP_SSE, OP_SSE, //2B
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //2F
                                                OP_SSE, OP_SSE, OP_SSE, OP_SSE, //33
                                                OP_SSE, OP_SSE, OP_INVALID, OP_SSE, //37
                                                OP_SSE, OP_SSE, OP_SSE, OP_SSE, //3B
                                                OP_SSE, OP_SSE, OP_SSE, OP_SSE, //3F
                                                OP_SSE, OP_SSE, OP_INVALID, OP_INVALID, //43
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //47
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //4B
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //4F
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //53
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //57
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //5B
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //5F
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //63
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //67
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //6B
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //6F
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //73
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //77
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //7B
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //7F
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //83
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //87
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //8B
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //8F
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //93
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //97
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //9B
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //9F
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //A3
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //A7
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //AB
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //AF
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //B3
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //B7
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //BB
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //BF
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //C3
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //C7
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //CB
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //CF
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //D3
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //D7
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //DB
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //DF
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //E3
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //E7
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //EB
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //EF
                                                OP_GEN_DATAMOV, OP_GEN_DATAMOV, OP_INVALID, OP_INVALID, //F3
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //F7
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //FB
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //FF
    };

    static constexpr uint8_t TypeOp3_3A[256] = {OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //03
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //07
                                                OP_SSE, OP_SSE, OP_SSE, OP_SSE, //0B
                                                OP_SSE, OP_SSE, OP_SSE, OP_SSE, //0F
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //13
                                                OP_SSE, OP_SSE, OP_SSE, OP_SSE, //17
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //1B
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //1F
                                                OP_SSE, OP_SSE, OP_SSE, OP_INVALID, //23
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //27
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //2B
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //2F
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //33
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //37
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //3B
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //3F
                                                OP_SSE, OP_SSE, OP_SSE, OP_INVALID, //43
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //47
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //4B
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //4F
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //53
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //57
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //5B
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //5F
                                                OP_SSE, OP_SSE, OP_SSE, OP_SSE, //63
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //67
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //6B
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //6F
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //73
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //77
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //7B
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //7F
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //83
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //87
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //8B
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //8F
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //93
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //97
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //9B
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //9F
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //A3
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //A7
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //AB
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //AF
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //B3
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //B7
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //BB
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //BF
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //C3
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //C7
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //CB
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //CF
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //D3
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //D7
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //DB
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //DF
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //E3
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //E7
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //EB
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //EF
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //F3
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //F7
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //FB
                                                OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, //FF
    };

    static constexpr uint8_t TypeOpX[32] = {
            // escapes for F6
            OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY,
            OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY,
            // escapes for F7
            OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_LOGICAL, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY,
            OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY,
            // escapes for FE
            OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID, OP_INVALID,
            OP_INVALID,
            // escapes for FF
            OP_GEN_ARITH_BINARY, OP_GEN_ARITH_BINARY, OP_GEN_BRANCH, OP_GEN_BRANCH, OP_GEN_BRANCH, OP_GEN_BRANCH,
            OP_GEN_STACK, OP_INVALID,};

    enum Prefixes {
        ES_OVERRIDE = 0x26,
        CS_OVERRIDE = 0x2E,
        SS_OVERRIDE = 0x36,
        DS_OVERRIDE = 0x3E,
        FS_OVERRIDE = 0x64,
        GS_OVERRIDE = 0x65,
        AD_OVERRIDE = 0x67,
        WAIT_FPU = 0x9B,
        LOCK = 0xF0,
        REP_N_STR = 0xF2,
        REP_STR = 0xF3,
    };

    enum Opcodes {
        // 1-byte opcodes of special interest (for one reason or another)
                OP_2BYTE = 0x0f, // start of 2-byte opcode
        OP_OSIZE = 0x66, // operand size prefix
        OP_CALLF = 0x9a,
        OP_RETNI = 0xc2, // ret near+immediate
        OP_RETN = 0xc3,
        OP_ENTER = 0xc8,
        OP_INT3 = 0xcc,
        OP_INTO = 0xce,
        OP_CALLN = 0xe8,
        OP_JMPF = 0xea,
        OP_ICEBP = 0xf1,
    };

    enum ExeState {
        Start = 0,
        Pref_Op_Size = 1,
        Pref_MultiByte_Op = 2,
        ParseFlags = 3,
        ExtraFlags = 4,
        ReadModRM = 5,
        Read_OP3_38 = 6,
        Read_OP3_3A = 7,
        ReadSIB = 8,
        Read8 = 9,
        Read16 = 10,
        Read32 = 11,
        Read8_ModRM = 12,
        Read16_f = 13,
        Read32_ModRM = 14,
        Error = 15,
    };

    static constexpr uint32_t CacheSize = 32;

    struct OpCache {
        uint32_t Op[CacheSize];
        uint32_t Index;
    };

    struct Instruction {
        uint32_t data;
        uint8_t prefix, code, ModRM, SIB, REX, flags, bytesRead, size, category;
        bool mustCheckRex, decoding, o16, imm8;
    };

    static constexpr int codeShift = 3U;
    static constexpr uint32_t codeMask = 0xFFU << codeShift; // 0x000007F8
    static constexpr uint32_t clearCodeMask = 0xFFFFFFFF ^codeMask; // 0xFFFFF807
    static constexpr uint32_t prefixMask = (1U << codeShift) - 1; // 0x07
    static constexpr uint32_t operandSizeOverride = 0x01U << (8 + codeShift); // 0x00000800
    static constexpr uint32_t multiByteOpcode = 0x02U << (8 + codeShift); // 0x00001000
    static constexpr uint32_t prefixRex = 0x04U << (8 + codeShift); // 0x00002000
    static constexpr uint32_t prefix38 = 0x08U << (8 + codeShift); // 0x00004000
    static constexpr uint32_t prefix3A = 0x10U << (8 + codeShift); // 0x00008000
    static constexpr uint32_t hasExtraFlags = 0x20U << (8 + codeShift); // 0x00010000
    static constexpr uint32_t hasModRm = 0x40U << (8 + codeShift); // 0x00020000
    static constexpr uint32_t ModRMShift = 7 + 8 + codeShift; // 18
    static constexpr uint32_t SIBScaleShift = ModRMShift + 8 - 6; // 20
    static constexpr uint32_t RegDWordDisplacement = 1U << (8 + SIBScaleShift); // 0x10000000
    static constexpr uint32_t AddressMode = 2U << (8 + SIBScaleShift); // 0x20000000
    static constexpr uint32_t TypeShift = 2U + 8 + SIBScaleShift; // 30
    static constexpr uint32_t CategoryShift = 5U;
    static constexpr uint32_t CategoryMask = ((1U << CategoryShift) - 1); //0x1F (31)
    static constexpr uint8_t ModRM_mod = 0xC0;
    static constexpr uint8_t ModRM_reg = 0x38;
    static constexpr uint8_t ModRM_rm = 0x07;
    static constexpr uint8_t SIB_scale = 0xC0;
    static constexpr uint8_t SIB_index = 0x38; //unused
    static constexpr uint8_t SIB_base = 0x07;
    static constexpr uint8_t REX_w = 0x08;

    static constexpr uint32_t minRequired = 8; // minimum required consecutive valid instructions to be considered as code
private:
    static constexpr int nCM1 = 10, nCM2 = 10, nIM = 1;

public:
    static constexpr int MIXERINPUTS = (nCM1 + nCM2) * (ContextMap2::MIXERINPUTS + ContextMap2::MIXERINPUTS_RUN_STATS +
                                                        ContextMap2::MIXERINPUTS_BYTE_HISTORY) +
                                       nIM * IndirectMap::MIXERINPUTS; // 142
    static constexpr int MIXERCONTEXTS = 1024 + 1024 + 1024 + 8192 + 8192 + 8192; // 27648
    static constexpr int MIXERCONTEXTSETS = 6;

private:
    const Shared *const shared;
    const ModelStats *const stats;
    ContextMap2 cm;
    IndirectMap iMap;
    OpCache cache;
    uint32_t stateBh[256];
    ExeState pState, state;
    Instruction op;
    uint32_t totalOps, opMask, opCategoryMask, context;
    uint64_t brkCtx; // hash
    bool valid;

    static inline bool isInvalidX64Op(const uint8_t op) {
      for( int i = 0; i < 19; i++ ) {
        if( op == InvalidX64Ops[i] )
          return true;
      }
      return false;
    }

    static inline bool isValidX64Prefix(const uint8_t prefix) {
      for( int i = 0; i < 8; i++ ) {
        if( prefix == X64Prefixes[i] )
          return true;
      }
      return ((prefix >= 0x40 && prefix <= 0x4F) || (prefix >= 0x64 && prefix <= 0x67));
    }

    static void processMode(Instruction &op, ExeState &state) {
      if((op.flags & fMODE) == fAM ) {
        op.data |= AddressMode;
        op.bytesRead = 0;
        switch( op.flags & fTYPE ) {
          case fDR:
            op.data |= (2 << TypeShift);
          case fDA:
            op.data |= (1 << TypeShift);
          case fAD: {
            state = Read32;
            break;
          }
          case fBR: {
            op.data |= (2 << TypeShift);
            state = Read8;
          }
        }
      } else {
        switch( op.flags & fTYPE ) {
          case fBI:
            state = Read8;
            break;
          case fWI: {
            state = Read16;
            op.data |= (1 << TypeShift);
            op.bytesRead = 0;
            break;
          }
          case fDI: {
            // x64 Move with 8byte immediate? [REX.W is set, opcodes 0xB8+r]
            op.imm8 = ((op.REX & REX_w) > 0 && (op.code & 0xF8U) == 0xB8U);
            if( !op.o16 || op.imm8 ) {
              state = Read32;
              op.data |= (2 << TypeShift);
            } else {
              state = Read16;
              op.data |= (3 << TypeShift);
            }
            op.bytesRead = 0;
            break;
          }
          default:
            state = Start; /*no immediate*/
        }
      }
    }

    static void processFlags2(Instruction &op, ExeState &state) {
      //if arriving from state ExtraFlags, we've already read the ModRM byte
      if((op.flags & fMODE) == fMR && state != ExtraFlags ) {
        state = ReadModRM;
        return;
      }
      processMode(op, state);
    }

    void processFlags(Instruction &op, ExeState &state) {
      if( op.code == OP_CALLF || op.code == OP_JMPF || op.code == OP_ENTER ) {
        op.bytesRead = 0;
        state = Read16_f;
        return; //must exit, ENTER has ModRM too
      }
      processFlags2(op, state);
    }

    void checkFlags(Instruction &op, ExeState &state) {
      //must peek at ModRM byte to read the REG part, so we can know the opcode
      if( op.flags == fMEXTRA )
        state = ExtraFlags;
      else if( op.flags == fERR ) {
        memset(&op, 0, sizeof(Instruction));
        state = Error;
      } else
        processFlags(op, state);
    }

    void readFlags(Instruction &op, ExeState &state) {
      op.flags = Table1[op.code];
      op.category = TypeOp1[op.code];
      checkFlags(op, state);
    }

    static void processModRm(Instruction &op, ExeState &state) {
      if((op.ModRM & ModRM_mod) == 0x40 )
        state = Read8_ModRM; //register+byte displacement
      else if((op.ModRM & ModRM_mod) == 0x80 || (op.ModRM & (ModRM_mod | ModRM_rm)) == 0x05 ||
              (op.ModRM < 0x40 && (op.SIB & SIB_base) == 0x05)) {
        state = Read32_ModRM; //register+dword displacement
        op.bytesRead = 0;
      } else
        processMode(op, state);
    }

    static void applyCodeAndSetFlag(Instruction &op, const uint32_t flag = 0) {
      op.data &= clearCodeMask;
      op.data |= (op.code << codeShift) | flag;
    }

    static inline uint32_t opN(OpCache &cache, const uint32_t n) {
      return cache.Op[(cache.Index - n) & (CacheSize - 1)];
    }

    static inline uint32_t opNCategory(uint32_t &mask, const uint32_t n) {
      return ((mask >> (CategoryShift * (n - 1))) & CategoryMask);
    }

    inline int pref(const int i) {
      INJECT_SHARED_buf
      return (buf(i) == 0x0f) + 2 * (buf(i) == 0x66) + 3 * (buf(i) == 0x67);
    }

    // Get context at buf(i) relevant to parsing 32-bit x86 code
    uint32_t execxt(int i, int x = 0) {
      INJECT_SHARED_buf
      int prefix = 0, opcode = 0, modrm = 0, sib = 0;
      if( i )
        prefix += 4 * pref(i--);
      if( i )
        prefix += pref(i--);
      if( i )
        opcode += buf(i--);
      if( i )
        modrm += buf(i--) & (ModRM_mod | ModRM_rm);
      if( i && ((modrm & ModRM_rm) == 4) && (modrm < ModRM_mod))
        sib = buf(i) & SIB_scale;
      return prefix | opcode << 4 | modrm << 12 | x << 20 | sib << (28 - 6);
    }

    void update();

public:
    ExeModel(const Shared *const sh, const ModelStats *const st, const uint64_t size) : shared(sh), stats(st),
                                                                                        cm(sh, size, nCM1 + nCM2, 64,
                                                                                           CM_USE_RUN_STATS |
                                                                                           CM_USE_BYTE_HISTORY),
                                                                                        iMap(sh, 20, 1, 64, 1023),
                                                                                        pState(Start), state(Start),
                                                                                        totalOps(0), opMask(0),
                                                                                        opCategoryMask(0), context(0),
                                                                                        brkCtx(0), valid(false) {
      assert(isPowerOf2(size));
      memset(&cache, 0, sizeof(OpCache));
      memset(&op, 0, sizeof(Instruction));
      memset(&stateBh, 0, sizeof(stateBh));
    }

    void mix(Mixer &m);
};

void ExeModel::update() {
  INJECT_SHARED_buf
  INJECT_STATS_blpos
  INJECT_SHARED_c1
  pState = state;
  op.size++;
  switch( state ) {
    case Start:
    case Error: {
      // previous code may have just been a REX prefix
      bool skip = false;
      if( op.mustCheckRex ) {
        op.mustCheckRex = false;
        // valid x64 code?
        if( !isInvalidX64Op(c1) && !isValidX64Prefix(c1)) {
          op.REX = op.code;
          op.code = c1;
          op.data = prefixRex | (op.code << codeShift) | (op.data & prefixMask);
          skip = true;
        }
      }

      op.ModRM = op.SIB = op.REX = op.flags = op.bytesRead = 0;
      if( !skip ) {
        op.code = c1;
        // possible REX prefix?
        op.mustCheckRex = ((op.code & 0xF0) == 0x40) && (!(op.decoding && ((op.data & prefixMask) == 1)));

        // check prefixes
        op.prefix =
                (op.code == ES_OVERRIDE || op.code == CS_OVERRIDE || op.code == SS_OVERRIDE || op.code == DS_OVERRIDE) +
                //invalid in x64
                (op.code == FS_OVERRIDE) * 2 + (op.code == GS_OVERRIDE) * 3 + (op.code == AD_OVERRIDE) * 4 +
                (op.code == WAIT_FPU) * 5 + (op.code == LOCK) * 6 + (op.code == REP_N_STR || op.code == REP_STR) * 7;

        if( !op.decoding ) {
          totalOps += (op.data != 0) - (cache.Index && cache.Op[cache.Index & (CacheSize - 1)] != 0);
          opMask = (opMask << 1) | (state != Error);
          opCategoryMask = (opCategoryMask << CategoryShift) | (op.category);
          op.size = 0;

          cache.Op[cache.Index & (CacheSize - 1)] = op.data;
          cache.Index++;

          if( !op.prefix )
            op.data = op.code << codeShift;
          else {
            op.data = op.prefix;
            op.category = TypeOp1[op.code];
            op.decoding = true;
            brkCtx = hash(0, op.prefix, opCategoryMask & CategoryMask);
            break;
          }
        } else {
          // we only have enough bits for one prefix, so the
          // instruction will be encoded with the last one
          if( !op.prefix ) {
            op.data |= (op.code << codeShift);
            op.decoding = false;
          } else {
            op.data = op.prefix;
            op.category = TypeOp1[op.code];
            brkCtx = hash(1, op.prefix, opCategoryMask & CategoryMask);
            break;
          }
        }
      }

      if((op.o16 = (op.code == OP_OSIZE)))
        state = Pref_Op_Size;
      else if( op.code == OP_2BYTE )
        state = Pref_MultiByte_Op;
      else
        readFlags(op, state);
      brkCtx = hash(2, state, op.code, (opCategoryMask & CategoryMask),
                    opN(cache, 1) & ((ModRM_mod | ModRM_reg | ModRM_rm) << ModRMShift));
      break;
    }
    case Pref_Op_Size: {
      op.code = c1;
      applyCodeAndSetFlag(op, operandSizeOverride);
      readFlags(op, state);
      brkCtx = hash(3, state);
      break;
    }
    case Pref_MultiByte_Op: {
      op.code = c1;
      op.data |= multiByteOpcode;

      if( op.code == 0x38 )
        state = Read_OP3_38;
      else if( op.code == 0x3A )
        state = Read_OP3_3A;
      else {
        applyCodeAndSetFlag(op);
        op.flags = Table2[op.code];
        op.category = TypeOp2[op.code];
        checkFlags(op, state);
      }
      brkCtx = hash(4, state);
      break;
    }
    case ParseFlags: {
      processFlags(op, state);
      brkCtx = hash(5, state);
      break;
    }
    case ExtraFlags:
    case ReadModRM: {
      op.ModRM = c1;
      op.data |= (op.ModRM << ModRMShift) | hasModRm;
      op.SIB = 0;
      if( op.flags == fMEXTRA ) {
        op.data |= hasExtraFlags;
        int i = ((op.ModRM >> 3) & 0x07) | ((op.code & 0x01) << 3) | ((op.code & 0x08) << 1);
        op.flags = TableX[i];
        op.category = TypeOpX[i];
        if( op.flags == fERR ) {
          memset(&op, 0, sizeof(Instruction));
          state = Error;
          brkCtx = hash(6, state);
          break;
        }
        processFlags(op, state);
        brkCtx = hash(7, state);
        break;
      }

      if((op.ModRM & ModRM_rm) == 4 && op.ModRM < ModRM_mod ) {
        state = ReadSIB;
        brkCtx = hash(8, state);
        break;
      }

      processModRm(op, state);
      brkCtx = hash(9, state, op.code);
      break;
    }
    case Read_OP3_38:
    case Read_OP3_3A: {
      op.code = c1;
      applyCodeAndSetFlag(op, prefix38 << (state - Read_OP3_38));
      if( state == Read_OP3_38 ) {
        op.flags = Table3_38[op.code];
        op.category = TypeOp3_38[op.code];
      } else {
        op.flags = Table3_3A[op.code];
        op.category = TypeOp3_3A[op.code];
      }
      checkFlags(op, state);
      brkCtx = hash(10, state);
      break;
    }
    case ReadSIB: {
      op.SIB = c1;
      op.data |= ((op.SIB & SIB_scale) << SIBScaleShift);
      processModRm(op, state);
      brkCtx = hash(11, state, op.SIB & SIB_scale);
      break;
    }
    case Read8:
    case Read16:
    case Read32: {
      if( ++op.bytesRead >= ((state - Read8) << int(op.imm8 + 1))) {
        op.bytesRead = 0;
        op.imm8 = false;
        state = Start;
      }
      brkCtx = hash(12, state, op.flags & fMODE, op.bytesRead,
                    ((op.bytesRead > 1) ? (buf(op.bytesRead) << 8) : 0) | ((op.bytesRead) ? c1 : 0));
      break;
    }
    case Read8_ModRM: {
      processMode(op, state);
      brkCtx = hash(13, state);
      break;
    }
    case Read16_f: {
      if( ++op.bytesRead == 2 ) {
        op.bytesRead = 0;
        processFlags2(op, state);
      }
      brkCtx = hash(14, state);
      break;
    }
    case Read32_ModRM: {
      op.data |= RegDWordDisplacement;
      if( ++op.bytesRead == 4 ) {
        op.bytesRead = 0;
        processMode(op, state);
      }
      brkCtx = hash(15, state);
      break;
    }
  }
  valid = (totalOps > 2 * minRequired) && ((opMask & ((1 << minRequired) - 1)) == ((1 << minRequired) - 1));
  context = state + 16 * op.bytesRead + 16 * (op.REX & REX_w);
  stateBh[context] = (stateBh[context] << 8) | c1;

  bool forced = stats->blockType == EXE;
  if( valid || forced ) {
    uint32_t mask = 0, count0 = 0;
    uint32_t i = 0;
    while( i < nCM1 ) {
      if( i > 1 ) {
        mask = mask * 2 + (buf(i - 1) == 0);
        count0 += mask & 1;
      }
      int j = (i < 4) ? i + 1 : 5 + (i - 4) * (2 + (i > 6));
      cm.set(hash(i, execxt(j, buf(1) * (j > 6)), ((1 << nCM1) | mask) * (count0 * nCM1 / 2 >= i),
                  (0x08 | (blpos & 0x07)) * (i < 4)));
      i++;
    }

    cm.set(brkCtx);

    mask = prefixMask | (0xF8 << codeShift) | multiByteOpcode | prefix38 | prefix3A;
    cm.set(hash(++i, opN(cache, 1) & (mask | RegDWordDisplacement | AddressMode), state + 16 * op.bytesRead,
                op.data & mask, op.REX, op.category));

    mask = 0x04 | (0xFE << codeShift) | multiByteOpcode | prefix38 | prefix3A | ((ModRM_mod | ModRM_reg) << ModRMShift);
    cm.set(hash(++i, opN(cache, 1) & mask, opN(cache, 2) & mask, opN(cache, 3) & mask,
                context + 256 * ((op.ModRM & ModRM_mod) == ModRM_mod),
                op.data & ((mask | prefixRex) ^ (ModRM_mod << ModRMShift))));

    mask = 0x04 | codeMask;
    cm.set(hash(++i, opN(cache, 1) & mask, opN(cache, 2) & mask, opN(cache, 3) & mask, opN(cache, 4) & mask,
                (op.data & mask) | (state << 11) | (op.bytesRead << 15)));

    mask = 0x04 | (0xFC << codeShift) | multiByteOpcode | prefix38 | prefix3A;
    cm.set(hash(++i, state + 16 * op.bytesRead, op.data & mask, op.category * 8 + (opMask & 0x07), op.flags,
                ((op.SIB & SIB_base) == 5) * 4 + ((op.ModRM & ModRM_reg) == ModRM_reg) * 2 +
                ((op.ModRM & ModRM_mod) == 0)));

    mask = prefixMask | codeMask | operandSizeOverride | multiByteOpcode | prefixRex | prefix38 | prefix3A |
           hasExtraFlags | hasModRm | ((ModRM_mod | ModRM_rm) << ModRMShift);
    cm.set(hash(++i, op.data & mask, state + 16 * op.bytesRead, op.flags));

    mask = prefixMask | codeMask | operandSizeOverride | multiByteOpcode | prefix38 | prefix3A | hasExtraFlags |
           hasModRm;
    cm.set(hash(++i, opN(cache, 1) & mask, state, op.bytesRead * 2 + ((op.REX & REX_w) > 0),
                op.data & ((uint16_t) (mask ^ operandSizeOverride))));

    mask = 0x04 | (0xFE << codeShift) | multiByteOpcode | prefix38 | prefix3A | (ModRM_reg << ModRMShift);
    cm.set(hash(++i, opN(cache, 1) & mask, opN(cache, 2) & mask, state + 16 * op.bytesRead,
                op.data & (mask | prefixMask | codeMask)));

    cm.set(hash(++i, state + 16 * op.bytesRead));

    cm.set(hash(++i, (0x100 | c1) * (op.bytesRead > 0), state + 16 * pState + 256 * op.bytesRead,
                ((op.flags & fMODE) == fAM) * 16 + (op.REX & REX_w) + (op.o16) * 4 + ((op.code & 0xFE) == 0xE8) * 2 +
                ((op.data & multiByteOpcode) != 0 && (op.code & 0xF0) == 0x80)));
  }
}

void ExeModel::mix(Mixer &m) {
  INJECT_SHARED_bpos
  auto forced = stats->blockType == EXE;
  if( bpos == 0 )
    update();

  if( valid || forced ) {
    cm.setScale(forced ? 128 : 64);
    cm.mix(m);
    iMap.set(hash(brkCtx, bpos));
    iMap.setScale(forced ? 128 : 64);
    iMap.mix(m);
  } else {
    for( int i = 0; i < MIXERINPUTS; ++i )
      m.add(0);
  }
  INJECT_SHARED_c0
  uint8_t s = ((stateBh[context] >> (28 - bpos)) & 0x08) | ((stateBh[context] >> (21 - bpos)) & 0x04) |
              ((stateBh[context] >> (14 - bpos)) & 0x02) | ((stateBh[context] >> (7 - bpos)) & 0x01) |
              ((op.category == OP_GEN_BRANCH) << 4) | (((c0 & ((1 << bpos) - 1)) == 0) << 5);

  m.set(context * 4 + (s >> 4), 1024);
  m.set(state * 64 + bpos * 8 + (op.bytesRead > 0) * 4 + (s >> 4), 1024);
  m.set((brkCtx & 0x1FF) | ((s & 0x20) << 4), 1024);
  m.set(finalize64(hash(op.code, state, opN(cache, 1) & codeMask), 13), 8192);
  m.set(finalize64(hash(state, bpos, op.code, op.bytesRead), 13), 8192);
  m.set(finalize64(hash(state, (bpos << 2) | (c0 & 3), opCategoryMask & CategoryMask,
                        ((op.category == OP_GEN_BRANCH) << 2) | (((op.flags & fMODE) == fAM) << 1) |
                        (op.bytesRead > 0)), 13), 8192);
}

#endif //PAQ8PX_EXEMODEL_HPP
