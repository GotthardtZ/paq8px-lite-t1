#include "WordModel.hpp"

WordModel::WordModel(ModelStats const *st, const uint64_t size) : stats(st), cm(size, nCM, 74, CM_USE_RUN_STATS | CM_USE_BYTE_HISTORY),
        infoNormal(st, cm), infoPdf(st, cm), pdfTextParserState(0) {}

void WordModel::reset() {
  infoNormal.reset();
  infoPdf.reset();
}

void WordModel::mix(Mixer &m) {
  INJECT_SHARED_bpos
  if( bpos == 0 ) {
    //extract text from pdf
    INJECT_SHARED_c4
    const uint8_t c1 = c4;
    if( c4 == 0x0a42540a /* "\nBT\n" */)
      pdfTextParserState = 1; // Begin Text
    else if( c4 == 0x0a45540a /* "\nET\n" */) {
      pdfTextParserState = 0;
    } // end Text
    bool doPdfProcess = true;
    if( pdfTextParserState != 0 ) {
      const uint8_t pC = c4 >> 8U;
      if( pC != '\\' ) {
        if( c1 == '[' ) {
          pdfTextParserState |= 2U;
        } //array begins
        else if( c1 == ']' ) {
          pdfTextParserState &= (255 - 2);
        } else if( c1 == '(' ) {
          pdfTextParserState |= 4U;
          doPdfProcess = false;
        } //signal: start text extraction from the next char
        else if( c1 == ')' ) {
          pdfTextParserState &= (255 - 4);
        } //signal: start pdf gap processing
      }
    }

    const bool isPdfText = (pdfTextParserState & 4U) != 0;
    if( isPdfText ) {
      const bool isExtendedChar = false;
      //predict the chars after "(", but the "(" must not be processed
      if( doPdfProcess ) {
        //printf("%c",c1); //debug: print the extracted pdf text
        infoPdf.processChar(isExtendedChar);
      }
      infoPdf.predict(pdfTextParserState);
      WordModel::Info::lineModelSkip(cm);
    } else {
      const bool isTextBlock = stats->blockType == TEXT || stats->blockType == TEXT_EOL;
      const bool isExtendedChar = isTextBlock && c1 >= 128;
      infoNormal.processChar(isExtendedChar);
      infoNormal.predict(pdfTextParserState);
      infoNormal.lineModelPredict();
    }
  }
  cm.mix(m);
}
