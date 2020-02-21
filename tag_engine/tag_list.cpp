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

#include "tag_list.h"

#include <string.h>
#include <malloc.h>

using namespace TagLEET;

FileReaderLineIterator::FileReaderLineIterator(FileReader *fr)
{
  InitErr = rb.Init(fr, 0, 128*1024);
}

TL_ERR FileReaderLineIterator::HandleNewLine(TL_ERR err)
{
  if (!err)
  {
    Line = (const char *)rb.Buff + rb.LineOffset;
    LineSize = rb.LineSize;
    return TL_ERR_OK;
  }

  Line = NULL;
  LineSize = 0;
  return err;
}

TL_ERR FileReaderLineIterator::MoveToFirstLine()
{
  TL_ERR err = InitErr ? InitErr : rb.FindFirstFullLine(true);
  return HandleNewLine(err);
}


TL_ERR FileReaderLineIterator::MoveToNextLine()
{
  TL_ERR err = InitErr ? InitErr : rb.FindNextFullLine(true);
  return HandleNewLine(err);
}

TagList::TagList():
  NodeMem(4*1024, sizeof(void *)),
  StrMem(8*1024, 1)
{
  List = NULL;
  Count = 0;
  LineNumFromTag = 0;
  TagForLineNum = NULL;
  TagsFilePath = NULL;
}

TagList::~TagList()
{
}

TL_ERR TagList::Create(const char *Tag, const char *in_TagsFilePath,
  TagFile *cache, bool DoPrefixMatch, uint32_t MaxItemCount)
{
  TL_ERR err;
  TagFile tf;
  TagIterator itr(DoPrefixMatch);
  TagListItem **NextItem = &List;

  NodeMem.Reset();
  StrMem.Reset();
  List = NULL;
  Count = 0;
  LineNumFromTag = 0;
  TagForLineNum = NULL;
  TagsFilePath = StrMem.StrDup(in_TagsFilePath);
  if (TagsFilePath == NULL)
    return TL_ERR_MEM_ALLOC;

  if (cache == NULL)
  {
    err = tf.Init(TagsFilePath);
    if (err)
      return err;
    cache = &tf;
  }
  err = itr.Init(cache, Tag, 128*1024);
  if (err)
    return err;

  TagsCaseInsensitive = cache->IsCaseInsensitive();

  while (Count < MaxItemCount)
  {
   uint32_t ExCmdCopySize, ExtTypeCopySize, ExtLineCopySize, ExtFieldsCopySize;
    // uint32_t ExCmdCopySize, ExtFieldsCopySize;
    TagLineProperties Props;
    TagListItem *NewItem;

    err = itr.GetNextTagLineProps(&Props);
    if (err)
      break;

    ExCmdCopySize = Props.ExCmdSize;
    if (Props.ExCmdSize > 1024)
    {
      /* Trim ExCmd but don't let it end with the special char '\\' */
      ExCmdCopySize = 1024;
      while (ExCmdCopySize > 0 && Props.ExCmd[ExCmdCopySize - 1] == '\\')
        ExCmdCopySize--;
    }

    ExtTypeCopySize = Props.ExtTypeSize;
    if (Props.ExtTypeSize > 1024)
    {
      /* Trim ExCmd but don't let it end with the special char '\\' */
     ExtTypeCopySize = 1024;
      while (ExtTypeCopySize > 0 && Props.ExtType[ExtTypeCopySize - 1] == '\\')
        ExtTypeCopySize--;
    }
    
    ExtLineCopySize = Props.ExtLineSize;
    if (Props.ExtLineSize > 1024)
    {
      /* Trim ExCmd but don't let it end with the special char '\\' */
      ExtLineCopySize = 1024;
      while (ExtLineCopySize > 0 && Props.ExtLine[ExtLineCopySize - 1] == '\\')
        ExtLineCopySize--;
    }

    ExtFieldsCopySize = Props.ExtFieldsSize;
    if (Props.ExtFieldsSize > 1024)
    {
      /* Trim ExCmd but don't let it end with the special char '\\' */
      ExtFieldsCopySize = 1024;
      while (ExtFieldsCopySize > 0 && Props.ExtFields[ExtFieldsCopySize - 1] == '\\')
        ExtFieldsCopySize--;
    }

    NewItem = (TagListItem *)NodeMem.Alloc(sizeof(*NewItem));
    if (NewItem == NULL)
      break;

    NewItem->Tag = StrMem.StrDup(Props.Tag, Props.TagSize);
    NewItem->FileName = StrMem.StrDup(Props.FileName, Props.FileNameSize);
    NewItem->ExCmd = StrMem.StrDup(Props.ExCmd, ExCmdCopySize);
    NewItem->ExtType = StrMem.StrDup(Props.ExtType, ExtTypeCopySize);
    NewItem->ExtLine = StrMem.StrDup(Props.ExtLine, ExtLineCopySize);
    NewItem->ExtFields = StrMem.StrDup(Props.ExtFields, ExtFieldsCopySize);
    if (NewItem->Tag == NULL || NewItem->FileName == NULL || 
      NewItem->ExCmd == NULL || NewItem->ExtFields == NULL
     || NewItem->ExtType == NULL ||
     NewItem->ExtLine == NULL || NewItem->ExtFields == NULL)
      break;

    NewItem->Kind = Props.Kind;
    NewItem->Next = NULL;
    *NextItem = NewItem;
    NextItem = &NewItem->Next;
    Count++;
  }

  return TL_ERR_OK;
}

