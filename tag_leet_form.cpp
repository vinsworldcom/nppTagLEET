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

#define SPLITTER_HEIGHT 8

#include "tag_leet_form.h"
#include "scintilla.h"

#include <stdlib.h>
#include <string>
#include <tchar.h>
#include <commctrl.h>
#include <malloc.h>
#include <fstream>
#include <richedit.h>

#include "resource.h"

using namespace TagLEET_NPP;

extern bool g_useNppColors;
extern bool g_useSciAutoC;
extern bool g_UpdateOnSave;
extern int  g_PeekPre;
extern int  g_PeekPost;

#define SORT_UP_IMG_IDX    13
#define SORT_DOWN_IMG_IDX  14
#define SCROLL_ADJUST      5

TagLeetForm::TagLeetForm(NppCallContext *NppC)
{
  int i;

  App = NppC->App;
  FormHWnd = NULL;
  LViewHWnd = NULL;
  StatusHWnd = NULL;
  EditHWnd = NULL;
  DoPrefixMatch = false;
  DoAutoComplete = false;
  ::memset(&BackLoc, 0, sizeof(BackLoc));
  BackLocBank = NppC->LocBank;

  LastMaxTagWidth = 0;
  LastMaxFilenameWidth = 0;
  LastMaxExCmdWidth = 0;
  NeedUpdateColumns = false;

  if ( g_useNppColors )
      SetNppColors();
  else
      SetSysColors();

  ::memset(KindToIndex, 0, sizeof(KindToIndex));
  KindToIndex[TAG_KIND_UNKNOWN] = (UINT)-1;
  KindToIndex[TAG_KIND_CLASSES] = 0;
  KindToIndex[TAG_KIND_MACRO_DEF] = 1;
  KindToIndex[TAG_KIND_ENUM_VAL] = 2;
  KindToIndex[TAG_KIND_FUNCTION_DEF] = 3;
  KindToIndex[TAG_KIND_ENUM_NAME] = 5;
  KindToIndex[TAG_KIND_LOCAL_VAR] = 11;
  KindToIndex[TAG_KIND_MEMBER] = 6;
  KindToIndex[TAG_KIND_NAMESPACE] = 0;
  KindToIndex[TAG_KIND_FUNCTION_PROTO] = 7;
  KindToIndex[TAG_KIND_STRUCT_NAME] = 8;
  KindToIndex[TAG_KIND_TYPEDEF] = 9;
  KindToIndex[TAG_KIND_UNION_NAME] = 10;
  KindToIndex[TAG_KIND_VAR_DEF] = 11;
  KindToIndex[TAG_KIND_EXTERNAL] = 11;
  KindToIndex[TAG_KIND_FILE] = 12;

  for (i = 0; i < ARRAY_SIZE(SortOrder); i++)
    SortOrder[i] = (uint8_t)i;
}

TagLeetForm::~TagLeetForm()
{
  if (BackLoc.FileName != NULL)
    ::free(const_cast<TCHAR *>(BackLoc.FileName));
}

void TagLeetForm::Destroy()
{
  bool DestroyApp = false;
  TagLeetApp *CurrApp = App;

  if (_hDefaultSplitterProc != nullptr) {
    ::SetWindowLongPtr(SplitterHWnd, GWLP_WNDPROC, (LONG_PTR)_hDefaultSplitterProc);
    _hDefaultSplitterProc = nullptr;
  }

  if (_hDefaultEditProc != nullptr) {
    ::SetWindowLongPtr(EditHWnd, GWLP_WNDPROC, (LONG_PTR)_hDefaultEditProc);
    _hDefaultEditProc = nullptr;
  }

  ::SetWindowLongPtr(FormHWnd, 0, (LONG_PTR)NULL);
  CurrApp->Lock();
  CurrApp->DetachForm(this, &DestroyApp);
  CurrApp->Unlock();

  delete this;
  if (DestroyApp)
    delete CurrApp;
}

TL_ERR TagLeetForm::CreateWnd(TagLookupContext *TLCtx)
{
  NppCallContext *NppC = TLCtx->NppC;
  INITCOMMONCONTROLSEX Icc;
  POINT Pt;
  unsigned int FormWidth;
  unsigned int FormHeight;
  TL_ERR err;

  BackLoc.FileName = ::_tcsdup(NppC->Path);
  BackLoc.I = NppC->Loc.I;
  BackLocBank = NppC->LocBank;

  Icc.dwSize = sizeof(Icc);
  Icc.dwICC  = ICC_LISTVIEW_CLASSES;
  ::InitCommonControlsEx(&Icc);

  App->GetFormSize(&FormWidth, &FormHeight);
  NppC->CalcFormPos(&Pt, FormWidth, FormHeight);

  std::wstring title = TEXT("TagLEET for Notepad++");
  if ( DoAutoComplete )
      title += TEXT(": Autocomplete");
  else
      title += TEXT(": Lookup Tag");

  FormHWnd = ::CreateWindowEx(
    WS_EX_TOOLWINDOW,
    TagLeetApp::WindowClassName,
    title.c_str(),
    WS_SIZEBOX | WS_SYSMENU,
    Pt.x, Pt.y, FormWidth, FormHeight,
    NppC->SciHndl,
    NULL,
    App->GetInstance(),
    this);

  if (FormHWnd == NULL || LViewHWnd == NULL)
    return TL_ERR_GENERAL;

  UpdateColumnWidths(0, 0, 0, 0, 0, 0);
  err = SetListFromTag(TLCtx);
  if (err)
  {
    PostCloseMsg();
    return err;
  }

  SetColumnSortArrow(SortOrder[0] & 0x7F, true,
    SortOrder[0] & 0x80 ? true : false);

  ::ShowWindow(FormHWnd, SW_SHOW);
  ::SetFocus(LViewHWnd);
  return TL_ERR_OK;
}

