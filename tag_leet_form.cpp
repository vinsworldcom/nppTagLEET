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

#include "tag_leet_form.h"

#include <tchar.h>
#include <commctrl.h>
#include <malloc.h>

#include "resource.h"

using namespace TagLEET_NPP;

#define SORT_UP_IMG_IDX    13
#define SORT_DOWN_IMG_IDX  14

TagLeetForm::TagLeetForm(NppCallContext *NppC)
{
  int i;

  App = NppC->App;
  FormHWnd = NULL;
  LViewHWnd = NULL;
  StatusHWnd = NULL;
  DoPrefixMatch = false;
  ::memset(&BackLoc, 0, sizeof(BackLoc));
  BackLocBank = NppC->LocBank;

  LastMaxTagWidth = 0;
  LastMaxFilenameWidth = 0;
  LastMaxExCmdWidth = 0;
  NeedUpdateColumns = false;

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

  FormHWnd = ::CreateWindowEx(
    WS_EX_TOOLWINDOW,
    TagLeetApp::WindowClassName,
    TEXT("TagLEET for Notepad++"),
    WS_SIZEBOX | WS_SYSMENU,
    Pt.x, Pt.y, FormWidth, FormHeight,
    NppC->SciHndl,
    NULL,
    App->GetInstance(),
    this);

  if (FormHWnd == NULL || LViewHWnd == NULL)
    return TL_ERR_GENERAL;

  UpdateColumnWidths(0, 0, 0, 0);
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
  int StatusHeight;
  int FocusIdx;

  ::GetClientRect(FormHWnd, &Rect);
  StatusHeight = App->GetStatusHeight();
  ::SetWindowPos(LViewHWnd, NULL, 0,
    0, Rect.right, Rect.bottom - StatusHeight,
    SWP_NOZORDER);
  FocusIdx = ListView_GetNextItem(LViewHWnd, -1, LVNI_FOCUSED);
  ListView_EnsureVisible(LViewHWnd, FocusIdx, FALSE);
  ::SetWindowPos(StatusHWnd, NULL,
    0, Rect.bottom - StatusHeight, Rect.right, StatusHeight,
    SWP_NOZORDER);

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
  LVCOLUMN LvCol;
  HWND HdrHndl;

  hImgList = ImageList_LoadImage(App->GetInstance(),
    MAKEINTRESOURCE(IDR_ICONS),16,0,CLR_DEFAULT,IMAGE_BITMAP,LR_DEFAULTCOLOR);
  if (hImgList == NULL)
    return TL_ERR_GENERAL;

  ::GetClientRect(hwnd, &Rect);
  StatusHWnd = ::CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), NULL,
    WS_CHILD | WS_VISIBLE | ES_READONLY,
    0, Rect.bottom - StatusHeight, Rect.right, StatusHeight,
    hwnd, NULL, App->GetInstance(), NULL);
  if (StatusHWnd != NULL && StatusFont != NULL)
    ::PostMessage(StatusHWnd, WM_SETFONT, (WPARAM)StatusFont, (LPARAM)0);

  LViewHWnd = ::CreateWindow(WC_LISTVIEW, NULL,
    WS_CHILD | LVS_REPORT | LVS_SINGLESEL | WS_VISIBLE,
    0, 0, Rect.right, Rect.bottom - StatusHeight,
    hwnd, NULL, App->GetInstance(), NULL);
  if (LViewHWnd == NULL)
    return TL_ERR_GENERAL;

  ListView_SetImageList(LViewHWnd, hImgList, LVSIL_SMALL);
  HdrHndl = ListView_GetHeader(LViewHWnd);
  if (HdrHndl != NULL)
  {
    hImgList = ImageList_LoadImage(App->GetInstance(),
      MAKEINTRESOURCE(IDR_ICONS),16,0,CLR_DEFAULT,IMAGE_BITMAP,LR_DEFAULTCOLOR);
    if (hImgList != NULL)
      Header_SetImageList(HdrHndl, hImgList);
  }

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
  LvCol.pszText = const_cast<LPTSTR>(_T("Line"));
  rc = ListView_InsertColumn(LViewHWnd, 2, &LvCol);
  err = rc == -1 && !err ? TL_ERR_GENERAL : err;

  LvCol.iSubItem = COLUMN_EXTFIELDS;
  LvCol.pszText = const_cast<LPTSTR>(_T("Extra"));
  rc = ListView_InsertColumn(LViewHWnd, 3, &LvCol);
  err = rc == -1 && !err ? TL_ERR_GENERAL : err;

  ListView_SetBkColor(LViewHWnd, GetSysColor(COLOR_INFOBK));
  ListView_SetTextBkColor(LViewHWnd, GetSysColor(COLOR_INFOBK));
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

