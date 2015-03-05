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

mkdir pdbs
mkdir pdbs\32bit
mkdir pdbs\64bit

copy ..\COPYING .\32bit\LICENSE
copy ..\release\obs.exe .\32bit\
copy ..\obsapi\release\obsapi.dll .\32bit\
copy ..\rundir\services.xconfig .\32bit\
copy ..\rundir\pdb32\stripped\*.pdb .\32bit\
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
copy ..\x264\libs\32bit\libx264-146.dll .\32bit
copy ..\QSVHelper\Release\QSVHelper.exe .\32bit
copy ..\ObsNvenc\Release\ObsNvenc.dll .\32bit
copy "%WindowsSDK80Path%Debuggers\x86\dbghelp.dll" .\32bit

copy ..\COPYING .\64bit\LICENSE
copy ..\x64\release\obs.exe .\64bit\
copy ..\obsapi\x64\release\obsapi.dll .\64bit\
copy ..\rundir\services.xconfig .\64bit\
copy ..\rundir\pdb64\stripped\*.pdb .\64bit\
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
copy ..\x264\libs\64bit\libx264-146.dll .\64bit
copy ..\QSVHelper\Release\QSVHelper.exe .\64bit
copy ..\ObsNvenc\x64\Release\ObsNvenc.dll .\64bit
copy "%WindowsSDK80Path%Debuggers\x64\dbghelp.dll" .\64bit

copy ..\rundir\pdb32\*.pdb .\pdbs\32bit
copy ..\rundir\pdb64\*.pdb .\pdbs\64bit

pause

mkdir upload
mkdir upload\DirectShowPlugin
mkdir upload\DirectShowPlugin\32bit
mkdir upload\DirectShowPlugin\32bit\DShowPlugin\locale
mkdir upload\DirectShowPlugin\32bit\DShowPlugin\shaders
mkdir upload\DirectShowPlugin\64bit
mkdir upload\DirectShowPlugin\64bit\DShowPlugin\locale
mkdir upload\DirectShowPlugin\64bit\DShowPlugin\shaders
mkdir upload\GraphicsCapturePlugin
mkdir upload\GraphicsCapturePlugin\32bit
mkdir upload\GraphicsCapturePlugin\32bit\GraphicsCapture
mkdir upload\GraphicsCapturePlugin\64bit
mkdir upload\GraphicsCapturePlugin\64bit\GraphicsCapture
mkdir upload\NoiseGatePlugin
mkdir upload\NoiseGatePlugin\32bit
mkdir upload\NoiseGatePlugin\64bit
mkdir upload\PSVPlugin
mkdir upload\PSVPlugin\32bit
mkdir upload\PSVPlugin\32bit\PSVPlugin\locale
mkdir upload\PSVPlugin\64bit
mkdir upload\PSVPlugin\64bit\PSVPlugin\locale
mkdir upload\scenesw
mkdir upload\scenesw\32bit
mkdir upload\scenesw\32bit\scenesw\locale
mkdir upload\scenesw\64bit
mkdir upload\scenesw\64bit\scenesw\locale
mkdir upload\OBS
mkdir upload\OBS\32bit
mkdir upload\OBS\64bit
mkdir upload\OBS\locale
mkdir upload\OBS\services
mkdir upload\OBS\shaders

copy 32bit\plugins\dshowplugin.dll .\upload\DirectShowPlugin\32bit\
copy 32bit\plugins\dshowplugin\locale\*.txt .\upload\DirectShowPlugin\32bit\DShowPlugin\locale\
copy 32bit\plugins\dshowplugin\shaders\*.?Shader .\upload\DirectShowPlugin\32bit\DShowPlugin\shaders\

copy 64bit\plugins\dshowplugin.dll .\upload\DirectShowPlugin\64bit\
copy 64bit\plugins\dshowplugin\locale\*.txt .\upload\DirectShowPlugin\64bit\DShowPlugin\locale\
copy 64bit\plugins\dshowplugin\shaders\*.?Shader .\upload\DirectShowPlugin\64bit\DShowPlugin\shaders\