void TagLeetForm::OnResize()
{
  RECT Rect;
  int StatusHeight = App->GetStatusHeight();
  int FocusIdx;
  int splitterPos = iSplitterPos;

  ::GetClientRect(FormHWnd, &Rect);

  if (splitterPos < 50)
    splitterPos = 50;
  else if (splitterPos > (Rect.bottom - 100) && !DoAutoComplete)
    splitterPos = Rect.bottom - 100;
  else if (splitterPos > (Rect.bottom - StatusHeight - SPLITTER_HEIGHT))
    splitterPos = Rect.bottom - StatusHeight - SPLITTER_HEIGHT;

  ::SetWindowPos(StatusHWnd, NULL,
    0, Rect.bottom - StatusHeight, Rect.right, StatusHeight,
    SWP_NOZORDER);

  ::SetWindowPos(LViewHWnd, NULL,
    0, 0, Rect.right, splitterPos,
    SWP_NOZORDER);

  ::SetWindowPos(SplitterHWnd, NULL,
    0, splitterPos, Rect.right, SPLITTER_HEIGHT,
    SWP_NOZORDER);

  ::SetWindowPos(EditHWnd, NULL,
    0, splitterPos + SPLITTER_HEIGHT, Rect.right, Rect.bottom - StatusHeight - SPLITTER_HEIGHT - splitterPos,
    SWP_NOZORDER);

  FocusIdx = ListView_GetNextItem(LViewHWnd, -1, LVNI_FOCUSED);
  ListView_EnsureVisible(LViewHWnd, FocusIdx, FALSE);

  ::GetWindowRect(FormHWnd, &Rect);
  App->SetFormSize(Rect.right - Rect.left, Rect.bottom - Rect.top, false);
  NeedUpdateColumns = true;
}

void TagLeetForm::ResizeForm(int change)
{
  NppCallContext NppC(App);
  PAINTSTRUCT Paint;
  POINT Pt;
  unsigned int FormWidth;
  unsigned int FormHeight;

  App->UpdateFormScale(change);
  App->GetFormSize(&FormWidth, &FormHeight);
  NppC.CalcFormPos(&Pt, FormWidth, FormHeight);

  ::BeginPaint(FormHWnd, &Paint);
  ::SetWindowPos(FormHWnd, NULL,
    Pt.x, Pt.y, FormWidth, FormHeight,
    SWP_NOZORDER);
  ::EndPaint(FormHWnd, &Paint);
}

