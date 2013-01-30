@echo off
copy ..\release\obs.exe .\
copy ..\obsapi\release\obsapi.dll .\
copy ..\dshowplugin\release\dshowplugin.dll .\plugins
copy ..\graphicscapture\release\graphicscapture.dll .\plugins
copy ..\graphicscapture\graphicscapturehook\release\graphicscapturehook.dll .\plugins\graphicscapture
copy ..\x264\libs\32bit\libx264-129.dll .\
