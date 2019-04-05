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

#define _FILE_OFFSET_BITS 64
#include "file_reader.h"
#include <sys/types.h>
#include <sys/stat.h>
//#include <sys/mman.h>
#include "../unistd.h"
#include <fcntl.h>
#include <malloc.h>
#include <string.h>

class FileReaderLin : public FileReader
{
public:
  FileReaderLin();
  virtual ~FileReaderLin();

  virtual TL_ERR Open(const char *FileName, bool RandomAccess);
  virtual TL_ERR Reopen();
  virtual void Close();
  virtual TL_ERR Read(tf_int_t Offset, void *buff, uint32_t Size);
  virtual TL_ERR Map(tf_int_t Offset, tf_int_t Size, void **BasePtr);
  virtual void Unmap(void *Base, tf_int_t Size);
  virtual TL_ERR Unmodified() const;
  virtual void AckNewTime();

private:
  int fd;
  uint32_t PageSize;
  const char *FileName;
  bool SaveRandomAccess;
  time_t OpenFileTime;
  time_t ReopenFileTime;
};

FileReaderLin::FileReaderLin():
  FileReader()
{
  fd = -1;
  PageSize = GetSystemPageSize();
}

FileReaderLin::~FileReaderLin()
{
  Close();
  if (FileName != NULL)
  {
    free(const_cast<char *>(FileName));
    FileName = NULL;
  }
}

void FileReaderLin::Close()
{
  if (fd != -1)
  {
    ::close(fd);
    fd = -1;
  }
}

TL_ERR FileReaderLin::Open(const char *in_FileName, bool RandomAccess)
{
  TL_ERR err;

  if (fd != -1)
    return TL_ERR_GENERAL;

  FileName = strdup(in_FileName);
  if (FileName == NULL)
    return TL_ERR_MEM_ALLOC;

  SaveRandomAccess = RandomAccess;
  err = FileReaderLin::Reopen();

  if (!err)
  {
    OpenFileTime = ReopenFileTime;
    return err;
  }

  free(const_cast<char *>(FileName));
  FileName = NULL;
  return TL_ERR_GENERAL;
}

TL_ERR FileReaderLin::Reopen()
{
  struct stat buf;
  int err;

  if (fd != -1)
    return TL_ERR_GENERAL;

  fd = open(FileName, O_RDONLY, 0);
  if (fd == -1)
    return TL_ERR_GENERAL;

  err = fstat(fd, &buf);
  if (err)
  {
    ::close(fd);
    fd = -1;
    return TL_ERR_GENERAL;
  }

  ReopenFileTime = buf.st_mtime;
  FileSize = buf.st_size;
  return TL_ERR_OK;
}

bool FileReader::MapSupported()
{
  return false;
}

TL_ERR FileReaderLin::Map(tf_int_t Offset, tf_int_t Size, void **BasePtr)
{
  uint8_t *Base;
  tf_int_t GranPad;

  if (fd == -1)
    return TL_ERR_GENERAL;

  GranPad = Offset & (PageSize - 1);
  Base = (uint8_t *)::mmap(NULL, (size_t)(GranPad + Size), PROT_READ,
    MAP_SHARED, fd, Offset - GranPad);

  if (Base != MAP_FAILED)
  {
    *BasePtr = Base + GranPad;
    return TL_ERR_OK;
  }

  return TL_ERR_GENERAL;
}

void FileReaderLin::Unmap(void *Base, tf_int_t Size)
{
  tf_int_t Offset = (tf_int_t)((uint8_t *)Base - (uint8_t *)0);
  tf_int_t GranPad = Offset & (PageSize - 1);

  ::munmap((uint8_t *)Base - GranPad, (size_t)(Size + GranPad));
}

TL_ERR FileReaderLin::Read(tf_int_t Offset, void *buff, uint32_t Size)
{
  off_t seek_rc;
  ssize_t read_rc;

  if (fd == -1)
    return TL_ERR_GENERAL;

  seek_rc = ::lseek(fd, Offset, SEEK_SET);
  if (seek_rc == (off_t)-1)
    return TL_ERR_GENERAL;
  read_rc = ::read(fd, buff, Size);
  if ((uint32_t)read_rc == Size)
    return TL_ERR_OK;

  return TL_ERR_GENERAL;
}

FileReader *FileReader::FileReaderCreate()
{
  return new FileReaderLin;
}

TL_ERR FileReaderLin::Unmodified() const
{
  if (fd == -1)
    return TL_ERR_GENERAL;

  return OpenFileTime == ReopenFileTime ? TL_ERR_OK : TL_ERR_MODIFIED;
}

void FileReaderLin::AckNewTime()
{
  OpenFileTime = ReopenFileTime;
}

void *FileReader::AllocateMem(uint32_t Size)
{
  return malloc(Size);
}

void FileReader::FreeMem(void *Ptr)
{
  free(Ptr);
}

uint32_t FileReader::GetSystemPageSize()
{
  return sysconf(_SC_PAGESIZE);
}

uint32_t FileReader::GetSystemAllocGranularity()
{
  return GetSystemPageSize();
}