void TagLeetForm::ResizeListViewFont(int change, bool reset)
{
    HFONT ListViewFont;
    int FocusIdx;

    ::SendMessage(LViewHWnd, WM_SETREDRAW, FALSE, 0);
    App->UpdateListViewFont(change, reset);
    ListViewFont = App->GetListViewFont();
    ::SendMessage(LViewHWnd, WM_SETFONT, (WPARAM)ListViewFont, (LPARAM)0);
    ::SendMessage(LViewHWnd, WM_SETREDRAW, TRUE, 0);
    FocusIdx = ListView_GetNextItem(LViewHWnd, -1, LVNI_FOCUSED);
    ListView_EnsureVisible(LViewHWnd, FocusIdx, FALSE);
    ::RedrawWindow(LViewHWnd, NULL, NULL,
      RDW_ERASE  | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void TagLeetForm::ResizeEditViewFont(int change, bool reset)
{
    int EditViewFontHeight;

    ::SendMessage(EditHWnd, WM_SETREDRAW, FALSE, 0);
    App->UpdateEditViewFontHeight(change, reset);
    EditViewFontHeight = App->GetEditViewFontHeight();

    CHARFORMAT2 charFormat;
    ZeroMemory(&charFormat, sizeof(charFormat));
    charFormat.cbSize = sizeof(charFormat);
    charFormat.dwMask = CFM_SIZE;
    charFormat.yHeight = EditViewFontHeight;
    SendMessage(EditHWnd, EM_SETCHARFORMAT, SPF_SETDEFAULT, (LPARAM) &charFormat);

    ::SendMessage(EditHWnd, WM_SETREDRAW, TRUE, 0);
    ::RedrawWindow(EditHWnd, NULL, NULL,
      RDW_ERASE  | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void TagLeetForm::SetNppColors()
{
  colorFg = ( COLORREF )::SendMessage( App->getCurrScintilla(), SCI_STYLEGETFORE, 0, 0 );
  colorBg = ( COLORREF )::SendMessage( App->getCurrScintilla(), SCI_STYLEGETBACK, 0, 0 );
}

void TagLeetForm::SetSysColors()
{
  colorFg = GetSysColor(COLOR_INFOTEXT);
  colorBg = GetSysColor(COLOR_INFOBK);
}

void TagLeetForm::ChangeColors()
{
  ::SendMessage(LViewHWnd, WM_SETREDRAW, FALSE, 0);

  ListView_SetBkColor(LViewHWnd, colorBg );
  ListView_SetTextBkColor(LViewHWnd, colorBg);
  ListView_SetTextColor(LViewHWnd, colorFg);

  SendMessage(EditHWnd, EM_SETBKGNDCOLOR, 0, colorBg);
  CHARFORMAT2 charFormat;
  ZeroMemory(&charFormat, sizeof(charFormat));
  charFormat.cbSize = sizeof(charFormat);
  charFormat.dwMask = CFM_COLOR | CFM_BACKCOLOR;
  charFormat.crTextColor = colorFg;
  charFormat.crBackColor = colorBg;
  SendMessage(EditHWnd, EM_SETCHARFORMAT, SPF_SETDEFAULT, (LPARAM) &charFormat);

  ::SendMessage(LViewHWnd, WM_SETREDRAW, TRUE, 0);
  ::RedrawWindow(LViewHWnd, NULL, NULL,
    RDW_ERASE  | RDW_INVALIDATE | RDW_ALLCHILDREN);

  ::SendMessage(EditHWnd, WM_SETREDRAW, TRUE, 0);
  ::RedrawWindow(EditHWnd, NULL, NULL,
    RDW_ERASE  | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

/* Called from WM_CREATE message of Form window. Note that at this point
 * CreateWindow of the Form Window has not returned yet so FormHWnd is NULL */
TL_ERR TagLeetForm::CreateListView(HWND hwnd)
{
  RECT Rect;
  int rc;
  TL_ERR err = TL_ERR_OK;
  HIMAGELIST hImgList;
  int StatusHeight = App->GetStatusHeight();
  HFONT StatusFont = App->GetStatusFont();
  HFONT ListViewFont = App->GetListViewFont();
  int EditViewFontHeight = App->GetEditViewFontHeight();
  LVCOLUMN LvCol;
  HWND HdrHndl;
  int EditHeight;
  int LViewHeight;

  hImgList = ImageList_LoadImage(App->GetInstance(),
    MAKEINTRESOURCE(IDR_ICONS),16,0,CLR_DEFAULT,IMAGE_BITMAP,LR_DEFAULTCOLOR);
  if (hImgList == NULL)
    return TL_ERR_GENERAL;

  ::GetClientRect(hwnd, &Rect);
  // No edit view if doing autocomplete
  if (DoAutoComplete)
    EditHeight = 0;
  else
    EditHeight  = ( Rect.bottom - StatusHeight - SPLITTER_HEIGHT ) / 2;
  LViewHeight = Rect.bottom - StatusHeight - SPLITTER_HEIGHT - EditHeight;

  StatusHWnd = ::CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), NULL,
    WS_CHILD | WS_VISIBLE | ES_READONLY,
    0, Rect.bottom - StatusHeight, Rect.right, StatusHeight,
    hwnd, NULL, App->GetInstance(), NULL);
  if (StatusHWnd != NULL && StatusFont != NULL)
    ::PostMessage(StatusHWnd, WM_SETFONT, (WPARAM)StatusFont, (LPARAM)0);

  LViewHWnd = ::CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
    WS_CHILD | LVS_REPORT | LVS_SINGLESEL | WS_VISIBLE,
    0, 0, Rect.right, LViewHeight,
    hwnd, NULL, App->GetInstance(), NULL);
  if (LViewHWnd == NULL)
    return TL_ERR_GENERAL;

  SplitterHWnd = ::CreateWindow(TEXT("BUTTON"), TEXT(""),
    WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
    0, Rect.bottom - StatusHeight - EditHeight - SPLITTER_HEIGHT, Rect.right, SPLITTER_HEIGHT,
    hwnd, NULL, App->GetInstance(), NULL);
  if (SplitterHWnd == NULL)
    return TL_ERR_GENERAL;
  ::SetWindowLongPtr(SplitterHWnd, GWLP_USERDATA, (LONG_PTR)this);
  _hDefaultSplitterProc = (WNDPROC)::SetWindowLongPtr(SplitterHWnd, GWLP_WNDPROC, (LONG_PTR)wndDefaultSplitterProc);
  iSplitterPos = LViewHeight;

  LoadLibrary(_T("Riched20.dll"));
  EditHWnd = ::CreateWindowEx(WS_EX_CLIENTEDGE, RICHEDIT_CLASS, NULL,
    WS_CHILD | WS_VSCROLL | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOHSCROLL | ES_AUTOVSCROLL,
    0, Rect.bottom - StatusHeight - EditHeight, Rect.right, EditHeight,
    hwnd, NULL, App->GetInstance(), NULL);
  if (EditHWnd == NULL)
    return TL_ERR_GENERAL;
  ::SetWindowLongPtr(EditHWnd, GWLP_USERDATA, (LONG_PTR)this);
  _hDefaultEditProc = (WNDPROC)::SetWindowLongPtr(EditHWnd, GWLP_WNDPROC, (LONG_PTR)wndDefaultEditProc);

  ListView_SetImageList(LViewHWnd, hImgList, LVSIL_SMALL);
  HdrHndl = ListView_GetHeader(LViewHWnd);
  if (HdrHndl != NULL)
  {
    hImgList = ImageList_LoadImage(App->GetInstance(),
      MAKEINTRESOURCE(IDR_ICONS),16,0,CLR_DEFAULT,IMAGE_BITMAP,LR_DEFAULTCOLOR);
    if (hImgList != NULL)
      Header_SetImageList(HdrHndl, hImgList);
  }

  TCHAR fontFace[] = TEXT("Courier New");
  CHARFORMAT2 charFormat;
  ZeroMemory(&charFormat, sizeof(charFormat));
  charFormat.cbSize = sizeof(charFormat);
  charFormat.dwMask = CFM_FACE | CFM_SIZE;
  charFormat.bPitchAndFamily = FF_MODERN | DEFAULT_PITCH;
  charFormat.yHeight = EditViewFontHeight;
  _tcscpy(charFormat.szFaceName, fontFace);
  SendMessage(EditHWnd, EM_SETCHARFORMAT, SPF_SETDEFAULT, (LPARAM) &charFormat);

  ::SendMessage(LViewHWnd, WM_SETFONT, (WPARAM)ListViewFont, (LPARAM)0);
  ListView_SetExtendedListViewStyle(LViewHWnd, LVS_EX_FULLROWSELECT);
  ::GetClientRect(LViewHWnd, &Rect);
  LvCol.mask = LVCF_FMT | LVCF_TEXT | LVCF_SUBITEM;
  LvCol.fmt = LVCFMT_LEFT;
  LvCol.iSubItem = COLUMN_TAG;
  LvCol.pszText = const_cast<LPTSTR>(_T("Tag"));
  rc = ListView_InsertColumn(LViewHWnd, 0, &LvCol);
  err = rc == -1 && !err ? TL_ERR_GENERAL : err;

  LvCol.iSubItem = COLUMN_FILENAME;
  LvCol.pszText = const_cast<LPTSTR>(_T("File"));
  rc = ListView_InsertColumn(LViewHWnd, 1, &LvCol);
  err = rc == -1 && !err ? TL_ERR_GENERAL : err;

  LvCol.iSubItem = COLUMN_EXCMD;
  LvCol.pszText = const_cast<LPTSTR>(_T("Text"));
  rc = ListView_InsertColumn(LViewHWnd, 2, &LvCol);
  err = rc == -1 && !err ? TL_ERR_GENERAL : err;

  LvCol.iSubItem = COLUMN_EXTTYPE;
  LvCol.pszText = const_cast<LPTSTR>(_T("Type"));
  rc = ListView_InsertColumn(LViewHWnd, 3, &LvCol);
  err = rc == -1 && !err ? TL_ERR_GENERAL : err;

  LvCol.iSubItem = COLUMN_EXTLINE;
  LvCol.pszText = const_cast<LPTSTR>(_T("Line"));
  rc = ListView_InsertColumn(LViewHWnd, 4, &LvCol);
  err = rc == -1 && !err ? TL_ERR_GENERAL : err;

  LvCol.iSubItem = COLUMN_EXTFIELDS;
  LvCol.pszText = const_cast<LPTSTR>(_T("Extra"));
  rc = ListView_InsertColumn(LViewHWnd, 5, &LvCol);
  err = rc == -1 && !err ? TL_ERR_GENERAL : err;

  ChangeColors();

  return err;
}

LRESULT APIENTRY TagLeetForm::InitialWndProc(HWND hwnd, UINT uMsg, WPARAM
  wParam, LPARAM lParam)
{
  TagLeetForm *This = (TagLeetForm *)GetWindowLongPtr(hwnd, 0);
  if (This != NULL)
    return This->WndProc(hwnd, uMsg, wParam, lParam);

  if (uMsg == WM_CREATE)
  {
    TL_ERR err;
    CREATESTRUCT *cs = (CREATESTRUCT *)lParam;
    This = (TagLeetForm *)cs->lpCreateParams;
    SetWindowLongPtr(hwnd, 0, (LONG_PTR)This);
    err = This->CreateListView(hwnd);
    return err ? -1 : 0;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void TagLeetForm::PostCloseMsg() const
{
  if (FormHWnd != NULL)
    ::PostMessage(FormHWnd, WM_CLOSE, 0, 0);
}

static void CleanExtType(const char **StrPtr, int *StrSizePtr)
{
  const char *Str = *StrPtr;
  int StrSize = *StrSizePtr;
// parse out what we want, e.g., remove "line:"
  *StrPtr = Str;
  *StrSizePtr = StrSize;
}

static void CleanExtLine(const char **StrPtr, int *StrSizePtr)
{
  const char *Str = *StrPtr;
  int StrSize = *StrSizePtr;
// parse out what we want, e.g., remove "line:"
  *StrPtr = Str;
  *StrSizePtr = StrSize;
}

static void CleanExtFields(const char **StrPtr, int *StrSizePtr)
{
  const char *Str = *StrPtr;
  int StrSize = *StrSizePtr;
// parse out what we want, e.g., remove "line:"
  *StrPtr = Str;
  *StrSizePtr = StrSize;
}

static void CleanExCmd(const char **StrPtr, int *StrSizePtr)
{
  const char *Str = *StrPtr;
  int StrSize = *StrSizePtr;

  if (StrSize >= 2 && Str[0] == '/' && Str[1] == '^')
  {
    Str += 2;
    StrSize -= 2;
  }
  if (StrSize >= 2 && Str[StrSize-1] == '/' && Str[StrSize-2] == '$')
  {
    StrSize -= 2;
  }

  while (StrSize > 0 && Str[0] == ' ' || Str[0] == '\t')
  {
    Str++;
    StrSize--;
  }

  *StrPtr = Str;
  *StrSizePtr = StrSize;
}

static void FileFromPath(const char **PathPtr, int *PathSizePtr)
{
  int i, last = 0;
  const char *Path = *PathPtr;
  int PathSize = *PathSizePtr;

  for (i = 0; i + 1 < PathSize; i++)
  {
    if (Path[i] == '\\' || Path[i] == '/')
      last = i + 1;
  }

  *PathPtr = Path + last;
  *PathSizePtr = PathSize - last;
}

static int comp_str(const char *str1, int length1,
  const char *str2, int length2, bool case_insensitive)
{
  int comp_val;
  int length;

  length = length1 <= length2 ? length1 : length2;
  comp_val = case_insensitive ?
    TagLEET::memicmp(str1, str2, length) :
    ::memcmp(str1, str2, length);
  if (comp_val == 0 && length1 != length2)
    comp_val = length1 < length2 ? -1 : 1;

  return comp_val;
}

static int comp_num(const char *str1, const char *str2)
{
  int comp_val = 0;
  int num1 = atoi( str1 );
  int num2 = atoi( str2 );

  if ( num1 < num2 )
      comp_val = -1;
  else if ( num1 > num2 )
      comp_val = 1;

  return comp_val;
}

/* First compare file names, if identical compare full path. */
static int comp_file_paths(const char *path1, int path_length1,
  const char *path2, int path_length2, bool case_insensitive)
{
  const char *str1;
  const char *str2;
  int comp_val, length1, length2;

  str1 = path1;
  length1 = path_length1;
  ::FileFromPath(&str1, &length1);

  str2 = path2;
  length2 = path_length2;
  ::FileFromPath(&str2, &length2);

  comp_val = comp_str(str1, length1, str2, length2, case_insensitive);
  if (comp_val != 0)
    return comp_val;

  return comp_str(path1, path_length1, path2, path_length2, case_insensitive);
}

int CALLBACK TagLeetForm::LvSortFunc(LPARAM Item1Ptr, LPARAM Item2Ptr,
    LPARAM FormPtr)
{
  TagList::TagListItem *Item1 = (TagList::TagListItem *)Item1Ptr;
  TagList::TagListItem *Item2 = (TagList::TagListItem *)Item2Ptr;
  TagLeetForm *Form = reinterpret_cast<TagLeetForm *>(FormPtr);
  int i;

  for (i = 0; i < ARRAY_SIZE(Form->SortOrder); i++)
  {
    int CompVal;
    const char *str1, *str2;
    int length1, length2;
    switch (Form->SortOrder[i] & 0x7F)
    {
      case COLUMN_TAG:
        str1 = Item1->Tag;
        length1 = (int)::strlen(str1);
        str2 = Item2->Tag;
        length2 = (int)::strlen(str2);
        CompVal = comp_str(str1, length1, str2, length2,
          Form->TList.TagsCaseInsensitive);
        break;
      case COLUMN_FILENAME:
        str1 = Item1->FileName;
        length1 = (int)::strlen(str1);
        str2 = Item2->FileName;
        length2 = (int)::strlen(str2);
        CompVal = comp_file_paths(str1, length1, str2, length2, false);
        break;
      case COLUMN_EXCMD:
        str1 = Item1->ExCmd;
        length1 = (int)::strlen(str1);
        ::CleanExCmd(&str1, &length1);
        str2 = Item2->ExCmd;
        length2 = (int)::strlen(str2);
        ::CleanExCmd(&str2, &length2);
        CompVal = comp_str(str1, length1, str2, length2,
          Form->TList.TagsCaseInsensitive);
        break;
      case COLUMN_EXTTYPE:
        str1 = Item1->ExtType;
        length1 = (int)::strlen(str1);
        str2 = Item2->ExtType;
        length2 = (int)::strlen(str2);
        CompVal = comp_str(str1, length1, str2, length2,
          Form->TList.TagsCaseInsensitive);
        break;
      case COLUMN_EXTLINE:
        str1 = Item1->ExtLine;
        str2 = Item2->ExtLine;
        CompVal = comp_num(str1, str2);
        break;
      case COLUMN_EXTFIELDS:
        str1 = Item1->ExtFields;
        length1 = (int)::strlen(str1);
        str2 = Item2->ExtFields;
        length2 = (int)::strlen(str2);
        CompVal = comp_str(str1, length1, str2, length2,
          Form->TList.TagsCaseInsensitive);
        break;
      default:
        continue;
    }

    if (CompVal != 0)
      return Form->SortOrder[i] & 0x80 ? -CompVal : CompVal;
  }
  return 0;
}

void TagLeetForm::SetColumnSortArrow(int ColumnIdx, bool Show, bool Down)
{
  HWND HdrHndl = ListView_GetHeader(LViewHWnd);
  HDITEM HdrItem;
  BOOL res;

  if (HdrHndl == NULL)
    return;

  ::memset(&HdrItem, 0, sizeof(HdrItem));
  HdrItem.mask = HDI_FORMAT | HDI_IMAGE;
  if (Show)
  {
    HdrItem.mask = HDI_FORMAT | HDI_IMAGE;
    res = Header_GetItem(HdrHndl, ColumnIdx, &HdrItem);
    if (!res)
      return;
    HdrItem.fmt |= HDF_IMAGE | HDF_BITMAP_ON_RIGHT;
    HdrItem.iImage = Down ? SORT_DOWN_IMG_IDX : SORT_UP_IMG_IDX;
  }
  else
  {
    HdrItem.mask = HDI_IMAGE;
    HdrItem.iImage = I_IMAGENONE;
  }
  Header_SetItem(HdrHndl, ColumnIdx, &HdrItem);
}

std::string ws2s(const std::wstring& wstr)
{
    int size_needed = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), int(wstr.length() + 1), 0, 0, 0, 0);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), int(wstr.length() + 1), &strTo[0], size_needed, 0, 0);
    return strTo;
}

void TagLeetForm::UpdateEditView()
{
    SetWindowTextA(EditHWnd, (LPCSTR)"");

    // Get current list view item (need filename and line number)
    int idx = ListView_GetNextItem( LViewHWnd, -1, LVIS_FOCUSED );
    TagList::TagListItem *Item;
    Item = GetItemData(idx);
    if (Item == NULL)
      return;

    int iLine = atoi(Item->ExtLine);
    if ( iLine == NULL )
        return;

    std::string strFileToOpen("");
    // relative filename, get tags file path to append
    if ( Item->FileName[0] != 'C' && 
         Item->FileName[1] != ':' )
    {
        // Get tagsfilepath (which contains the '\tags' filename, so remove it)
        char Path[TL_MAX_PATH + 16];
        int n = (int)::strlen(TList.TagsFilePath);
        ::memcpy(Path, TList.TagsFilePath, n * sizeof(char));
        if (Path[n-1] == 's' &&
            Path[n-2] == 'g' &&
            Path[n-3] == 'a' &&
            Path[n-4] == 't' &&
            Path[n-5] == '\\')
        {
          Path[n-5] = '\0';
        }

        strFileToOpen += Path;
        strFileToOpen += "\\";
    }
    // otherwise assume full file path/name
    strFileToOpen += Item->FileName;

    // open the file for reading
    std::ifstream file(strFileToOpen.c_str());
    std::string strFileContent;
    std::string strTemp;
    if (!file.is_open())
    {
        strFileContent += strFileToOpen.c_str();
        strFileContent += " NOT FOUND!";
        SetWindowTextA(EditHWnd, (LPCSTR)strFileContent.c_str());
        return;
    }

    int discard = max( 0, (iLine - g_PeekPre) );
    // throw away top lines
    int i;
    for (i = 1; i < discard; i++)
    {
        std::getline(file, strTemp);
    }

    // read PeekPre lines
    i = 0;
    while (i < g_PeekPre && std::getline(file, strTemp))
    {
        strFileContent += strTemp;
        strFileContent += "\r\n";
        i++;
    }

    // read THE LINE
    i = 0;
    while (i < 1 && std::getline(file, strTemp))
    {
        strFileContent += strTemp;
        strFileContent += "\r\n";
        i++;
    }

    // read PeekPost lines
    i = 0;
    while (i < g_PeekPost && std::getline(file, strTemp))
    {
        strFileContent += strTemp;
        strFileContent += "\r\n";
        i++;
    }

    SetWindowTextA(EditHWnd, (LPCSTR)strFileContent.c_str());

    CHARFORMAT2 charFormat;
    ZeroMemory(&charFormat, sizeof(charFormat));
    charFormat.cbSize = sizeof(charFormat);
    charFormat.dwMask = CFM_BOLD | CFM_ITALIC | CFM_COLOR | CFM_BACKCOLOR;
    charFormat.dwEffects = CFE_BOLD | CFE_ITALIC;
    charFormat.crTextColor = colorBg;
    charFormat.crBackColor = colorFg;

    // highlight tag line
    int selStart = (int)::SendMessage( EditHWnd, EM_LINEINDEX, g_PeekPre, 0 );
    int selEnd   = (int)::SendMessage( EditHWnd, EM_LINEINDEX, g_PeekPre+1, 0 );
    SendMessage(EditHWnd, EM_SETSEL , selStart, selEnd);
    SendMessage(EditHWnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM) &charFormat);

    // scroll to tag line
    POINT Pt;
    Pt.x = 0;
    Pt.y = max(0, ( ( g_PeekPre - 1 ) * (( App->GetEditViewFontHeight() / 20 ) + SCROLL_ADJUST ) ) );
    SendMessage(EditHWnd, EM_SETSCROLLPOS, 0, ( LPARAM )&Pt);
}

