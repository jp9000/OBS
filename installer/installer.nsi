; Script generated with the Venis Install Wizard

; Define your application name
!define APPNAME "Open Broadcaster Software"
!define APPNAMEANDVERSION "Open Broadcaster Software 0.65b"

; Additional script dependencies
!include WinVer.nsh
!include x64.nsh

; Main Install settings
Name "${APPNAMEANDVERSION}"
InstallDir "$PROGRAMFILES32\OBS"
InstallDirRegKey HKLM "Software\${APPNAME}" ""
OutFile "OBS_Installer.exe"

; Use compression
SetCompressor LZMA

; Need Admin
RequestExecutionLevel admin

; Modern interface settings
!include "MUI.nsh"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$PROGRAMFILES32\OBS\OBS.exe"

!define MUI_PAGE_CUSTOMFUNCTION_LEAVE PreReqCheck

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "gplv2.txt"
;!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

;!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_COMPONENTS
!insertmacro MUI_UNPAGE_INSTFILES

; Set languages (first is default language)
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_RESERVEFILE_LANGDLL

Function PreReqCheck
	; Abort on XP or lower
	${If} ${AtMostWinXP}
		MessageBox MB_OK|MB_ICONSTOP "Due to extensive use of DirectX 10 features, ${APPNAME} requires Windows Vista SP2 or higher and cannot be installed on this version of Windows."
		Quit
	${EndIf}
	
	; Vista specific checks
	${If} ${IsWinVista}
		; Check Vista SP2
		${If} ${AtMostServicePack} 1
			MessageBox MB_YESNO|MB_ICONEXCLAMATION "${APPNAME} requires Service Pack 2 when running on Vista. Would you like to download it?" IDYES sptrue IDNO spfalse
			sptrue:
				ExecShell "open" "http://windows.microsoft.com/en-US/windows-vista/Learn-how-to-install-Windows-Vista-Service-Pack-2-SP2"
			spfalse:
			Quit
		${EndIf}
		
		; Check Vista Platform Update
		nsexec::exectostack "$SYSDIR\wbem\wmic.exe qfe where HotFixID='KB971512' get HotFixID /Format:list"
		pop $0
		pop $0
		strcpy $1 $0 17 6
		strcmps $1 "HotFixID=KB971512" gotPatch
			MessageBox MB_YESNO|MB_ICONEXCLAMATION "${APPNAME} requires the Windows Vista Platform Update. Would you like to download it?" IDYES putrue IDNO pufalse
			putrue:
				${If} ${RunningX64}
					; 64 bit
					ExecShell "open" "http://www.microsoft.com/en-us/download/details.aspx?id=4390"
				${Else}
					; 32 bit
					ExecShell "open" "http://www.microsoft.com/en-us/download/details.aspx?id=3274"
				${EndIf}
			pufalse:
			Quit
		gotPatch:
	${EndIf}
	
	; DirectX Version Check
	ClearErrors
	GetDLLVersion "D3DX10_43.DLL" $R0 $R1
	GetDLLVersion "D3D10_1.DLL" $R0 $R1
	GetDLLVersion "DXGI.DLL" $R0 $R1
	GetDLLVersion "D3DCompiler_43.dll" $R0 $R1
	IfErrors dxMissing dxOK
	dxMissing:
		MessageBox MB_YESNO|MB_ICONEXCLAMATION "Your system is missing DirectX components that ${APPNAME} requires. Would you like to download them?" IDYES dxtrue IDNO dxfalse
		dxtrue:
			ExecShell "open" "http://www.microsoft.com/en-us/download/details.aspx?id=35"
		dxfalse:
		Quit
	dxOK:
	ClearErrors
	
	;XINPUT Check (not present on server OSes)
	GetDLLVersion "xinput9_1_0.dll" $R0 $R1
	IfErrors xinputMissing xinputOK
	xinputMissing:
		MessageBox MB_YESNO|MB_ICONEXCLAMATION "Your system is missing XINPUT components (xinput_9_1_0.dll). This may happen if you are running on a Windows Server OS. Would you like to download the required files from a 3rd party website?" IDYES xinputtrue IDNO xinputfalse
		xinputtrue:
			ExecShell "open" "http://www.win2012workstation.com/xinput-and-xaudio-dlls/"
		xinputfalse:
		Quit
	xinputOK:
	ClearErrors	
	
	; Check previous instance
	System::Call 'kernel32::OpenMutexW(i 0x100000, b 0, w "OBSMutex") i .R0'
	IntCmp $R0 0 notRunning
    System::Call 'kernel32::CloseHandle(i $R0)'
    MessageBox MB_OK|MB_ICONEXCLAMATION "${APPNAME} is already running. Please close it first before installing a new version." /SD IDOK
    Quit
notRunning:
	
FunctionEnd

