NAME

Notepad++ Tag LEET inline CTags viewer


DESCRIPTION

The original plugin was posted here:
https://sourceforge.net/projects/tagleet/

Uses ctags (http://ctags.sourceforge.net/) to index a file and provide
inline jump to tags.

This enhancement adds another column to the pop-up window showing additional 
tag file information (i.e., line number if --fields=+n).


COMPILING

I compiled with MS Visual Studio Community 2017 and this seems to work
OK.

For 32-bit:
  [x86 Native Tools Command Prompt for VS 2017]
  C:\> set Configuration=Release
  C:\> set Platform=x86
  C:\> msbuild

For 64-bit:
  [x64 Native Tools Command Prompt for VS 2017]
  C:\> set Configuration=Release
  C:\> set Platform=x64
  C:\> msbuild

INSTALLATION

Copy the:

32-bit:
    ./bin/TagLEET.dll

64-bit:
    ./bin64/TagLEET.dll

to the Notepad++ plugins folder:
  - In N++ <7.6, directly in the plugins/ folder
  - In N++ >=7.6, in a directory called TagLEET in the plugins/ folder
    (plugins/TagLEET/)
