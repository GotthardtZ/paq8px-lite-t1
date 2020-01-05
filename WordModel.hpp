#ifndef PAQ8PX_WORDMODEL_HPP
#define PAQ8PX_WORDMODEL_HPP

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
    const Shared *const shared;
    ModelStats const *stats;
    ContextMap2 cm;
    static constexpr int wPosBits = 16;
    static constexpr int maxWordLen = 45;
    static constexpr int maxLineMatch = 16;
    static constexpr int maxLastUpper = 63;
    static constexpr int maxLastLetter = 16;

    class Info {
        const Shared *const shared;
        ModelStats const *stats;
        ContextMap2 &cm;
        Array<uint32_t> wpos {1 << wPosBits}; // last positions of whole words/numbers
        Array<uint16_t> wchk {1 << wPosBits}; // checksums for whole words/numbers
        uint32_t c4; // last 4 processed characters
        uint8_t c, pC, ppC; // last char, previous char, char before the previous char (converted to lower case)
        bool isLetter, isLetterPc, isLetterPpC;
        uint8_t opened; // "(", "[", "{", "<", opening QUOTE, opening APOSTROPHE (or 0 when none of them)
        uint8_t wordLen0, wordLen1, exprLen0; // length of word0 and word1
        uint64_t line0, firstWord; // hash of line content, hash of first word on line
        uint64_t word0, word1, word2, word3, word4; // wordToken hashes, word0 is the partially processed ("current") word
        uint64_t expr0, expr1, expr2, expr3, expr4; // wordToken hashes for expressions
        uint64_t keyword0, gapToken0, gapToken1; // hashes
        uint16_t w, chk; // finalized hash and checksum of last partially processed word
        int firstChar; // category of first character of the current line (or -1 when in first column)
        int lineMatch; // the length of match of the current line vs the previous line
        int nl1, nl2; // current newline position, previous newline position
        uint64_t groups; // 8 last character categories
        uint64_t text0; // uninterrupted stream of letters
        uint32_t lastLetter, lastUpper, wordGap;
        uint32_t mask, expr0Chars, mask2, f4;

    public:
        Info(const Shared *const sh, ModelStats const *st, ContextMap2 &contextmap) : shared(sh), stats(st), cm(contextmap) {
          reset();
        }

        void reset() {
          //zero contents
          memset(&wpos[0], 0, (1 << wPosBits) * sizeof(uint32_t));
          memset(&wchk[0], 0, (1 << wPosBits) * sizeof(uint16_t));
          c4 = 0;
          c = pC = ppC = 0;
          isLetter = isLetterPc = isLetterPpC = false;
          opened = wordLen0 = wordLen1 = exprLen0 = 0;
          line0 = firstWord = 0;
          word0 = word1 = word2 = word3 = word4 = 0;
          expr0 = expr1 = expr2 = expr3 = expr4 = 0;
          keyword0 = gapToken0 = gapToken1 = 0;
          w = chk = 0;
          firstChar = lineMatch = nl1 = nl2 = 0;
          groups = text0 = 0;
          lastLetter = lastUpper = wordGap = 0;
          mask = expr0Chars = mask2 = f4 = 0;
          //---
          lastUpper = maxLastUpper;
          lastLetter = wordGap = maxLastLetter;
          firstChar = lineMatch = -1;
        }

        void shiftWords() {
          word4 = word3;
          word3 = word2;
          word2 = word1;
          word1 = word0;
          wordLen1 = wordLen0;
        }

        void killWords() {
          word4 = word3 = word2 = word1 = 0;
          gapToken1 = 0; //not really necessary - tiny gain
        }

        void process_char(const bool is_extended_char) {
          //note: the pdf model too uses this function, therefore only c1 may be referenced, c4, c8, etc. should not

          //shift history
          ppC = pC;
          pC = c;
          isLetterPpC = isLetterPc;
          isLetterPc = isLetter;

          INJECT_SHARED_c1
          c4 = c4 << 8 | c1;
          c = c1;
          if( c >= 'A' && c <= 'Z' ) {
            c += 'a' - 'A';
            lastUpper = 0;
          }

          isLetter = (c >= 'a' && c <= 'z') || is_extended_char;
          const bool is_number =
                  (c >= '0' && c <= '9') || (pC >= '0' && pC <= '9' && c == '.' /* decimal point (english) or thousand separator (misc) */);
          const bool is_newline = c == NEW_LINE || c == 0;
          const bool is_newline_pC = pC == NEW_LINE || pC == 0;

          lastUpper = min(lastUpper + 1, maxLastUpper);
          lastLetter = min(lastLetter + 1, maxLastLetter);

          mask2 <<= 8;

          if( isLetter || is_number ) {
            if( wordLen0 == 0 ) { // the beginning of a new word
              if( pC == NEW_LINE && lastLetter == 3 && ppC == '+' ) {
                // correct hyphenation with "+" (book1 from Calgary)
                // (undo some of the recent context changes)
                word0 = word1;
                word1 = word2;
                word2 = word3;
                word3 = word4 = 0;
                wordLen0 = wordLen1;
              } else {
                wordGap = lastLetter;
                gapToken1 = gapToken0;
                if( pC == QUOTE || (pC == APOSTROPHE && !isLetterPpC))
                  opened = pC;
              }
              gapToken0 = 0;
              mask2 = 0;
            }
            lastLetter = 0;
            word0 = combine64(word0, c);
            w = uint16_t(finalize64(word0, wPosBits));
            chk = uint16_t(checksum64(word0, wPosBits, 16));
            text0 = (text0 << 8 | c) & 0xffffffffff; // last 5 alphanumeric chars (other chars are ignored)
            wordLen0 = min(wordLen0 + 1, maxWordLen);
            //last letter types
            if( isLetter ) {
              if( c == 'e' )
                mask2 |= c; // vowel/e // separating 'a' is also ok
              else if( c == 'a' || c == 'i' || c == 'o' || c == 'u' )
                mask2 |= 'a'; // vowel
              else if( c >= 'b' && c <= 'z' ) {
                if( c == 'y' )
                  mask2 |= 'y'; // y
                else if( pC == 't' && c == 'h' ) {
                  mask2 = ((mask2 >> 8) & 0x00ffff00) | 't';
                } // consonant/th
                else
                  mask2 |= 'b'; // consonant
              } else
                mask2 |= 128; // extended_char: c>=128
            } else { // is_number
              if( c == '.' )
                mask2 |= '.'; // decimal point or thousand separator
              else
                mask2 |= c == '0' ? '0' : '1'; // number
            }
          } else { //it's not a letter/number
            gapToken0 = combine64(gapToken0, is_newline ? ' ' : c1);
            if( is_newline && pC == '+' && isLetterPpC ) {
            } //calgary/book1 hyphenation - don't shift again
            else if( c == '?' || pC == '!' || pC == '.' )
              killWords(); //end of sentence (probably)
            else if( c == pC ) {
            } //don't shift when anything repeats
            else if((c == SPACE || is_newline) && (pC == SPACE || is_newline_pC)) {
            } //ignore repeating whitespace
            else
              shiftWords();
            if( wordLen0 != 0 ) { //beginning of a new non-word token
              INJECT_SHARED_pos
              wpos[w] = pos;
              wchk[w] = chk;
              w = 0;
              chk = 0;

              if( c == ':' || c == '=' )
                keyword0 = word0; // enwik, world95.txt, html/xml
              if( firstWord == 0 )
                firstWord = word0;

              word0 = 0;
              wordLen0 = 0;
              mask2 = 0;
            }

            if( c1 == '.' || c1 == '!' || c1 == '?' )
              mask2 |= '!';
            else if( c1 == ',' || c1 == ';' || c1 == ':' )
              mask2 |= ',';
            else if( c1 == '(' || c1 == '{' || c1 == '[' || c1 == '<' ) {
              mask2 |= '(';
              opened = c1;
            } else if( c1 == ')' || c1 == '}' || c1 == ']' || c1 == '>' ) {
              mask2 |= ')';
              opened = 0;
            } else if( c1 == QUOTE || c1 == APOSTROPHE ) {
              mask2 |= c1;
              opened = 0;
            } else if( c1 == '-' )
              mask2 |= c1;
            else
              mask2 |= c1; //0, SPACE, NEW_LINE, /\+=%$ etc.
          }

          //const uint8_t chargrp = stats->Text.chargrp;
          uint8_t g = c1;
          if( g >= 128 ) {
            //utf8 code points (weak context)
            if((g & 0xf8) == 0xf0 )
              g = 1;
            else if((g & 0xf0) == 0xe0 )
              g = 2;
            else if((g & 0xe0) == 0xc0 )
              g = 3;
            else if((g & 0xc0) == 0x80 )
              g = 4;
              //the rest of the values
            else if( g == 0xff )
              g = 5;
            else
              g = c1 & 0xf0;
          } else if( g >= '0' && g <= '9' )
            g = '0';
          else if( g >= 'a' && g <= 'z' )
            g = 'a';
          else if( g >= 'A' && g <= 'Z' )
            g = 'A';
          else if( g < 32 && !is_newline )
            g = 6;
          groups = groups << 8 | g;

          // Expressions (pure words separated by single spaces)
          //
          // Remarks: (1) formatted text files may contain SPACE+NEW_LINE (dickens) or NEW_LINE+SPACE (world95.txt)
          // or just a NEW_LINE instead of a SPACE. (2) quotes and apostrophes are ignored during processing
          if( isLetter ) {
            expr0Chars = expr0Chars << 8 | c; // last 4 consecutive letters
            expr0 = combine64(expr0, c);
            exprLen0 = min(exprLen0 + 1, maxWordLen);
          } else {
            expr0Chars = 0;
            exprLen0 = 0;
            if((c == SPACE || is_newline) && (isLetterPc || pC == APOSTROPHE || pC == QUOTE)) {
              expr4 = expr3;
              expr3 = expr2;
              expr2 = expr1;
              expr1 = expr0;
              expr0 = 0;
            } else if( c == APOSTROPHE || c == QUOTE || (is_newline && pC == SPACE) || (c == SPACE && is_newline_pC)) {
              //ignore
            } else {
              expr4 = expr3 = expr2 = expr1 = expr0 = 0;
            }
          }
        }

        void line_model_predict() {
          uint64_t i = 1024;

          INJECT_SHARED_c1
          INJECT_SHARED_pos
          const bool is_newline = c == NEW_LINE || c == 0;
          if( is_newline ) { // a new line has just started (or: zero in asciiz or in binary data)
            nl2 = nl1;
            nl1 = pos;
            firstChar = -1;
            firstWord = 0;
            line0 = 0;
          }
          line0 = combine64(line0, c1);
          cm.set(hash(++i, line0));

          int col = pos - nl1;
          if( col == 1 )
            firstChar = groups & 0xff;
          INJECT_SHARED_buf
          const uint8_t c_above = buf[nl2 + col];
          const uint8_t pC_above = buf[nl2 + col - 1];

          const bool is_new_line_start = col == 0 && nl2 > 0;
          const bool is_prev_char_match_above = c1 == pC_above && col != 0 && nl2 != 0;
          const uint32_t above_ctx = c_above << 1 | uint32_t(is_prev_char_match_above);
          if( is_new_line_start )
            lineMatch = 0; //first char not yet known = nothing to match
          else if( lineMatch >= 0 && is_prev_char_match_above )
            lineMatch = min(lineMatch + 1, maxLineMatch); //match continues
          else
            lineMatch = -1; //match does not continue

          // context: matches with the previous line
          if( lineMatch >= 0 )
            cm.set(hash(++i, c_above, lineMatch));
          else {
            cm.skip();
            i++;
          }
          cm.set(hash(++i, above_ctx, c1));
          cm.set(hash(++i, col << 9 | above_ctx, c1));
          const int line_length = nl1 - nl2;
          cm.set(hash(++i, nl1 - nl2, col, above_ctx, groups & 0xff)); // english_mc
          cm.set(hash(++i, nl1 - nl2, col, above_ctx, c1)); // english_mc

          // modeling line content per column (and NEW_LINE is some extent)
          cm.set(hash(++i, col, c1 == SPACE)); // after space vs after other char in this column // world95.txt
          cm.set(hash(++i, col, c1));
          cm.set(hash(++i, col, mask & 0x1ff));
          cm.set(hash(++i, col, line_length)); // the length of the previous line may foretell the content of columns

          cm.set(hash(++i, col, firstChar, ((int) lastUpper < col) << 8 | (groups & 0xff))); // book1 book2 news

          // content of lines, paragraphs
          cm.set(hash(++i, nl1)); //chars occurring in this paragraph (order 0)
          cm.set(hash(++i, nl1, c)); //chars occurring in this paragraph (order 1)
          cm.set(hash(++i, firstChar)); //chars occurring in a paragraph that began with firstChar (order 0)
          cm.set(hash(++i, firstChar, c)); //chars occurring in a paragraph that began with firstChar (order 1)
          cm.set(hash(++i, firstWord)); //chars occurring in a paragraph that began with firstWord (order 0)
          cm.set(hash(++i, firstWord, c)); //chars occurring in a paragraph that began with firstWord (order 1)
          assert(i == 1024 + nCM1);
        }

        static void lineModelSkip(ContextMap2 &cm) {
          for( int i = 0; i < nCM1; i++ )
            cm.skip();
        }

        void predict(const uint8_t pdfTextParserState) {
          INJECT_SHARED_pos
          const uint32_t lastPos = wchk[w] != chk ? 0 : wpos[w]; //last occurrence (position) of a whole word or number
          const uint32_t dist = lastPos == 0 ? 0 : min(llog(pos - lastPos + 120) >> 4, 20);
          const bool word0MayEndNow = lastPos != 0;
          const uint8_t mayBeCaps = uint8_t(c4 >> 8) >= 'A' && uint8_t(c4 >> 8) <= 'Z' && uint8_t(c4) >= 'A' && uint8_t(c4) <= 'Z';
          const bool isTextBlock = stats->blockType == TEXT || stats->blockType == TEXT_EOL;

          uint64_t i = 2048 * isTextBlock;

          cm.set(hash(++i, text0)); //strong

          // expressions in normal text
          if( isTextBlock ) {
            cm.set(hash(++i, expr0, expr1, expr2, expr3, expr4));
            cm.set(hash(++i, expr0, expr1, expr2));
          } else {
            cm.skip();
            i++;
            cm.skip();
            i++;
          }

          // sections introduced by keywords (enwik world95.txt html/xml)
          cm.set(hash(++i, gapToken0,
                      keyword0)); // chars occurring in a section introduced by "keyword:" or "keyword=" (order 0 and variable order for the gap)
          cm.set(hash(++i, word0, c,
                      keyword0)); // tokens occurring in a section introduced by "keyword:" or "keyword=" (order 1 and variable order for a word)

          cm.set(hash(++i, word0, dist));
          cm.set(hash(++i, word1, gapToken0, dist));

          cm.set(hash(++i, pos >> 10, word0)); //word/number tokens and characters occurring in this 1K block

          // Simple word morphology (order 1-4)

          const int wme_mbc = word0MayEndNow << 1 | mayBeCaps;
          const int wl = min(wordLen0, 6);
          const int wl_wme_mbc = wl << 2 | wme_mbc;
          cm.set(hash(++i, wl_wme_mbc, mask2 /*last 1-4 char types*/));

          if( exprLen0 >= 1 ) {
            const int wl_wme_mbc = min(exprLen0, 1 + 3) << 2 | wme_mbc;
            cm.set(hash(++i, wl_wme_mbc, c));
          } else {
            cm.skip();
            i++;
          }

          if( exprLen0 >= 2 ) {
            const int wl_wme_mbc = min(exprLen0, 2 + 3) << 2 | wme_mbc;
            cm.set(hash(++i, wl_wme_mbc, expr0Chars & 0xffff));
          } else {
            cm.skip();
            i++;
          }

          if( exprLen0 >= 3 ) {
            const int wl_wme_mbc = min(exprLen0, 3 + 3) << 2 | wme_mbc;
            cm.set(hash(++i, wl_wme_mbc, expr0Chars & 0xffffff));
          } else {
            cm.skip();
            i++;
          }

          if( exprLen0 >= 4 ) {
            const int wl_wme_mbc = min(exprLen0, 4 + 3) << 2 | wme_mbc;
            cm.set(hash(++i, wl_wme_mbc, expr0Chars));
          } else {
            cm.skip();
            i++;
          }

          cm.set(hash(++i, word0, pdfTextParserState));

          cm.set(hash(++i, word0, gapToken0)); // stronger
          cm.set(hash(++i, c, word0, gapToken1)); // stronger
          cm.set(hash(++i, c, gapToken0, word1)); // stronger //french texts need that "c"
          cm.set(hash(++i, word0, word1)); // stronger
          cm.set(hash(++i, word0, word1, word2)); // weaker
          cm.set(hash(++i, gapToken0, word1, gapToken1, word2)); // stronger
          cm.set(hash(++i, word0, word1, gapToken1, word2)); // weaker
          cm.set(hash(++i, word0, word1, gapToken1)); // weaker

          const uint8_t c1 = c4 & 0xff;
          if( isTextBlock ) {
            cm.set(hash(++i, word0, c1, word2));
            cm.set(hash(++i, word0, c1, word3));
            cm.set(hash(++i, word0, c1, word4));
            cm.set(hash(++i, word0, c1, word1, word4));
            cm.set(hash(++i, word0, c1, word1, word3));
            cm.set(hash(++i, word0, c1, word2, word3));
          } else {
            cm.skip();
            i++;
            cm.skip();
            i++;
            cm.skip();
            i++;
            cm.skip();
            i++;
            cm.skip();
            i++;
            cm.skip();
            i++;
          }

          const uint8_t g = groups & 0xff;
          cm.set(hash(++i, opened, wl_wme_mbc, g, pdfTextParserState));
          cm.set(hash(++i, opened, c, dist != 0, pdfTextParserState));
          cm.set(hash(++i, opened, word0)); // book1, book2, dickens, enwik


          cm.set(hash(++i, groups));
          cm.set(hash(++i, groups, c));
          cm.set(hash(++i, groups, c4 & 0x0000ffff));

          f4 = (f4 << 4) | (c1 == ' ' ? 0 : c1 >> 4);
          cm.set(hash(++i, f4 & 0xfff));
          cm.set(hash(++i, f4));

          int fl = 0;
          if( c1 != 0 ) {
            if( isalpha(c1))
              fl = 1;
            else if( ispunct(c1))
              fl = 2;
            else if( isspace(c1))
              fl = 3;
            else if((c1) == 0xff )
              fl = 4;
            else if((c1) < 16 )
              fl = 5;
            else if((c1) < 64 )
              fl = 6;
            else
              fl = 7;
          }
          mask = (mask << 3) | fl;

          cm.set(hash(++i, mask));
          cm.set(hash(++i, mask, c1));
          cm.set(hash(++i, mask, c4 & 0x00ffff00));
          cm.set(hash(++i, mask & 0x1ff, f4 & 0x00fff0)); // for some binary files

          if( isTextBlock ) {
            cm.set(hash(++i, word0, c1, llog(wordGap), mask & 0x1FF, ((wordLen1 > 3) << 2) | ((lastUpper < lastLetter + wordLen1) << 1) |
                                                                     (lastUpper < wordLen0 + wordLen1 + wordGap))); //weak
          } else {
            cm.skip();
            i++;
          }
          assert(int(i) == 2048 * is_textblock + nCM2);
        }
    };

    Info info_normal; //used for general content
    Info info_pdf; //used only in case of pdf text - in place of info_normal
    uint8_t pdf_text_parser_state; // 0..7