Function filesInUse
	MessageBox MB_OK|MB_ICONEXCLAMATION "Some files were not able to be installed. If this is the first time you are installing OBS, please disable any anti-virus or other security software and try again. If you are re-installing or updating OBS, close any applications that may be have been hooked, or reboot and try again."  /SD IDOK
FunctionEnd

Var outputErrors

Section "Open Broadcaster Software" Section1

	; Set Section properties
	SetOverwrite on
	
	; Empty the shader cache in case the user is reinstalling to try and fix corrupt shader issues
	; We no longer use shader cache
	;RMDir /R "$APPDATA\OBS\shaderCache"

	; Set Section Files and Shortcuts
	SetOutPath "$PROGRAMFILES32\OBS"
	File "/oname=LICENSE" "..\COPYING"
	File "32bit\OBS.exe"
	File "32bit\libx264-146.dll"
	File "32bit\QSVHelper.exe"
	File "32bit\OBSApi.dll"
	File "32bit\services.xconfig"
	File "32bit\\*.pdb"
	File "32bit\ObsNvenc.dll"
	File "$%WindowsSDK80Path%Debuggers\x86\dbghelp.dll"
	SetOutPath "$PROGRAMFILES32\OBS\locale"
	File "32bit\locale\*.txt"
	SetOutPath "$PROGRAMFILES32\OBS\shaders\"
	File "32bit\shaders\*.?Shader"
	SetOutPath "$PROGRAMFILES32\OBS\plugins\"
	File "32bit\plugins\NoiseGate.dll"
	File "32bit\plugins\DShowPlugin.dll"
	File "32bit\plugins\GraphicsCapture.dll"
	File "32bit\plugins\PSVPlugin.dll"
	File "32bit\plugins\scenesw.dll"
	SetOutPath "$PROGRAMFILES32\OBS\plugins\PSVPlugin\locale\"
	File "32bit\plugins\PSVPlugin\locale\*.txt"
	SetOutPath "$PROGRAMFILES32\OBS\plugins\scenesw\locale\"
	File "32bit\plugins\scenesw\locale\*.txt"
	SetOutPath "$PROGRAMFILES32\OBS\plugins\DShowPlugin\locale\"
	File "32bit\plugins\DShowPlugin\locale\*.txt"
	SetOutPath "$PROGRAMFILES32\OBS\plugins\DShowPlugin\shaders\"
	File "32bit\plugins\DShowPlugin\shaders\*.?Shader"
	SetOutPath "$PROGRAMFILES32\OBS\plugins\GraphicsCapture\"
	
	ClearErrors
	
	File "32bit\plugins\GraphicsCapture\GraphicsCaptureHook.dll"
	File "32bit\plugins\GraphicsCapture\GraphicsCaptureHook64.dll"
	File "32bit\plugins\GraphicsCapture\injectHelper.exe"
	File "32bit\plugins\GraphicsCapture\injectHelper64.exe"
	
	IfErrors 0 +2
		StrCpy $outputErrors "yes"
	
	${if} ${RunningX64}
		SetOutPath "$PROGRAMFILES64\OBS"
		File "/oname=LICENSE" "..\COPYING"
		File "64bit\OBS.exe"
		File "64bit\libx264-146.dll"
		File "64bit\QSVHelper.exe"
		File "64bit\OBSApi.dll"
		File "64bit\services.xconfig"
		File "64bit\*.pdb"
		File "64bit\ObsNvenc.dll"
		File "$%WindowsSDK80Path%Debuggers\x64\dbghelp.dll"
		SetOutPath "$PROGRAMFILES64\OBS\locale"
		File "64bit\locale\*.txt"
		SetOutPath "$PROGRAMFILES64\OBS\shaders\"
		File "64bit\shaders\*.?Shader"
		SetOutPath "$PROGRAMFILES64\OBS\plugins\"
		File "64bit\plugins\NoiseGate.dll"
		File "64bit\plugins\DShowPlugin.dll"
		File "64bit\plugins\GraphicsCapture.dll"
		File "64bit\plugins\PSVPlugin.dll"
		File "64bit\plugins\scenesw.dll"
		SetOutPath "$PROGRAMFILES64\OBS\plugins\PSVPlugin\locale\"
		File "64bit\plugins\PSVPlugin\locale\*.txt"
		SetOutPath "$PROGRAMFILES64\OBS\plugins\scenesw\locale\"
		File "64bit\plugins\scenesw\locale\*.txt"
		SetOutPath "$PROGRAMFILES64\OBS\plugins\DShowPlugin\locale\"
		File "64bit\plugins\DShowPlugin\locale\*.txt"
		SetOutPath "$PROGRAMFILES64\OBS\plugins\DShowPlugin\shaders\"
		File "64bit\plugins\DShowPlugin\shaders\*.?Shader"
		SetOutPath "$PROGRAMFILES64\OBS\plugins\GraphicsCapture\"
		
		ClearErrors
		
		File "64bit\plugins\GraphicsCapture\GraphicsCaptureHook.dll"
		File "64bit\plugins\GraphicsCapture\GraphicsCaptureHook64.dll"
		File "64bit\plugins\GraphicsCapture\injectHelper.exe"
		File "64bit\plugins\GraphicsCapture\injectHelper64.exe"
		
		IfErrors 0 +2
			StrCpy $outputErrors "yes"

	${endif}

	WriteUninstaller "$PROGRAMFILES32\OBS\uninstall.exe"

	SetOutPath "$PROGRAMFILES32\OBS"
	CreateShortCut "$DESKTOP\Open Broadcaster Software.lnk" "$PROGRAMFILES32\OBS\OBS.exe"
	CreateDirectory "$SMPROGRAMS\Open Broadcaster Software"
	CreateShortCut "$SMPROGRAMS\Open Broadcaster Software\Open Broadcaster Software (32bit).lnk" "$PROGRAMFILES32\OBS\OBS.exe"
	CreateShortCut "$SMPROGRAMS\Open Broadcaster Software\Uninstall.lnk" "$PROGRAMFILES32\OBS\uninstall.exe"

	${if} ${RunningX64}
		SetOutPath "$PROGRAMFILES64\OBS"
		CreateShortCut "$SMPROGRAMS\Open Broadcaster Software\Open Broadcaster Software (64bit).lnk" "$PROGRAMFILES64\OBS\OBS.exe"
	${endif}

	SetOutPath "$PROGRAMFILES32\OBS"
	
	StrCmp $outputErrors "yes" 0 +2
		Call filesInUse
