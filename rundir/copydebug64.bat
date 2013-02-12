@echo off
copy ..\x64\debug\obs.exe .\
copy ..\obsapi\x64\debug\obsapi.dll .\
copy ..\dshowplugin\x64\debug\dshowplugin.dll .\plugins
copy ..\graphicscapture\x64\debug\graphicscapture.dll .\plugins
copy ..\graphicscapture\graphicscapturehook\debug\graphicscapturehook.dll .\plugins\graphicscapture
copy ..\graphicscapture\graphicscapturehook\x64\debug\graphicscapturehook64.dll .\plugins\graphicscapture
copy ..\x264\libs\64bit\libx264-129.dll .\
