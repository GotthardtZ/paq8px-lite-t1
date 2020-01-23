#ifndef PAQ8PX_WORDMODEL_HPP
#define PAQ8PX_WORDMODEL_HPP

#include "../ContextMap2.hpp"
#include "../ModelStats.hpp"
#include "../RingBuffer.hpp"
#include "../Shared.hpp"
#include "Info.hpp"
#include <cctype>

#ifdef USE_TEXTMODEL

/**
 * Model words, expressions, numbers, paragraphs/lines, etc.
 * simple processing of pdf text
 * simple modeling of some binary content
 */
class WordModel {
private:
    static constexpr int nCM1 = 17; // pdf / non_pdf contexts
    static constexpr int nCM2 = 41; // common contexts
    static constexpr int nCM = nCM1 + nCM2; // 58
public:
    static constexpr int MIXERINPUTS =
            nCM * (ContextMap2::MIXERINPUTS + ContextMap2::MIXERINPUTS_RUN_STATS + ContextMap2::MIXERINPUTS_BYTE_HISTORY); // 406
    static constexpr int MIXERCONTEXTS = 0;
    static constexpr int MIXERCONTEXTSETS = 0;

private:
    Shared *shared = Shared::getInstance();
    ModelStats const *stats;
    ContextMap2 cm;
    Info infoNormal; //used for general content
    Info infoPdf; //used only in case of pdf text - in place of infoNormal
    uint8_t pdfTextParserState; // 0..7
public:
    WordModel(ModelStats const *st, uint64_t size);
    void reset();
    void mix(Mixer &m);
};

#else
class WordModel {
public:
    static constexpr int MIXERINPUTS = 0;
    static constexpr int MIXERCONTEXTS = 0;
    static constexpr int MIXERCONTEXTSETS = 0;
};
#endif //USE_TEXTMODEL

#endif //PAQ8PX_WORDMODEL_HPP
