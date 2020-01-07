#ifndef PAQ8PX_NORMALMODEL_HPP
#define PAQ8PX_NORMALMODEL_HPP

/**
 * Model for order 0-14 contexts
 * Contexts are hashes of previous 0..14 bytes.
 * Order 0..6, 8 and 14 are used for prediction.
 * Note: order 7+ contexts are modeled by matchModel as well.
 */
class NormalModel {
private:
    static constexpr int nCM = 9;
    static constexpr int nSM = 3;

public:
    static constexpr int MIXERINPUTS =
            nCM * (ContextMap2::MIXERINPUTS + ContextMap2::MIXERINPUTS_RUN_STATS + ContextMap2::MIXERINPUTS_BYTE_HISTORY) + nSM; //66
    static constexpr int MIXERCONTEXTS = 64 + 8 + 1024 + 256 + 256 + 256 + 256 + 1536; //3656
    static constexpr int MIXERCONTEXTSETS = 7;

private:
    const Shared *const shared;
    ModelStats *stats;
    ContextMap2 cm;
    StateMap smOrder0Slow;
    StateMap smOrder1Slow;
    StateMap smOrder1Fast;
    uint64_t cxt[15] {}; // context hashes
public:
    NormalModel(const Shared *const sh, ModelStats *st, const uint64_t cmsize) : shared(sh), stats(st),
            cm(sh, cmsize, nCM, 64, CM_USE_RUN_STATS | CM_USE_BYTE_HISTORY), smOrder0Slow(sh, 1, 255, 1023, StateMap::Generic),
            smOrder1Slow(sh, 1, 255 * 256, 1023, StateMap::Generic),
            smOrder1Fast(sh, 1, 255 * 256, 64, StateMap::Generic) // 64->16 is also ok
    {
      assert(isPowerOf2(cmsize));
    }

    void reset() {
      memset(&cxt[0], 0, sizeof(cxt));
    }

    /**
     * update order 1..14 context hashes.
     * Note: order 0 context does not need an update so its hash never changes.
     */
    void updateHashes() {
      INJECT_SHARED_c1
      for( int i = 14; i > 0; --i )
        cxt[i] = combine64(cxt[i - 1], c1 + (i << 10));
    }

    void mix(Mixer &m) {
      INJECT_SHARED_bpos
      if( bpos == 0 ) {
        updateHashes();
        for( int i = 1; i <= 7; ++i )
          cm.set(cxt[i]);
        cm.set(cxt[9]);
        cm.set(cxt[14]);
      }
      cm.mix(m);

      INJECT_SHARED_c0
      m.add((stretch(smOrder0Slow.p1(c0 - 1))) >> 2); //order 0
      INJECT_SHARED_c1
      m.add((stretch(smOrder1Fast.p1((c0 - 1) << 8 | c1))) >> 2); //order 1
      m.add((stretch(smOrder1Slow.p1((c0 - 1) << 8 | c1))) >> 2); //order 1

      const int order = max(0, cm.order - (nCM - 7)); //0-7
      assert(0 <= order && order <= 7);
      m.set(order << 3U | bpos, 64);
      stats->order = order;
    }

    /**
     * setting more mixer contexts after skipping the special blockTypes
     * @param m
     */
    void mixPost(Mixer &m) {
      INJECT_SHARED_c4
      uint32_t c2 = (c4 >> 8U) & 0xff, c3 = (c4 >> 16U) & 0xff, c;

      INJECT_SHARED_c0
      INJECT_SHARED_c1
      INJECT_SHARED_bpos
      m.set(8 + (c1 | (bpos > 5) << 8 | (((c0 & ((1 << bpos) - 1)) == 0) || (c0 == ((2 << bpos) - 1))) << 9), 8 + 1024);
      m.set(c0, 256);
      m.set(stats->order | ((c4 >> 6) & 3) << 3 | (bpos == 0) << 5 | (c1 == c2) << 6 | (stats->blockType == EXE) << 7, 256);
      m.set(c2, 256);
      m.set(c3, 256);
      if( bpos != 0 ) {
        c = c0 << (8 - bpos);
        if( bpos == 1 )
          c |= c3 >> 1;
        c = min(bpos, 5) << 8 | c1 >> 5 | (c2 >> 5) << 3 | (c & 192);
      } else
        c = c3 >> 7 | (c4 >> 31) << 1 | (c2 >> 6) << 2 | (c1 & 240);
      m.set(c, 1536);
    }
};

#endif //PAQ8PX_NORMALMODEL_HPP