LRESULT TagLeetForm::editWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_CHAR:
      if (wParam == VK_ESCAPE)
      {
        PostCloseMsg();
        return 0;
      }
      else if (wParam == VK_TAB)
        ::SetFocus( LViewHWnd );
      break;
  }
  return ::CallWindowProc(_hDefaultEditProc, hwnd, uMsg, wParam, lParam);
}

LRESULT TagLeetForm::splitterWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_LBUTTONDOWN:
    {
      _isLeftButtonDown = TRUE;
      ::GetCursorPos(&_ptOldPos);
//      SetCursor(_hSplitterCursorUpDown);
      break;
    }

    case WM_LBUTTONUP:
    {
      RECT Rect;

      ::GetClientRect(FormHWnd, &Rect);
      _isLeftButtonDown = FALSE;

//      SetCursor(_hSplitterCursorUpDown);
      if (iSplitterPos < 50)
        iSplitterPos = 50;
      else if (iSplitterPos > (Rect.bottom - 100) && !DoAutoComplete)
        iSplitterPos = Rect.bottom - 100;
      break;
    }
    case WM_MOUSEMOVE:
    {
      if (_isLeftButtonDown == TRUE)
      {
        POINT pt;
        ::GetCursorPos(&pt);

        if (_ptOldPos.y != pt.y)
        {
          iSplitterPos -= _ptOldPos.y - pt.y;
          ::SendMessage(FormHWnd, WM_SIZE, 0, 0);
        }
        _ptOldPos = pt;
      }

//      SetCursor(_hSplitterCursorUpDown);
      break;
    }

    default:
        break;
  }
  return ::CallWindowProc(_hDefaultSplitterProc, hwnd, uMsg, wParam, lParam);
}

