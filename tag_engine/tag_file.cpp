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

#include "tag_file.h"
#include "file_reader.h"
#include "avl.h"

#include <malloc.h>
#include <memory.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

using namespace TagLEET;

struct TagLEET::TagStrRef
{
  const char *Str;
  uint32_t Size;
};

/* Descriptor for a page within the tag file.
 * Offset - offset of the page within the file.
 * Size - size of the descriptor. Usually size of a single page.
 * TagOffsetInDesc - the offset within the descriptor where the 1st full line
 * starts and also the tag that represents this descriptor.
 * TagStr - The tag that represents the descriptor.
 * Node - Used to hold the descriptor in an AVL tree sorted by tags.
 */
struct TagLEET::TfPageDesc
{
  tf_int_t Offset;
  tf_int_t Size;
  uint32_t TagOffsetInDesc;
  const char *TagStr;
  avl_node_t Node;
};

/* AVL callback to compare nodes in the Tag File Descriptor tree */
int TagFile::tag_tree_comp_func(void *ctx, avl_node_t *node, void *key)
{
  TagFile *tf = reinterpret_cast<TagFile *>(ctx);
  int res;
  TagStrRef *Tag = (TagStrRef *)key;
  TfPageDesc *Desc = AVL_CONTREC(node, TfPageDesc, Node);

  res = tf->StrnCmp(Desc->TagStr, Tag->Str, Tag->Size);
  return res != 0 ? res : Desc->TagStr[Tag->Size] == '\0' ? 0 : 1;
}

#define sizeof_max(x,y) (sizeof(x) >= sizeof(y) ? sizeof(x) : sizeof(y))

TagFile::TagFile():
  DescAllocator(4096, sizeof_max(void *, tf_int_t)),
  TagStrAllocator(4*1024, 1),
  StrCmp(::strcmp),
  StrnCmp(::strncmp),
  CaseInsensitive(false)
{
  PageSize = 8*1024;
  ::memset(&LookupTree, 0, sizeof(LookupTree));
  LookupTree.comp = tag_tree_comp_func;
  LookupTree.ctx = this;
  fr = NULL;
};

TagFile::~TagFile()
{
  if (fr != NULL)
  {
    delete fr;
  }
};

static TL_ERR read_tag_int(ReaderBuff *Rb, uint32_t *val_ptr)
{
  const char *Line;
  uint32_t i, val, digit;
  TL_ERR err = TL_ERR_INVALID;

  Line = (char *)Rb->Buff + Rb->LineOffset;
  i = Rb->TagSize;

  /* Skip whitespace. */
  while (i < Rb->LineSize)
  {
    if (Line[i] != ' ' && Line[i] != '\t')
      break;
    i++;
  }

  val = 0;
  while (i < Rb->LineSize)
  {
    if (Line[i] < '0' || Line[i] > '9')
      break;

    digit = Line[i++] - '0';
    val *= 10;
    val += digit;
    err = TL_ERR_OK;
  }

  if (!err)
    *val_ptr = val;

  return err;
}

TL_ERR TagFile::Process_FILE_SORTED_flag(ReaderBuff *Rb)
{
  TL_ERR err;
  uint32_t val;

  err = read_tag_int(Rb, &val);
  if (err)
    return TL_ERR_OK;

  switch (val)
  {
  case 0: /* Unsorted */
    return TL_ERR_SORT;

  case 2: /* Case insensitive. */
    CaseInsensitive = true;
    StrCmp = TagLEET::stricmp;
    StrnCmp = TagLEET::strnicmp;
    break;

  default:
    break;
  }

  return TL_ERR_OK;
}

