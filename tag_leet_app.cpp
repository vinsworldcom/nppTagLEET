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
#include "tag_leet_form.h"
#include <tchar.h>
#include <stdio.h>
#include <malloc.h>
#include "resource.h"

#define DEFAULT_SCREEN_HEIGHT 1024
#define DEFAULT_FORM_WIDTH 700
#define DEFAULT_FORM_HEIGHT 350

using namespace TagLEET_NPP;

static TCHAR AboutText[] =
  TEXT("TagLEET - ctags plugin for Notepad++\n")
  TEXT("Version: ") TEXT(VERSION_TAGLEET) TEXT("\n\n")
  TEXT("Copyright 2014-2018 by Gur Stavi\n")
  TEXT("gur.stavi@gmail.com");

const TCHAR TagLeetApp::WindowClassName[] = TEXT("TagLEET-form");
const uint8_t TagLeetApp::TestEolArr[2] = {10, 13};

TagLeetApp::TagLeetApp(const struct NppData *NppDataObj)
{
  WNDCLASS wndclass;

  InitializeCriticalSection(&CritSec);
  InitHandles(NppDataObj);
  Form = NULL;
  DestroyOnDetachForm = false;
  LastTagFile[0] = _T('\0');

  wndclass.style          = CS_HREDRAW|CS_VREDRAW;
  wndclass.lpfnWndProc    = TagLeetForm::InitialWndProc;
  wndclass.cbClsExtra     = 0;
  wndclass.cbWndExtra     = sizeof(void *);
  wndclass.hInstance      = InstanceHndl;
  wndclass.hIcon          = NULL;
  wndclass.hCursor        = LoadCursor (NULL, IDC_ARROW);
  wndclass.hbrBackground  = (HBRUSH)GetStockObject (WHITE_BRUSH);
  wndclass.lpszMenuName   = NULL;
  wndclass.lpszClassName  = WindowClassName;
  ::RegisterClass (&wndclass);

  ListViewFontHeight = GetScreenHeight() / 60;
  if (ListViewFontHeight < 20)
  {
    ListViewFontHeight = 20;
  }
  DefaultListViewFontHeight = ListViewFontHeight;

  StatusFont = CreateStatusFont();
  ListViewFont = CreateListViewFont();
  FormScale = 100;
  HeightWidthValid = false;
}

TagLeetApp::~TagLeetApp()
{
  Lock();
  if (Form != NULL)
  {
    Form->PostCloseMsg();
    Unlock();
    /* Not a full-proof solution but will do for now. */
    ::Sleep(300);
  }
  else
  {
    Unlock();
  }
  ::UnregisterClass(WindowClassName, InstanceHndl);
  if (StatusFont != NULL)
    ::DeleteObject(StatusFont);
  if (ListViewFont != NULL)
    ::DeleteObject(ListViewFont);
  DeleteCriticalSection(&CritSec);
}

void TagLeetApp::Shutdown()
{
  Lock();
  if (Form != NULL)
  {
    DestroyOnDetachForm = true;
    Unlock();
    return;
  }

  delete this;
}

void TagLeetApp::GetFormSize(unsigned int *Width,
  unsigned int *Height)
{
  if (HeightWidthValid) {
    *Width = FormWidth * FormScale / 100;
    *Height = FormHeight * FormScale / 100;
    return;
  }

  NppCallContext NppC(this);
  HMONITOR MonitorHndl;
  MONITORINFO MonitorInfo;
  int CurrWidth;
  int CurrHeight;
  DWORD res;

  MonitorHndl = ::MonitorFromWindow (NppC.SciHndl, MONITOR_DEFAULTTONEAREST);
  ::memset(&MonitorInfo, 0, sizeof(MonitorInfo));
  MonitorInfo.cbSize = sizeof(MonitorInfo);
  res = ::GetMonitorInfo(MonitorHndl, &MonitorInfo);
  if (res != 0)
  {
    int W = MonitorInfo.rcWork.right - MonitorInfo.rcWork.left;
    int H = MonitorInfo.rcWork.bottom - MonitorInfo.rcWork.top;
    CurrWidth = W * 55 / 100;
    CurrHeight = H * 35 / 100;
  }
  else
  {
    CurrWidth = DEFAULT_FORM_WIDTH;
    CurrHeight = DEFAULT_FORM_HEIGHT;
  }

  *Width = CurrWidth * FormScale / 100;
  *Height = CurrHeight * FormScale / 100;
}

