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

#ifndef TAG_LEET_FORM_H
#define TAG_LEET_FORM_H

#include "tag_leet_app.h"
#include <commctrl.h>
#include <string>
#include "tag_engine/tag_file.h"
#include "tag_engine/tag_list.h"

std::string ws2s(const std::wstring&);

namespace TagLEET_NPP {
using namespace TagLEET;

class TagLeetApp;
class NppCallContext;

class TagLeetForm {
public :
  TagLeetForm(NppCallContext *NppC);
  virtual ~TagLeetForm();
  void RefreshList(TagLookupContext *TLCtx);
  static LRESULT APIENTRY InitialWndProc(HWND hwnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam);
  TL_ERR CreateWnd(TagLookupContext *TLCtx);
  void PostCloseMsg() const;
  void setDoPrefixMatch();
  void setDoAutoComplete();

private:
  TL_ERR CreateListView(HWND hwnd);
  void Destroy();
  void SetItemText(int ColumnIdx, int SubItem, const char *Str, int *Width = NULL,
    void (*ModifyStr)(const char **Str, int *StrSize) = NULL);
  TL_ERR SetListFromTag(TagLookupContext *TLCtx);
  void UpdateColumnWidths(int MaxTagWidth, int MaxFilenameWidth,
    int MaxExCmdWidth, int MaxExtTypeWidth,
    int MaxExtLineWidth, int MaxExtFieldsWidth);
  TL_ERR PopulateTagListHelperGlobal(TagLookupContext *TLCtx, TagFile *tf);
  TL_ERR PopulateTagListHelper(TagLookupContext *TLCtx, TagFile *tf);
  TL_ERR PopulateTagList(TagLookupContext *TLCtx);
  void GoToSelectedTag();
  void DoSelectedAutoComplete();
  LRESULT WndProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  TagList::TagListItem *GetItemData(int ItemIdx);
  TL_ERR OpenTagLoc(TagList::TagListItem *Item);
  void UpdateEditView();
  void UpdateStatusText(std::wstring message);
  void UpdateStatusLine(int FocusIdx);
  static int CALLBACK LvSortFunc(LPARAM Item1Ptr, LPARAM Item2Ptr,
    LPARAM FormPtr);
  void SetColumnSortArrow(int ColumnIdx, bool Show, bool Down=false);
  void ResizeForm(int change);
  void ResizeListViewFont(int change, bool reset);
  void ResizeEditViewFont(int change, bool reset);
  void SetNppColors();
  void SetSysColors();
  void ChangeColors();
  void OnResize();

  LRESULT editWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK wndDefaultEditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    return (((TagLeetForm *)(::GetWindowLongPtr(hwnd, GWLP_USERDATA)))->editWndProc(hwnd, uMsg, wParam, lParam));
  };

  LRESULT splitterWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK wndDefaultSplitterProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    return (((TagLeetForm *)(::GetWindowLongPtr(hwnd, GWLP_USERDATA)))->splitterWndProc(hwnd, uMsg, wParam, lParam));
  };

  TagLeetApp *App;
  TagList TList;
  UINT KindToIndex[TAG_KIND_LAST];
  bool DoPrefixMatch;
  bool DoAutoComplete;
  uint8_t SortOrder[6];
  int LastMaxTagWidth;
  int LastMaxFilenameWidth;
  int LastMaxExCmdWidth;
  int LastMaxExtTypeWidth;
  int LastMaxExtLineWidth;
  int LastMaxExtFieldsWidth;
  bool NeedUpdateColumns;
  COLORREF colorFg;
  COLORREF colorBg;

  /* Location in NPP file during tag open. If we end up 'going' to a tag's
   * location this location will be pushed into the 'Back' queue */
  NppLoc BackLoc;
  NppLocBank *BackLocBank;

  HWND FormHWnd;
  HWND LViewHWnd;
  HWND SplitterHWnd;
  HWND StatusHWnd;

  HWND EditHWnd;
  WNDPROC _hDefaultEditProc;

  /* splitter values */
  WNDPROC _hDefaultSplitterProc;
  POINT   _ptOldPos;
  POINT   _ptOldPosHorizontal;
  BOOL    _isLeftButtonDown;
  int     iSplitterPos;
};

enum {
  COLUMN_TAG = 0,
  COLUMN_FILENAME,
  COLUMN_EXCMD,
  COLUMN_EXTTYPE,
  COLUMN_EXTLINE,
  COLUMN_EXTFIELDS
};

} /* namespace TagLEET_NPP */

#endif /* TAG_LEET_FORM_H */