SectionEnd

Section -FinishSection

	WriteRegStr HKLM "Software\${APPNAME}" "" "$PROGRAMFILES32\OBS"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName" "${APPNAME}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString" "$PROGRAMFILES32\OBS\uninstall.exe"

SectionEnd

; Modern install component descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${Section1} ""
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;Uninstall section
Section "un.OBS Program Files"
	
	SectionIn RO

	;Remove from registry...
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
	DeleteRegKey HKLM "SOFTWARE\${APPNAME}"

	; Delete self
	Delete "$PROGRAMFILES32\OBS\uninstall.exe"

	; Delete Shortcuts
	Delete "$DESKTOP\Open Broadcaster Software.lnk"
	Delete "$SMPROGRAMS\Open Broadcaster Software\Open Broadcaster Software (32bit).lnk"
	Delete "$SMPROGRAMS\Open Broadcaster Software\Uninstall.lnk"
	${if} ${RunningX64}
		Delete "$SMPROGRAMS\Open Broadcaster Software\Open Broadcaster Software (64bit).lnk"
	${endif}

	; Clean up Open Broadcaster Software
	Delete "$PROGRAMFILES32\OBS\OBS.exe"
	Delete "$PROGRAMFILES32\LICENSE"
	Delete "$PROGRAMFILES32\OBS\libx264-146.dll"
	Delete "$PROGRAMFILES32\OBS\QSVHelper.exe"
	Delete "$PROGRAMFILES32\OBS\OBSApi.dll"
	Delete "$PROGRAMFILES32\OBS\services.xconfig"
	Delete "$PROGRAMFILES32\OBS\*.pdb"
	Delete "$PROGRAMFILES32\OBS\ObsNvenc.dll"
	Delete "$PROGRAMFILES32\OBS\dbghelp.dll"
	Delete "$PROGRAMFILES32\OBS\locale\*.txt"
	Delete "$PROGRAMFILES32\OBS\shaders\*.?Shader"
	Delete "$PROGRAMFILES32\OBS\plugins\DShowPlugin.dll"
	Delete "$PROGRAMFILES32\OBS\plugins\GraphicsCapture.dll"
	Delete "$PROGRAMFILES32\OBS\plugins\NoiseGate.dll"
	Delete "$PROGRAMFILES32\OBS\plugins\PSVPlugin.dll"
	Delete "$PROGRAMFILES32\OBS\plugins\PSVPlugin\locale\*.txt"
	Delete "$PROGRAMFILES32\OBS\plugins\scenesw.dll"
	Delete "$PROGRAMFILES32\OBS\plugins\scenesw\locale\*.txt"
	Delete "$PROGRAMFILES32\OBS\plugins\DShowPlugin\locale\*.txt"
	Delete "$PROGRAMFILES32\OBS\plugins\DShowPlugin\shaders\*.?Shader"
	Delete "$PROGRAMFILES32\OBS\plugins\GraphicsCapture\*.dll"
	Delete "$PROGRAMFILES32\OBS\plugins\GraphicsCapture\*.exe"
	${if} ${RunningX64}
		Delete "$PROGRAMFILES64\OBS\OBS.exe"
		Delete "$PROGRAMFILES64\LICENSE"
		Delete "$PROGRAMFILES64\OBS\libx264-146.dll"
		Delete "$PROGRAMFILES64\OBS\QSVHelper.exe"
		Delete "$PROGRAMFILES64\OBS\OBSApi.dll"
		Delete "$PROGRAMFILES64\OBS\services.xconfig"
		Delete "$PROGRAMFILES64\OBS\*.pdb"
		Delete "$PROGRAMFILES64\OBS\ObsNvenc.dll"
		Delete "$PROGRAMFILES64\OBS\dbghelp.dll"
		Delete "$PROGRAMFILES64\OBS\locale\*.txt"
		Delete "$PROGRAMFILES64\OBS\shaders\*.?Shader"
		Delete "$PROGRAMFILES64\OBS\plugins\DShowPlugin.dll"
		Delete "$PROGRAMFILES64\OBS\plugins\GraphicsCapture.dll"
		Delete "$PROGRAMFILES64\OBS\plugins\NoiseGate.dll"
		Delete "$PROGRAMFILES64\OBS\plugins\PSVPlugin.dll"
		Delete "$PROGRAMFILES64\OBS\plugins\PSVPlugin\locale\*.txt"
		Delete "$PROGRAMFILES64\OBS\plugins\scenesw.dll"
		Delete "$PROGRAMFILES64\OBS\plugins\scenesw\locale\*.txt"
		Delete "$PROGRAMFILES64\OBS\plugins\DShowPlugin\locale\*.txt"
		Delete "$PROGRAMFILES64\OBS\plugins\DShowPlugin\shaders\*.?Shader"
		Delete "$PROGRAMFILES64\OBS\plugins\GraphicsCapture\*.dll"
		Delete "$PROGRAMFILES64\OBS\plugins\GraphicsCapture\*.exe"
	${endif}

	; Remove remaining directories
	RMDir "$SMPROGRAMS\Open Broadcaster Software"
	RMDir "$PROGRAMFILES32\OBS\plugins\GraphicsCapture\"
	RMDir "$PROGRAMFILES32\OBS\plugins\DShowPlugin\shaders\"
	RMDir "$PROGRAMFILES32\OBS\plugins\DShowPlugin\locale\"
	RMDir "$PROGRAMFILES32\OBS\plugins\DShowPlugin\"
	RMDir "$PROGRAMFILES32\OBS\plugins\PSVPlugin\locale\"
	RMDir "$PROGRAMFILES32\OBS\plugins\PSVPlugin\"
	RMDir "$PROGRAMFILES32\OBS\plugins\scenesw\locale\"
	RMDir "$PROGRAMFILES32\OBS\plugins\scenesw\"
	RMDir "$PROGRAMFILES32\OBS\plugins"
	RMDir "$PROGRAMFILES32\OBS\locale"
	RMDir "$PROGRAMFILES32\OBS\shaders"
	${if} ${RunningX64}
		RMDir "$PROGRAMFILES64\OBS\plugins\GraphicsCapture\"
		RMDir "$PROGRAMFILES64\OBS\plugins\DShowPlugin\shaders\"
		RMDir "$PROGRAMFILES64\OBS\plugins\DShowPlugin\locale\"
		RMDir "$PROGRAMFILES64\OBS\plugins\DShowPlugin\"
		RMDir "$PROGRAMFILES64\OBS\plugins\PSVPlugin\locale\"
		RMDir "$PROGRAMFILES64\OBS\plugins\PSVPlugin\"
		RMDir "$PROGRAMFILES64\OBS\plugins\scenesw\locale\"
		RMDir "$PROGRAMFILES64\OBS\plugins\scenesw\"
		RMDir "$PROGRAMFILES64\OBS\plugins"
		RMDir "$PROGRAMFILES64\OBS\locale"
		RMDir "$PROGRAMFILES64\OBS\shaders"
		RMDir "$PROGRAMFILES64\OBS"
	${endif}
	RMDir "$PROGRAMFILES32\OBS"
SectionEnd

Section /o "un.3rd Party Plugins" Section2
	RMDir /r "$PROGRAMFILES32\OBS\plugins"
	
	${if} ${RunningX64}
		RMDir /r "$PROGRAMFILES64\OBS\plugins"
	${endif}
SectionEnd

Section /o "un.User Settings" Section3
	RMDir /R "$APPDATA\OBS"
SectionEnd

!insertmacro MUI_UNFUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${Section1} "Remove the OBS program files."
	!insertmacro MUI_DESCRIPTION_TEXT ${Section2} "Removes any 3rd party plugins that may be installed."
	!insertmacro MUI_DESCRIPTION_TEXT ${Section3} "Removes all settings, scenes and sources, profiles, log files and other application data."
!insertmacro MUI_UNFUNCTION_DESCRIPTION_END

; eof