TL_ERR TagFile::TestCaseSensitivity()
{
  static const char TagsSortedFlag[] = "!_TAG_FILE_SORTED";
  ReaderBuff Rb;
  const char *Line;
  TL_ERR err;
  uint32_t LineCount;
  int res;

  err = Rb.Init(fr, 0, 1024);
  if (err)
    return err;

  LineCount = 0;
  while (LineCount < 5)
  {
    err = Rb.FindNextFullLine();
    if (err)
      return TL_ERR_OK;

    Line = (char *)Rb.Buff + Rb.LineOffset;
    if (Rb.TagSize == sizeof(TagsSortedFlag) - 1)
    {
      res = ::memcmp(Line, TagsSortedFlag, Rb.TagSize);
      if (res == 0)
      {
        err = Process_FILE_SORTED_flag(&Rb);
        break;
      }
    }

    if (Rb.TagSize > 0 && Line[0] != '!')
      break;

    LineCount++;
  }

  return err;
}

TL_ERR TagFile::CommonInit(const wchar_t *FileNameW, const char *FileNameA)
{
  TL_ERR err;

  if (fr != NULL)
  {
    Reset();
    delete fr;
    fr = NULL;
  }

  if (FileNameW == NULL && FileNameA == NULL)
    return TL_ERR_OK;

  fr = FileReader::FileReaderCreate();
  if (fr == NULL)
    return TL_ERR_GENERAL;

  if (FileNameW != NULL)
    err = fr->Open(FileNameW, true);
  else
    err = fr->Open(FileNameA, true);

  if (!err)
    err = TestCaseSensitivity();

  if (err)
  {
    delete fr;
    fr = NULL;
    return err;
  }
  return TL_ERR_OK;
}

TL_ERR TagFile::Init(const wchar_t *FileName)
{
  return CommonInit(FileName, NULL);
}

TL_ERR TagFile::Init(const char *FileName)
{
  return CommonInit(NULL, FileName);
}

void TagFile::CloseFile()
{
  if (fr != NULL)
    fr->Close();
}

TL_ERR TagFile::ReopenFile()
{
  TL_ERR err;
  if (fr == NULL)
    return TL_ERR_FILE_NOT_EXIST;

  err = fr->Reopen();
  if (err)
    return err;
  err = fr->Unmodified();
  if (err)
  {
    Reset();
    fr->AckNewTime();
  }
  return TL_ERR_OK;
}

void TagFile::Reset()
{
  LookupTree.root = NULL;
  LookupTree.list = NULL;
  DescAllocator.Reset();
  TagStrAllocator.Reset();
}


TfPageDesc *TagFile::AllocDesc(const TagStrRef *Tag)
{
  TfPageDesc *NewDesc;
  char *NewTagStr;

  NewDesc = (TfPageDesc *)DescAllocator.Alloc(sizeof(*NewDesc));
  if (NewDesc == NULL)
    return NULL;
  ::memset(NewDesc, 0, sizeof(*NewDesc));
  NewTagStr = (char *)TagStrAllocator.Alloc(Tag->Size + 1);
  if (NewTagStr == NULL)
  {
    DescAllocator.UndoAlloc(sizeof(*NewDesc));
    return NULL;
  }
  ::memcpy(NewTagStr, Tag->Str, Tag->Size);
  NewTagStr[Tag->Size] = '\0';
  NewDesc->TagStr = NewTagStr;
  return NewDesc;
}

TL_ERR TagFile::TestSort(tf_int_t Offset, avl_loc_t *loc) const
{
  avl_node_t *node;
  TfPageDesc *Desc;

  node = avl_get_prev_node(&LookupTree, loc);
  if (node != NULL)
  {
    Desc = AVL_CONTREC(node, TfPageDesc, Node);
    if (Desc->Offset + Desc->Size > Offset)
      return TL_ERR_SORT;
  }
  node = avl_get_next_node(loc);
  if (node != NULL)
  {
    Desc = AVL_CONTREC(node, TfPageDesc, Node);
    if (Offset + PageSize > Desc->Offset)
      return TL_ERR_SORT;
  }
  return TL_ERR_OK;
}