copy 32bit\plugins\graphicscapture.dll .\upload\GraphicsCapturePlugin\32bit\
copy 32bit\plugins\graphicscapture\graphicscapturehook.dll .\upload\GraphicsCapturePlugin\32bit\GraphicsCapture\
copy 32bit\plugins\graphicscapture\graphicscapturehook64.dll .\upload\GraphicsCapturePlugin\32bit\GraphicsCapture\
copy 32bit\plugins\graphicscapture\injectHelper.exe .\upload\GraphicsCapturePlugin\32bit\GraphicsCapture\
copy 32bit\plugins\graphicscapture\injectHelper64.exe .\upload\GraphicsCapturePlugin\32bit\GraphicsCapture\

copy 64bit\plugins\graphicscapture.dll .\upload\GraphicsCapturePlugin\64bit\
copy 64bit\plugins\graphicscapture\graphicscapturehook.dll .\upload\GraphicsCapturePlugin\64bit\GraphicsCapture\
copy 64bit\plugins\graphicscapture\graphicscapturehook64.dll .\upload\GraphicsCapturePlugin\64bit\GraphicsCapture\
copy 64bit\plugins\graphicscapture\injectHelper.exe .\upload\GraphicsCapturePlugin\64bit\GraphicsCapture\
copy 64bit\plugins\graphicscapture\injectHelper64.exe .\upload\GraphicsCapturePlugin\64bit\GraphicsCapture\

copy 32bit\plugins\noisegate.dll .\upload\NoiseGatePlugin\32bit\
copy 64bit\plugins\noisegate.dll .\upload\NoiseGatePlugin\64bit\

copy 32bit\plugins\psvplugin.dll .\upload\PSVPlugin\32bit\
copy 32bit\plugins\psvplugin\locale\*.txt .\upload\PSVPlugin\32bit\PSVPlugin\locale\

copy 64bit\plugins\psvplugin.dll .\upload\PSVPlugin\64bit\
copy 64bit\plugins\psvplugin\locale\*.txt .\upload\PSVPlugin\64bit\PSVPlugin\locale\

copy 32bit\plugins\scenesw.dll .\upload\scenesw\32bit\
copy 32bit\plugins\scenesw\locale\*.txt .\upload\scenesw\32bit\scenesw\locale\

copy 64bit\plugins\scenesw.dll .\upload\scenesw\64bit\
copy 64bit\plugins\scenesw\locale\*.txt .\upload\scenesw\64bit\scenesw\locale\

copy 32bit\obs.exe .\upload\OBS\32bit\
copy 32bit\obsapi.dll .\upload\OBS\32bit\
copy 32bit\*.pdb .\upload\OBS\32bit\
copy 32bit\libx264-146.dll .\upload\OBS\32bit
copy 32bit\QSVHelper.exe .\upload\OBS\32bit
copy 32bit\ObsNvenc.dll .\upload\OBS\32bit
copy "%WindowsSDK80Path%Debuggers\x86\dbghelp.dll" .\upload\OBS\32bit

copy 64bit\obs.exe .\upload\OBS\64bit\
copy 64bit\obsapi.dll .\upload\OBS\64bit\
copy 64bit\*.pdb .\upload\OBS\64bit\
copy 64bit\libx264-146.dll .\upload\OBS\64bit
copy 64bit\QSVHelper.exe .\upload\OBS\64bit
copy 64bit\ObsNvenc.dll .\upload\OBS\64bit
copy "%WindowsSDK80Path%Debuggers\x64\dbghelp.dll" .\upload\OBS\64bit

copy 32bit\locale\*.txt .\upload\OBS\locale\

copy 32bit\services.xconfig .\upload\OBS\services\

copy 32bit\shaders\*.?Shader .\upload\OBS\shaders\