/* If ExCmd contains \\ or \/ make a copy with \ or / */
const char *TagList::FixExCmd(const char *ExCmd)
{
  static const char BSlashOrNULL[8] = {0, 0, 0, 0, '\\', 0, 0, 0};
  static const char BSlashOrSlesh[2] = {'\\', '/'};
  char *NewStr;
  uint32_t  i, dst;

  for (i = 0; ExCmd[i] != BSlashOrNULL[ExCmd[i]&7]; i++);
  if (ExCmd[i] == '\0')
    return ExCmd;

  NewStr = (char *)StrMem.Alloc((uint32_t)::strlen(ExCmd) + 1);
  if (NewStr == NULL)
    return ExCmd;

  for (i = dst = 0; ; NewStr[dst++] = ExCmd[i++])
  {
    if (ExCmd[i] != BSlashOrNULL[ExCmd[i]&7])
      continue;
    if (ExCmd[i] == '\0')
      break;
    if (ExCmd[i+1] == BSlashOrSlesh[ExCmd[i+1] & 1])
      i++;
  }
  NewStr[dst] = '\0';
  return NewStr;
}

static bool IsLetter(char Ch)
{
  return (Ch >= 'a' && Ch <= 'z') || (Ch >= 'A' && Ch <= 'Z');
}

static bool IsAbsolutePath(const char *Path)
{
  if (Path[0] == '/' || Path[0] == '\\')
    return true;
  if (IsLetter(Path[0]))
  {
    if (Path[1] == ':' && Path[2] == '\\')
      return true;
  }
  return false;
}

/* For [Prefix]/[Letter]/Path
 * 1. Cygwin path: /cygdrive/[letter]/path
 * 2. MinGW path: /[letter]/path
 */
static TL_ERR TestUnixStylePrefix(const char *Path, uint32_t n, char *Buff,
  uint32_t BuffSize, const char *Prefix, uint32_t PrefixSize)
{
  char DriveLetter;

  /* We need to remove 'Prefix/X/' and add 'X:\' + '\0' */
  if (n + 1 - PrefixSize > BuffSize)
    return TL_ERR_TOO_BIG;

  /* Cygwin path: /cygdrive/[letter]/path */
  if (n <= PrefixSize + 3)
    return TL_ERR_TOO_BIG;
  if (Path[PrefixSize] != '/' || Path[PrefixSize+2] != '/')
    return TL_ERR_INVALID;
  if (PrefixSize > 0 && ::memcmp(Path, Prefix, PrefixSize) != 0)
    return TL_ERR_INVALID;

  DriveLetter = Path[PrefixSize+1];
  if (!IsLetter(DriveLetter))
    return TL_ERR_INVALID;

  Buff[0] = DriveLetter;
  Buff[1] = ':';
  Buff[2] = '\\';
  /* Copy the remaining path with the '\0' */
  PrefixSize += 3;
  memcpy(Buff + 3, Path + PrefixSize, n + 1 - PrefixSize);
  return TL_ERR_OK;
}

