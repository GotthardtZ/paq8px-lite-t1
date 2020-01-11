#include "ExeModel.hpp"

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
        op.prefix = (op.code == ES_OVERRIDE || op.code == CS_OVERRIDE || op.code == SS_OVERRIDE || op.code == DS_OVERRIDE) +
                    //invalid in x64
                    (op.code == FS_OVERRIDE) * 2 + (op.code == GS_OVERRIDE) * 3 + (op.code == AD_OVERRIDE) * 4 + (op.code == WAIT_FPU) * 5 +
                    (op.code == LOCK) * 6 + (op.code == REP_N_STR || op.code == REP_STR) * 7;

        if( !op.decoding ) {
          totalOps += (op.data != 0) - (cache.Index && cache.Op[cache.Index & (CacheSize - 1)] != 0);
          opMask = (opMask << 1) | (state != Error);
          opCategoryMask = (opCategoryMask << categoryShift) | (op.category);
          op.size = 0;

          cache.Op[cache.Index & (CacheSize - 1)] = op.data;
          cache.Index++;

          if( !op.prefix )
            op.data = op.code << codeShift;
          else {
            op.data = op.prefix;
            op.category = typeOp1[op.code];
            op.decoding = true;
            brkCtx = hash(0, op.prefix, opCategoryMask & categoryMask);
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
            op.category = typeOp1[op.code];
            brkCtx = hash(1, op.prefix, opCategoryMask & categoryMask);
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
      brkCtx = hash(2, state, op.code, (opCategoryMask & categoryMask), opN(cache, 1) & ((ModRM_mod | ModRM_reg | ModRM_rm) << ModRMShift));
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
        op.flags = table2[op.code];
        op.category = typeOp2[op.code];
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
        op.flags = tableX[i];
        op.category = typeOpX[i];
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
        op.flags = table3_38[op.code];
        op.category = typeOp3_38[op.code];
      } else {
        op.flags = table3_3A[op.code];
        op.category = typeOp3_3A[op.code];
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
      op.data |= regDWordDisplacement;
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
      cm.set(hash(i, exeCxt(j, buf(1) * (j > 6)), ((1 << nCM1) | mask) * (count0 * nCM1 / 2 >= i), (0x08 | (blPos & 0x07)) * (i < 4)));
      i++;
    }

    cm.set(brkCtx);

    mask = prefixMask | (0xF8 << codeShift) | multiByteOpcode | prefix38 | prefix3A;
    cm.set(hash(++i, opN(cache, 1) & (mask | regDWordDisplacement | addressMode), state + 16 * op.bytesRead, op.data & mask, op.REX,
                op.category));

    mask = 0x04 | (0xFE << codeShift) | multiByteOpcode | prefix38 | prefix3A | ((ModRM_mod | ModRM_reg) << ModRMShift);
    cm.set(hash(++i, opN(cache, 1) & mask, opN(cache, 2) & mask, opN(cache, 3) & mask,
                context + 256 * ((op.ModRM & ModRM_mod) == ModRM_mod), op.data & ((mask | prefixRex) ^ (ModRM_mod << ModRMShift))));

    mask = 0x04 | codeMask;
    cm.set(hash(++i, opN(cache, 1) & mask, opN(cache, 2) & mask, opN(cache, 3) & mask, opN(cache, 4) & mask,
                (op.data & mask) | (state << 11) | (op.bytesRead << 15)));

    mask = 0x04 | (0xFC << codeShift) | multiByteOpcode | prefix38 | prefix3A;
    cm.set(hash(++i, state + 16 * op.bytesRead, op.data & mask, op.category * 8 + (opMask & 0x07), op.flags,
                ((op.SIB & SIB_base) == 5) * 4 + ((op.ModRM & ModRM_reg) == ModRM_reg) * 2 + ((op.ModRM & ModRM_mod) == 0)));

    mask = prefixMask | codeMask | operandSizeOverride | multiByteOpcode | prefixRex | prefix38 | prefix3A | hasExtraFlags | hasModRm |
           ((ModRM_mod | ModRM_rm) << ModRMShift);
    cm.set(hash(++i, op.data & mask, state + 16 * op.bytesRead, op.flags));

    mask = prefixMask | codeMask | operandSizeOverride | multiByteOpcode | prefix38 | prefix3A | hasExtraFlags | hasModRm;
    cm.set(hash(++i, opN(cache, 1) & mask, state, op.bytesRead * 2 + ((op.REX & REX_w) > 0),
                op.data & ((uint16_t) (mask ^ operandSizeOverride))));

    mask = 0x04 | (0xFE << codeShift) | multiByteOpcode | prefix38 | prefix3A | (ModRM_reg << ModRMShift);
    cm.set(hash(++i, opN(cache, 1) & mask, opN(cache, 2) & mask, state + 16 * op.bytesRead, op.data & (mask | prefixMask | codeMask)));

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
              ((op.category == OP_GEN_BRANCH) << 4) | (((c0 & ((1U << bpos) - 1)) == 0) << 5U);

  m.set(context * 4 + (s >> 4U), 1024);
  m.set(state * 64 + bpos * 8 + (op.bytesRead > 0) * 4 + (s >> 4U), 1024);
  m.set((brkCtx & 0x1FFU) | ((s & 0x20U) << 4U), 1024);
  m.set(finalize64(hash(op.code, state, opN(cache, 1) & codeMask), 13), 8192);
  m.set(finalize64(hash(state, bpos, op.code, op.bytesRead), 13), 8192);
  m.set(finalize64(hash(state, (bpos << 2U) | (c0 & 3U), opCategoryMask & categoryMask,
                        ((op.category == OP_GEN_BRANCH) << 2U) | (((op.flags & fMODE) == fAM) << 1) | (op.bytesRead > 0)), 13), 8192);
}