TfPageDesc *TagFile::AddNewPageToTree(tf_int_t Offset)
{
  TL_ERR err;
  avl_loc_t loc;
  avl_node_t *node;
  TfPageDesc *Desc;
  uint32_t ReadSize = PageSize;
  uint32_t NewDescSize;
  ReaderBuff Rb;

  if (Offset + ReadSize > fr->FileSize)
    ReadSize = (uint32_t)(fr->FileSize - Offset);

  NewDescSize = ReadSize;
  /* Usually we will perform one iteration. The loop is for the case where the
   * page does not contain a single full line due to very long lines. In that
   * case we grow it backward until we find a page that does */
  for(;;)
  {
    Rb.Init(fr, Offset, ReadSize);
    if (Rb.Buff == NULL)
      return NULL;

    err = Offset > 0 ? Rb.FindFirstFullLine(): Rb.FindNextFullLine();
    if (!err)
      break;
    if (err != TL_ERR_NO_MORE || Offset == 0)
      return NULL;
    ReadSize = PageSize;
    Offset -= PageSize;
    NewDescSize += PageSize;
  }

  TagStrRef Tag = {(char *)Rb.Buff + Rb.LineOffset, Rb.TagSize};
  node = avl_lookup(&LookupTree, &Tag, &loc);
  /* Test if the same Tag is already in the tree - unlikely */
  if (node != NULL)
  {
    Desc = AVL_CONTREC(node, TfPageDesc, Node);
    /* If Desc is before the new page - grow it to contain new page */
    if (Desc->Offset < Offset)
    {
      Desc->Size = Offset + NewDescSize - Desc->Offset;
      return Desc;
    }
    Desc->Offset = Offset;
    Desc->Size = Desc->Offset + Desc->Size - Offset;
    if (Desc->Size < NewDescSize)
      Desc->Size = NewDescSize;
    Desc->TagOffsetInDesc = Rb.LineOffset;
    return Desc;
  }

  err = TestSort(Offset, &loc);
  if (err)
    return NULL;

  Desc = AllocDesc(&Tag);
  if (Desc == NULL)
  {
    err = TL_ERR_MEM_ALLOC;
    return NULL;
  }
  Desc->Offset = Offset;
  Desc->Size = NewDescSize;
  Desc->TagOffsetInDesc = Rb.LineOffset;
  avl_insert(&LookupTree, &loc, &Desc->Node);
  return Desc;
}

/* Starting from the given descriptor's offset start adding the prior pages to
 * the LookupTree. Pages that has the same tag will be merged to the descriptor.
 * Stop when a page with different tag is added or when start of file reached.
 * Return in DescPtr the descriptor with the different tag or if start of file
 * was reached the same descriptor  */
TL_ERR TagFile::GrowDescBackward(TfPageDesc **DescPtr)
{
  TfPageDesc *Desc = *DescPtr;
  TfPageDesc *PrevDesc = Desc;
  tf_int_t Offset = Desc->Offset;
  tf_int_t PrevOffset = 0;
  avl_node_t *prev_node;

  if (Offset == 0)
    return TL_ERR_OK;

  prev_node = avl_prev(&LookupTree, &Desc->Node);
  if (prev_node != NULL)
  {
    PrevDesc = AVL_CONTREC(prev_node, TfPageDesc, Node);
    PrevOffset = PrevDesc->Offset + PrevDesc->Size;
  }

  while (Offset > PrevOffset)
  {
    TfPageDesc *NewDesc;

    NewDesc = AddNewPageToTree(Offset - PageSize);
    if (NewDesc == NULL)
      return TL_ERR_GENERAL;
    /* New descriptor was added and not merged into ours */
    if (NewDesc->Offset + NewDesc->Size <= Offset)
    {
      *DescPtr = NewDesc;
      return TL_ERR_OK;
    }
    Offset = NewDesc->Offset;
  }
  *DescPtr = PrevDesc;
  return TL_ERR_OK;
}