static TL_ERR TryConvertSpecialAbsPath(const char *Path, uint32_t n, char *Buff,
  uint32_t BuffSize)
{
  static char CygwinPrefix[] = "/cygdrive";
  TL_ERR err;

  err = TestUnixStylePrefix(Path, n, Buff, BuffSize, CygwinPrefix,
    sizeof(CygwinPrefix) - 1);
  if (!err)
    return TL_ERR_OK;
  /* MinGW style */
  err = TestUnixStylePrefix(Path, n, Buff, BuffSize, NULL, 0);
  if (!err)
    return TL_ERR_OK;

  return TL_ERR_NOT_EXIST;
}

/* Try to locate a source file of a tag and open it for reading */
TL_ERR TagList::OpenSrcFile(
  IN  const TagListItem *Item,
  OUT FileReader *fr,
  OUT char *SrcFilePathBuff,
  IN  uint32_t BuffSize)
{
  uint32_t i, n;
  bool IsAbs = IsAbsolutePath(Item->FileName);
  TL_ERR err;

  n = (uint32_t)::strlen(Item->FileName);
  if (IsAbs)
  {
    if (n + 1 > BuffSize)
      return TL_ERR_TOO_BIG;
    ::memcpy(SrcFilePathBuff, Item->FileName, n + 1);
    err = fr->Open(SrcFilePathBuff);
    if (!err)
      return TL_ERR_OK;
    err = TryConvertSpecialAbsPath(Item->FileName, n, SrcFilePathBuff, BuffSize);
    return err ? err : fr->Open(SrcFilePathBuff);
  }

  if (TagsFilePath == NULL)
    return TL_ERR_GENERAL;

  i = (uint32_t)::strlen(TagsFilePath);
  while (i > 0)
  {
    i--;
    if (TagsFilePath[i] != '\\' && TagsFilePath[i] != '/')
      continue;
    i++;
    break;
  }
  if (i + n + 1 > BuffSize)
    return TL_ERR_TOO_BIG;

  ::memcpy(SrcFilePathBuff, TagsFilePath, i);
  ::memcpy(SrcFilePathBuff + i, Item->FileName, n + 1);
  return fr->Open(SrcFilePathBuff);
}

static TL_ERR IsLineNumber(const char *ExCmd, uint32_t *LineNumber)
{
  uint32_t i = 0;
  uint32_t n = 0;

  while (ExCmd[i] >= '0' && ExCmd[i] <= '9')
  {
    n *= 10;
    n += ExCmd[i] - '0';
    i++;
  }

  if (i > 0 && ExCmd[i] == '\0')
  {
    *LineNumber = n;
    return TL_ERR_OK;
  }
  return TL_ERR_INVALID;
}

uint32_t TagList::FindTagInExCmd(const char *Tag, const char *ExCmd)
{
  uint32_t ExCmdSize = (uint32_t)::strlen(ExCmd);
  uint32_t TagSize = (uint32_t)::strlen(Tag);
  int (*MemCmp)(const void *s1, const void *s2, size_t n);
  uint32_t i;

  MemCmp = TagsCaseInsensitive ? TagLEET::memicmp : ::memcmp;

  for (i = 0; i + TagSize <= ExCmdSize; i++)
  {
    if (ExCmd[i] != Tag[0])
      continue;
    if (MemCmp(ExCmd + i, Tag, TagSize) == 0)
      return i + TagSize;
  }
  return 0;
}