void TagLeetApp::SetFormSize(unsigned int Width,
  unsigned int Height, bool reset)
{
  if (reset)
  {
    HeightWidthValid = false;
    FormScale = 100;
    return;
  }

  FormWidth = Width * 100 / FormScale;
  FormHeight = Height * 100 / FormScale;
  HeightWidthValid = true;
}

void TagLeetApp::UpdateFormScale(int change)
{
    unsigned NewScale;

    NewScale = FormScale + change;
    if (NewScale >= 40 && NewScale <= 170)
    {
        FormScale = NewScale;
    }
}

int TagLeetApp::GetScreenHeight()
{
  HMONITOR MonitorHndl;
  MONITORINFO MonitorInfo;
  DWORD res;

  MonitorHndl = ::MonitorFromWindow(NppHndl, MONITOR_DEFAULTTONEAREST);
  ::memset(&MonitorInfo, 0, sizeof(MonitorInfo));
  MonitorInfo.cbSize = sizeof(MonitorInfo);
  res = ::GetMonitorInfo(MonitorHndl, &MonitorInfo);
  if (res == 0)
  {
    return DEFAULT_SCREEN_HEIGHT;
  }
  return MonitorInfo.rcWork.bottom - MonitorInfo.rcWork.top;
}

static int CALLBACK EnumFontCallback(const LOGFONT *lf, const TEXTMETRIC *,
  DWORD, LPARAM lParam)
{
  *(LOGFONT *)lParam = *lf;
  return 0;
}

HFONT TagLeetApp::CreateSpecificFont(const TCHAR **FontList, int FontListSize,
	int Height)
{
  LOGFONT EnumLf, lf;
  const TCHAR *FontName;
  int res, i;

  HDC hDC = GetDC(NULL);
  memset(&lf, 0, sizeof(lf));

  for (i = 0; i <= FontListSize; i++)
  {
    ::memset(&EnumLf, 0, sizeof(EnumLf));
    FontName = (i < FontListSize) ? FontList[i] : TEXT("");
    EnumLf.lfCharSet = ANSI_CHARSET;
    _tcsncpy(EnumLf.lfFaceName, FontName, ARRAY_SIZE(EnumLf.lfFaceName) - 1);
    res = ::EnumFontFamiliesEx(hDC, &EnumLf, EnumFontCallback, (LPARAM)&lf, 0);
    if (res == 0)
      break;
  }

  lf.lfHeight = -MulDiv(Height, ::GetDeviceCaps(hDC, LOGPIXELSY), 144);
  lf.lfWidth = 0;
  lf.lfWeight = FW_NORMAL;
  return ::CreateFontIndirect(&lf);
}

HFONT TagLeetApp::CreateStatusFont()
{
  static const TCHAR *FontList[] = {
    TEXT("Segoe Condensed"),
    TEXT("DejaVu Sans Condensed"),
    TEXT("Nina"),
    TEXT("Kokila"),
    TEXT("Arial Narrow"),
    TEXT("Tahoma"),
    TEXT("Tunga"),
    TEXT("Times New Roman")};

  StatusHeight = GetScreenHeight() / 50;
  if (StatusHeight < 25)
  {
    StatusHeight = 25;
  }

  return CreateSpecificFont(FontList, ARRAY_SIZE(FontList), StatusHeight - 3);
}

HFONT TagLeetApp::CreateListViewFont()
{
  static const TCHAR *FontList[] = {
    TEXT("Times New Roman"),
    TEXT("Arial"),
    TEXT("FreeSans"),
    TEXT("Tahoma"),
    TEXT("DejaVu Sans Light"),
    TEXT("Cousine"),
    };

  return CreateSpecificFont(FontList, ARRAY_SIZE(FontList), ListViewFontHeight);
}

