#include <windows.h>
#include <shlwapi.h>

#include "tag_leet_app.h"
#include "resource.h"

using namespace TagLEET_NPP;

extern bool g_useNppColors;
extern bool g_useNppAutoC;
extern bool g_UpdateOnSave;
extern int  g_PeekPre;
extern int  g_PeekPost;
extern char g_GlobalTagsFile[TL_MAX_PATH];

#define MAX_LINES 25

// TODO:2020-06-30:MVINCENT:use button IDC_BTN_GLOBALTAGSFILE for file picker

void refreshSettings( HWND hWndDlg )
{
    SendMessage( GetDlgItem( hWndDlg, IDC_CHK_NPPCOLORS ), BM_SETCHECK,
                 ( WPARAM )( g_useNppColors ? 1 : 0 ), 0 );
    SendMessage( GetDlgItem( hWndDlg, IDC_CHK_NPPAUTOC ), BM_SETCHECK,
                 ( WPARAM )( g_useNppAutoC ? 1 : 0 ), 0 );
    SendMessage( GetDlgItem( hWndDlg, IDC_CHK_UPDSAVE ), BM_SETCHECK,
                 ( WPARAM )( g_UpdateOnSave ? 1 : 0 ), 0 );

    TCHAR strHint[50] = {0};
    wsprintf( strHint, TEXT( "%d" ), g_PeekPre );
    SendMessage( GetDlgItem( hWndDlg, IDC_EDT_PEEKPRE ), WM_SETTEXT, 0,
                 ( LPARAM )strHint );

    wsprintf( strHint, TEXT( "%d" ), g_PeekPost );
    SendMessage( GetDlgItem( hWndDlg, IDC_EDT_PEEKPOST ), WM_SETTEXT, 0,
                 ( LPARAM )strHint );

    SendMessageA( GetDlgItem( hWndDlg, IDC_EDT_GLOBALTAGSFILE ), WM_SETTEXT, 0,
                  ( LPARAM )g_GlobalTagsFile );
}

INT_PTR CALLBACK SettingsDlg( HWND hWndDlg, UINT msg, WPARAM wParam,
                              LPARAM lParam )
{

    switch ( msg )
    {
        case WM_INITDIALOG:
        {
            refreshSettings( hWndDlg );
            return TRUE;
        }

        case WM_CLOSE:
        {
            PostMessage( hWndDlg, WM_DESTROY, 0, 0 );
            return TRUE;
        }

        case WM_DESTROY:
        {
            EndDialog( hWndDlg, 0 );
            return TRUE;
        }

        case WM_COMMAND:
        {
            switch ( wParam )
            {
                case IDB_OK:
                {
                    SendMessageA( GetDlgItem( hWndDlg, IDC_EDT_GLOBALTAGSFILE ), WM_GETTEXT,
                                  TL_MAX_PATH, ( LPARAM )g_GlobalTagsFile );

                    if ( ( g_GlobalTagsFile[0] != '\0' ) && ( ! PathFileExistsA( g_GlobalTagsFile ) ) )
                        MessageBoxA( hWndDlg, g_GlobalTagsFile, "File Not Found", MB_OK | MB_ICONEXCLAMATION);
                    else
                        PostMessage( hWndDlg, WM_CLOSE, 0, 0 );

                    return TRUE;
                }

                case IDC_CHK_NPPCOLORS:
                {
                    int check = ( int )::SendMessage( GetDlgItem( hWndDlg, IDC_CHK_NPPCOLORS ),
                                                      BM_GETCHECK, 0, 0 );

                    if ( check & BST_CHECKED )
                        g_useNppColors = true;
                    else
                        g_useNppColors = false;

                    return TRUE;
                }

                case IDC_CHK_NPPAUTOC:
                {
                    int check = ( int )::SendMessage( GetDlgItem( hWndDlg, IDC_CHK_NPPAUTOC ),
                                                      BM_GETCHECK, 0, 0 );

                    if ( check & BST_CHECKED )
                        g_useNppAutoC = true;
                    else
                        g_useNppAutoC = false;

                    return TRUE;
                }

                case IDC_CHK_UPDSAVE:
                {
                    int check = ( int )::SendMessage( GetDlgItem( hWndDlg, IDC_CHK_UPDSAVE ),
                                                      BM_GETCHECK, 0, 0 );

                    if ( check & BST_CHECKED )
                        g_UpdateOnSave = true;
                    else
                        g_UpdateOnSave = false;

                    return TRUE;
                }

                case MAKELONG( IDC_EDT_PEEKPRE, EN_CHANGE ) :
                {
                    BOOL isSuccessful;
                    int val = ( int )::GetDlgItemInt( hWndDlg, IDC_EDT_PEEKPRE, &isSuccessful,
                                                      FALSE );

                    if ( val >= 0 && val <= MAX_LINES )
                        g_PeekPre = val;

                    return TRUE;
                }

                case MAKELONG( IDC_EDT_PEEKPOST, EN_CHANGE ) :
                {
                    BOOL isSuccessful;
                    int val = ( int )::GetDlgItemInt( hWndDlg, IDC_EDT_PEEKPOST, &isSuccessful,
                                                      FALSE );

                    if ( val >= 0 && val <= MAX_LINES )
                        g_PeekPost = val;

                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}

void TagLeetApp::ShowSettings()
{
    DialogBoxParam( this->GetInstance(), MAKEINTRESOURCE( IDD_SETTINGS ),
                    NppHndl, SettingsDlg, 0 );
}
