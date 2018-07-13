/* LzmaUtil.c -- Test application for LZMA compression
2008-11-23 : Igor Pavlov : Public domain */

//#define _CRT_SECURE_NO_WARNINGS

//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
#include "7zTypes.h" //lint !e7 !e322
#include "LzmaDec.h" //lint !e7 !e322


static unsigned char DictBuf[4096];
static unsigned char probsBuf[6764];
void *MyAlloc(size_t size)
{
  if (size == 0)
    return 0;
  if (sizeof(DictBuf) == size)
	  return DictBuf;
  if (sizeof(probsBuf) == size)
	  return probsBuf;
  return 0;
}

void MyFree(void *address)
{
  (void)address;
}
//lint -e64 -e438 -e528
static void *SzAlloc(void *p, size_t size) { p = p; return MyAlloc(size); }
static void SzFree(void *p, void *address) { p = p; MyFree(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };


#define IN_BUF_SIZE 128 //(1 << 16)
#define OUT_BUF_SIZE 128 //(1 << 16)

static SRes Decode2(CLzmaDec *state, ISeqOutStream *outStream, ISeqInStream *inStream,
    UInt64 unpackSize)
{
  int thereIsSize = (unpackSize != (UInt64)(Int64)-1);
  Byte inBuf[IN_BUF_SIZE];
  Byte  outBuf[OUT_BUF_SIZE];
  size_t inPos = 0, inSize = 0, outPos = 0;
  LzmaDec_Init(state);
  for (;;)
  {
    if (inPos == inSize)
    {
      inSize = IN_BUF_SIZE;
      RINOK(inStream->Read(inStream, inBuf, &inSize));
      inPos = 0;
    }
    {
      SRes res;
      SizeT inProcessed = inSize - inPos;
      SizeT outProcessed = OUT_BUF_SIZE - outPos;
      ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
      ELzmaStatus status;
      if (thereIsSize && (outProcessed > unpackSize))
	  {
        outProcessed = (SizeT)unpackSize;
        finishMode = LZMA_FINISH_END;
      }
      
      res = LzmaDec_DecodeToBuf(state, outBuf + outPos, &outProcessed,
        inBuf + inPos, &inProcessed, finishMode, &status);
      inPos += inProcessed;
      outPos += outProcessed;
      unpackSize -= outProcessed;
      
      if (outStream)
        if (outStream->Write(outStream, outBuf, outPos) != outPos)
          return SZ_ERROR_WRITE;
        
      outPos = 0;
      
      if ((res != SZ_OK) || (thereIsSize && unpackSize == 0))
        return res;
      
      if (inProcessed == 0 && outProcessed == 0)
      {
        if (thereIsSize || status != LZMA_STATUS_FINISHED_WITH_MARK)
          return SZ_ERROR_DATA;
        return res;
      }
    }
  }
}

SRes Decode(ISeqOutStream *outStream, ISeqInStream *inStream)
{
  UInt64 unpackSize;
  int i;
  SRes res = 0;

  CLzmaDec state;

  /* header: 5 bytes of LZMA properties and 8 bytes of uncompressed size */
  unsigned char header[LZMA_PROPS_SIZE + 8];

  /* Read and parse header */
  size_t header_size = sizeof(header);
  RINOK(inStream->Read(inStream, header, &header_size));

  unpackSize = 0;
  for (i = 0; i < 8; i++)
    unpackSize += (UInt64)header[LZMA_PROPS_SIZE + i] << (i * 8);
  LzmaDec_Construct(&state);
  RINOK(LzmaDec_Allocate(&state, header, LZMA_PROPS_SIZE, &g_Alloc));
  res = Decode2(&state, outStream, inStream, unpackSize);
  LzmaDec_Free(&state, &g_Alloc);
  return res;
}


int lzma_update(SRes (*Read)(void *, void *, size_t *),size_t (*Write)(void *, const void *, size_t))
{
  ISeqOutStream outStream = {Write};
  ISeqInStream inStream = {Read};

  return Decode(&outStream, &inStream);
}

