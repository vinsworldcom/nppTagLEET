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
#include "Notepad_plus_msgs.h"
#include "Scintilla.h"

#include <tchar.h>
#include <stdio.h>
#include <string>
#include <shlobj.h>
#include <shellapi.h>

#include <malloc.h>
#include <shlwapi.h>
#include "resource.h"

#define DEFAULT_SCREEN_HEIGHT 1024
#define DEFAULT_FORM_WIDTH 700
#define DEFAULT_FORM_HEIGHT 350
#define DEFAULT_WAIT_TIME_MSEC 10000
#define DEFAULT_PRE_LINES 2
#define DEFAULT_POST_LINES 9

using namespace TagLEET_NPP;

static TCHAR AboutText[] =
  TEXT("TagLEET - ctags plugin for Notepad++\n")
  TEXT("Version: 1.3.2.0\n\n")
  TEXT("Copyright 2014-2018 by Gur Stavi\n")
  TEXT("gur.stavi@gmail.com")
  TEXT("\n\n----\n\nVersion: ") TEXT(VERSION_TAGLEET) TEXT("\n\n")
  TEXT("2019-2020 by VinsWorldcom");

const TCHAR TagLeetApp::WindowClassName[] = TEXT("TagLEET-form");
const uint8_t TagLeetApp::TestEolArr[2] = {10, 13};

TCHAR iniFilePath[MAX_PATH];
const TCHAR configFileName[]    = TEXT( "TagLEET.ini" );
const TCHAR sectionName[]       = TEXT( "Settings" );
const TCHAR iniUseNppColors[]   = TEXT( "UseNppColors" );
const TCHAR iniUseSciAutoC[]    = TEXT( "UseSciAutoC" );
const TCHAR iniUpdateOnSave[]   = TEXT( "UpdateOnSave" );
const TCHAR iniRecurseDirs[]    = TEXT( "RecurseSubDirs" );
const TCHAR iniPeekPre[]        = TEXT( "PeekPre" );
const TCHAR iniPeekPost[]       = TEXT( "PeekPost" );
const TCHAR iniGlobalTagsFile[] = TEXT( "GlobalTagsFile" );
const TCHAR iniWaitTimeMsec[]   = TEXT( "WaitTimeMsec" );

bool g_useNppColors = false;
bool g_useSciAutoC  = true;
bool g_UpdateOnSave = false;
bool g_RecurseDirs  = true;
int  g_PeekPre      = DEFAULT_PRE_LINES;
int  g_PeekPost     = DEFAULT_POST_LINES;
int  g_WaitTimeMsec = DEFAULT_WAIT_TIME_MSEC;
char g_GlobalTagsFile[TL_MAX_PATH];

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define REGIMGIDL 21580
#define REGIMGIDG 21581
const char *xpmTlL[] = {
/* columns rows colors chars-per-pixel */
    "16 16 9 1 ",
    "  c #0f0f0f",
    ". c #800000",
    "X c red",
    "o c #008000",
    "O c green",
    "+ c navy",
    "@ c blue",
    "# c #808080",
    "$ c None",
    /* pixels */
    "$$$$ $$$$$$$$$$$",
    "$$$ O $$$$$$$$$$",
    "$$ OOO $$$$$$$$$",
    "$ OOOoo $$$$$ $$",
    " OOOoo $$$$$ X $",
    "$ Ooo ##### X.. ",
    "$$ o $$$#$ X.. $",
    "$$$ $$$$#$$ . $$",
    "$$$$$$$$#$$$ $$$",
    "$$$$$$$$#$$$$$$$",
    "$$$$$$$$#$$$ $$$",
    "$$$$$$$$### @ $$",
    "$$$$$$$$$$ @++ $",
    "$$$$$$$$$ @++ $$",
    "$$$$$$$$$$ + $$$",
    "$$$$$$$$$$$ $$$$"
};

