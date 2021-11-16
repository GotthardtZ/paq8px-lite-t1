#include "ContextMap2.hpp"

ContextMap2::ContextMap2(const Shared* const sh, const uint64_t size) :
  shared(sh),
  hashTable(size),
  runMap1{},
  stateMap1{},
  mask(uint32_t(hashTable.size() - 1)), hashBits(ilog2(mask + 1)) {
  assert(size >= 64 && isPowerOf2(size));
}

void ContextMap2::updatePendingContextsInSlot(HashElementForContextMap* const p, uint32_t c) {
  // in case of a collision updating (mixing) is slightly better (but slightly slower) then resetting, so we update
  StateTable::update(&p->bitState, (c >> 2) & 1);
  StateTable::update(&p->bitState0 + ((c >> 2) & 1), (c >> 1) & 1);
  StateTable::update(&p->bitStates.bitState00 + ((c >> 1) & 3), c & 1);
}

void ContextMap2::updatePendingContexts(uint32_t ctx, uint16_t checksum, uint32_t c) {
  // update pending bit histories for bits 2, 3, 4
  HashElementForContextMap* const p1A = hashTable[(ctx + (c >> 6)) & mask].find(checksum);
  updatePendingContextsInSlot(p1A, c >> 3);
  // update pending bit histories for bits 5, 6, 7
  HashElementForContextMap* const p1B = hashTable[(ctx + (c >> 3)) & mask].find(checksum);
  updatePendingContextsInSlot(p1B, c);
}

void ContextMap2::set(const int index, const uint64_t contexthash) { //set per index
  assert(index >= 0 && index < C);
  ContextInfo *contextInfo = &contextInfoList[index];
  const uint32_t ctx = contextInfo->tableIndex = finalize64(contexthash, hashBits);
  const uint16_t chk = contextInfo->tableChecksum = checksum16(contexthash, hashBits);
  HashElementForContextMap* const slot0 =  hashTable[ctx].find(chk);
  contextInfo->slot0 = slot0;
  contextInfo->slot012 = slot0;

  uint8_t ctxflags = 0;
  if (slot0->bitState <= 6) { // while constructing statistics for the first 3 bytes (states: 0; 1-2; 3-6) defer updating bit statistics in slot1 and slot2
    ctxflags |= FLAG_DEFERRED_UPDATE;
  }
  else if (slot0->bitState <= 14) { // the first 3 bytes in this context are now known, it's time to update pending bit histories
    if (slot0->byteStats.runcount == 2) {
      updatePendingContexts(ctx, chk, slot0->byteStats.byte2 + 256);
      updatePendingContexts(ctx, chk, slot0->byteStats.byte1 + 256);
      updatePendingContexts(ctx, chk, slot0->byteStats.byte1 + 256);
    }
    else {
      updatePendingContexts(ctx, chk, slot0->byteStats.byte3 + 256);
      updatePendingContexts(ctx, chk, slot0->byteStats.byte2 + 256);
      updatePendingContexts(ctx, chk, slot0->byteStats.byte1 + 256);
    }
  }
  
  contextInfo->flags = ctxflags;
}


ALWAYS_INLINE
size_t ContextMap2::getStateByteLocation(const uint32_t bpos, const uint32_t c0) {
  uint32_t pis = 0; //state byte position in slot
  if (false) {
    // this version is for readability
    switch (bpos) {
    case 0: //slot0
      pis = 0;
      break;
    case 1: //slot0
      pis = 1 + (c0 & 1);
      break;
    case 2: //slot1
      pis = 0;
      break;
    case 3: //slot1
      pis = 1 + (c0 & 1);
      break;
    case 4: //slot1
      pis = 3 + (c0 & 3);
      break;
    case 5: //slot2
      break;
    case 6: //slot2
      pis = 1 + (c0 & 1);
      break;
    case 7: //slot2
      pis = 3 + (c0 & 3);
      break;
    }
  }
  else {
    // this is a speed optimized (branchless) version of the above
    const uint32_t smask = (UINT32_C(0x31031010) >> (bpos << 2)) & 0x0F;
    pis = smask + (c0 & smask);
  }

  return pis;
}


