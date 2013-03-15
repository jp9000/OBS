@echo off
copy ..\release\obs.exe .\
copy ..\obsapi\release\obsapi.dll .\
copy ..\dshowplugin\release\dshowplugin.dll .\plugins
copy ..\graphicscapture\release\graphicscapture.dll .\plugins
copy ..\graphicscapture\graphicscapturehook\release\graphicscapturehook.dll .\plugins\graphicscapture
copy ..\graphicscapture\graphicscapturehook\x64\release\graphicscapturehook64.dll .\plugins\graphicscapture
copy ..\noisegate\release\noisegate.dll .\plugins
copy ..\x264\libs\32bit\libx264-130.dll .\
copy ..\injectHelper\Release\injectHelper.exe .\plugins\graphicscapture
copy ..\injectHelper\x64\Release\injectHelper64.exe .\plugins\graphicscapture