const char *xpmTlG[] = {
    "16 16 19 1 ",
    "  c #0f0f0f",
    ". c #00003A",
    "X c #660000",
    "o c #000066",
    "O c #800000",
    "+ c red",
    "@ c #B6663A",
    "# c #DB903A",
    "$ c #FFB666",
    "% c navy",
    "& c #0066B6",
    "* c blue",
    "= c #3A90DB",
    "- c #66B6FF",
    "; c #808080",
    ": c #FFFFB6",
    "> c #90DBFF",
    ", c #B6FFFF",
    "< c None",
    /* pixels */
    "<:X  &<<<<<<<<<<",
    ":X-<<@><<<<<<<<<",
    "$&<<<<<<<<<<<<<<",
    "#=<# .><<<<<< <<",
    "$&<<<@><<<<< + <",
    ":X-<<@><;;; +OO ",
    "<:X  o,<;< +OO <",
    "<<<<<<<<;<< O <<",
    "<<<<<<<<;<<< <<<",
    "<<<<<<<<;<<<<<<<",
    "<<<<<<<<;<<< <<<",
    "<<<<<<<<;;; * <<",
    "<<<<<<<<<< *%% <",
    "<<<<<<<<< *%% <<",
    "<<<<<<<<<< % <<<",
    "<<<<<<<<<<< <<<<"
};

static int __stdcall BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM, LPARAM pData)
{
  if (uMsg == BFFM_INITIALIZED)
    ::SendMessage(hwnd, BFFM_SETSELECTION, TRUE, pData);
  return 0;
}

DWORD WINAPI ShExecCtags(LPVOID lpParam)
{
  LPSHELLEXECUTEINFOA lpShExecInfo;
  lpShExecInfo = (LPSHELLEXECUTEINFOA)lpParam;

  ShellExecuteExA(lpShExecInfo);
  return 0;
}

void CreateTagsDb(HWND NppHndl, NppCallContext *NppC, char *TagsFilePath)
{
  char moduleFileName[MAX_PATH];
  GetModuleFileNameA((HMODULE)NppC->App->GetInstance(), moduleFileName, MAX_PATH);

  std::string strModuleFileName(moduleFileName);
  size_t lastindex = strModuleFileName.find_last_of(".");
  strModuleFileName = strModuleFileName.substr(0, lastindex);
  strModuleFileName += "\\ctags.exe";
  std::string strArgs = " --extras=+Ffq --fields=+Kn ";
  if (g_RecurseDirs)
      strArgs += " -R ";
  else
  {
    TCHAR path[MAX_PATH];
    ::SendMessage(NppHndl, NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)path);
    strArgs += ws2s(path);
  }

  DWORD err, errw = 0;
  LPSHELLEXECUTEINFOA lpShExecInfo;
  lpShExecInfo = (LPSHELLEXECUTEINFOA) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(SHELLEXECUTEINFOA));

  lpShExecInfo->cbSize = sizeof(SHELLEXECUTEINFO);
  lpShExecInfo->fMask = SEE_MASK_NOCLOSEPROCESS;
  lpShExecInfo->hwnd = NULL;
  lpShExecInfo->lpVerb = NULL;
  lpShExecInfo->lpFile = strModuleFileName.c_str();
  lpShExecInfo->lpParameters = strArgs.c_str();
  lpShExecInfo->lpDirectory = TagsFilePath;
  lpShExecInfo->nShow = SW_HIDE;
  lpShExecInfo->hInstApp = NULL;

  // std::string errMsg = " ";
  // errMsg += lpShExecInfo.lpFile;
  // errMsg += " ";
  // errMsg += lpShExecInfo.lpParameters;
  // errMsg += "\n\n in directory\n\n";
  // errMsg += lpShExecInfo.lpDirectory;
  // MessageBoxA(NULL, errMsg.c_str(), "Ready to generate ctags database", MB_OK | MB_ICONEXCLAMATION);

  HANDLE hThread = CreateThread(NULL, 0, ShExecCtags, lpShExecInfo, 0, NULL);
  errw = WaitForSingleObject(hThread, g_WaitTimeMsec);
  CloseHandle(hThread);

  HeapFree(GetProcessHeap(), 0, lpShExecInfo);

  GetExitCodeProcess(lpShExecInfo->hProcess, &err);
  if ( err != 0 && err != STILL_ACTIVE )
  {
      std::string errMsg = "[" + std::to_string(errw) + "] ";
      errMsg += lpShExecInfo->lpFile;
      errMsg += " ";
      errMsg += lpShExecInfo->lpParameters;
      errMsg += "\n\n in directory\n\n";
      errMsg += lpShExecInfo->lpDirectory;
      MessageBoxA(NULL, errMsg.c_str(), "Cannot generate ctags database", MB_OK | MB_ICONEXCLAMATION);
  }
}

