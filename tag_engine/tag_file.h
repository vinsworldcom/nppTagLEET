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

#ifndef _TAG_FILE_H_
#define _TAG_FILE_H_

#include "tl_types.h"
#include "file_reader.h"
#include "avl.h"

namespace TagLEET {

class FileReader;
struct TagStrRef;
struct TfPageDesc;

/* A memory allocator. Useful for allocating a sequence of small objects and
 * than quickly release all of them */
class TfAllocator
{
public:
  TfAllocator(uint32_t in_AllocPageSize, uint32_t in_AllocAlign);
  ~TfAllocator();
  void Reset();
  void *Alloc(uint32_t Size);
  char *StrDup(const char *Str, int32_t StrSize = 0);
  void UndoAlloc(uint32_t Size);

  private:
  uint8_t *CurrPage;
  uint32_t AllocOffset;
  uint32_t AllocPageSize;
  uint32_t AllocAlign;
  uint32_t HeaderSize;
};

/* Tag file interface and cache.
 * The main method is Lookup which finds the offset within the file of the page
 * that contains the tag. */
class TagFile
{
public:
  TagFile();
  virtual ~TagFile();
  TL_ERR Init(const char *FileName);
  TL_ERR Init(const wchar_t *FileName);
  void Reset();
  TL_ERR Lookup(const char *TagStr, tf_int_t *RangeStart,
    tf_int_t *RangeSize);
  void CloseFile();
  TL_ERR ReopenFile();
  FileReader *GetFileReader() const { return fr; }
  bool IsCaseInsensitive() const { return CaseInsensitive; }

private:
  TL_ERR CommonInit(const wchar_t *FileNameW, const char *FileNameA);
  TL_ERR TestCaseSensitivity();
  TL_ERR Process_FILE_SORTED_flag(ReaderBuff *Rb);
  TfPageDesc *AddNewPageToTree(tf_int_t Offset);
  TfPageDesc *AllocDesc(const TagStrRef *Tag);
  TL_ERR GrowDescBackward(TfPageDesc **Desc);
  TL_ERR TestSort(tf_int_t Offset, avl_loc_t *loc) const;
  static int tag_tree_comp_func(void *ctx, avl_node_t *node, void *key);

private:
  uint32_t PageSize;
  avl_root_t LookupTree;
  FileReader *fr;
  TfAllocator DescAllocator;
  TfAllocator TagStrAllocator;
  int (*StrCmp)(const char *s1, const char *s2);
  int (*StrnCmp)(const char *s1, const char *s2, size_t n);
  bool CaseInsensitive;
};

enum TagKind {
  TAG_KIND_UNKNOWN = 0,
  TAG_KIND_CLASSES,
  TAG_KIND_MACRO_DEF,
  TAG_KIND_ENUM_VAL,
  TAG_KIND_FUNCTION_DEF,
  TAG_KIND_ENUM_NAME,
  TAG_KIND_LOCAL_VAR,
  TAG_KIND_MEMBER,
  TAG_KIND_NAMESPACE,
  TAG_KIND_FUNCTION_PROTO,
  TAG_KIND_STRUCT_NAME,
  TAG_KIND_TYPEDEF,
  TAG_KIND_UNION_NAME,
  TAG_KIND_VAR_DEF,
  TAG_KIND_EXTERNAL,
  TAG_KIND_FILE,
  TAG_KIND_LAST
  };

struct TagLineProperties {
  const char *Tag;
  uint32_t TagSize;
  const char *FileName;
  uint32_t FileNameSize;
  const char *ExCmd;
  uint32_t ExCmdSize;
  const char *ExtType;
  uint32_t ExtTypeSize;
  const char *ExtLine;
  uint32_t ExtLineSize;
  const char *ExtFields;
  uint32_t ExtFieldsSize;
  TagKind Kind;
};

class TagIterator
{
public:
  TagIterator(bool in_MatchPrefix=false);
  virtual ~TagIterator();

  TL_ERR Init(TagFile *tf, const char *in_TagStr, uint32_t ReadSize=0);
  void Release();
  TL_ERR GetNextTagLine(const char **Line, uint32_t *LineSize);
  TL_ERR GetTagLineProps(TagLineProperties *Props) const;
  TL_ERR GetNextTagLineProps(TagLineProperties *Props);
  uint32_t GetLineCount() const { return LineCount; };

private:
  ReaderBuff rb;
  const char *TagStr;
  uint32_t TagSize;
  uint32_t LineCount;
  bool IsFirstLine;
  bool MatchPrefix;
  int (*MemCmp)(const void *s1, const void *s2, size_t n);
};

int memicmp(const void *s1, const void *s2, size_t n);
int strnicmp(const char *str1, const char *str2, size_t n);
int stricmp(const char *str1, const char *str2);

} /* namespace TagLEET */

#endif /* _TAG_FILE_H_ */

