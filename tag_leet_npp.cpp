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

#include "tag_leet_app.h"

#include <tchar.h>
#include <stdio.h>
#include <malloc.h>

#include "Notepad_plus_msgs.h"
#include "Scintilla.h"
#include "SciLexer.h"
#include "PluginInterface.h"

using namespace TagLEET_NPP;

static TagLeetApp *TheApp = NULL;
HINSTANCE TagLeetApp::InstanceHndl = NULL;

static void NppLookupTag();
static void NppFindRefs();
static void NppGoBack();
static void NppGoForward();
static void NppAutoComplete();
static void NppDeleteTags();
static void TagLeetSettings();
static void TagLeetAbout();

extern bool g_useSciAutoC;

BOOL APIENTRY DllMain( HINSTANCE hModule, DWORD Reason, LPVOID lpReserved)
{
  switch (Reason)
  {
    case DLL_PROCESS_ATTACH:
      TagLeetApp::SetInstance(hModule);
      break;
    case DLL_PROCESS_DETACH:
      /* Process is terminating. Don't bother to free resources */
      if (lpReserved != NULL)
        break;
      /* FreeLibrary. The process may resume so free unneeded resources */
      if (TheApp != NULL)
      {
        TagLeetApp *App = TheApp;
        TheApp = NULL;
        App->Shutdown();
      }
  }

  return TRUE;
}

/*
 *--------------------------------------------------
 * The 4 extern functions are mandatory
 * They will be called by Notepad++ plugins system
 *--------------------------------------------------
*/

extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData)
{
  if (TheApp == NULL)
    TheApp = new TagLeetApp(&notpadPlusData);
}

extern "C" __declspec(dllexport) const TCHAR * getName()
{
  return _T("TagLEET");
}

extern "C" __declspec(dllexport) FuncItem * getFuncsArray(int *nbF)
{
  static ShortcutKey TagLeetShortcuts[] = {
    {false, true, false, VK_SPACE},
    {false, true, false, VK_LEFT},
    {false, true, false, VK_RIGHT}};
  static struct FuncItem TagLeetFuncs[] = {
    {_T("&Lookup Tag"),      NppLookupTag,    0, false, TagLeetShortcuts},
    {_T("Find &References"), NppFindRefs,     0, false, NULL},
    {_T("&Back"),            NppGoBack,       0, false, TagLeetShortcuts + 1},
    {_T("&Forward"),         NppGoForward,    0, false, TagLeetShortcuts + 2},
    {_T("&Autocomplete"),    NppAutoComplete, 0, false, NULL},
    {_T("&Delete Tags file"),NppDeleteTags,   0, false, NULL},
    {_T("-SEPARATOR-"),      NULL,            0, false, NULL},
    {_T("&Settings"),        TagLeetSettings, 0, false, NULL},
    {_T("Abou&t"),           TagLeetAbout,    0, false, NULL}};

  *nbF = ARRAY_SIZE(TagLeetFuncs);
  return TagLeetFuncs;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification *notifyCode)
{
  switch (notifyCode->nmhdr.code)
  {
    case NPPN_FILESAVED:
      if (TheApp != NULL)
        TheApp->UpdateTagDb();
      break;
 
    case SCN_CHARADDED:
      if (g_useSciAutoC)
        TheApp->SciAutoComplete();

    default:
      return;
  }
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM)
{
  return TRUE;
}

extern "C" __declspec(dllexport) BOOL isUnicode() {
  return TRUE;
}

static void NppLookupTag()
{
  if (TheApp != NULL)
    TheApp->LookupTag();
}

static void NppFindRefs()
{
  if (TheApp != NULL)
    TheApp->FindRefs();
}

static void NppGoForward()
{
  if (TheApp != NULL)
    TheApp->GoForward();
}

static void NppGoBack()
{
  if (TheApp != NULL)
    TheApp->GoBack();
}

static void NppAutoComplete()
{
  if (TheApp != NULL)
    TheApp->AutoComplete();
}

static void NppDeleteTags()
{
  if (TheApp != NULL)
    TheApp->DeleteTags();
}

void TagLeetSettings()
{
  if (TheApp != NULL)
    TheApp->ShowSettings();
}

void TagLeetAbout()
{
  if (TheApp != NULL)
    TheApp->ShowAbout();
}

NppCallContext::NppCallContext(TagLeetApp *in_App)
{
  App = in_App;
  ReInit();
}

typedef sptr_t (*sci_fn_direct_t)(void *ptr, unsigned int iMessage,
  uptr_t wParam, sptr_t lParam);