LRESULT TagLeetForm::WndProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_DESTROY:
      Destroy();
      return 0;

    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;

    case WM_NCDESTROY:
      return 0;

    case WM_SIZE:
      OnResize();
      break;

    case WM_PAINT:
      if (NeedUpdateColumns)
      {
        UpdateColumnWidths(LastMaxTagWidth, LastMaxFilenameWidth, LastMaxExCmdWidth,
                           LastMaxExtTypeWidth, LastMaxExtLineWidth, LastMaxExtFieldsWidth);
        NeedUpdateColumns = false;
      }
      break;

    case WM_TIMER:
    {
        KillTimer( FormHWnd, 1 );
        UpdateEditView();
        return FALSE;
    }

    case WM_DRAWITEM:
    {
        DRAWITEMSTRUCT* pDrawItemStruct = (DRAWITEMSTRUCT *)lParam;

        if (pDrawItemStruct->hwndItem == SplitterHWnd)
        {
            RECT   rc      = pDrawItemStruct->rcItem;
            HDC    hDc     = pDrawItemStruct->hDC;
            HBRUSH bgbrush = ::CreateSolidBrush(::GetSysColor(COLOR_BTNFACE));

            /* fill background */
            ::FillRect(hDc, &rc, bgbrush);

            ::DeleteObject(bgbrush);
            return TRUE;
        }
        break;
    }

    case WM_NOTIFY:
    {
      LPNMHDR pnmh = (LPNMHDR)lParam;
      switch (pnmh->code)
      {
        case NM_KILLFOCUS:
          if (GetFocus() != EditHWnd && GetFocus() != SplitterHWnd)
            PostCloseMsg();
          return 0;
        case NM_DBLCLK:
          if ( DoAutoComplete )
            DoSelectedAutoComplete();
          else
            GoToSelectedTag();
          break;
        case LVN_KEYDOWN:
        {
          LPNMLVKEYDOWN pnkd = (LPNMLVKEYDOWN) lParam;
          switch (pnkd->wVKey)
          {
            case VK_TAB:
            {
              if ( !DoAutoComplete )
                ::SetFocus( EditHWnd );
              break;
            }
            case VK_RETURN:
            case VK_SPACE:
            {
              // tooltip
              if ( DoAutoComplete )
                DoSelectedAutoComplete();
              else
                GoToSelectedTag();
              break;
            }
            case VK_ADD:
              if (::GetKeyState(VK_CONTROL) & 0x8000)
              {
                ResizeListViewFont(1, false);
                ResizeEditViewFont(1, false);
              }
              else
              {
                ResizeForm(10);
              }
              break;
            case VK_SUBTRACT:
              if (::GetKeyState(VK_CONTROL) & 0x8000)
              {
                ResizeListViewFont(-1, false);
                ResizeEditViewFont(-1, false);
              }
              else
              {
                ResizeForm(-10);
              }
              break;
            case VK_MULTIPLY:
              if (::GetKeyState(VK_CONTROL) & 0x8000 && ::GetKeyState(VK_MENU) & 0x8000)
              {
                if ( g_UpdateOnSave )
                {
                  g_UpdateOnSave = false;
                  UpdateStatusText(TEXT("Update on Save DISabled"));
                }
                else
                {
                  g_UpdateOnSave = true;
                  UpdateStatusText(TEXT("Update on Save ENabled"));
                }
              }
              else if (::GetKeyState(VK_CONTROL) & 0x8000)
              {
                if ( g_useSciAutoC )
                {
                  g_useSciAutoC = false;
                  UpdateStatusText(TEXT("Do NOT Use Scintilla Autocomplete"));
                }
                else
                {
                  g_useSciAutoC = true;
                  UpdateStatusText(TEXT("Use Scintilla Autocomplete"));
                }
              }
              else
              {
                if ( g_useNppColors )
                {
                  SetSysColors();
                  g_useNppColors = false;
                }
                else
                {
                  SetNppColors();
                  g_useNppColors = true;
                }
              }
              ChangeColors();
              break;
            case VK_DIVIDE:
              if (::GetKeyState(VK_CONTROL) & 0x8000)
              {
                ResizeListViewFont(0, true);
                ResizeEditViewFont(0, true);
              }
              else
              {
                App->SetFormSize(0, 0, true);
                ResizeForm(0);
              }
              break;
            case VK_ESCAPE:
              PostCloseMsg();
              return 0;
          }
          break;
        }
        case LVN_ITEMCHANGED:
        {
          NMLISTVIEW *LvInfo = (NMLISTVIEW *)lParam;
          if (LvInfo->uNewState & LVIS_FOCUSED)
            UpdateStatusLine(LvInfo->iItem);

          KillTimer( FormHWnd, 1 );
          SetTimer( FormHWnd, 1, 200, NULL );

          return 0;
        }
        case LVN_COLUMNCLICK:
        {
          NMLISTVIEW *LvInfo = (NMLISTVIEW *)lParam;
          int i;
          for (i = 0; i < ARRAY_SIZE(SortOrder); i++)
          {
            if ((SortOrder[i] & 0x7F) != LvInfo->iSubItem)
              continue;
            if (i == 0)
            {
              SortOrder[0] ^= 0x80;
              SetColumnSortArrow(SortOrder[0] & 0x7F, true,
                SortOrder[0] & 0x80 ? true : false);
            }
            else
            {
              SetColumnSortArrow(SortOrder[0] & 0x7F, false);
              SetColumnSortArrow(SortOrder[i] & 0x7F, true,
                SortOrder[i] & 0x80 ? true : false);

              uint8_t Tmp = SortOrder[i];
              while (i > 0)
              {
                SortOrder[i] = SortOrder[i-1];
                i--;
              }
              SortOrder[0] = Tmp;
            }
            ListView_SortItems(LViewHWnd, LvSortFunc, (WPARAM)this);
            break;
          }
          return 0;
        }

      }
      return 0;
    }
  }
  return DefWindowProc(hwnd,uMsg, wParam, lParam);
}

