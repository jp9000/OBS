@echo off
copy ..\x64\debug\obs.exe .\
copy ..\obsapi\x64\debug\obsapi.dll .\
copy ..\dshowplugin\x64\debug\dshowplugin.dll .\plugins
copy ..\graphicscapture\x64\debug\graphicscapture.dll .\plugins
copy ..\graphicscapture\graphicscapturehook\debug\graphicscapturehook.dll .\plugins\graphicscapture
copy ..\graphicscapture\graphicscapturehook\x64\debug\graphicscapturehook64.dll .\plugins\graphicscapture
copy ..\noisegate\x64\debug\noisegate.dll .\plugins
copy ..\x264\libs\64bit\libx264-130.dll .\
copy ..\injectHelper\Release\injectHelper.exe .\plugins\graphicscapture
copy ..\injectHelper\x64\Release\injectHelper64.exe .\plugins\graphicscapture