HFONT TagLeetApp::UpdateListViewFont(int change, bool reset)
{
  int NewFontHeight;
  int OldFontHeight;
  HFONT OldListViewFont;

  OldFontHeight = ListViewFontHeight;
  OldListViewFont = ListViewFont;

  NewFontHeight = reset ?
    DefaultListViewFontHeight : ListViewFontHeight + change;
  if (NewFontHeight < 14 || NewFontHeight > 40)
  {
    return ListViewFont;
  }

  ListViewFontHeight = NewFontHeight;
  ListViewFont = CreateListViewFont();
  if (ListViewFont != NULL)
  {
    ::DeleteObject(OldListViewFont);
  }
  else
  {
    ListViewFont = OldListViewFont;
    ListViewFontHeight = OldFontHeight;
    
  }

  return ListViewFont;
}

void TagLeetApp::SetInstance(HINSTANCE in_InstanceHndl)
{
  if (InstanceHndl == NULL)
    InstanceHndl = in_InstanceHndl;
}

void TagLeetApp::DetachForm(TagLeetForm *OldForm, bool *DestroyApp)
{
  if (Form == OldForm)
  {
    Form = NULL;
    *DestroyApp = DestroyOnDetachForm;
  }
}

void TagLeetApp::Lock()
{
  EnterCriticalSection(&CritSec);
}

void TagLeetApp::Unlock()
{
  LeaveCriticalSection(&CritSec);
}

/* Use the current filename from NppC and try to find a 'tags' file in its
 * path */
TL_ERR TagLeetApp::GetTagsFilePath(NppCallContext *NppC, char *TagFileBuff,
  int BuffSize)
{
  TL_ERR err;
  static const TCHAR TagsFileName[] = _T("tags");
  TCHAR Path[TL_MAX_PATH + 16];
  HANDLE FileHndl;
  int n;

  n = (int)::_tcslen(NppC->Path);
  if (n + 16 >= ARRAY_SIZE(Path))
    return TL_ERR_TOO_BIG;

  ::memcpy(Path, NppC->Path, n * sizeof(TCHAR));
  while (n > 0)
  {
    n--;
    if (Path[n] != _T('\\') && Path[n] != _T('/'))
      continue;
    ::memcpy(Path + n + 1, TagsFileName, sizeof(TagsFileName));
    FileHndl = ::CreateFile(Path, GENERIC_READ, FILE_SHARE_READ, NULL,
      OPEN_EXISTING, 0, NULL);
    if (FileHndl == INVALID_HANDLE_VALUE)
      continue;
    ::CloseHandle(FileHndl);
    TSTR_to_str(Path, -1, TagFileBuff, BuffSize);
    LastTagFileSet(Path);
    return TL_ERR_OK;
  }

  /* Failed to find a tag file on the path. Try using the last tag file we
   * found. */
  err = LastTagFileGet(Path, ARRAY_SIZE(Path));
  if (err)
    return err;
  FileHndl = ::CreateFile(Path, GENERIC_READ, FILE_SHARE_READ, NULL,
    OPEN_EXISTING, 0, NULL);
  if (FileHndl == INVALID_HANDLE_VALUE)
    return TL_ERR_NOT_EXIST;
  ::CloseHandle(FileHndl);
  TSTR_to_str(Path, -1, TagFileBuff, BuffSize);
  return TL_ERR_OK;
}

void TagLeetApp::LookupTag()
{
  TL_ERR err;
  TlAppSync Sync(this);
  NppCallContext NppC(this);
  char TagsFilePath[TL_MAX_PATH];
  TCHAR Msg[2048];
  int i;

  err = GetTagsFilePath(&NppC, TagsFilePath, sizeof(TagsFilePath));
  if (err)
  {
    ::_sntprintf(Msg, ARRAY_SIZE(Msg),
      TEXT("'tags' file not found on path of:\n%s"), NppC.Path);
    ::MessageBox(NppHndl, Msg, TEXT("TagLEET"), MB_ICONEXCLAMATION);
    return;
  }

  TagLookupContext TLCtx(&NppC, TagsFilePath);
  /* Test that word is valid */
  for (i = 0; i < TLCtx.TagLength; i++)
  {
    char ch = TLCtx.TextBuff[TLCtx.TagOffset + i];
    if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
      return;
  }

  if (Form != NULL)
  {
    Form->RefreshList(&TLCtx);
    return;
  }

  Form = new TagLeetForm(&NppC);
  if (Form == NULL)
    return;

  err = Form->CreateWnd(&TLCtx);
  if (!err)
    return;

  switch (err)
  {
    case TL_ERR_SORT:
      ::_sntprintf(Msg, ARRAY_SIZE(Msg), TEXT("Unsorted tags file: %hs"), TagsFilePath);
      break;
    default:
      ::_sntprintf(Msg, ARRAY_SIZE(Msg), TEXT("unexpected error(%u)"),
        (unsigned)err);
      break;
  }
  ::MessageBox(NppHndl, Msg, TEXT("TagLEET"), MB_ICONEXCLAMATION);
}