void TagLeetForm::UpdateStatusText(std::wstring message)
{
  ::SetWindowText(StatusHWnd, message.c_str());
}

void TagLeetForm::UpdateStatusLine(int FocusIdx)
{
  TagList::TagListItem *Item;
  TCHAR TmpBuff[TL_MAX_PATH];

  if (StatusHWnd == NULL)
    return;

  if (FocusIdx == -1)
  {
    ::SetWindowText(StatusHWnd, TEXT(""));
    return;
  }

  Item = GetItemData(FocusIdx);
  if (Item == NULL)
    return;

  str_to_TSTR(Item->FileName, -1, TmpBuff, ARRAY_SIZE(TmpBuff));
  ::SetWindowText(StatusHWnd, TmpBuff);
}

TagList::TagListItem *TagLeetForm::GetItemData(int ItemIdx)
{
  LVITEM LvItem;
  LvItem.mask = LVIF_PARAM;
  LvItem.iItem = ItemIdx;
  ListView_GetItem(LViewHWnd, &LvItem);
  return (TagList::TagListItem *)LvItem.lParam;
}

void TagLeetForm::SetItemText(int ColumnIdx, int SubItem, const char *Str,
  int *Width, void (*ModifyStr)(const char **Str, int *StrSize))
{
  LVITEM LvItem;
  int StrSize = (int)::strlen(Str);
  TCHAR TmpStr[300];

  if (ModifyStr != NULL)
    ModifyStr(&Str, &StrSize);
  ::str_to_TSTR(Str, StrSize, TmpStr, ARRAY_SIZE(TmpStr));

  if (Width != NULL)
  {
    int TextWidth = ListView_GetStringWidth(LViewHWnd, TmpStr);
    if (TextWidth > *Width)
      *Width = TextWidth;
  }

  LvItem.mask = LVIF_TEXT;
  LvItem.iItem = ColumnIdx;
  LvItem.pszText = TmpStr;
  LvItem.iSubItem = SubItem;
  ListView_SetItem(LViewHWnd, &LvItem);
}

