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

#ifndef _FILE_READER_H_
#define _FILE_READER_H_

#include "tl_types.h"

namespace TagLEET {

class FileReader
{
public:
  FileReader() {};
  virtual ~FileReader() {};

  virtual TL_ERR Open(const char *FileName, bool RandomAccess = false) = 0;
  virtual TL_ERR Open(const wchar_t *FileName, bool RandomAccess = false) = 0;
  virtual TL_ERR Reopen() = 0;
  virtual void Close() = 0;
  virtual TL_ERR Read(tf_int_t Offset, void *buff, uint32_t Size) = 0;
  virtual TL_ERR Map(tf_int_t Offset, tf_int_t Size, void **BasePtr) = 0;
  virtual void Unmap(void *Base, tf_int_t Size) = 0;
  virtual TL_ERR Unmodified() const = 0;
  virtual void AckNewTime() = 0;

  static bool MapSupported();
  static FileReader *FileReaderCreate();
  /* Used for allocating full pages by the OS, may use faster implementation
   * than regular malloc/free */
  static void *AllocateMem(uint32_t Size);
  static void FreeMem(void *ptr);
  static uint32_t GetSystemPageSize();
  static uint32_t GetSystemAllocGranularity();

  tf_int_t FileSize;
};


class ReaderBuff
{
public:
  ReaderBuff();
  virtual ~ReaderBuff();
  TL_ERR Init(FileReader *in_fr, tf_int_t in_Offset, uint32_t in_Size);
  void Release();

  TL_ERR FindFirstFullLine(bool SlideBuffer = false);
  TL_ERR FindNextFullLine(bool SlideBuffer = false);
private:
  TL_ERR GetEolSize(uint32_t EolOffset, uint32_t *EolSize);

public:
  /* Buffer to file data. Either to mapped data or allocated buffer with data
   * that was read. */
  uint8_t *Buff;
  /* Number of valid mapped bytes or allocated bytes in 'Buff'. */
  uint32_t Size;
  /* Indication if 'Buff' is mapped (true) or allocated (false). */
  bool IsMapped;
  /* Offset in bytes from start of 'Buff' to beginning of current line. */
  uint32_t LineOffset;
  /* Offset in bytes from start of 'Buff' to beginning of next line. */
  uint32_t NextLineOffset;
  /* Size in bytes of current line excluding end of line bytes. */
  uint32_t LineSize;
  /* Size in bytes of tag in current line. */
  uint32_t TagSize;
  /* Offset in bytes into file to which 'Buff' refers. */
  tf_int_t Offset;
  FileReader *fr;
};

} /* namespace TagLEET */

#endif /* _FILE_READER_H_ */