void TagLeetApp::GoBack()
{
  NppCallContext NppC(this);
  NppLoc *Loc;

  Loc = NppC.LocBank->PopLocFromBack(&NppC.Loc);
  if (Loc != NULL)
  {
    GoToFileLine(Loc);
    ::free(Loc);
  }
}

void TagLeetApp::GoForward()
{
  NppCallContext NppC(this);
  NppLoc *Loc;

  Loc = NppC.LocBank->PopLocFromForward(&NppC.Loc);
  if (Loc != NULL)
  {
    GoToFileLine(Loc);
    ::free(Loc);
  }
}

void TagLeetApp::ShowAbout() const
{
  if (NppHndl != NULL)
  {
    ::MessageBox(NppHndl, AboutText, TEXT("About TagLEET"), MB_OK);
  }
}

TlAppSync::TlAppSync(TagLeetApp *in_Obj)
{
  LockRefCount = 0;
  Obj = in_Obj;
  Lock();
}

void TagLeetApp::LastTagFileSet(const TCHAR *TagsFileName)
{
  int size = (int)((::_tcslen(TagsFileName) + 1) * sizeof(TCHAR));

  if (size <= sizeof(LastTagFile))
    ::memcpy(LastTagFile, TagsFileName, size);
}

TL_ERR TagLeetApp::LastTagFileGet(TCHAR *FnBuff, int FnBuffCount) const
{
  int count = (int)::_tcslen(LastTagFile) + 1;

  if (count <= FnBuffCount)
  {
    ::memcpy(FnBuff, LastTagFile, count * sizeof(TCHAR));
    return TL_ERR_OK;
  }
  return count == 0 ? TL_ERR_NOT_EXIST : TL_ERR_TOO_BIG;
}

TlAppSync::~TlAppSync()
{
  if (LockRefCount > 0)
    Obj->Unlock();
}

void TlAppSync::Unlock()
{
  LockRefCount--;
  if (LockRefCount == 0)
    Obj->Unlock();
}

void TlAppSync::Lock()
{
  if (LockRefCount == 0)
    Obj->Lock();

  LockRefCount++;
}

static NppLoc *NppLocAlloc(const TCHAR *CurFileName)
{
  NppLoc *NewLoc;
  int n = (int)::_tcslen(CurFileName) + 1;
  int m = ARRAY_SIZE(NewLoc->FnStorage);
  int AllocSize = sizeof(NppLoc);

  if (n > m)
    AllocSize += (n - m) * sizeof(TCHAR);
  NewLoc = (NppLoc *)::malloc(AllocSize);
  if (NewLoc == NULL)
    return NULL;
  ::memcpy(NewLoc->FnStorage, CurFileName, n * sizeof(TCHAR));
  NewLoc->FileName = NewLoc->FnStorage;

  return NewLoc;
}

NppLocStack::NppLocStack()
{
  ::memset(Arr, 0, sizeof(Arr));
  Index = Count = 0;
}

NppLocStack::~NppLocStack()
{
  Clear();
}

TL_ERR NppLocStack::Push(const NppLoc *Loc)
{
  NppLoc *NewLoc;

  if (Count > 0)
  {
    NewLoc = Arr[Index];
    if (::_tcscmp(NewLoc->FileName, Loc->FileName) == 0 &&
      NewLoc->I.LineNum == Loc->I.LineNum)
    {
      return TL_ERR_OK;
    }
  }

  NewLoc = NppLocAlloc(Loc->FileName);
  if (NewLoc == NULL)
    return TL_ERR_MEM_ALLOC;
  NewLoc->I = Loc->I;

  Index = (Index + 1) % ARRAY_SIZE(Arr);
  if (Count == ARRAY_SIZE(Arr))
    ::free(Arr[Index]);
  else
    Count++;
  Arr[Index] = NewLoc;
  return TL_ERR_OK;
}