void SetTagsFilePath(HWND NppHndl, NppCallContext *NppC, char *TagsFilePath)
{
  TCHAR Msg[2048];

  ::_sntprintf(Msg, ARRAY_SIZE(Msg),
    TEXT("'tags' file not found on path of:\n%s\n\nYES = Create recursively\nNO = Current file only\nCANCEL = do nothing"), NppC->Path);
  int response = ::MessageBox(NppHndl, Msg, TEXT("TagLEET"), MB_YESNOCANCEL | MB_ICONWARNING);
  if (response == IDYES)
  {
    LPMALLOC pShellMalloc = 0;
    if (::SHGetMalloc(&pShellMalloc) == NO_ERROR)
    {
      // If we were able to get the shell malloc object,
      // then proceed by initializing the BROWSEINFO stuct
      BROWSEINFO info;
      ZeroMemory(&info, sizeof(info));
      info.hwndOwner      = NppHndl;
      info.pidlRoot       = NULL;
      info.pszDisplayName = (LPTSTR)new TCHAR[MAX_PATH];
      info.lpszTitle      = TEXT( "CTags root directory (indexed recursively)" );
      info.ulFlags        = BIF_RETURNONLYFSDIRS | BIF_USENEWUI | BIF_NONEWFOLDERBUTTON;
      info.lpfn           = BrowseCallbackProc;
      info.lParam         = (LPARAM)TagsFilePath;

      ::SendMessage(NppHndl, NPPM_GETCURRENTDIRECTORY, MAX_PATH, (LPARAM)TagsFilePath);

      // Execute the browsing dialog.
      LPITEMIDLIST pidl = ::SHBrowseForFolder(&info);

      // pidl will be null if they cancel the browse dialog.
      // pidl will be not null when they select a folder.
      if (pidl)
      {
        ::SHGetPathFromIDListA( pidl, TagsFilePath );
        g_RecurseDirs = true;
        CreateTagsDb(NppHndl, NppC, TagsFilePath);

        pShellMalloc->Free(pidl);
      }
      pShellMalloc->Release();
      delete [] info.pszDisplayName;
    }
  }
  else if (response == IDNO)
  {
      ::SendMessage(NppHndl, NPPM_GETCURRENTDIRECTORY, MAX_PATH, (LPARAM)TagsFilePath);
      g_RecurseDirs = false;
      CreateTagsDb(NppHndl, NppC, TagsFilePath);
  }
  return;
}

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
    ListViewFontHeight = 20;
  DefaultListViewFontHeight = ListViewFontHeight;

  EditViewFontHeight = (int)::SendMessage(getCurrScintilla(), SCI_STYLEGETSIZE, 0, 0);
  if (EditViewFontHeight > 0)
    EditViewFontHeight *= 20;
  DefaultEditViewFontHeight = EditViewFontHeight;

  StatusFont = CreateStatusFont();
  ListViewFont = CreateListViewFont();
  FormScale = 100;
  HeightWidthValid = false;

  ::SendMessage( NppHndl, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH,
                 ( LPARAM )iniFilePath );

  // if config path doesn't exist, we create it
  if ( PathFileExists( iniFilePath ) == FALSE )
      ::CreateDirectory( iniFilePath, NULL );

  // make your plugin config file full file path name
  PathAppend( iniFilePath, configFileName );

  // get the parameter value from plugin config
  g_useNppColors = ::GetPrivateProfileInt( sectionName, iniUseNppColors, 0,
                   iniFilePath );
  g_useSciAutoC  = ::GetPrivateProfileInt( sectionName, iniUseSciAutoC, 1,
                   iniFilePath );
  g_UpdateOnSave = ::GetPrivateProfileInt( sectionName, iniUpdateOnSave, 0,
                   iniFilePath );
  g_RecurseDirs  = ::GetPrivateProfileInt( sectionName, iniRecurseDirs, 1,
                   iniFilePath );
  g_PeekPre      = ::GetPrivateProfileInt( sectionName, iniPeekPre,
                   DEFAULT_PRE_LINES, iniFilePath );
  g_PeekPost     = ::GetPrivateProfileInt( sectionName, iniPeekPost,
                   DEFAULT_POST_LINES, iniFilePath );
  g_WaitTimeMsec = ::GetPrivateProfileInt( sectionName, iniWaitTimeMsec,
                   DEFAULT_WAIT_TIME_MSEC, iniFilePath );
  TCHAR globalTagsFile[TL_MAX_PATH];
  ::GetPrivateProfileString( sectionName, iniGlobalTagsFile, TEXT("\0"),
                             globalTagsFile, MAX_PATH, iniFilePath );

  DWORD fileOrDir = GetFileAttributes( globalTagsFile );
  if ( ( globalTagsFile[0] != '\0' ) &&
      ( ( fileOrDir == FILE_ATTRIBUTE_DIRECTORY ) ||
          ( fileOrDir == INVALID_FILE_ATTRIBUTES ) ) )
  {
      _tcscpy( globalTagsFile, TEXT("\0") );
  }
  size_t nNumCharConverted;
  wcstombs_s(&nNumCharConverted, g_GlobalTagsFile, TL_MAX_PATH, globalTagsFile, TL_MAX_PATH);
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
  TCHAR buf[64];

  Lock();
  if (Form != NULL)
  {
    DestroyOnDetachForm = true;
    Unlock();
    return;
  }
  ::WritePrivateProfileString( sectionName, iniUseNppColors,
                               g_useNppColors ? TEXT( "1" ) : TEXT( "0" ), iniFilePath );
  ::WritePrivateProfileString( sectionName, iniUseSciAutoC,
                               g_useSciAutoC ? TEXT( "1" ) : TEXT( "0" ), iniFilePath );
  ::WritePrivateProfileString( sectionName, iniUpdateOnSave,
                               g_UpdateOnSave ? TEXT( "1" ) : TEXT( "0" ), iniFilePath );
  ::WritePrivateProfileString( sectionName, iniRecurseDirs,
                               g_RecurseDirs ? TEXT( "1" ) : TEXT( "0" ), iniFilePath );
  _itot_s( g_PeekPre, buf, 64, 10 );
  ::WritePrivateProfileString( sectionName, iniPeekPre, buf, iniFilePath );
  _itot_s( g_PeekPost, buf, 64, 10 );
  ::WritePrivateProfileString( sectionName, iniPeekPost, buf, iniFilePath );
  _itot_s( g_WaitTimeMsec, buf, 64, 10 );
  ::WritePrivateProfileString( sectionName, iniWaitTimeMsec, buf, iniFilePath );

  TCHAR globalTagsFile[TL_MAX_PATH];
  size_t nNumCharConverted;
  mbstowcs_s(&nNumCharConverted, globalTagsFile, TL_MAX_PATH, g_GlobalTagsFile, TL_MAX_PATH);
  ::WritePrivateProfileString( sectionName, iniGlobalTagsFile, globalTagsFile, iniFilePath );

  delete this;
}