/* Should be called after an operation that may change NPP current scintilla */
void NppCallContext::ReInit()
{
  int Pos, LineStartPos;

  SciDirectFuncPtr = NULL;
  SciDirectPtr = NULL;
  SciEditIndex = 0;
  ::SendMessage(App->NppHndl, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&SciEditIndex);
  if (SciEditIndex == 0)
  {
    SciHndl = App->SciMainHndl;
  }
  else
  {
    SciHndl = App->SciSecHndl;
  }
  LocBank = &App->LocBank;

  SciDirectPtr = (char *)0 + SciMsg(SCI_GETDIRECTPOINTER);
  if (SciDirectPtr != NULL)
  {
    SciDirectFuncPtr = (char *)0 + SciMsg(SCI_GETDIRECTFUNCTION);
  }

  Pos = (int)SciMsg(SCI_GETSELECTIONSTART);
  Loc.I.LineNum = (uint32_t)SciMsg(SCI_LINEFROMPOSITION, Pos);
  Loc.I.TopVisibleLine = (int32_t)SciMsg(SCI_GETFIRSTVISIBLELINE, Pos);
  LineStartPos = (int)SciMsg(SCI_POSITIONFROMLINE, Loc.I.LineNum);
  Loc.I.XPos = Pos > LineStartPos ? Pos - LineStartPos : 0;
  NppMsg(NPPM_GETFULLCURRENTPATH, ARRAY_SIZE(Path), (LPARAM)Path);
  Loc.FileName = Path;
}

LRESULT NppCallContext::SciMsg(int Msg, WPARAM wParam, LPARAM lParam)
{
  sci_fn_direct_t SciDirectFunc;

  SciDirectFunc = (sci_fn_direct_t)SciDirectFuncPtr;
  return SciDirectFunc != NULL ?
    SciDirectFunc(SciDirectPtr, Msg, wParam, lParam) :
    SciHndl != NULL ? ::SendMessage(SciHndl, Msg, wParam, lParam) : 0;
}

LRESULT NppCallContext::NppMsg(int Msg, WPARAM wParam, LPARAM lParam)
{
  return App->NppHndl == NULL ? 0 :
    (int)::SendMessage(App->NppHndl, Msg, wParam, lParam);
}

void NppCallContext::UpdateSelection(int Start, int End)
{
  SciMsg(SCI_SETSELECTIONSTART, Start);
  SciMsg(SCI_SETSELECTIONEND, End);
}

void NppCallContext::CalcFormPos(POINT *Pt, int width, int height)
{
  int Pos, x, y;
  int TextHeight;
  RECT Rect;
  bool WordIsVisible;
  HMONITOR MonitorHndl;
  MONITORINFO MonitorInfo;
  BOOL res;

  TextHeight = (int)SciMsg(SCI_TEXTHEIGHT, (WPARAM)Loc.I.LineNum);
  ::GetWindowRect(SciHndl, &Rect);
  Pos = (int)SciMsg(SCI_GETSELECTIONSTART);
  x = (int)SciMsg(SCI_POINTXFROMPOSITION, 0, (LPARAM)Pos);
  y = (int)SciMsg(SCI_POINTYFROMPOSITION, 0, (LPARAM)Pos);
  if (x >= 0 && Rect.left + x < Rect.right && y >= 0 &&
    Rect.top + y < Rect.bottom)
  {
    /* x,y is inside the visible window */
    x += Rect.left;
    y += Rect.top + TextHeight;
    WordIsVisible = true;
  }
  else
  {
    /* x,y is outside the visible window (due to scroll) */
    x = Rect.left + 20;
    y = Rect.top + 20;
    WordIsVisible = false;
  }

  MonitorHndl = ::MonitorFromWindow (SciHndl, MONITOR_DEFAULTTONEAREST);
  ::memset(&MonitorInfo, 0, sizeof(MonitorInfo));
  MonitorInfo.cbSize = sizeof(MonitorInfo);
  res = ::GetMonitorInfo(MonitorHndl, &MonitorInfo);
  if (res == 0)
    return;

  if (x + width + 8 > MonitorInfo.rcWork.right)
    x = MonitorInfo.rcWork.right - width - 8;
  if ( x < 8)
    x = 8;

  if (y + height + 8 > MonitorInfo.rcWork.bottom)
  {
    if (WordIsVisible)
      y -= height + TextHeight;
    else
      y = MonitorInfo.rcWork.bottom - height - 8;
  }
  if (y < 8)
    y = 8;
  Pt->x = x;
  Pt->y = y;
}

