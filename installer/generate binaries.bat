@echo off
mkdir 32bit
mkdir 32bit\locale
mkdir 32bit\shaders
mkdir 32bit\plugins
mkdir 32bit\plugins\DShowPlugin
mkdir 32bit\plugins\DShowPlugin\locale
mkdir 32bit\plugins\DShowPlugin\shaders
mkdir 32bit\plugins\GraphicsCapture

mkdir 64bit
mkdir 64bit\locale
mkdir 64bit\shaders
mkdir 64bit\plugins
mkdir 64bit\plugins\DShowPlugin
mkdir 64bit\plugins\DShowPlugin\locale
mkdir 64bit\plugins\DShowPlugin\shaders
mkdir 64bit\plugins\GraphicsCapture

copy ..\release\obs.exe .\32bit\
copy ..\obsapi\release\obsapi.dll .\32bit\
copy ..\rundir\OBSHelp.chm .\32bit\
copy ..\rundir\services.xconfig .\32bit\
copy ..\rundir\pdb32\stripped\*.pdb .\32bit\
copy ..\rundir\locale\*.txt .\32bit\locale\
copy ..\rundir\shaders\*.?Shader .\32bit\shaders\
copy ..\dshowplugin\release\dshowplugin.dll .\32bit\plugins
copy ..\rundir\plugins\dshowplugin\locale\*.txt .\32bit\plugins\dshowplugin\locale\
copy ..\rundir\plugins\dshowplugin\shaders\*.?Shader .\32bit\plugins\dshowplugin\shaders\
copy ..\graphicscapture\release\graphicscapture.dll .\32bit\plugins
copy ..\graphicscapture\graphicscapturehook\release\graphicscapturehook.dll .\32bit\plugins\graphicscapture

copy ..\x64\release\obs.exe .\64bit\
copy ..\obsapi\x64\release\obsapi.dll .\64bit\
copy ..\rundir\OBSHelp.chm .\64bit\
copy ..\rundir\services.xconfig .\64bit\
copy ..\rundir\pdb64\stripped\*.pdb .\64bit\
copy ..\rundir\locale\*.txt .\64bit\locale\
copy ..\rundir\shaders\*.?Shader .\64bit\shaders\
copy ..\dshowplugin\x64\release\dshowplugin.dll .\64bit\plugins
copy ..\rundir\plugins\dshowplugin\locale\*.txt .\64bit\plugins\dshowplugin\locale\
copy ..\rundir\plugins\dshowplugin\shaders\*.?Shader .\64bit\plugins\dshowplugin\shaders\
copy ..\graphicscapture\x64\release\graphicscapture.dll .\64bit\plugins
copy ..\graphicscapture\graphicscapturehook\x64\release\graphicscapturehook.dll .\64bit\plugins\graphicscapture
