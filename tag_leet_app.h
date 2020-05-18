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

#ifndef TAG_LEET_APP_H
#define TAG_LEET_APP_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "tag_engine/tl_types.h"
#include "tag_engine/tag_list.h"

struct NppData;

namespace TagLEET_NPP {
using namespace TagLEET;

class TagLeetForm;
class NppCallContext;

int TSTR_to_str(const TCHAR *Str, int StrSize, char *Buff, int BuffSize);
int str_to_TSTR(const char *Str, int StrSize, TCHAR *Buff, int BuffSize);

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))
#define IS_EOL_CHAR(c) \
  (TagLEET_NPP::TagLeetApp::TestEolArr[(uint8_t)(c) & 1] == (uint8_t)(c))
#define TL_MAX_PATH 512

struct NppLoc {
  const TCHAR *FileName;
  struct {
    uint32_t LineNum, XPos;
    int32_t TopVisibleLine;
  } I; /* This holds vars that can be simply copied */
  TCHAR FnStorage[sizeof(void *)];
};

class NppLocStack {
public:
  NppLocStack();
  ~NppLocStack();
  TL_ERR Push(const NppLoc *Loc);
  NppLoc *Pop();
  void Clear();

private:
  NppLoc *Arr[16];
  int Index;
  int Count;
};

class NppLocBank {
public:
  NppLocBank() {};
  ~NppLocBank() {};
  TL_ERR PushLocToBack(const NppLoc *Loc);
  NppLoc *PopLocFromBack(const NppLoc *Loc);
  NppLoc *PopLocFromForward(const NppLoc *Loc);
private:
  NppLocStack Back;
  NppLocStack Forward;
};

// Need to move this earlier as TagLeetApp needs this class for 2 ported functions:
// PopulateTagList(), PopulateTagListHelper()
class TagLookupContext
{
public:
  TagLookupContext(NppCallContext *in_NppC, const char *in_TagsFilePath);
  ~TagLookupContext() {};
  void GetLineNumFromTag(bool PrefixMatch, TagList *tl) const;

  NppCallContext *NppC;
  const char *TagsFilePath;
  char TextBuff[512];
  int TextLength;
  int TagOffset;
  int TagLength;
  int LineNum;
  int LineStartPos;
  int LineEndPos;
  int TextStartPos;
};

class TagLeetApp
{
public:
  TagLeetApp(const struct NppData *NppDataObj);
  virtual ~TagLeetApp();

  void InitHandles(const struct NppData *NppDataObj);

  void LookupTag();
  void GoBack();
  void GoForward();
  void AutoComplete();
  void UpdateTagDb();
  void Lock();
  void Unlock();
  TL_ERR GoToFileLine(const NppLoc *Loc, const char *Tag = NULL);
  HINSTANCE GetInstance() const { return InstanceHndl; }
  void DetachForm(TagLeetForm *Form, bool *DestroyApp);
  void ShowAbout() const;
  HWND getCurrScintilla();
  HFONT GetStatusFont() const { return StatusFont; }
  HFONT GetListViewFont() const { return ListViewFont; }
  int GetStatusHeight() const { return StatusHeight; }
  void Shutdown();
  TL_ERR GetTagsFilePath(NppCallContext *NppC, char *TagFileBuff, int BuffSize);

  void SetFormSize(unsigned int Width, unsigned int Height, bool reset);
  void GetFormSize(unsigned int *Width, unsigned int *Height);
  void UpdateFormScale(int change);

  HFONT UpdateListViewFont(int change, bool reset);

  static void SetInstance(HINSTANCE in_InstanceHndl);

private:
  void LastTagFileSet(const TCHAR *TagsFileName);
  TL_ERR LastTagFileGet(TCHAR *FnBuff, int FnBuffCount) const;
  TL_ERR SelectTagInLine(NppCallContext *NppC, uint32_t LineNum,
    const char *Tag);
  int GetScreenHeight();

  TL_ERR PopulateTagListHelper(TagLookupContext *TLCtx, TagFile *tf);
  TL_ERR PopulateTagList(TagLookupContext *TLCtx);

  HFONT CreateSpecificFont(const TCHAR **FontList, int FontListSize,
    int Height);
  HFONT CreateStatusFont();
  HFONT CreateListViewFont();

  TagList TList;
  bool DoPrefixMatch;

  static HINSTANCE InstanceHndl;
  CRITICAL_SECTION CritSec;
  NppLocBank LocBank;
  HWND NppHndl;
  HWND SciMainHndl;
  HWND SciSecHndl;
  TagLeetForm *Form;
  unsigned int FormScale;
  unsigned int FormWidth;
  unsigned int FormHeight;  
  bool HeightWidthValid;
  int StatusHeight;
  HFONT StatusFont;
  unsigned int ListViewFontHeight;
  unsigned int DefaultListViewFontHeight;
  HFONT ListViewFont;
  bool DestroyOnDetachForm;
  TCHAR LastTagFile[TL_MAX_PATH];
public:
  static const TCHAR WindowClassName[];
  static const uint8_t TestEolArr[2];

  friend class NppCallContext;
};

class NppCallContext
{
public:
  NppCallContext(TagLeetApp *in_App);
  ~NppCallContext() {};
  void ReInit();
  void CalcFormPos(POINT *pt, int width, int height);
  LRESULT SciMsg(int Msg, WPARAM wParam = 0, LPARAM lParam = 0);
  LRESULT NppMsg(int Msg, WPARAM wParam = 0, LPARAM lParam = 0);
  void UpdateSelection(int Start, int End);

  void *SciDirectFuncPtr;
  void *SciDirectPtr;

  TagLeetApp *App;
  int SciEditIndex;
  HWND SciHndl;
  NppLocBank *LocBank;
  NppLoc Loc;
  TCHAR Path[TL_MAX_PATH];
};

class NppFileLineIterator : public LineIterator
{
public:
  NppFileLineIterator(TagLeetApp *App, const char *FileName);
  ~NppFileLineIterator();

  TL_ERR MoveToFirstLine();
  TL_ERR MoveToNextLine();

private:
  TL_ERR MoveToLine(int LineNum);

  NppCallContext NppC;
  TL_ERR InitErr;
  char LocalBuff[1024];
  char *LineBuff;
  int LineBuffSize;
  int CurrLineNum;
};


class TlAppSync
{
public:
  TlAppSync(TagLeetApp *in_Obj);
  ~TlAppSync();
  void Unlock();
  void Lock();
private:
  int LockRefCount;
  TagLeetApp *Obj;
};

} /* namespace TagLEET_NPP */

#endif /* TAG_LEET_APP_H */

