@echo off
copy ..\x64\release\obs.exe .\
copy ..\obsapi\x64\release\obsapi.dll .\
copy ..\dshowplugin\x64\release\dshowplugin.dll .\plugins
copy ..\graphicscapture\x64\release\graphicscapture.dll .\plugins
copy ..\graphicscapture\graphicscapturehook\release\graphicscapturehook.dll .\plugins\graphicscapture
copy ..\graphicscapture\graphicscapturehook\x64\release\graphicscapturehook64.dll .\plugins\graphicscapture
copy ..\noisegate\x64\release\noisegate.dll .\plugins
copy ..\psvplugin\x64\release\psvplugin.dll .\plugins
copy ..\x264\libs\64bit\libx264-140.dll .\
copy ..\injectHelper\Release\injectHelper.exe .\plugins\graphicscapture
copy ..\injectHelper\x64\Release\injectHelper64.exe .\plugins\graphicscapture
