# Notepad++ Tag LEET inline CTags viewer


## Description

The original plugin was posted here:
https://sourceforge.net/projects/tagleet/

Uses [Universal Ctags](https://github.com/universal-ctags/ctags-win32) to 
index a file and provide inline jump to tags.

This enhancement adds lots of cool features including automated tag file 
creation and update on saves, another column to the pop-up window showing 
additional tag file information (i.e., line number, context), autocomplete 
based on tags file and find references using Notepad++ Find in Files feature 
from the tags directory.

## Compiling

I compiled with MS Visual Studio Community 2017 and this seems to work OK.

For 32-bit:
```
    [x86 Native Tools Command Prompt for VS 2017]
    C:\> set Configuration=Release
    C:\> set Platform=x86
    C:\> msbuild
```

For 64-bit:
```
    [x64 Native Tools Command Prompt for VS 2017]
    C:\> set Configuration=Release
    C:\> set Platform=x64
    C:\> msbuild
```

## Installation

TagLEET 1.4.1.1 and newer requires Notepad++ 8.4.6 or newer.

Copy the:

+ 32-bit:  ./Release/Win32/TagLEET.dll
+ 64-bit:  ./Release/x64/TagLEET.dll

to the Notepad++ plugins folder:
  + In N++ <7.6, directly in the plugins/ folder
  + In N++ >=7.6, in a directory called TagLEET in the plugins/ folder (plugins/TagLEET/)

You will also need to create a subdirectory 'TagLEET' in the same directory as 
the `TagLEET.dll` file and put the `ctags.exe` executable in that directory.  
This can be obtained from the ZIP file from the appropriate 
[Release](https://github.com/vinsworldcom/nppTagLEET/releases). 

## Usage

Assign the Plugins => TagLEET => Lookup Tag menu item to a hotkey of your 
preference.  In a source file, put the cursor on a word you would like to 
lookup and press the hotkey.

### First Time Usage

If no `tags` file exists in the path of the current Notepad++ document, you 
will be prompted to create one.  This assumes the current directory of the 
current Notepad++ document but for best results, put the `tags` file in the 
top level directory of your project.  If you were prompted to create the 
`tags` file, just press the hotkey again and this time you will get a 
popup window with the results (if any).

Whenever you directory navigate out of the path of the current `tags` file 
and press the hotkey, you will be prompted to create a new one.  There is no 
master `tags` file, each project can have its own unique `tags` file.

### Normal Operations

While in the popup window:

Key Stroke | Function
-----------|-----------
NUMPAD(+)              | increase window size
NUMPAD(-)              | decrease window size
NUMPAD(/)              | reset window size
CTRL + NUMPAD(+)       | increase font size
CTRL + NUMPAD(-)       | decrease font size
CTRL + NUMPAD(/)       | reset font size
NUMPAD(*)              | toggle default / N++ colors
ALT + A                | toggle Scintilla Autocomplete
ALT + R                | toggle Recurse Subdirectories
ALT + S                | toggle Update on Save

Update on save will auto update the current `tags` file after each file save.
This does not take too much extra resources and makes the TagLEET experience 
much more dynamic.  It is disabled by default.

To navigate to a lookup, press the `Return` or `Space` while the item is 
highlighted in the top listview.  Use `Tab` to switch between the top and 
bottom panes.

The bottom pane contains a snapshot (read-only) of the text surrounding the 
active lookup (highlighted in the top list view).  By default 2 lines before 
and 9 lines after are shown; the active lookup line is highlighted and 
italicized.  You can configure the number of lines before and after in the 
'Settings' dialog (Peek PRE lines, Peek POST lines).

The popup window losing focus while the top list view is active will auto close 
the popup window.  It will *not* auto close when losing focus from the bottom 
edit view; this allows for cut and paste and comparison.

Press `ESC` to close the popup window or just use the Windows close button.

#### Recurse Subdirectories

By default, tags files are created with the '-R' option to recurse all files 
in the current directory as well as all files in the current directory's 
subdirectories.  By turning off this option, tags files are created only 
based on the current active file in Notepad++.

#### Global Tags File

You can also configure a global tags file in the 'Settings' dialog for the 
current language you are using.  This file should be generated with at least 
the following `ctags.exe` options:

    --extras=+Ffq --fields=+n -R

and must use absolute file names in the tags file.  This file will be consulted 
if a match is not found in the local project's tags file.  Leaving the 
'Settings' dialog empty does not use a global tags file.

#### Scintilla AutoComplete

This plugin's menu item `Autocomplete` will always launch the TagLEET 
autocomplete feature.  This is manual.  The 'Settings' dialog 
'Use Scintilla autocomplete' will use the 
[Scintilla autocompletion](https://www.scintilla.org/ScintillaDoc.html#Autocompletion) 
functionality with the tags file contents.  This is automatically triggered 
while typing.  If a tags file does not exist, this functionality is silently 
ignored.  This does not integrate with Notepad++ autocomplete 
(Settings => Preferences => Auto-Completion) but also does not conflict (much).