void TagLeetApp::InitHandles(const struct NppData *NppDataObj)
{
  NppHndl     = NppDataObj->_nppHandle;
  SciMainHndl = NppDataObj->_scintillaMainHandle;
  SciSecHndl  = NppDataObj->_scintillaSecondHandle;
}

TL_ERR TagLeetApp::GoToFileLine(const NppLoc *Loc, const char *Tag)
{
  LRESULT res;
  TL_ERR err;
  uint32_t LinesOnScreen;
  uint32_t TopVisibleLine;
  NppCallContext NppC(this);

  res = ::SendMessage(NppHndl, NPPM_DOOPEN, 0, (LPARAM)Loc->FileName);
  NppC.NppMsg(NPPM_DOOPEN, 0, (LPARAM)Loc->FileName);
  if (res == 0)
    return TL_ERR_GENERAL;

  NppC.ReInit();
  LinesOnScreen = (uint32_t)NppC.SciMsg(SCI_LINESONSCREEN);
  if (LinesOnScreen > 100)
    LinesOnScreen = 20;
  else if (LinesOnScreen < 1)
    LinesOnScreen = 2;

  TopVisibleLine = (uint32_t)NppC.SciMsg(SCI_GETFIRSTVISIBLELINE);

  err = Tag == NULL ? TL_ERR_NOT_EXIST :
    SelectTagInLine(&NppC, Loc->I.LineNum, Tag);

  if (err)
  {
    int SciLineSize, Pos;
    Pos = (int)NppC.SciMsg(SCI_POSITIONFROMLINE, Loc->I.LineNum);
    SciLineSize = (int)NppC.SciMsg(SCI_GETLINE, Loc->I.LineNum);
    if (Loc->I.XPos < (uint32_t)SciLineSize)
      Pos += Loc->I.XPos;
    NppC.SciMsg(SCI_GOTOPOS, Pos);
  }

  /* If requested line is already on screen, ignore TopVisibleLine in Loc and
   * don't scroll */
  if (Loc->I.LineNum >= TopVisibleLine &&
    TopVisibleLine + LinesOnScreen > Loc->I.LineNum)
  {
    return TL_ERR_OK;
  }

  /* If using TopVisibleLine will leave the requested line out of the screen
   * then fix it so the line will appear in the middle of the screen */
  TopVisibleLine = Loc->I.TopVisibleLine >= 0 ? Loc->I.TopVisibleLine :
    Loc->I.LineNum;
  if (TopVisibleLine >= Loc->I.LineNum ||
    TopVisibleLine + LinesOnScreen <= Loc->I.LineNum + 1)
  {
    uint32_t HalfScr = LinesOnScreen / 2;
    TopVisibleLine = Loc->I.LineNum > HalfScr ? Loc->I.LineNum - HalfScr : 0;
  }

  NppC.SciMsg(SCI_SETFIRSTVISIBLELINE, TopVisibleLine);
  NppC.SciMsg(SCI_SCROLLCARET);
  return TL_ERR_OK;
}

TL_ERR TagLeetApp::SelectTagInLine(NppCallContext *NppC, uint32_t LineNum,
  const char *Tag)
{
  int TagLength = (int)::strlen(Tag);
  char Buff[1024], *Line;
  int SciLineSize, LineStartPos, i;

  SciLineSize = (int)NppC->SciMsg(SCI_GETLINE, LineNum);
  Line = SciLineSize < TagLength || SciLineSize > 256*1024 ? NULL :
    SciLineSize < sizeof(Buff) ? Buff : (char *)::malloc(SciLineSize);
  if (Line == NULL)
    return TL_ERR_NOT_EXIST;

  NppC->SciMsg(SCI_GETLINE, LineNum, (LPARAM)Line);
  for (i = 0; i + TagLength <= SciLineSize; i++)
  {
    if (Line[i] == Tag[0] && ::memcmp(Tag, Line + i, TagLength) == 0)
      break;
  }
  if (Line != Buff)
    ::free(Line);
  if (i + TagLength > SciLineSize)
    return TL_ERR_NOT_EXIST;

  LineStartPos = (int)NppC->SciMsg(SCI_POSITIONFROMLINE, LineNum);
  NppC->UpdateSelection(LineStartPos + i, LineStartPos + i + TagLength);
  return TL_ERR_OK;
}

