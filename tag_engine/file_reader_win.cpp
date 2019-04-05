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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace TagLEET;

class FileReaderWin : public FileReader
{
public:
  FileReaderWin();
  virtual ~FileReaderWin();

  virtual TL_ERR Open(const char *FileName, bool RandomAccess);
  virtual TL_ERR Open(const wchar_t *FileName, bool RandomAccess);
  virtual TL_ERR Reopen();
  virtual void Close();
  virtual TL_ERR Read(tf_int_t Offset, void *buff, uint32_t Size);
  virtual TL_ERR Map(tf_int_t Offset, tf_int_t Size, void **BasePtr);
  virtual void Unmap(void *Base, tf_int_t Size);
  virtual TL_ERR Unmodified() const;
  virtual void AckNewTime();

private:
  tf_int_t Size() const;
  TL_ERR DoOpen(bool RandomAccess);


private:
  HANDLE FileHndl;
  HANDLE MapHndl;
  const wchar_t *FileNameW;
  bool SaveRandomAccess;
  FILETIME OpenFileTime;
  FILETIME ReopenFileTime;
  DWORD AllocGranularity;
};

static void file_name_w_free(const wchar_t *FileNameW)
{
  ::HeapFree(GetProcessHeap(), 0, const_cast<wchar_t *>(FileNameW));
}

static wchar_t *file_name_w_dup(const wchar_t *FileName)
{
  size_t size;
  wchar_t *new_str;

  size = (wcslen(FileName) + 1) * sizeof(wchar_t);
  new_str = (wchar_t *)::HeapAlloc(GetProcessHeap(), 0, size);
  ::memcpy(new_str, FileName, size);
  return new_str;
}

static wchar_t *file_name_w_alloc(const char *FileName)
{
  int size;
  wchar_t *new_str;

  size = ::MultiByteToWideChar(CP_UTF8, 0, FileName, -1, NULL, 0);
  if (size == 0)
    return NULL;
  size *= sizeof(wchar_t);
  new_str = (wchar_t *)::HeapAlloc(GetProcessHeap(), 0, size);
  if (new_str == NULL)
    return NULL;
  size = ::MultiByteToWideChar(CP_UTF8, 0, FileName, -1, new_str, size);
  if (size != 0)
    return new_str;
  file_name_w_free(new_str);
  return NULL;
}

FileReaderWin::FileReaderWin():
  FileReader()
{
  SYSTEM_INFO SysInfo;

  FileHndl = INVALID_HANDLE_VALUE;
  MapHndl = NULL;
  FileNameW = NULL;

  ::GetSystemInfo(&SysInfo);
  AllocGranularity = SysInfo.dwAllocationGranularity;
}

FileReaderWin::~FileReaderWin()
{
  Close();
  if (FileNameW != NULL)
  {
    file_name_w_free(FileNameW);
    FileNameW = NULL;
  }
}

void FileReaderWin::Close()
{
  if (MapHndl != NULL)
  {
    ::CloseHandle(MapHndl);
    MapHndl = NULL;
  }
  if (FileHndl != INVALID_HANDLE_VALUE)
  {
    ::CloseHandle(FileHndl);
    FileHndl = INVALID_HANDLE_VALUE;
  }
}

TL_ERR FileReaderWin::DoOpen(bool RandomAccess)
{
  TL_ERR err;

  if (FileNameW == NULL)
    return TL_ERR_MEM_ALLOC;
  SaveRandomAccess = RandomAccess;
  err = FileReaderWin::Reopen();
  if (!err)
  {
    OpenFileTime = ReopenFileTime;
    return err;
  }

  file_name_w_free(FileNameW);
  FileNameW = NULL;
  return TL_ERR_GENERAL;
}

TL_ERR FileReaderWin::Open(const char *FileName, bool RandomAccess)
{
  if (FileHndl != INVALID_HANDLE_VALUE)
    return TL_ERR_GENERAL;

  FileNameW = file_name_w_alloc(FileName);
  return DoOpen(RandomAccess);
}

TL_ERR FileReaderWin::Open(const wchar_t *FileName, bool RandomAccess)
{
  if (FileHndl != INVALID_HANDLE_VALUE)
    return TL_ERR_GENERAL;

  FileNameW = file_name_w_dup(FileName);
  return DoOpen(RandomAccess);
}