void TagLeetApp::GetFormSize(unsigned int *Width, unsigned int *Height)
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

void TagLeetApp::SetFormSize(unsigned int Width, unsigned int Height, bool reset)
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
    return ListViewFont;

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

int TagLeetApp::UpdateEditViewFontHeight(int change, bool reset)
{
  int NewFontHeight;

  NewFontHeight = reset ?
    DefaultEditViewFontHeight : EditViewFontHeight + change*20;
  if (NewFontHeight < 140 || NewFontHeight > 400)
    return EditViewFontHeight;

  EditViewFontHeight = NewFontHeight;
  return EditViewFontHeight;
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
  // TL_ERR err;
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

  return TL_ERR_NOT_EXIST;
  // /* Failed to find a tag file on the path. Try using the last tag file we
   // * found. */
// 2020-05-21:MVINCENT: Actually, do not do this, prompt for create instead
  // err = LastTagFileGet(Path, ARRAY_SIZE(Path));
  // if (err)
    // return err;
  // FileHndl = ::CreateFile(Path, GENERIC_READ, FILE_SHARE_READ, NULL,
    // OPEN_EXISTING, 0, NULL);
  // if (FileHndl == INVALID_HANDLE_VALUE)
    // return TL_ERR_NOT_EXIST;
  // ::CloseHandle(FileHndl);
  // TSTR_to_str(Path, -1, TagFileBuff, BuffSize);
  // return TL_ERR_OK;
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
    SetTagsFilePath(NppHndl, &NppC, TagsFilePath);
    return;
  }

  TagLookupContext TLCtx(&NppC, TagsFilePath, g_GlobalTagsFile);

  /* Test that word is valid */
  if ( TLCtx.TagLength == 0 )
      return;
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