TL_ERR TagList::FindLineNumberInFile(
  IN  LineIterator *li,
  IN  const TagListItem *Item,
  OUT uint32_t *out_LineNumber)
{
  TL_ERR err;
  const char *ExCmd;
  uint32_t LineNumber;
  uint32_t PrefixSize;
  int (*StrCmp)(const char *s1, const char *s2);

  StrCmp = TagsCaseInsensitive ? TagLEET::stricmp : ::strcmp;
  if (Item->Kind == TAG_KIND_FILE && LineNumFromTag > 0 &&
    TagForLineNum != NULL && StrCmp(TagForLineNum, Item->Tag) == 0)
  {
    *out_LineNumber = LineNumFromTag - 1;
    return TL_ERR_OK;
  }

  err = ::IsLineNumber(Item->ExCmd, &LineNumber);
  if (!err && LineNumber >= 1)
  {
    *out_LineNumber = LineNumber - 1;
    return TL_ERR_OK;
  }

  ExCmd = FixExCmd(Item->ExCmd);
  err = DoFindLineNumberInFile(li, ExCmd, out_LineNumber);
  if (!err)
    return err;
  /* Perhaps line changed slightly. Find tag in ExCmd and then re-search file
   * For the prefix of ExCmd up until the tag */
  PrefixSize = FindTagInExCmd(Item->Tag, ExCmd);
  if (PrefixSize > 0)
  {
    char TmpBuff[256];
    char *ExCmdPrefix;

    ExCmdPrefix = PrefixSize + 1 < sizeof(TmpBuff) ? TmpBuff :
      (char *)::malloc(PrefixSize + 1);
    if (ExCmdPrefix != NULL)
    {
      ::memcpy(ExCmdPrefix, ExCmd, PrefixSize);
      ExCmdPrefix[PrefixSize] = '\0';
      err = DoFindLineNumberInFile(li, ExCmdPrefix, out_LineNumber);
      if (ExCmdPrefix != TmpBuff)
        ::free(ExCmdPrefix);
      if (!err)
        return err;
    }
  }
  /* As last resort just search for the tag itself */
  return DoFindLineNumberInFile(li, Item->Tag, out_LineNumber);
}

TL_ERR TagList::DoFindLineNumberInFile(
  IN  LineIterator *li,
  IN  const char *ExCmd,
  OUT uint32_t *out_LineNumber)
{
  TL_ERR err;
  uint32_t LineNumber;

  err = li->MoveToFirstLine();
  if (err)
    return err;

  bool FromStart = false;
  bool ToEnd = false;
  uint32_t ExCmdSize = (uint32_t)::strlen(ExCmd);
  if (ExCmdSize >= 2 && ExCmd[0] == '/' && ExCmd[1] == '^')
  {
    ExCmd += 2;
    ExCmdSize -= 2;
    FromStart = true;
  }
  if (ExCmdSize >= 2 && ExCmd[ExCmdSize-1] == '/' && ExCmd[ExCmdSize-2] == '$')
  {
    ExCmdSize -= 2;
    ToEnd = true;
  }
  if (ExCmdSize == 0)
    return TL_ERR_INVALID;

  for (LineNumber = 1;; LineNumber++)
  {
    const char *Line = (const char *)li->Line;
    uint32_t LineSize = li->LineSize;
    if (LineSize < ExCmdSize)
    {
      /* Do nothing */
    }
    else if (FromStart)
    {
      if (ToEnd)
      {
        if (LineSize == ExCmdSize && ::memcmp(Line, ExCmd, ExCmdSize) == 0)
          break;
      }
      else
      {
        if (LineSize >= ExCmdSize && ::memcmp(Line, ExCmd, ExCmdSize) == 0)
          break;
      }
    }
    else /* not FromStart */
    {
      if (ToEnd)
      {
        if (LineSize >= ExCmdSize &&
          ::memcmp(Line + LineSize - ExCmdSize, ExCmd, ExCmdSize) == 0)
        {
          break;
        }
      }
      else /* Substring match */
      {
        uint32_t  i;
        for (i = 0; i + ExCmdSize <= LineSize; i++)
        {
          if (Line[i] != ExCmd[0])
            continue;
          if (::memcmp(Line + i, ExCmd, ExCmdSize) == 0)
            break;
        }
        if (i + ExCmdSize <= LineSize)
          break;
      }
    }

    err = li->MoveToNextLine();
    if (err && err != TL_ERR_LINE_TOO_BIG)
      return err;
  }
  *out_LineNumber = LineNumber;
  return TL_ERR_OK;;
}

void TagList::SetTagAndLine(const char *Tag, int TagSize, int LineNum)
{
  TagForLineNum = StrMem.StrDup(Tag, TagSize);
  LineNumFromTag = LineNum;
}

