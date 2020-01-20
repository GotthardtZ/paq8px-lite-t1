#include "WordModel.hpp"

WordModel::WordModel(ModelStats const *st, const uint64_t size) : stats(st), cm(size, nCM, 74, CM_USE_RUN_STATS | CM_USE_BYTE_HISTORY),
        infoNormal(st, cm), infoPdf(st, cm), pdfTextParserState(0) {}

void WordModel::reset() {
  infoNormal.reset();
  infoPdf.reset();
}

void WordModel::mix(Mixer &m) {
  if( shared->bitPosition == 0 ) {
    //extract text from pdf
    const uint8_t c1 = shared->c4;
    if( shared->c4 == 0x0a42540aU /* "\nBT\n" */) {
      pdfTextParserState = 1; // Begin Text
    } else if( shared->c4 == 0x0a45540aU /* "\nET\n" */) {
      pdfTextParserState = 0;
    } // end Text
    bool doPdfProcess = true;
    if( pdfTextParserState != 0 ) {
      const uint8_t pC = shared->c4 >> 8U;
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
      Info::lineModelSkip(cm);
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