TL_ERR FileReaderWin::Reopen()
{
  BOOL rc;
  DWORD Flags;

  if (FileHndl != INVALID_HANDLE_VALUE)
    return TL_ERR_GENERAL;

  Flags = FILE_ATTRIBUTE_READONLY |
    (SaveRandomAccess ? FILE_FLAG_RANDOM_ACCESS : FILE_FLAG_SEQUENTIAL_SCAN);
  FileHndl = ::CreateFileW(FileNameW, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
    Flags, NULL);
  if (FileHndl == INVALID_HANDLE_VALUE)
    return TL_ERR_GENERAL;

  rc = GetFileTime(FileHndl, NULL, NULL, &ReopenFileTime);
  if (rc == 0)
  {
    ::CloseHandle(FileHndl);
    FileHndl = INVALID_HANDLE_VALUE;
    return TL_ERR_GENERAL;
  }
  FileSize = Size();
  return TL_ERR_OK;
}


bool FileReader::MapSupported()
{
  return true;
}

TL_ERR FileReaderWin::Map(tf_int_t Offset, tf_int_t Size, void **BasePtr)
{
  uint8_t *Base;
  tf_int_t GranPad;
  ULARGE_INTEGER U64;

  if (FileHndl == INVALID_HANDLE_VALUE)
    return TL_ERR_GENERAL;

  if (MapHndl == NULL)
  {
    MapHndl = CreateFileMapping(FileHndl, NULL, PAGE_READONLY, 0, 0, NULL);
    if (MapHndl == NULL)
      return TL_ERR_GENERAL;
  }

  GranPad = Offset & (AllocGranularity - 1);
  U64.QuadPart = Offset - GranPad;
  Base = (uint8_t *)MapViewOfFile(MapHndl, FILE_MAP_READ,
    U64.HighPart, U64.LowPart, (SIZE_T)(GranPad + Size));
  if (Base != NULL)
  {
    *BasePtr = Base + GranPad;
    return TL_ERR_OK;
  }

  return TL_ERR_GENERAL;
}

void FileReaderWin::Unmap(void *Base, tf_int_t)
{
  tf_int_t Offset = (tf_int_t)((uint8_t *)Base - (uint8_t *)0);
  tf_int_t GranPad = Offset & (AllocGranularity - 1);

  UnmapViewOfFile((uint8_t *)Base - GranPad);
}

TL_ERR FileReaderWin::Read(tf_int_t Offset, void *buff, uint32_t Size)
{
  BOOL rc;
  DWORD BytesRead;
  LARGE_INTEGER i64;

  if (FileHndl == INVALID_HANDLE_VALUE)
    return TL_ERR_GENERAL;

  i64.QuadPart = Offset;
  rc = ::SetFilePointerEx(FileHndl, i64, NULL, FILE_BEGIN);
  if (rc == 0)
    return TL_ERR_GENERAL;
  rc = ::ReadFile(FileHndl, buff, Size, &BytesRead, NULL);
  if (rc != 0 && BytesRead == Size)
    return TL_ERR_OK;

  return TL_ERR_GENERAL;
}

tf_int_t FileReaderWin::Size() const
{
  BOOL rc;
  LARGE_INTEGER i64;

  if (FileHndl != INVALID_HANDLE_VALUE)
  {
    rc = ::GetFileSizeEx(FileHndl, &i64);
    if (rc != 0)
      return (tf_int_t)i64.QuadPart;
  }

  return 0;
}

FileReader *FileReader::FileReaderCreate()
{
  return new FileReaderWin;
}

TL_ERR FileReaderWin::Unmodified() const
{
  LONG CompRes;

  if (FileHndl == INVALID_HANDLE_VALUE)
    return TL_ERR_GENERAL;

  CompRes = CompareFileTime(&OpenFileTime, &ReopenFileTime);
  return CompRes == 0 ? TL_ERR_OK : TL_ERR_MODIFIED;
}

void FileReaderWin::AckNewTime()
{
  OpenFileTime = ReopenFileTime;
}

void *FileReader::AllocateMem(uint32_t Size)
{
  return VirtualAlloc(NULL, Size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void FileReader::FreeMem(void *ptr)
{
  VirtualFree(ptr, 0, MEM_RELEASE);
}

uint32_t FileReader::GetSystemPageSize()
{
  SYSTEM_INFO SysInfo;
  ::GetSystemInfo(&SysInfo);
  return (uint32_t)SysInfo.dwPageSize;
}

uint32_t FileReader::GetSystemAllocGranularity()
{
  SYSTEM_INFO SysInfo;
  ::GetSystemInfo(&SysInfo);
  return (uint32_t)SysInfo.dwAllocationGranularity;
}

