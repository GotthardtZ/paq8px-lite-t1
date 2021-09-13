#include "ContextModel.hpp"
#include "ContextModelGeneric.cpp"
#include "ContextModelText.cpp"
#include "ContextModelImage1.cpp"
#include "ContextModelImage4.cpp"
#include "ContextModelImage8.cpp"
#include "ContextModelImage24.cpp"
#include "ContextModelJpeg.cpp"
#include "ContextModelDec.cpp"
#include "ContextModelAudio8.cpp"
#include "ContextModelAudio16.cpp"

ContextModel::ContextModel(Shared* const sh, Models* const models) : shared(sh), models(models) {}

int ContextModel::p() {
  uint32_t &blpos = shared->State.blockPos;
  // Parse block type and block size
  INJECT_SHARED_bpos
  if( bpos == 0 ) {
    --blockSize;
    blpos++;
    INJECT_SHARED_c1
    if( blockSize == -1 ) {
      nextBlockType = static_cast<BlockType>(c1); //got blockType but don't switch (we don't have all the info yet)
      bytesRead = 0;
      readSize = true;
    } else if( blockSize < 0 ) {
      if( readSize ) {
        bytesRead |= int(c1 & 0x7FU) << ((-blockSize - 2) * 7);
        if((c1 >> 7U) == 0 ) {
          readSize = false;
          if( !hasInfo(nextBlockType)) {
            blockSize = bytesRead;
            if( hasRecursion(nextBlockType)) {
              blockSize = 0;
            }
            blpos = 0;
          } else {
            blockSize = -1;
          }
        }
      } else if( blockSize == -5 ) {
        INJECT_SHARED_c4
        blockSize = bytesRead;
        blockInfo = c4;
        blpos = 0;
      }
    }

    if(blpos == 0 ) {
      blockType = nextBlockType; //got all the info - switch to next blockType
      shared->State.blockType = blockType;
    }
    if( blockSize == 0 ) {
      blockType = BlockType::DEFAULT;
      shared->State.blockType = blockType;
    }
  }


  switch( blockType ) {

    case BlockType::IMAGE1: {
      static ContextModelImage1 contextModelImage1{ shared, models };
      return contextModelImage1.p(blockInfo);
    }

    case BlockType::IMAGE4: {
      static ContextModelImage4 contextModelImage4{ shared, models };
      return contextModelImage4.p(blockInfo);
    }

    case BlockType::IMAGE8:
    case BlockType::PNG8:
    case BlockType::IMAGE8GRAY:
    case BlockType::PNG8GRAY: {
      static ContextModelImage8 contextModelImage8{ shared, models };
      int isGray = blockType == BlockType::IMAGE8GRAY || blockType == BlockType::PNG8GRAY;
      int isPNG = blockType == BlockType::PNG8 || blockType == BlockType::PNG8GRAY;
      return contextModelImage8.p(blockInfo, isGray, isPNG);
    }

    case BlockType::IMAGE24:
    case BlockType::PNG24:
    case BlockType::IMAGE32:
    case BlockType::PNG32: {
      static ContextModelImage24 contextModelImage24{ shared, models };
      int isAlpha = blockType == BlockType::IMAGE32 || blockType == BlockType::PNG32;
      int isPNG = blockType == BlockType::PNG24 || blockType == BlockType::PNG32;
      return contextModelImage24.p(blockInfo, isAlpha, isPNG);
    }

#ifndef DISABLE_AUDIOMODEL
    case BlockType::AUDIO:
    case BlockType::AUDIO_LE: {
      if ((blockInfo & 2) == 0) {

        static ContextModelAudio8 contextModelAudio8{ shared, models };
        return contextModelAudio8.p(blockInfo);
      }
      else {
        static ContextModelAudio16 contextModelAudio16{ shared, models };
        return contextModelAudio16.p(blockInfo);
      }
      }
#endif //DISABLE_AUDIOMODEL

    case BlockType::JPEG: {
      static ContextModelJpeg contextModelJpeg{ shared, models };
      return contextModelJpeg.p(blockInfo);
    }

    case BlockType::DEC_ALPHA: {
      static ContextModelDec contextModelDec{ shared, models };
      return contextModelDec.p();
    }

    case BlockType::TEXT:
    case BlockType::TEXT_EOL: {
      static ContextModelText contextModelText{ shared, models };
      return contextModelText.p();
    }

    default:
      static ContextModelGeneric contextModelGeneric{ shared, models };
      return contextModelGeneric.p();
  }

}
