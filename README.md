# Notepad++ Tag LEET inline CTags viewer


## Description

The original plugin was posted here:
https://sourceforge.net/projects/tagleet/

Uses [ctags](http://ctags.sourceforge.net/) to index a file and provide
inline jump to tags.

This enhancement adds another column to the pop-up window showing additional 
tag file information (i.e., line number if --fields=+n).

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

Copy the:

+ 32-bit:  ./bin/TagLEET.dll
+ 64-bit:  ./bin64/TagLEET.dll

to the Notepad++ plugins folder:
  + In N++ <7.6, directly in the plugins/ folder
  + In N++ >=7.6, in a directory called TagLEET in the plugins/ folder (plugins/TagLEET/)

## Usage

Assing the Plugins => TagLEET => Lookup Tag menu item to a hotkey of your 
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
CTRL + NUMPAD(*)       | toggle TagLEET / N++ for autocomplete
CTRL + ALT + NUMPAD(*) | toggle update on save

Update on save will auto update the current `tags` file after each file save.
This does not take too much extra resources and makes the TagLEET experience 
much more dynamic.  It is disabled by default.

To navigate to a lookup, press the `Return` or `Space` while the item is 
highligted in the top listview.

The bottom pane contains a snapshot (read-only) of the text surrounding the 
active lookup (highlighted in the top list view).  By default 2 lines before 
and 9 lines after are shown; the active lookup line is highlighed and 
italicized.  You can configure the number of lines before and after by editing 
the configuation file items (PeekPre, PeekPost), changes will take effect on 
the next Notepad++ launch.

THe popup window losing focus while the top list view is active will auto close 
the popup window.  It will *not* auto close when losing focus from the bottom 
edit view; this allows for cut and paste and comparison.

Press `ESC` to close the popup window or just use the Windows close button.