TL_ERR TagFile::Lookup(const char *TagStr, tf_int_t *RangeStart,
  tf_int_t *RangeSize)
{
  TL_ERR err;
  tf_int_t GapBase, GapSize;
  TfPageDesc *D1, *D2;
  avl_node_t *node;
  avl_loc_t loc;
  TagStrRef Tag = {TagStr, (uint32_t)strlen(TagStr)};

  if (fr == NULL)
    return TL_ERR_FILE_NOT_OPEN;

  /* The concept for the lookup is to have 2 descriptors with a gap in the
   * middle: [D1] Gap [D2]
   * D1 <= Tag  -and-  D2 > Tag
   * The binary search eliminates the gap. In every step either D1 or D2 are
   * moved into the middle of the gap. When the gap is gone the tag should be
   * looked up in D1.
   * Note that D2 need not actually be maintained during the search */

  /* Find intial D1 */
  node = avl_lookup(&LookupTree, &Tag, &loc);
  if (node != NULL)
  {
    D1 = AVL_CONTREC(node, TfPageDesc, Node);
  }
  else
  {
    avl_node_t *prev_node = avl_get_prev_node(&LookupTree, &loc);
    D1 = prev_node == NULL ? NULL : AVL_CONTREC(prev_node, TfPageDesc, Node);
  }
  /* Set GapBase to after D1 and select initial 'node' for D2 */
  if (D1 != NULL)
  {
    GapBase = D1->Offset + D1->Size;
    node = D1->Node.next;
  }
  else
  {
    GapBase = 0;
    node = LookupTree.list;
  }
  /* Get D2 and calculate GapSize */
  D2 = node == NULL ? NULL : AVL_CONTREC(node, TfPageDesc, Node);
  GapSize = D2 != NULL ? D2->Offset - GapBase : fr->FileSize - GapBase;

  /* Main binary search loop */
  while (GapSize > 0)
  {
    TfPageDesc *NewDesc;
    tf_int_t GapPageCount, NewPageOffset;
    int res;

    /* Select a page between D1 and D2 */
    GapPageCount = (GapSize + PageSize - 1) / PageSize;
    NewPageOffset = GapBase + (GapPageCount / 2) * PageSize;
    /* Add the new page to the AVL tree, note that the new page may be marged
     * to existing descriptor including D1 */
    NewDesc = AddNewPageToTree(NewPageOffset);
    if (NewDesc == NULL)
      return TL_ERR_GENERAL;
    res = StrCmp(NewDesc->TagStr, Tag.Str);
    if (res <= 0)
    {
      /* Tag is in the range [NewDesc]-[D2]. Move [D1] to [NewDesc] */
      tf_int_t OldBase = GapBase;
      GapBase = NewDesc->Offset + NewDesc->Size;
      GapSize -= GapBase - OldBase;
      D1 = NewDesc;
    }
    else
    {
      /* Tag is in the range [D1]-[NewDesc]. Move [D2] to [NewDesc] */
      GapSize = NewDesc->Offset - GapBase;
    }
  }

  if (D1 == NULL)
  {
    /* Tag is smaller than 1st tag in the file */
    *RangeStart = *RangeSize = 0;
    return TL_ERR_OK;
  }
  if (StrCmp(D1->TagStr, Tag.Str) != 0)
  {
    /* Tag is in D1 - this is the most common scenario */
    *RangeStart = D1->Offset;
    *RangeSize = D1->Size;
    return TL_ERR_OK;
  }
  /* Tag is in the first line of D1. It may also exist in pages before D1.
   * Start adding pages below D1 to the tree until a smaller tag is
   * encountered */
  err = GrowDescBackward(&D1);
  if (err)
    return err;

  *RangeStart = D1 ? D1->Offset : 0;
  *RangeSize = GapBase - *RangeStart;
  return TL_ERR_OK;
}