static void CleanExtFields(const char **StrPtr, int *StrSizePtr)
{
// TODO:2019-04-06:MVINCENT:    
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
        UpdateColumnWidths(LastMaxTagWidth, LastMaxFilenameWidth,
          LastMaxExCmdWidth, LastMaxExtFieldsWidth);
        NeedUpdateColumns = false;
      }
      break;

    case WM_NOTIFY:
    {
      LPNMHDR pnmh = (LPNMHDR)lParam;
      switch (pnmh->code)
      {
        case NM_KILLFOCUS :
          PostCloseMsg();
          return 0;
        case NM_DBLCLK:
          GoToSelectedTag();
          break;
        case LVN_KEYDOWN:
        {
          LPNMLVKEYDOWN pnkd = (LPNMLVKEYDOWN) lParam;
          switch (pnkd->wVKey)
          {
            case VK_RETURN:
            case VK_SPACE:
            {
              GoToSelectedTag();
              break;
            }
            case VK_ADD:
              if (::GetKeyState(VK_CONTROL) & 0x8000)
              {
                ResizeListViewFont(1, false);
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
              }
              else
              {
                ResizeForm(-10);
              }
              break;
            case VK_DIVIDE:
              if (::GetKeyState(VK_CONTROL) & 0x8000)
              {
                ResizeListViewFont(0, true);
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
  if (DoPrefixMatch)
    return err;

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
      return err;
  }

  return TL_ERR_OK;
}

void TagLeetForm::UpdateColumnWidths(int MaxTagWidth, int MaxFilenameWidth,
  int MaxExCmdWidth, int MaxExtFieldsWidth)
{
  RECT Rect;
  int TotalWidth, MaxWidth;
  int TagWidth, FilenameWidth, ExCmdWidth, ExtFieldsWidth, ItemsHight;
  DWORD ApproxRect;

  LastMaxTagWidth = MaxTagWidth;
  LastMaxFilenameWidth = MaxFilenameWidth;
  LastMaxExCmdWidth = MaxExCmdWidth;
  LastMaxExtFieldsWidth = MaxExtFieldsWidth;

  MaxTagWidth += 28;
  MaxFilenameWidth += 12;
  MaxExCmdWidth += 12;
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
  TotalWidth = MaxTagWidth + MaxFilenameWidth + MaxExCmdWidth + MaxExtFieldsWidth;
  if (TotalWidth < MaxWidth)
  {
    /* Try default 40%-30%-30% */
    TagWidth = MaxWidth * 35 / 100;
    FilenameWidth = MaxWidth * 20 / 100;
    ExCmdWidth = MaxWidth * 20 / 100;
    ExtFieldsWidth = MaxWidth - TagWidth - FilenameWidth - ExCmdWidth;
    /* If no fit then just expand each 'max' */
    if (MaxTagWidth > TagWidth || MaxFilenameWidth > FilenameWidth ||
      MaxExCmdWidth > ExCmdWidth || MaxExtFieldsWidth > ExtFieldsWidth)
    {
      int Extra = (MaxWidth - TotalWidth)/4;
      TagWidth = MaxTagWidth + Extra;
      FilenameWidth = MaxFilenameWidth + Extra;
      ExCmdWidth = MaxExCmdWidth + Extra;
    }
  }
  else
  {
    int Remaining = MaxWidth;
    /* Give tag up to 55% */
    TagWidth = MaxTagWidth * 100 <= Remaining * 55 ? MaxTagWidth :
      Remaining * 55 / 100;
    Remaining -= TagWidth;
    /* Give filename up to 80% of the remaining width */
    FilenameWidth = MaxFilenameWidth * 100 <= Remaining * 80 ? MaxFilenameWidth :
      Remaining * 80 / 100;
  }

  ExtFieldsWidth = MaxWidth - TagWidth - FilenameWidth - ExCmdWidth;
  ListView_SetColumnWidth(LViewHWnd, 0, TagWidth);
  ListView_SetColumnWidth(LViewHWnd, 1, FilenameWidth);
  ListView_SetColumnWidth(LViewHWnd, 2, ExCmdWidth);
  ListView_SetColumnWidth(LViewHWnd, 3, ExtFieldsWidth);
}

TL_ERR TagLeetForm::SetListFromTag(TagLookupContext *TLCtx)
{
  TL_ERR err;
  int FocusIdx, Idx;
  TagList::TagListItem *Item;
  int MaxTagWidth = 0;
  int MaxFilenameWidth = 0;
  int MaxExCmdWidth = 0;
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
// TODO:2019-04-06:MVINCENT:    SetItemText(LvIdx, COLUMN_EXTFIELDS, Item->ExtFields, &MaxExtFieldsWidth, ::CleanExtFields);
  }

  if (Idx > 0)
  {
    UpdateColumnWidths(MaxTagWidth, MaxFilenameWidth, MaxExCmdWidth, MaxExtFieldsWidth);
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

