/*  Copyright 2013-2014, Gur Stavi, gur.stavi@gmail.com  */

/*
    This file is part of TagLEET.

    TagLEET is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    TagLEET is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with TagLEET.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "file_reader.h"
#include <stddef.h>
#include <assert.h>

using namespace TagLEET;

ReaderBuff::ReaderBuff()
{
  fr = NULL;
  Offset = 0;
  Buff = NULL;
  Size = 0;
  LineOffset = 0;
  NextLineOffset = 0;
  LineSize = 0;
  TagSize = 0;
  IsMapped = false;
}

ReaderBuff::~ReaderBuff()
{
  if ( Buff != NULL)
  {
    if (IsMapped)
      fr->Unmap(Buff, Size);
    else
      fr->FreeMem(Buff);
  }
}

TL_ERR ReaderBuff::Init(FileReader *in_fr, tf_int_t in_Offset, uint32_t in_Size)
{
  TL_ERR err;

  if (Buff != NULL)
    Release();

  fr = in_fr;
  Offset = in_Offset;
  Size = in_Size;
  LineOffset = 0;
  NextLineOffset = 0;
  LineSize = 0;
  TagSize = 0;

  if (Offset + Size > fr->FileSize)
  {
    Size = Offset >= fr->FileSize ? 0 : (uint32_t)(fr->FileSize - Offset);
    if (Size == 0)
      return TL_ERR_NO_MORE;
  }

  if (fr->MapSupported())
  {
    IsMapped = true;
    err = fr->Map(Offset, Size, (void **)&Buff);
    return err;
  }

  IsMapped = false;
  Buff = (uint8_t *)fr->AllocateMem(Size);
  if (Buff == NULL)
    return TL_ERR_MEM_ALLOC;
  err = fr->Read(Offset, Buff, Size);
  if (err)
  {
    fr->FreeMem(Buff);
    Buff = NULL;
  }
  return err;
}

void ReaderBuff::Release()
{
  if (Buff == NULL)
    return;

  if (IsMapped)
    fr->Unmap(Buff, Size);
  else
    fr->FreeMem(Buff);

  Buff = NULL;
}

static uint8_t TestEolArr[2] = {10,13};
#define IS_EOL_CHAR(c) (TestEolArr[(uint8_t)(c) & 1] == (c))

TL_ERR ReaderBuff::FindFirstFullLine(bool SlideBuffer)
{
  TL_ERR err;

  NextLineOffset = 0;
  err = FindNextFullLine(SlideBuffer);
  if (!err || err == TL_ERR_LINE_TOO_BIG)
    err = FindNextFullLine(SlideBuffer);
  return err;
}

TL_ERR ReaderBuff::GetEolSize(uint32_t EolOffset, uint32_t *EolSize)
{
  int n;

  assert (EolOffset < Size);
  if (Buff[EolOffset] == '\n')
  {
    n = 1;
  }
  else if (Buff[EolOffset] == '\r')
  {
    if (EolOffset + 1 == Size)
      return TL_ERR_NO_MORE;
    if (Buff[EolOffset+1] == '\n')
      n = 2;
    else
      n = 1;
  }
  else
  {
    n = 0;
  }
  *EolSize = n;
  return TL_ERR_OK;
}

TL_ERR ReaderBuff::FindNextFullLine(bool SlideBuffer)
{
  TL_ERR err;
  uint32_t i;
  bool TooBig = false;

  if (Buff == NULL)
    return TL_ERR_GENERAL;
  for (;;)
  {
    uint32_t EolSize;
    TagSize = 0;

    i = LineOffset = NextLineOffset;
    /* Search for Next EOL */
    while (i < Size && !IS_EOL_CHAR(Buff[i]))
    {
      if (Buff[i] == '\t' && TagSize == 0)
        TagSize = i - LineOffset;
      i++;
    }

    if (i == Size)
    {
      /* Test if this is a last line without EOL */
      if (Offset + Size == fr->FileSize)
      {
        if (i > LineOffset)
        {
          LineSize = i - LineOffset;
          NextLineOffset = LineOffset + LineSize;
          if (!TooBig)
            return TL_ERR_OK;
          LineSize = 0;
          return TL_ERR_LINE_TOO_BIG;
        }
        return TL_ERR_NO_MORE;
      }
    }
    else /* i < Size */
    {
      /* Remember that we should also return empty lines in case caller need
       * to get accurate line count */
      err = GetEolSize(i, &EolSize);
      if (!err)
      {
        LineSize = i - LineOffset;
        NextLineOffset = i + EolSize;
        return LineSize + EolSize > 0 ? TL_ERR_OK : TL_ERR_NO_MORE;
      }
    }

    if (SlideBuffer == false)
      return TL_ERR_NO_MORE;

    if (LineOffset > 0)
    {
      Offset += LineOffset;
    }
    else
    {
      TooBig = true;
      Offset += i;
    }

    /* Slide the buffer further in the file to read the next lines */
    if (Offset + Size > fr->FileSize)
    {
      Size = (uint32_t)(fr->FileSize - Offset);
      if (Size == 0)
        return TL_ERR_NO_MORE;
    }

    LineOffset = 0;
    NextLineOffset = 0;
    LineSize = 0;
    TagSize = 0;

    if (IsMapped)
    {
      fr->Unmap(Buff, Size);
      err = fr->Map(Offset, Size, (void **)&Buff);
    }
    else
    {
      err = fr->Read(Offset, Buff, Size);
    }
    if (err)
    {
      if (!IsMapped)
        fr->FreeMem(Buff);
      Buff = NULL;
      return err;
    }
  }
}