// Following 3 are ported from tag_leet_form.cpp to put in our namespace
// since we call them from AutoComplete()
TL_ERR TagLeetApp::PopulateTagListHelperGlobal(TagLookupContext *TLCtx, TagFile *tf)
{
  TL_ERR err;
  char SavedChar;
  char *Tag = TLCtx->TextBuff + TLCtx->TagOffset;

  SavedChar = Tag[TLCtx->TagLength];
  /* Ensure Tag is NULL terminated */
  Tag[TLCtx->TagLength] = '\0';
  err = TList.Create(Tag, TLCtx->GlobalTagsFilePath, tf, DoPrefixMatch);
  Tag[TLCtx->TagLength] = SavedChar;
  return err;
}

TL_ERR TagLeetApp::PopulateTagListHelper(TagLookupContext *TLCtx, TagFile *tf)
{
  TL_ERR err;
  char SavedChar;
  char *Tag = TLCtx->TextBuff + TLCtx->TagOffset;

  SavedChar = Tag[TLCtx->TagLength];
  /* Ensure Tag is NULL terminated */
  Tag[TLCtx->TagLength] = '\0';
  err = TList.Create(Tag, TLCtx->TagsFilePath, tf, DoPrefixMatch);
  Tag[TLCtx->TagLength] = SavedChar;
  return err;
}

TL_ERR TagLeetApp::PopulateTagList(TagLookupContext *TLCtx)
{
  TL_ERR err;
  TagFile tf;
  char *Tag;

  err = tf.Init(TLCtx->TagsFilePath);
  if (err)
    return err;

  err = PopulateTagListHelper(TLCtx, &tf);
  /* For prefix match we don't have any fall backs */
  // if (DoPrefixMatch)
    // return err;

  Tag = TLCtx->TextBuff + TLCtx->TagOffset;
  /* If no match, perhaps file extension need to be added to selection */
  if (!err && TList.Count == 0 && Tag[TLCtx->TagLength] == '.')
  {
    TLCtx->TagLength++;
    for (; TLCtx->TagLength < TLCtx->TextLength; TLCtx->TagLength++)
    {
      char ch = Tag[TLCtx->TagLength];
      if (ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z' ||
        ch >= '0' && ch <= '9' || ch == '_')
      {
        continue;
      }
      break;
    }
    err = PopulateTagListHelper(TLCtx, &tf);
    if (err)
    {
        if ( TLCtx->GlobalTagsFilePath[0] != '\0' )
        {
            FromLocalFile = false;
            err = PopulateTagListHelperGlobal(TLCtx, &tf);
            if (err)
                return err;
        }
        else
            return err;
    }
  }
  else if (!err && TList.Count == 0)
  {
    if ( TLCtx->GlobalTagsFilePath[0] != '\0' )
    {
        FromLocalFile = false;
        err = PopulateTagListHelperGlobal(TLCtx, &tf);
        if (err)
            return err;
    }
  }

  return TL_ERR_OK;
}

