#ifndef PAQ8PX_CHARGROUPMODEL_HPP
#define PAQ8PX_CHARGROUPMODEL_HPP

/**
 * modeling ascii character sequences
 * Although there have been similar approaches in wordModel (f4, mask), and different "masks" in TextModel,
 * so there is some overlap, but there is novelty in charGroupModel.
 * Words (letter-sequences) and numbers (digit-sequences) are collapsed into one character.
 * So
 *     "A (fairly simple) sentence." becomes "A (a a) a.",
 * and
 *     "A picture is worth a 1000 words!" becomes "A a a a a 1 a!"
 * And the model uses these simplified "strings" as contexts for a ContextMap.
 * a "mask" is a simplification of the last some bytes of the input sequence emphasizing some feature that looks promising to use it as a context.
 */
class CharGroupModel {
private:
    static constexpr int nCM = 7;
    const Shared *const shared;
    ContextMap2 cm;
    uint32_t gAscii3 = 0, gAscii2 = 0, gAscii1 = 0; // group identifiers of the last 12 (4+4+4) characters; the most recent is 'gAscii1'
public:
    static constexpr int MIXERINPUTS =
            nCM * (ContextMap2::MIXERINPUTS + ContextMap2::MIXERINPUTS_RUN_STATS + ContextMap2::MIXERINPUTS_BYTE_HISTORY); // 35
    static constexpr int MIXERCONTEXTS = 0;
    static constexpr int MIXERCONTEXTSETS = 0;

    CharGroupModel(const Shared *const sh, const uint64_t size) : shared(sh),
            cm(sh, size, nCM, 64, CM_USE_RUN_STATS | CM_USE_BYTE_HISTORY) {}

    void mix(Mixer &m) {
      INJECT_SHARED_bpos
      if( bpos == 0 ) {
        INJECT_SHARED_c1
        uint32_t g = c1; // group identifier
        if( '0' <= g && g <= '9' )
          g = '0'; //all digits are in one group
        else if( 'A' <= g && g <= 'Z' )
          g = 'A'; //all uppercase letters are in one group
        else if( 'a' <= g && g <= 'z' )
          g = 'a'; //all lowercase letters are in one group
        else if( g >= 128 )
          g = 128;

        const bool toBeCollapsed = (g == '0' || g == 'A' || g == 'a') && (g == (gAscii1 & 0xffu));
        if( !toBeCollapsed ) {
          gAscii3 <<= 8U;
          gAscii3 |= gAscii2 >> (32U - 8U);
          gAscii2 <<= 8U;
          gAscii2 |= gAscii1 >> (32U - 8U);
          gAscii1 <<= 8U;
          gAscii1 |= g;
        }

        uint64_t i = toBeCollapsed * 8;
        cm.set(hash((++i), gAscii3, gAscii2, gAscii1)); // last 12 groups
        cm.set(hash((++i), gAscii2, gAscii1)); // last 8 groups
        cm.set(hash((++i), gAscii2 & 0xffffu, gAscii1)); // last 6 groups
        cm.set(hash((++i), gAscii1)); // last 4 groups
        cm.set(hash((++i), gAscii1 & 0xffffu)); // last 2 groups
        INJECT_SHARED_c4
        cm.set(hash((++i), gAscii2 & 0xffffffu, gAscii1, c4 & 0x0000ffffu)); // last 7 groups + last 2 chars
        cm.set(hash((++i), gAscii2 & 0xffu, gAscii1, c4 & 0x00ffffffu)); // last 5 groups + last 3 chars
      }
      cm.mix(m);
    }
};

#endif //PAQ8PX_CHARGROUPMODEL_HPP