TfAllocator::TfAllocator(uint32_t in_AllocPageSize, uint32_t in_AllocAlign)
{
  uint32_t AllocGranularity = FileReader::GetSystemAllocGranularity();

  CurrPage = NULL;
  AllocPageSize = in_AllocPageSize;
  AllocAlign = in_AllocAlign;
  assert((AllocAlign & (AllocAlign - 1)) == 0);
  assert(AllocPageSize % AllocAlign == 0);
  HeaderSize = ((uint32_t)sizeof(uint8_t *) + AllocAlign - 1) & ~(AllocAlign - 1);
  assert(HeaderSize < in_AllocPageSize);

  if (AllocPageSize < AllocGranularity)
    AllocPageSize = AllocGranularity;
}

TfAllocator::~TfAllocator()
{
  Reset();
}

void TfAllocator::Reset()
{
  while (CurrPage != NULL)
  {
    uint8_t *TmpPage = CurrPage;
    CurrPage = *(uint8_t **)CurrPage;
    FileReader::FreeMem(TmpPage);
  }
}

void *TfAllocator::Alloc(uint32_t Size)
{
  void *Ptr;

  if (Size + HeaderSize > AllocPageSize)
    return NULL;

  if (AllocOffset + Size > AllocPageSize || CurrPage == NULL)
  {
    uint8_t *NewPage = (uint8_t *)FileReader::AllocateMem(AllocPageSize);
    if (NewPage == NULL)
      return NULL;
    *(uint8_t **)NewPage = CurrPage;
    CurrPage = NewPage;
    AllocOffset = HeaderSize;
  }

  Ptr = CurrPage + AllocOffset;
  AllocOffset = (AllocOffset + Size + AllocAlign - 1) & ~(AllocAlign - 1);
  return Ptr;
}

/* StrSize > 0  - copy this much chars.
 * StrSize == 0 - copy strlen chars.
 * StrSize < 0  - copy min(strlen, -StrSize) chars. */
char *TfAllocator::StrDup(const char *Str, int32_t StrSize)
{
  char *TmpStr;

  if (Str == NULL)
    return NULL;

  if (StrSize <= 0)
  {
    int32_t n = (uint32_t)::strlen(Str);
    StrSize = StrSize == 0 || n <= -StrSize ? n : -StrSize;
  }

  TmpStr = (char *)Alloc(StrSize + 1);
  if (TmpStr == NULL)
    return NULL;
  ::memcpy(TmpStr, Str, StrSize);
  TmpStr[StrSize] = '\0';
  return TmpStr;
}

void TfAllocator::UndoAlloc(uint32_t Size)
{
  uint32_t UndoSize = (Size + AllocAlign - 1) & ~(AllocAlign - 1);
  if (AllocOffset >= UndoSize + HeaderSize)
    AllocOffset -= UndoSize;
}

TagIterator::TagIterator(bool in_MatchPrefix)
{
  TagStr = NULL;
  TagSize = 0;
  LineCount = 0;
  MatchPrefix = in_MatchPrefix;
  MemCmp = ::memcmp;
}

TagIterator::~TagIterator()
{
  Release();
}

/* Implement case insensitive mem compare.
 * It seems that for Windows memicmp will sort '_' before letters as if all
 * letters are lower case while ctags sorts '_' after letters as if all
 * letters are upper case. */
int TagLEET::memicmp(const void *s1, const void *s2, size_t n)
{
  const uint8_t *str1 = (const uint8_t *)s1;
  const uint8_t *str2 = (const uint8_t *)s2;
  size_t i;
  int c1, c2;

  for (i = 0; i < n; i++)
  {
    c1 = toupper(str1[i]);
    c2 = toupper(str2[i]);
    if (c1 != c2)
      return c1 - c2;
  }
  return 0;
}

int TagLEET::strnicmp(const char *str1, const char *str2, size_t n)
{
  size_t i;
  int c1, c2;

  for (i = 0; i < n; i++)
  {
    c1 = toupper(str1[i]);
    c2 = toupper(str2[i]);
    if (c1 != c2)
      return c1 - c2;
    if (c1 == 0)
      break;
  }
  return 0;
}