public:
    WordModel(const Shared *const sh, ModelStats const *st, const uint64_t size) : shared(sh), stats(st), cm(sh, size, nCM, 74,
                                                                                                             CM_USE_RUN_STATS |
                                                                                                             CM_USE_BYTE_HISTORY),
                                                                                   info_normal(sh, st, cm), info_pdf(sh, st, cm),
                                                                                   pdf_text_parser_state(0) {}

    void reset() {
      info_normal.reset();
      info_pdf.reset();
    }

    void mix(Mixer &m) {
      INJECT_SHARED_bpos
      if( bpos == 0 ) {
        //extract text from pdf
        INJECT_SHARED_c4
        const uint8_t c1 = c4;
        if( c4 == 0x0a42540a /* "\nBT\n" */)
          pdf_text_parser_state = 1; // Begin Text
        else if( c4 == 0x0a45540a /* "\nET\n" */) {
          pdf_text_parser_state = 0;
        } // end Text
        bool do_pdf_process = true;
        if( pdf_text_parser_state != 0 ) {
          const uint8_t pC = c4 >> 8;
          if( pC != '\\' ) {
            if( c1 == '[' ) {
              pdf_text_parser_state |= 2;
            } //array begins
            else if( c1 == ']' ) {
              pdf_text_parser_state &= (255 - 2);
            } else if( c1 == '(' ) {
              pdf_text_parser_state |= 4;
              do_pdf_process = false;
            } //signal: start text extraction from the next char
            else if( c1 == ')' ) {
              pdf_text_parser_state &= (255 - 4);
            } //signal: start pdf gap processing
          }
        }

        const bool isPdfText = (pdf_text_parser_state & 4) != 0;
        if( isPdfText ) {
          const bool isExtendedChar = 0;
          //predict the chars after "(", but the "(" must not be processed
          if( do_pdf_process ) {
            //printf("%c",c1); //debug: print the extracted pdf text
            info_pdf.process_char(isExtendedChar);
          }
          info_pdf.predict(pdf_text_parser_state);
          info_pdf.lineModelSkip(cm);
        } else {
          const bool isTextBlock = stats->blockType == TEXT || stats->blockType == TEXT_EOL;
          const bool isExtendedChar = isTextBlock && c1 >= 128;
          info_normal.process_char(isExtendedChar);
          info_normal.predict(pdf_text_parser_state);
          info_normal.line_model_predict();
        }
      }
      cm.mix(m);
    }
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