NppLoc *NppLocStack::Pop()
{
  NppLoc *Loc;

  if (Count == 0)
    return NULL;

  Loc = Arr[Index];
  Arr[Index] = NULL;
  Index = (Index + ARRAY_SIZE(Arr) - 1) % ARRAY_SIZE(Arr);
  Count--;
  return Loc;
}

void NppLocStack::Clear()
{
  NppLoc *Loc;

  while (Count > 0)
  {
    Loc = Pop();
    ::free(Loc);
  }
}

TL_ERR NppLocBank::PushLocToBack(const NppLoc *CurrLoc)
{
  Forward.Clear();
  return Back.Push(CurrLoc);
}

NppLoc *NppLocBank::PopLocFromBack(const NppLoc *CurrLoc)
{
  NppLoc *Loc = Back.Pop();
  if (Loc != NULL)
    Forward.Push(CurrLoc);
  return Loc;
}

NppLoc *NppLocBank::PopLocFromForward(const NppLoc *CurrLoc)
{
  NppLoc *Loc = Forward.Pop();
  if (Loc != NULL)
    Back.Push(CurrLoc);
  return Loc;
}

int TagLEET_NPP::TSTR_to_str(const TCHAR *Str, int StrSize, char *Buff,
  int BuffSize)
{
  int n = 0;

  if (StrSize == -1)
    StrSize = (int)::_tcslen(Str);

  if (StrSize > 0)
  {
#ifdef UNICODE
    n = ::WideCharToMultiByte(CP_UTF8, 0, Str, StrSize, Buff, BuffSize,
      NULL, NULL);
    if (n <= 0 || n >= BuffSize)
      n = BuffSize - 1;
#else
    if (n > BuffSize)
      n = BuffSize - 1;
    ::memcpy(Buff, Str, n);
#endif
  }

  Buff[n] = _T('\0');
  return n;
}

int TagLEET_NPP::str_to_TSTR(const char *Str, int StrSize, TCHAR *Buff,
  int BuffSize)
{
  int n = 0;

  if (StrSize == -1)
    StrSize = (int)::strlen(Str);

  if (StrSize > 0)
  {
#ifdef UNICODE
    n = ::MultiByteToWideChar (CP_UTF8, 0, Str, StrSize, Buff, BuffSize);
    if (n <= 0 || n >= BuffSize)
      n = BuffSize - 1;
#else
    if (n > BuffSize)
      n = BuffSize - 1;
    ::memcpy(Buff, Str, n);
#endif
  }

  Buff[n] = _T('\0');
  return n;
}

  /* If lookup was performed from 'TAG:NUMBER' text then save NUMBER. If the
   * user will select a 'file' TAG then NUMBER will be used as the line number
   * to goto into that file */
void TagLookupContext::GetLineNumFromTag(bool PrefixMatch, TagList *tl) const
{
  static unsigned char ValidFileNameChars[128] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,  33,   0,  35,  36,  37,  38,   0,   0,   0,   0,  43,   0,  45,  46,  47,
     48,  49,  50,  51,  52,  53,  54,  55,  56,  57,   0,   0,   0,  61,   0,   0,
     64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,
     80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,   0,  92,   0,  94,  95,
      0,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122,   0,   0,   0, 126,   0};

  int Offset, i;
  int LineNumFromTag = 0;

  if (PrefixMatch)
  {
    for (Offset = TagOffset; Offset < TextLength; Offset++)
    {
      unsigned char ch = (unsigned char)TextBuff[Offset];
      if (ch >= ARRAY_SIZE(ValidFileNameChars) || ValidFileNameChars[ch] != ch)
        break;
    }
    if (Offset < TagLength)
      return;
  }
  else
  {
    Offset = TagOffset + TagLength;
  }
  if (TextBuff[Offset] != ':' && TextBuff[Offset] != '(')
    return;

  for (i = 1; Offset + i < TextLength; i++)
  {
    if (TextBuff[Offset + i] < '0' || TextBuff[Offset + i] > '9' )
      break;
    LineNumFromTag *= 10;
    LineNumFromTag += TextBuff[Offset + i] - '0';
  }

  if (LineNumFromTag < 1)
    return;
  if (TextBuff[Offset] == '(' && TextBuff[Offset + i] != ')')
    return;

  tl->SetTagAndLine(TextBuff + TagOffset, Offset - TagOffset, LineNumFromTag);
}