bool ExeModel::isInvalidX64Op(const uint8_t op) {
  for( int i = 0; i < 19; i++ ) {
    if( op == invalidX64Ops[i] )
      return true;
  }
  return false;
}

bool ExeModel::isValidX64Prefix(const uint8_t prefix) {
  for( int i = 0; i < 8; i++ ) {
    if( prefix == x64Prefixes[i] )
      return true;
  }
  return ((prefix >= 0x40 && prefix <= 0x4F) || (prefix >= 0x64 && prefix <= 0x67));
}

void ExeModel::processMode(ExeModel::Instruction &op, ExeModel::ExeState &state) {
  if((op.flags & fMODE) == fAM ) {
    op.data |= addressMode;
    op.bytesRead = 0;
    switch( op.flags & fTYPE ) {
      case fDR:
        op.data |= (2U << typeShift);
      case fDA:
        op.data |= (1U << typeShift);
      case fAD: {
        state = Read32;
        break;
      }
      case fBR: {
        op.data |= (2U << typeShift);
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
        op.data |= (1U << typeShift);
        op.bytesRead = 0;
        break;
      }
      case fDI: {
        // x64 Move with 8byte immediate? [REX.W is set, opcodes 0xB8+r]
        op.imm8 = ((op.REX & REX_w) > 0 && (op.code & 0xF8U) == 0xB8U);
        if( !op.o16 || op.imm8 ) {
          state = Read32;
          op.data |= (2U << typeShift);
        } else {
          state = Read16;
          op.data |= (3U << typeShift);
        }
        op.bytesRead = 0;
        break;
      }
      default:
        state = Start; /*no immediate*/
    }
  }
}

void ExeModel::processFlags2(ExeModel::Instruction &op, ExeModel::ExeState &state) {
  //if arriving from state ExtraFlags, we've already read the ModRM byte
  if((op.flags & fMODE) == fMR && state != ExtraFlags ) {
    state = ReadModRM;
    return;
  }
  processMode(op, state);
}

void ExeModel::processFlags(ExeModel::Instruction &op, ExeModel::ExeState &state) {
  if( op.code == OP_CALLF || op.code == OP_JMPF || op.code == OP_ENTER ) {
    op.bytesRead = 0;
    state = Read16_f;
    return; //must exit, ENTER has ModRM too
  }
  processFlags2(op, state);
}

void ExeModel::checkFlags(ExeModel::Instruction &op, ExeModel::ExeState &state) {
  //must peek at ModRM byte to read the REG part, so we can know the opcode
  if( op.flags == fMEXTRA )
    state = ExtraFlags;
  else if( op.flags == fERR ) {
    memset(&op, 0, sizeof(Instruction));
    state = Error;
  } else
    processFlags(op, state);
}

void ExeModel::readFlags(ExeModel::Instruction &op, ExeModel::ExeState &state) {
  op.flags = table1[op.code];
  op.category = typeOp1[op.code];
  checkFlags(op, state);
}

void ExeModel::processModRm(ExeModel::Instruction &op, ExeModel::ExeState &state) {
  if((op.ModRM & ModRM_mod) == 0x40 )
    state = Read8_ModRM; //register+byte displacement
  else if((op.ModRM & ModRM_mod) == 0x80 || (op.ModRM & (ModRM_mod | ModRM_rm)) == 0x05 ||
          (op.ModRM < 0x40 && (op.SIB & SIB_base) == 0x05)) {
    state = Read32_ModRM; //register+dword displacement
    op.bytesRead = 0;
  } else
    processMode(op, state);
}

void ExeModel::applyCodeAndSetFlag(ExeModel::Instruction &op, uint32_t flag) {
  op.data &= clearCodeMask;
  op.data |= (op.code << codeShift) | flag;
}

uint32_t ExeModel::opN(ExeModel::OpCache &cache, const uint32_t n) {
  return cache.Op[(cache.Index - n) & (CacheSize - 1)];
}

uint32_t ExeModel::opNCategory(uint32_t &mask, const uint32_t n) {
  return ((mask >> (categoryShift * (n - 1))) & categoryMask);
}

int ExeModel::pref(const int i) {
  INJECT_SHARED_buf
  return (buf(i) == 0x0f) + 2 * (buf(i) == 0x66) + 3 * (buf(i) == 0x67);
}

uint32_t ExeModel::exeCxt(int i, int x) {
  INJECT_SHARED_buf
  int prefix = 0, opcode = 0, modRm = 0, sib = 0;
  if( i )
    prefix += 4 * pref(i--);
  if( i )
    prefix += pref(i--);
  if( i )
    opcode += buf(i--);
  if( i )
    modRm += buf(i--) & (ModRM_mod | ModRM_rm);
  if( i && ((modRm & ModRM_rm) == 4) && (modRm < ModRM_mod))
    sib = buf(i) & SIB_scale;
  return prefix | opcode << 4 | modRm << 12 | x << 20 | sib << (28 - 6);
}