void ContextMap2::update() {

  INJECT_SHARED_y
  INJECT_SHARED_bpos
  INJECT_SHARED_c1
  INJECT_SHARED_c0
  for( uint32_t i = 0; i < C; i++ ) {
    ContextInfo* contextInfo = &contextInfoList[i];
    const uint8_t flags = contextInfo->flags;

    uint8_t* pState = &contextInfo->slot012->bitState + getStateByteLocation((bpos - 1) & 7, (bpos == 0 ? c1 + 256u : c0) >> 1);
    StateTable::update(pState, y);
      
    assume(bpos >= 0 && bpos <= 7);
    if (bpos == 0) {
      // update byte history and run statistics
      if (contextInfo->slot0->bitState < 3) {
        contextInfo->slot0->byteStats.byte3 = contextInfo->slot0->byteStats.byte2 = contextInfo->slot0->byteStats.byte1 = c1;
        contextInfo->slot0->byteStats.runcount = 1;
      }
      else {
        const bool isMatch = contextInfo->slot0->byteStats.byte1 == c1;
        if (isMatch) {
          uint8_t runCount = contextInfo->slot0->byteStats.runcount;
          if (runCount < 255) {
            contextInfo->slot0->byteStats.runcount = runCount + 1;
          }
        }
        else {
          // shift byte candidates
          contextInfo->slot0->byteStats.runcount = 1;
          contextInfo->slot0->byteStats.byte3 = contextInfo->slot0->byteStats.byte2;
          contextInfo->slot0->byteStats.byte2 = contextInfo->slot0->byteStats.byte1;
          contextInfo->slot0->byteStats.byte1 = c1; //last byte seen
        }
      }
    }
    else if( bpos==2 || bpos==5 ) {
      if (flags & FLAG_DEFERRED_UPDATE) { //when in deferred mode...
        // ...reconstruct bit states in temporary location from last seen bytes 

        memset(&contextInfo->bitStateTmp, 0, 7);
        contextInfo->slot012 = &contextInfo->bitStateTmp;
          
        const uint8_t bit0state = contextInfo->slot0->bitState;
        if (bit0state >= 3) { // at least 1 byte was seen
          const uint8_t byte2 = bit0state >= 7 ? contextInfo->slot0->byteStats.byte2 : contextInfo->slot0->byteStats.byte1;
          const uint8_t mask = ((1 << bpos) - 1);
          const int shift = 8 - bpos;
          if (((c0 ^ (byte2 >> shift)) & mask) == 0) { // last 2/5 bits must match otherwise it's not the current slot location
            updatePendingContextsInSlot(&contextInfo->bitStateTmp, byte2 >> (shift - 3)); // simulate the current states at the temporary location
          }
          if (bit0state >= 7) { // at least 2 bytes were seen
            const uint8_t byte1 = contextInfo->slot0->byteStats.byte1;
            if (((c0 ^ (byte1 >> shift)) & mask) == 0) { // last 2/5 bits must match otherwise it's not the current slot location
              updatePendingContextsInSlot(&contextInfo->bitStateTmp, byte1 >> (shift - 3)); // simulate the current states at the temporary location
            }
          }
        }
      }
      else {
        //when pbos==2: switch from slot 0 to slot 1
        //when bpos==5: switch from slot 1 to slot 2
        const uint32_t ctx = contextInfo->tableIndex;
        const uint16_t chk = contextInfo->tableChecksum;
        contextInfo->slot012 = hashTable[(ctx + c0) & mask].find(chk);
      }
    }
  }
}

void ContextMap2::mix(Mixer &m) {

  order = 0;
  confidence = 0;

  INJECT_SHARED_bpos
  INJECT_SHARED_c0
  for( uint32_t i = 0; i < C; i++ ) {
    ContextInfo* contextInfo = &contextInfoList[i];
    uint8_t* pState = &contextInfo->slot012->bitState + getStateByteLocation(bpos, c0);
    const int state = *pState;
    const int n0 = StateTable::next(state, 2);
    const int n1 = StateTable::next(state, 3);
    const int bitIsUncertain = int(n0 != 0 && n1 != 0);

    // predict from last byte(s) in context
    uint8_t byteState = contextInfo->slot0->bitState;
    const bool complete1 = (byteState >= 3) || (byteState >= 1 && bpos == 0);
    const bool complete2 = (byteState >= 7) || (byteState >= 3 && bpos == 0);

    bool skippedRunMap = true;
    if( complete1 ) {
      if(((contextInfo->slot0->byteStats.byte1 + 256u) >> (8 - bpos)) == c0 ) { // 1st candidate (last byte seen) matches
        const int predictedBit = (contextInfo->slot0->byteStats.byte1 >> (7 - bpos)) & 1;
        const int byte1IsUncertain = static_cast<const int>(contextInfo->slot0->byteStats.byte2 != contextInfo->slot0->byteStats.byte1);
        const int runCount = contextInfo->slot0->byteStats.runcount; // 1..255
        m.add(stretch(runMap1.p1(runCount << 2 | byte1IsUncertain << 1 | predictedBit)) >> (byte1IsUncertain));
        skippedRunMap = false;
      } else if( complete2 && ((contextInfo->slot0->byteStats.byte2 + 256u) >> (8 - bpos)) == c0 ) { // 2nd candidate matches
        const int predictedBit = (contextInfo->slot0->byteStats.byte2 >> (7 - bpos)) & 1;
        const int byte2IsUncertain = static_cast<const int>(contextInfo->slot0->byteStats.byte3 != contextInfo->slot0->byteStats.byte2);
        m.add(stretch(runMap1.p1(bitIsUncertain << 1 | predictedBit)) >> (1 + byte2IsUncertain));
        skippedRunMap = false;
      }
    }
    if(skippedRunMap) {
      m.add(0);
    }

    // predict from bit context
    confidence *= 3;
    if( state == 0 ) {
      m.add(0);
      m.add(0);
    } else {
      const int p1 = stateMap1.p1(state);
      const int st = stretch(p1);
      const int contextIsYoung = int(state <= 6);
      m.add(st >> (contextIsYoung + 1));
      m.add((p1 - 2048) >> 2);
      order++;
      confidence += 1 + bitIsUncertain;
    }
  }
}

void ContextMap2::print() {
  uint64_t used = 0;
  uint64_t empty = 0;
  for (int i = 0; i < hashTable.size(); i++)
  {
    auto bucket = &hashTable[i];
    bucket->stat(used, empty);
  }
  printf("ContextMap2 used: %" PRIu64 " empty: %" PRIu64, used, empty);
}