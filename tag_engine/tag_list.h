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

#ifndef _FILE_LIST_H_
#define _FILE_LIST_H_

#include "tag_file.h"
#include <stddef.h>

namespace TagLEET {

class LineIterator
{
public:
  LineIterator():Line(NULL),LineSize(0) {}
  virtual ~LineIterator() {};

  virtual TL_ERR MoveToFirstLine() = 0;
  virtual TL_ERR MoveToNextLine() = 0;

  const char *Line;
  uint32_t LineSize;
};

class FileReaderLineIterator : public LineIterator
{
public:
  FileReaderLineIterator(FileReader *fr);
  ~FileReaderLineIterator() {};

  TL_ERR MoveToFirstLine();
  TL_ERR MoveToNextLine();

private:
  TL_ERR HandleNewLine(TL_ERR err);

  ReaderBuff rb;
  TL_ERR InitErr;
};

class TagList
{
public:
  TagList();
  ~TagList();

  TL_ERR Create(const char *Tag, const char *in_TagsFilePath,
    TagFile *cache = NULL, bool DoPrefixMatch = false,
    uint32_t MaxItemCount = 200);

  struct TagListItem
  {
    const char *Tag;
    const char *FileName;
    const char *ExCmd;
    const char *ExtType;
    const char *ExtLine;
    const char *ExtFields;
    TagKind Kind;
    TagListItem *Next;
  };

  TL_ERR OpenSrcFile(
    IN  const TagListItem *Item,
    OUT FileReader *fr,
    OUT char *SrcFilePathBuff,
    IN  uint32_t BuffSize);
  TL_ERR FindLineNumberInFile(
    IN  LineIterator *li,
    IN  const TagListItem *Item,
    OUT uint32_t *out_LineNumber);
  void SetTagAndLine(const char *Tag, int TagSize, int LineNum);

public:
  TagListItem *List;
  uint32_t Count;
  uint32_t LineNumFromTag;
  const char *TagForLineNum;
  const char *TagsFilePath;
  bool TagsCaseInsensitive;

private:
  TL_ERR DoFindLineNumberInFile(
    IN  LineIterator *li,
    IN  const char *ExCmd,
    OUT uint32_t *out_LineNumber);
  const char *FixExCmd(const char *ExCmd);
  uint32_t FindTagInExCmd(const char *Tag, const char *ExCmd);

  TfAllocator NodeMem;
  TfAllocator StrMem;
};

} /* namespace TagLEET */

#endif /* _FILE_LIST_H_ */
