# NSutils ([NSIS](https://github.com/negrutiu/nsis) plugin)
NSutils has multiple functions packed in one plugin<br>
Check out the [readme file](NSutils.Readme.txt) for additional details and usage examples<br>
This plugin is known to work well on NT4, Win11 and everything in between...

[![License: zlib/libpng](https://img.shields.io/badge/License-zlib%2Flibpng-blue.svg)](LICENSE)
[![Latest Release](https://img.shields.io/badge/dynamic/json.svg?label=Latest%20Release&url=https%3A%2F%2Fapi.github.com%2Frepos%2Fnegrutiu%2Fnsis-nsutils%2Freleases%2Flatest&query=%24.name&colorB=orange)](../../releases/latest)
[![Downloads](https://img.shields.io/github/downloads/negrutiu/nsis-nsutils/total.svg?label=Downloads&colorB=orange)](../../releases/latest)
[![GitHub issues](https://img.shields.io/github/issues/negrutiu/nsis-nsutils.svg?label=Issues)](../../issues)

### Functions:

Function|Details
-|-
GetVersionInfoString|Retrieve strings from an executable's version info
GetFileVersion|Extract numeric file version from an executable's version info
GetProductVersion|Extract numeric product version from an executable's version info
ReadResourceString<br>WriteResourceString|Read/Write strings to an executable's string table
DisableProgressStepBack<br>RestoreProgressStepBack|Prevent installer's progress bar to step back during loops
StartTimer<br>StopTimer|Work with timers in NSIS scrips
StartReceivingClicks<br>StopReceivingClicks|Callback NSIS functions for custom buttons
LoadImageFile|Loads an image in memory and returns a HBITMAP handle. Supports `bmp`, `jpeg`, `gif`
RejectCloseMessages|Protects your installer against "close" windows messages such as `WM_CLOSE`, `WM_DESTROY`, `WM_COMMAND(IDCANCEL)`, etc.
CloseFileHandles|Close files that are locked by other programs
RegMultiSzInsertBefore<br>RegMultiSzInsertAfter<br>RegMultiSzInsertAtIndex<br>RegMultiSzDelete|Operations with `REG_MULTI_SZ` registry values
CPUID|Retrieve CPU capabilities
CompareFiles|Check if two files are equal (content comparison)
DriveIsSSD|Check if a drive is SSD by examining its `TRIM` capability
