@echo off
mkdir 32bit
mkdir 32bit\locale
mkdir 32bit\shaders
mkdir 32bit\plugins
mkdir 32bit\plugins\DShowPlugin
mkdir 32bit\plugins\DShowPlugin\locale
mkdir 32bit\plugins\DShowPlugin\shaders
mkdir 32bit\plugins\GraphicsCapture
mkdir 32bit\plugins\PSVPlugin
mkdir 32bit\plugins\PSVPlugin\locale
mkdir 32bit\plugins\scenesw
mkdir 32bit\plugins\scenesw\locale

mkdir 64bit
mkdir 64bit\locale
mkdir 64bit\shaders
mkdir 64bit\plugins
mkdir 64bit\plugins\DShowPlugin
mkdir 64bit\plugins\DShowPlugin\locale
mkdir 64bit\plugins\DShowPlugin\shaders
mkdir 64bit\plugins\GraphicsCapture
mkdir 64bit\plugins\PSVPlugin
mkdir 64bit\plugins\PSVPlugin\locale
mkdir 64bit\plugins\scenesw
mkdir 64bit\plugins\scenesw\locale

copy ..\release\obs.exe .\32bit\
copy ..\obsapi\release\obsapi.dll .\32bit\
copy ..\rundir\services.xconfig .\32bit\
copy ..\rundir\pdb32\OBS.pdb .\32bit\
copy ..\rundir\pdb32\OBSApi.pdb .\32bit\
copy ..\rundir\pdb32\DShowPlugin.pdb .\32bit\plugins\
copy ..\rundir\pdb32\GraphicsCapture.pdb .\32bit\plugins\
copy ..\rundir\pdb32\NoiseGate.pdb .\32bit\plugins\
copy ..\rundir\pdb32\PSVPlugin.pdb .\32bit\plugins\
copy ..\rundir\pdb32\scenesw.pdb .\32bit\plugins\
copy ..\rundir\locale\*.txt .\32bit\locale\
copy ..\rundir\shaders\*.?Shader .\32bit\shaders\
copy ..\dshowplugin\release\dshowplugin.dll .\32bit\plugins
copy ..\noisegate\release\noisegate.dll .\32bit\plugins
copy ..\psvplugin\release\psvplugin.dll .\32bit\plugins
copy ..\scenesw\release\scenesw.dll .\32bit\plugins
copy ..\rundir\plugins\dshowplugin\locale\*.txt .\32bit\plugins\dshowplugin\locale\
copy ..\rundir\plugins\dshowplugin\shaders\*.?Shader .\32bit\plugins\dshowplugin\shaders\
copy ..\rundir\plugins\psvplugin\locale\*.txt .\32bit\plugins\psvplugin\locale\
copy ..\rundir\plugins\scenesw\locale\*.txt .\32bit\plugins\scenesw\locale\
copy ..\graphicscapture\release\graphicscapture.dll .\32bit\plugins
copy ..\graphicscapture\graphicscapturehook\release\graphicscapturehook.dll .\32bit\plugins\graphicscapture
copy ..\graphicscapture\graphicscapturehook\x64\release\graphicscapturehook64.dll .\32bit\plugins\graphicscapture
copy ..\injectHelper\release\injectHelper.exe .\32bit\plugins\graphicscapture
copy ..\injectHelper\x64\release\injectHelper64.exe .\32bit\plugins\graphicscapture
copy ..\x264\libs\32bit\libx264-142.dll .\32bit
copy ..\QSVHelper\Release\QSVHelper.exe .\32bit
copy ..\ObsNvenc\Release\ObsNvenc.dll .\32bit
copy "%WindowsSDK80Path%Debuggers\x86\dbghelp.dll" .\32bit

copy ..\x64\release\obs.exe .\64bit\
copy ..\obsapi\x64\release\obsapi.dll .\64bit\
copy ..\rundir\services.xconfig .\64bit\
copy ..\rundir\pdb64\OBS.pdb .\64bit\
copy ..\rundir\pdb64\OBSApi.pdb .\64bit\
copy ..\rundir\pdb64\DShowPlugin.pdb .\64bit\plugins\
copy ..\rundir\pdb64\GraphicsCapture.pdb .\64bit\plugins\
copy ..\rundir\pdb64\NoiseGate.pdb .\64bit\plugins\
copy ..\rundir\pdb64\PSVPlugin.pdb .\64bit\plugins\
copy ..\rundir\pdb64\scenesw.pdb .\64bit\plugins\
copy ..\rundir\locale\*.txt .\64bit\locale\
copy ..\rundir\shaders\*.?Shader .\64bit\shaders\
copy ..\dshowplugin\x64\release\dshowplugin.dll .\64bit\plugins
copy ..\noisegate\x64\release\noisegate.dll .\64bit\plugins
copy ..\psvplugin\x64\release\psvplugin.dll .\64bit\plugins
copy ..\scenesw\x64\release\scenesw.dll .\64bit\plugins
copy ..\rundir\plugins\dshowplugin\locale\*.txt .\64bit\plugins\dshowplugin\locale\
copy ..\rundir\plugins\dshowplugin\shaders\*.?Shader .\64bit\plugins\dshowplugin\shaders\
copy ..\rundir\plugins\psvplugin\locale\*.txt .\64bit\plugins\psvplugin\locale\
copy ..\rundir\plugins\scenesw\locale\*.txt .\64bit\plugins\scenesw\locale\
copy ..\graphicscapture\x64\release\graphicscapture.dll .\64bit\plugins
copy ..\graphicscapture\graphicscapturehook\release\graphicscapturehook.dll .\64bit\plugins\graphicscapture
copy ..\graphicscapture\graphicscapturehook\x64\release\graphicscapturehook64.dll .\64bit\plugins\graphicscapture
copy ..\injectHelper\release\injectHelper.exe .\64bit\plugins\graphicscapture
copy ..\injectHelper\x64\release\injectHelper64.exe .\64bit\plugins\graphicscapture
copy ..\x264\libs\64bit\libx264-142.dll .\64bit
copy ..\QSVHelper\Release\QSVHelper.exe .\64bit
copy ..\ObsNvenc\x64\Release\ObsNvenc.dll .\64bit
copy "%WindowsSDK80Path%Debuggers\x64\dbghelp.dll" .\64bit