void TagLeetApp::UpdateTagDb()
{
  if (!g_UpdateOnSave)
    return;

  TL_ERR err;
  TlAppSync Sync(this);
  NppCallContext NppC(this);
  char TagsFilePath[TL_MAX_PATH];

  err = GetTagsFilePath(&NppC, TagsFilePath, sizeof(TagsFilePath));
  if (err)
    return;

  char Path[TL_MAX_PATH + 16];
  int n = (int)::strlen(TagsFilePath);
  ::memcpy(Path, TagsFilePath, n * sizeof(char));

  if (Path[n-1] == 's' &&
      Path[n-2] == 'g' &&
      Path[n-3] == 'a' &&
      Path[n-4] == 't' &&
      Path[n-5] == '\\')
  {
    Path[n-5] = '\0';
    CreateTagsDb(NppHndl, &NppC, Path);
  }
}

void TagLeetApp::FindRefs()
{
  TL_ERR err;
  NppCallContext NppC(this);
  char TagsFilePath[TL_MAX_PATH];

  err = GetTagsFilePath(&NppC, TagsFilePath, sizeof(TagsFilePath));
  if (err)
    return;

  char Path[TL_MAX_PATH + 16];
  int n = (int)::strlen(TagsFilePath);
  ::memcpy(Path, TagsFilePath, n * sizeof(char));

  if (Path[n-1] == 's' &&
      Path[n-2] == 'g' &&
      Path[n-3] == 'a' &&
      Path[n-4] == 't' &&
      Path[n-5] == '\\')
  {
    Path[n-5] = '\0';
    size_t newsize = strlen(Path) + 1;
    TCHAR *wPath = new wchar_t[newsize];
    size_t convertedChars = 0;
    mbstowcs_s(&convertedChars, wPath, newsize, Path, _TRUNCATE);
    ::SendMessage(NppHndl, NPPM_LAUNCHFINDINFILESDLG, (WPARAM)wPath, NULL);
    delete wPath;
  }
}