int TagLEET::stricmp(const char *str1, const char *str2)
{
  size_t i;
  int c1, c2;

  for (i = 0; ; i++)
  {
    c1 = toupper(str1[i]);
    c2 = toupper(str2[i]);
    if (c1 != c2)
      return c1 - c2;
    if (c1 == 0)
      break;
  }
  return 0;
}

TL_ERR TagIterator::Init(TagFile *tf, const char *in_TagStr, uint32_t ReadSize)
{
  TL_ERR err;
  tf_int_t Base = 0;
  tf_int_t Size = 0;

  if (tf->IsCaseInsensitive())
    MemCmp = TagLEET::memicmp;
  TagStr = ::_strdup(in_TagStr);
  err = TagStr == NULL ? TL_ERR_MEM_ALLOC : TL_ERR_OK;
  if (!err)
    err = tf->Lookup(in_TagStr, &Base, &Size);
  if (!err)
  {
    if (ReadSize == 0)
      ReadSize = 16*1024;
    err = rb.Init(tf->GetFileReader(), Base, ReadSize);
  }

  if (err)
  {
    Release();
    return err;
  }

  IsFirstLine = Base != 0 ? true : false;
  TagSize = (uint32_t)strlen(TagStr);
  LineCount = 0;
  return TL_ERR_OK;
}

void TagIterator::Release()
{
  rb.Release();
  if (TagStr != NULL)
  {
    ::free(const_cast<char *>(TagStr));
    TagStr = NULL;
  }
}

TL_ERR TagIterator::GetNextTagLine(const char **Line, uint32_t *LineSize)
{
  TL_ERR err;
  int res;

  for(;;)
  {
    if (IsFirstLine)
    {
      err = rb.FindFirstFullLine(true);
      IsFirstLine = false;
    }
    else
    {
      err = rb.FindNextFullLine(true);
    }
    if (err)
    {
      if (err == TL_ERR_LINE_TOO_BIG)
        continue;
      return err;
    }

    if (rb.TagSize >= TagSize)
    {
      res = MemCmp(rb.Buff + rb.LineOffset, TagStr, TagSize);
      if (res == 0 && rb.TagSize > TagSize)
        res = MatchPrefix ? 0 : 1;
    }
    else
    {
      res = MemCmp(rb.Buff + rb.LineOffset, TagStr, rb.TagSize);
      if (res == 0)
        res = -1;
    }

    if (res < 0)
      continue;
    if (res == 0)
    {
      LineCount++;
      *Line = (char *)rb.Buff + rb.LineOffset;
      *LineSize = rb.LineSize;
      return TL_ERR_OK;
    }
    return TL_ERR_NO_MORE;
  }
}

void find_line_num(const char *ExtStr, uint32_t ExtStrSize, TagLineProperties *Props)
{
  uint32_t start, i;

  start = i = 0;
  for(;;)
  {
    if (i < ExtStrSize && ExtStr[i] != '\t')
    {
      i++;
      continue;
    }

    if (i - start >= 6 && ::memcmp(ExtStr + start, "line:", 5) == 0)
    {
      Props->ExtLine = ExtStr + start + 5;
      Props->ExtLineSize = i - 5;

      Props->ExtFieldsSize -= i;
      if ( Props->ExtFieldsSize <= 0 )
      {
          Props->ExtFields = "";
          Props->ExtFieldsSize = 0;
      }
      else
          Props->ExtFields = ExtStr + start + i;

      break;
    }
    else if (i == ExtStrSize)
    {
      break;
    }
    else
    {
      start = i + 1;
      i = start;
      continue;
    }
  }
  return;    
}