void TagLeetForm::setDoPrefixMatch()
{
  DoPrefixMatch = true;
}

void TagLeetForm::setDoAutoComplete()
{
  DoAutoComplete = true;
}

void TagLeetForm::RefreshList(TagLookupContext *TLCtx)
{
  DoPrefixMatch = TList.TagsFilePath == NULL ||
    ::strcmp(TList.TagsFilePath, TLCtx->TagsFilePath) != 0 ? false :
    !DoPrefixMatch;

  ::SendMessage(LViewHWnd, WM_SETREDRAW, FALSE, 0);
  SetListFromTag(TLCtx);
  ::SendMessage(LViewHWnd, WM_SETREDRAW, TRUE, 0);
  ::RedrawWindow(LViewHWnd, NULL, NULL,
    RDW_ERASE  | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

TL_ERR TagLeetForm::PopulateTagListHelperGlobal(TagLookupContext *TLCtx, TagFile *tf)
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

TL_ERR TagLeetForm::PopulateTagListHelper(TagLookupContext *TLCtx, TagFile *tf)
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

TL_ERR TagLeetForm::PopulateTagList(TagLookupContext *TLCtx)
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
        err = PopulateTagListHelperGlobal(TLCtx, &tf);
        if (err)
            return err;
    }
  }

  return TL_ERR_OK;
}

void TagLeetForm::UpdateColumnWidths(int MaxTagWidth, int MaxFilenameWidth,
    int MaxExCmdWidth, int MaxExtTypeWidth,
    int MaxExtLineWidth, int MaxExtFieldsWidth)
{
  RECT Rect;
  int TotalWidth, MaxWidth;
  int TagWidth, FilenameWidth, ExCmdWidth, ExtTypeWidth, ExtLineWidth, ExtFieldsWidth, ItemsHight;
  DWORD ApproxRect;

  LastMaxTagWidth = MaxTagWidth;
  LastMaxFilenameWidth = MaxFilenameWidth;
  LastMaxExCmdWidth = MaxExCmdWidth;
  LastMaxExtTypeWidth = MaxExtTypeWidth;
  LastMaxExtLineWidth = MaxExtLineWidth;
  LastMaxExtFieldsWidth = MaxExtFieldsWidth;

  MaxTagWidth += 28;
  MaxFilenameWidth += 12;
  MaxExCmdWidth += 12;
  MaxExtTypeWidth += 12;
  MaxExtLineWidth += 12;
  MaxExtFieldsWidth += 12;

  ::ShowScrollBar(LViewHWnd, SB_BOTH, FALSE);
  ::GetClientRect(LViewHWnd, &Rect);
  /* Test if we will need vertical scrollbar or not so we can take its width
   * into account */
  ApproxRect = ListView_ApproximateViewRect(LViewHWnd, -1, -1, -1);
  ItemsHight = HIWORD(ApproxRect);
  if (ItemsHight >= Rect.bottom)
  {
    ::ShowScrollBar(LViewHWnd, SB_VERT, TRUE);
    ::GetClientRect(LViewHWnd, &Rect);
  }
  MaxWidth = Rect.right;
  TotalWidth = MaxTagWidth + MaxFilenameWidth + MaxExCmdWidth +
               MaxExtTypeWidth + MaxExtLineWidth + MaxExtFieldsWidth;
  if (TotalWidth < MaxWidth)
  {
    /* Try default 40%-30%-30% */
    TagWidth = MaxWidth * 15 / 100;
    FilenameWidth = MaxWidth * 20 / 100;
    ExCmdWidth = MaxWidth * 40 / 100;
    ExtTypeWidth = MaxWidth * 5 / 100;
    ExtLineWidth = MaxWidth * 5 / 100;
    ExtFieldsWidth = MaxWidth - TagWidth - FilenameWidth - ExCmdWidth - ExtTypeWidth - ExtLineWidth;
    /* If no fit then just expand each 'max' */
    if (MaxTagWidth > TagWidth || MaxFilenameWidth > FilenameWidth ||
      MaxExCmdWidth > ExCmdWidth || MaxExtTypeWidth > ExtTypeWidth ||
      MaxExtLineWidth > ExtLineWidth || MaxExtFieldsWidth > ExtFieldsWidth)
    {
      int Extra = (MaxWidth - TotalWidth)/6;
      TagWidth = MaxTagWidth + Extra;
      FilenameWidth = MaxFilenameWidth + Extra;
      ExCmdWidth = MaxExCmdWidth + Extra;
      ExtTypeWidth = MaxExtTypeWidth + Extra;
      ExtLineWidth = MaxExtLineWidth + Extra;
      ExtFieldsWidth = MaxExtFieldsWidth + Extra;
    }
  }
  else
  {
    int Remaining = MaxWidth;
    /* Give tag up to 55% */
    TagWidth = MaxTagWidth * 100 <= Remaining * 35 ? MaxTagWidth :
      Remaining * 35 / 100;
    Remaining -= TagWidth;
    /* Give filename up to 80% of the remaining width */
    FilenameWidth = MaxFilenameWidth * 100 <= Remaining * 80 ? MaxFilenameWidth :
      Remaining * 80 / 100;
  }

  ExtFieldsWidth = MaxWidth - TagWidth - FilenameWidth - ExCmdWidth - ExtTypeWidth - ExtLineWidth;
  ListView_SetColumnWidth(LViewHWnd, 0, TagWidth);
  ListView_SetColumnWidth(LViewHWnd, 1, FilenameWidth);
  ListView_SetColumnWidth(LViewHWnd, 2, ExCmdWidth);
  ListView_SetColumnWidth(LViewHWnd, 3, ExtTypeWidth);
  ListView_SetColumnWidth(LViewHWnd, 4, ExtLineWidth);
  ListView_SetColumnWidth(LViewHWnd, 5, ExtFieldsWidth);
}

