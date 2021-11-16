#ifndef PAQ8PX_ENCODER_HPP
#define PAQ8PX_ENCODER_HPP

#include "Predictor.hpp"
#include "ArithmeticEncoder.hpp"
#include "Shared.hpp"

typedef enum {
    COMPRESS, DECOMPRESS
} Mode;

/**
 * An Encoder does arithmetic encoding.
 * If shared->level is 0, then data is stored without arithmetic coding.
 */
class Encoder {
private:
    ArithmeticEncoder ari;
    const Mode mode; /**< Compress or decompress? */
    File* archive; /**< Compressed data file */
    File *alt; /**< decompressByte() source in COMPRESS mode */
    float p1 {}, p2 {}; /**< percentages for progress indicator: 0.0 .. 1.0 */
    Shared * const shared;

    void updateModels(Predictor* predictor, uint32_t p, int y);

public:

    Predictor predictorMain;
    //Predictor predictor;

    /**
     * Encoder(COMPRESS, f) creates encoder for compression to archive @ref f, which
     * must be open past any header for writing in binary mode.
     * Encoder(DECOMPRESS, f) creates encoder for decompression from archive @ref f,
     * which must be open past any header for reading in binary mode.
     * @param m the mode to operate in
     * @param f the file to read from or write to
     */
    Encoder(Shared* const sh, Mode m, File *f);
    [[nodiscard]] auto getMode() const -> Mode;

    /**
     * Returns current length of archive
     * @return length of archive so far
     */
    [[nodiscard]] auto size() const -> uint64_t;

    /**
     * Should be called exactly once after compression is done and
     * before closing @ref f. It does nothing in DECOMPRESS mode.
     */
    void flush();

    /**
     * Sets alternate source to @ref f for decompressByte() in COMPRESS mode (for testing transforms).
     * @param f
     */
    void setFile(File *f);

    /**
     * compressByte(c) in COMPRESS mode compresses one byte.
     * @param c the byte to be compressed
     */
    void compressByte(Predictor *predictor, uint8_t c);

    /**
     * decompressByte() in DECOMPRESS mode decompresses and returns one byte.
     * @return the decompressed byte
     */
    uint8_t decompressByte(Predictor *predictor);

    void setStatusRange(float perc1, float perc2);
    void printStatus(uint64_t n, uint64_t size) const;
    void printStatus() const;
};


#endif //PAQ8PX_ENCODER_HPP