TagLookupContext::TagLookupContext(NppCallContext *in_NppC,
  const char *in_TagsFilePath, const char *in_GlobalTagsFilePath):
  NppC(in_NppC),
  TagsFilePath(in_TagsFilePath),
  GlobalTagsFilePath(in_GlobalTagsFilePath)
{
  bool NewSelection = false;
  int Start, End;
  Sci_TextRangeFull tr;

  Start = (int)NppC->SciMsg(SCI_GETSELECTIONSTART);
  End = (int)NppC->SciMsg(SCI_GETSELECTIONEND);
  if (Start >= End)
  {
    NewSelection = true;
    int Pos = (int)NppC->SciMsg(SCI_GETCURRENTPOS);
    Start = (int)NppC->SciMsg(SCI_WORDSTARTPOSITION, Pos, true);
    End = (int)NppC->SciMsg(SCI_WORDENDPOSITION, Pos, true);
    if (Start >= End)
      Start = End = Pos;
  }

  LineNum = (int)NppC->SciMsg(SCI_LINEFROMPOSITION, Start);
  LineStartPos = (int)NppC->SciMsg(SCI_POSITIONFROMLINE, LineNum);
  LineEndPos = (int)NppC->SciMsg(SCI_GETLINEENDPOSITION, LineNum);

  TextStartPos = Start;
  if (Start == End || Start < LineStartPos || End > LineEndPos)
  {
    TextLength = TagOffset = TagLength = 0;
    return;
  }

  if (NewSelection)
    NppC->UpdateSelection(Start, End);

  TextLength = LineEndPos - Start;
  if (TextLength + 1 > sizeof(TextBuff))
    TextLength = sizeof(TextBuff) - 1;

  tr.chrg.cpMin = Start;
  tr.chrg.cpMax = Start + TextLength;
  tr.lpstrText = TextBuff;
  TextLength = (int)NppC->SciMsg(SCI_GETTEXTRANGEFULL, 0, (LPARAM)&tr);
  TagOffset = 0;
  TagLength = End - Start;
}

NppFileLineIterator::NppFileLineIterator(TagLeetApp *App, const char *FileName):
  LineIterator(),
  NppC(App),
  CurrLineNum(1)
{
  TCHAR FileNameBuff[TL_MAX_PATH];
  LRESULT res;

  LineBuff = LocalBuff;
  LineBuffSize = sizeof(LocalBuff);

  str_to_TSTR(FileName, -1, FileNameBuff, ARRAY_SIZE(FileNameBuff));
  res = NppC.NppMsg(NPPM_DOOPEN, 0, (LPARAM)FileNameBuff);
  if (res == 0)
  {
    InitErr = TL_ERR_GENERAL;
    return;
  }
  NppC.ReInit();
  InitErr = TL_ERR_OK;
}

NppFileLineIterator::~NppFileLineIterator()
{
  if (LineBuff != LocalBuff)
    ::free(LineBuff);
}

TL_ERR NppFileLineIterator::MoveToLine(int LineNum)
{
  int LineCount;
  int SciLineSize;

  if (InitErr)
    return InitErr;

  LineCount = (int)NppC.SciMsg(SCI_GETLINECOUNT);
  if (LineNum > LineCount)
    return TL_ERR_NOT_EXIST;

  SciLineSize = (int)NppC.SciMsg(SCI_GETLINE, LineNum);
  if (SciLineSize + 1 > LineBuffSize)
  {
    uint32_t AllocSize = (SciLineSize + 0x400) & ~0x3FF;
    char *NewBuff = LineBuff == LocalBuff ? (char *)::malloc(AllocSize) :
      (char *)::realloc(LineBuff, AllocSize);
    if (NewBuff != NULL)
    {
      LineBuff = NewBuff;
      LineBuffSize = AllocSize;
    }
  }
  if (SciLineSize + 1 <= LineBuffSize)
  {
    NppC.SciMsg(SCI_GETLINE, LineNum, (LPARAM)LineBuff);
    while (SciLineSize > 0 && IS_EOL_CHAR(LineBuff[SciLineSize - 1]))
      SciLineSize--;
    LineBuff[SciLineSize] = '\0';
    LineSize = SciLineSize;
    Line = LineBuff;
  }
  else
  {
    Line = NULL;
    LineSize = 0;
  }
  CurrLineNum = LineNum;
  return TL_ERR_OK;
}

TL_ERR NppFileLineIterator::MoveToFirstLine()
{
  return MoveToLine(1);
}

TL_ERR NppFileLineIterator::MoveToNextLine()
{
  return MoveToLine(CurrLineNum + 1);
}