static TagKind get_tag_kind(const char *ExtStr, uint32_t ExtStrSize, TagLineProperties *Props)
{
  uint32_t start, i;
  char kind_char;

  start = i = 0;
  for(;;)
  {
    if (i < ExtStrSize && ExtStr[i] != '\t')
    {
      i++;
      continue;
    }

    if (i - start >= 1)
    {
      int header = 0;
      if (i - start >= 6 && ::memcmp(ExtStr + start, "kind:", 5) == 0)
          header = 5;

      kind_char = ExtStr[start + header];
      Props->ExtKind = ExtStr + start + header;
      Props->ExtKindSize = i - start - header;
      Props->ExtFieldsSize -= (i - start);
      if ( Props->ExtFieldsSize <= 0 )
      {
          Props->ExtFields = "";
          Props->ExtFieldsSize = 0;
      }
      else
          Props->ExtFields = ExtStr + start + (i - start);
    }
    else if (i == ExtStrSize)
    {
      break;
    }
    else
    {
      start = i + 1;
      i = start;
      continue;
    }

    switch (kind_char)
    {
      case 'F': return TAG_KIND_FILE;
      case 'c': return TAG_KIND_CLASSES;
      case 'd': return TAG_KIND_MACRO_DEF;
      case 'e': return TAG_KIND_ENUM_VAL;
      case 'f': return TAG_KIND_FUNCTION_DEF;
      case 'g': return TAG_KIND_ENUM_NAME;
      case 'l': return TAG_KIND_LOCAL_VAR;
      case 'm': return TAG_KIND_MEMBER;
      case 'n': return TAG_KIND_NAMESPACE;
      case 'p': return TAG_KIND_FUNCTION_PROTO;
      case 's': return TAG_KIND_STRUCT_NAME;
      case 't': return TAG_KIND_TYPEDEF;
      case 'u': return TAG_KIND_UNION_NAME;
      case 'v': return TAG_KIND_VAR_DEF;
      case 'x': return TAG_KIND_EXTERNAL;
      default:
        return TAG_KIND_UNKNOWN;
    }
  }
  return TAG_KIND_UNKNOWN;
}

TL_ERR TagIterator::GetNextTagLineProps(TagLineProperties *Props)
{
  TL_ERR err;
  const char *Line;
  uint32_t LineSize;

  err = GetNextTagLine(&Line, &LineSize);
  if (!err)
    err = GetTagLineProps(Props);
  return err;
}

TL_ERR TagIterator::GetTagLineProps(TagLineProperties *Props) const
{
  const char *Line = (char *)rb.Buff + rb.LineOffset;
  uint32_t i = rb.TagSize + 1;
  uint32_t start;
  bool HasExtField = false;

  start = i;
  while (i < rb.LineSize && Line[i] != '\t')
    i++;

  Props->Tag = (char *)rb.Buff + rb.LineOffset;
  Props->TagSize = rb.TagSize;
  Props->FileName = Line + start;
  Props->FileNameSize = i - start;

  start = ++i;
  if (i == rb.LineSize)
    return TL_ERR_GENERAL;

  while (i < rb.LineSize)
  {
    if (Line[i] == ';' && i + 2 < rb.LineSize && Line[i+1] == '"' &&
      Line[i+2] == '\t')
    {
      HasExtField = true;
      break;
    }
    i++;
  }
  Props->ExCmd = Line + start;
  Props->ExCmdSize = i - start;
  if (HasExtField && i + 3 < rb.LineSize)
  {
    start = i + 3;
    Props->ExtFields = Line + start;
    Props->ExtFieldsSize = rb.LineSize - start;
  }
  else
  {
    Props->ExtFields = NULL;
    Props->ExtFieldsSize = 0;
  }

  // These are be parsed in the following function calls
  Props->ExtKind = " ";
  Props->ExtKindSize = 1;
  Props->ExtLine = " ";
  Props->ExtLineSize = 1;

  Props->Kind = get_tag_kind(Props->ExtFields, Props->ExtFieldsSize, Props);

  if ( Props->ExtFieldsSize > 0 )
    find_line_num(Props->ExtFields, Props->ExtFieldsSize, Props);

  return TL_ERR_OK;
}