void TagLeetApp::SciAutoComplete()
{
  TL_ERR err;
  TlAppSync Sync(this);
  NppCallContext NppC(this);
  char TagsFilePath[TL_MAX_PATH];
  TCHAR Msg[2048];
  int i;

  FromLocalFile = true;

  err = GetTagsFilePath(&NppC, TagsFilePath, sizeof(TagsFilePath));
  if (err)
    return;

  // if at start of word, just return
  int currpos = ( int )::SendMessage( NppC.SciHndl, SCI_GETCURRENTPOS, 0, 0 );
  int wordStart = (int)::SendMessage( NppC.SciHndl, SCI_WORDSTARTPOSITION, currpos, true );
  if (currpos == wordStart)
    return;

  // cannot handle multiple selections
  int sels = (int)::SendMessage( NppC.SciHndl, SCI_GETSELECTIONS, 0, 0 );
  if (sels > 1)
    return;

  TagLookupContext TLCtx(&NppC, TagsFilePath, g_GlobalTagsFile);

  /* Test that word is valid */
  if ( TLCtx.TagLength == 0 )
      return;
  for (i = 0; i < TLCtx.TagLength; i++)
  {
    char ch = TLCtx.TextBuff[TLCtx.TagOffset + i];
    if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
      return;
  }

  // Above same as LookupTag()
  // Below implements the Form() part of LookupTag()
  err = PopulateTagList(&TLCtx);
  if (err)
    return;

  int Idx;
  TagList::TagListItem *Item;
  std::string wList;
  std::string prevWord;
  prevWord = "";

  // We want prefix match, after all, this is autocomplete
  DoPrefixMatch = true;
  TLCtx.GetLineNumFromTag(DoPrefixMatch, &TList);
  // Loop from SetListFromTag()
  for (Item = TList.List, Idx=0; Item != NULL; Item = Item->Next, Idx++)
  {
    if ( prevWord.compare(Item->Tag) == 0 )
        continue;
    prevWord = Item->Tag;
    wList += Item->Tag;
    wList += "?";
    if (FromLocalFile)
        wList += STR(REGIMGIDL);
    else
        wList += STR(REGIMGIDG);
    if (Item->Next != NULL)
        wList += " ";
  }

  // Must clear the selection that TagLookupContext did for us
  SendMessage( NppC.SciHndl, SCI_SETEMPTYSELECTION, currpos, 0 );
  if (Idx == 0) // if no tags found, then just return
    return;

  SendMessage(NppC.SciHndl, SCI_AUTOCSETSEPARATOR, WPARAM(' '), 0 );
  SendMessage(NppC.SciHndl, SCI_AUTOCSETTYPESEPARATOR, WPARAM('?'), 0 );
  SendMessage(NppC.SciHndl, SCI_AUTOCSETIGNORECASE, true, 0 );
  SendMessage(NppC.SciHndl, SCI_REGISTERIMAGE, REGIMGIDL, (LPARAM)xpmTlL);
  SendMessage(NppC.SciHndl, SCI_REGISTERIMAGE, REGIMGIDG, (LPARAM)xpmTlG);
  SendMessage(NppC.SciHndl, SCI_AUTOCSHOW, TLCtx.TagLength, (LPARAM) wList.c_str());

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

void TagLeetApp::AutoComplete()
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
    SetTagsFilePath(NppHndl, &NppC, TagsFilePath);
    return;
  }

  TagLookupContext TLCtx(&NppC, TagsFilePath, g_GlobalTagsFile);

  /* Test that word is valid */
  if ( TLCtx.TagLength == 0 )
      return;
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

  Form->setDoPrefixMatch();
  Form->setDoAutoComplete();
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

void TagLeetApp::DeleteTags()
{
  TL_ERR err;
  NppCallContext NppC(this);
  char TagsFilePath[TL_MAX_PATH + 16];
  TCHAR Path[TL_MAX_PATH + 16];
  char Msg[2048];

  err = GetTagsFilePath(&NppC, TagsFilePath, sizeof(TagsFilePath));
  if (err)
  {
      err = LastTagFileGet(Path, ARRAY_SIZE(Path));
      if (err)
      {
        MessageBox(NppHndl, TEXT("Current and last tag files not found."),
                   TEXT("File Not Found"), MB_ICONEXCLAMATION);
        return;
      }
      TSTR_to_str(Path, -1, TagsFilePath, sizeof(TagsFilePath));
      sprintf(Msg, "Current tags file not found.  Last tags file:\n\n%s\n\nDelete?", TagsFilePath);
      int response = MessageBoxA(NppHndl, Msg, "Confirm", MB_YESNO | MB_ICONWARNING);
      if (response == IDNO)
        return;
      else
        LastTagFile[0] = _T('\0');
  }
  remove(TagsFilePath);
}

void TagLeetApp::ShowAbout() const
{
  if (NppHndl != NULL)
  {
    ::MessageBox(NppHndl, AboutText, TEXT("About TagLEET"), MB_OK);
  }
}

HWND TagLeetApp::getCurrScintilla()
{
  int which = -1;
  ::SendMessage( this->NppHndl, NPPM_GETCURRENTSCINTILLA, 0,
                 ( LPARAM )&which );
  return ( which == 0 ) ? this->SciMainHndl :
         this->SciSecHndl;
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
  if (count <= 1)
      return TL_ERR_NOT_EXIST;

  if (count <= FnBuffCount)
  {
    ::memcpy(FnBuff, LastTagFile, count * sizeof(TCHAR));
    return TL_ERR_OK;
  }
  return TL_ERR_TOO_BIG;
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