TL_ERR TagLeetForm::SetListFromTag(TagLookupContext *TLCtx)
{
  TL_ERR err;
  int FocusIdx, Idx;
  TagList::TagListItem *Item;
  int MaxTagWidth = 0;
  int MaxFilenameWidth = 0;
  int MaxExCmdWidth = 0;
  int MaxExtTypeWidth = 0;
  int MaxExtLineWidth = 0;
  int MaxExtFieldsWidth = 0;

  /* If there is currently an item with focus, save its location so we could
   * restore it */
  FocusIdx = ListView_GetNextItem(LViewHWnd, -1, LVNI_FOCUSED);

  ListView_DeleteAllItems(LViewHWnd);
  err = PopulateTagList(TLCtx);
  if (err)
    return err;

  TLCtx->GetLineNumFromTag(DoPrefixMatch, &TList);
  for (Item = TList.List, Idx=0; Item != NULL; Item = Item->Next, Idx++)
  {
    LVITEM lvItem;
    int LvIdx;

    lvItem.mask = LVIF_IMAGE | LVIF_PARAM | LVIF_STATE;
    lvItem.iItem = Idx;
    lvItem.iSubItem = 0;
    lvItem.iImage = KindToIndex[Item->Kind];
    lvItem.lParam = (LPARAM)Item;
    lvItem.stateMask = 0xFF;
    lvItem.state = 0;
    LvIdx = ListView_InsertItem(LViewHWnd, &lvItem);
    if (LvIdx == -1)
      break;

    SetItemText(LvIdx, COLUMN_TAG, Item->Tag, &MaxTagWidth);
    SetItemText(LvIdx, COLUMN_FILENAME, Item->FileName, &MaxFilenameWidth, ::FileFromPath);
    SetItemText(LvIdx, COLUMN_EXCMD, Item->ExCmd, &MaxExCmdWidth, ::CleanExCmd);
    SetItemText(LvIdx, COLUMN_EXTTYPE, Item->ExtType, &MaxExtTypeWidth, ::CleanExtType);
    SetItemText(LvIdx, COLUMN_EXTLINE, Item->ExtLine, &MaxExtLineWidth, ::CleanExtLine);
    SetItemText(LvIdx, COLUMN_EXTFIELDS, Item->ExtFields, &MaxExtFieldsWidth, ::CleanExtFields);
  }

  if (Idx > 0)
  {
    UpdateColumnWidths(MaxTagWidth, MaxFilenameWidth, MaxExCmdWidth, MaxExtTypeWidth, MaxExtLineWidth, MaxExtFieldsWidth);
    UINT state = LVIS_FOCUSED | LVIS_SELECTED;
    if (FocusIdx == -1)
      FocusIdx = ListView_GetNextItem(LViewHWnd, -1, LVNI_ALL);
    else if (FocusIdx >= Idx)
      FocusIdx = Idx - 1;

    ListView_SortItems(LViewHWnd, LvSortFunc, (WPARAM)this);
    ListView_SetItemState(LViewHWnd, FocusIdx, state, state);
    ListView_EnsureVisible(LViewHWnd, FocusIdx, FALSE);
    UpdateStatusLine(FocusIdx);
  }
  else
  {
    UpdateStatusLine(-1);
  }
  return TL_ERR_OK;
}

void TagLeetForm::DoSelectedAutoComplete()
{
    LVITEM LvItem;
    TCHAR  tag[MAX_PATH] = {0};
    int idx = ListView_GetNextItem( LViewHWnd, -1, LVIS_FOCUSED );

    // Autocomplete
    memset( &LvItem, 0, sizeof(LvItem) );
    LvItem.mask       = LVIF_TEXT;
    LvItem.iSubItem   = COLUMN_TAG;
    LvItem.pszText    = tag;
    LvItem.cchTextMax = MAX_PATH;
    LvItem.iItem      = idx;

    SendMessage( LViewHWnd, LVM_GETITEMTEXT, idx, (LPARAM)&LvItem );
    std::string sTag = ws2s(tag);

    SendMessage( App->getCurrScintilla(), SCI_REPLACESEL, 0, ( LPARAM) sTag.c_str() );

    // Tooltip
    memset( &LvItem, 0, sizeof(LvItem) );
    LvItem.mask       = LVIF_TEXT;
    LvItem.iSubItem   = COLUMN_EXCMD;
    LvItem.pszText    = tag;
    LvItem.cchTextMax = MAX_PATH;
    LvItem.iItem      = idx;

    SendMessage( LViewHWnd, LVM_GETITEMTEXT, idx, (LPARAM)&LvItem );
    sTag = ws2s(tag);

    int pos = ( int )::SendMessage( App->getCurrScintilla(), SCI_GETCURRENTPOS, 0, 0 );
    SendMessage( App->getCurrScintilla(), SCI_CALLTIPSHOW, ( WPARAM )pos, ( LPARAM)sTag.c_str() );

    PostCloseMsg();
}

void TagLeetForm::GoToSelectedTag()
{
  int idx = ListView_GetNextItem(LViewHWnd, -1, LVIS_FOCUSED);

  if (idx != -1)
  {
    TagList::TagListItem *Item = GetItemData(idx);
    if (Item != NULL)
    {
      OpenTagLoc(Item);
      return;
    }
  }
  PostCloseMsg();
}

TL_ERR TagLeetForm::OpenTagLoc(TagList::TagListItem *Item)
{
  TL_ERR err;
  FileReader *fr;
  char FileNameBuff[TL_MAX_PATH];
  TCHAR LocFileNameBuff[TL_MAX_PATH];
  NppLoc TagLoc;

  fr = FileReader::FileReaderCreate();
  if (fr == NULL)
    return TL_ERR_MEM_ALLOC;

  err = TList.OpenSrcFile(Item, fr, FileNameBuff, sizeof(FileNameBuff));
  delete fr;
  if (err)
  {
    return err;
  }
  else
  {
    NppFileLineIterator li(App, FileNameBuff);
    err = TList.FindLineNumberInFile(&li, Item, &TagLoc.I.LineNum);
    if (err)
      TagLoc.I.LineNum = 0;
  }

  str_to_TSTR(FileNameBuff, -1, LocFileNameBuff, ARRAY_SIZE(LocFileNameBuff));
  TagLoc.FileName = LocFileNameBuff;
  TagLoc.I.XPos = 0;
  TagLoc.I.TopVisibleLine = -1;

  BackLocBank->PushLocToBack(&BackLoc);
  App->GoToFileLine(&TagLoc, Item->Tag);
  return TL_ERR_OK;
}
