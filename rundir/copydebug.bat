@echo off
copy ..\debug\obs.exe .\
copy ..\obsapi\debug\obsapi.dll .\
copy ..\dshowplugin\debug\dshowplugin.dll .\plugins
copy ..\graphicscapture\debug\graphicscapture.dll .\plugins
copy ..\graphicscapture\graphicscapturehook\debug\graphicscapturehook.dll .\plugins\graphicscapture
copy ..\graphicscapture\graphicscapturehook\x64\debug\graphicscapturehook64.dll .\plugins\graphicscapture
copy ..\noisegate\debug\noisegate.dll .\plugins
copy ..\psvplugin\debug\psvplugin.dll .\plugins
copy ..\scenesw\debug\scenesw.dll .\plugins
copy ..\x264\libs\32bit\libx264-146.dll .\
copy ..\injectHelper\Release\injectHelper.exe .\plugins\graphicscapture
copy ..\injectHelper\x64\Release\injectHelper64.exe .\plugins\graphicscapture
copy ..\ObsNvenc\Debug\ObsNvenc.dll .\
