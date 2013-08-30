; Script generated with the Venis Install Wizard

; Define your application name
!define APPNAME "Open Broadcaster Software"
!define APPNAMEANDVERSION "Open Broadcaster Software 0.553b"

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
!define MUI_FINISHPAGE_RUN "$INSTDIR\OBS.exe"

!define MUI_PAGE_CUSTOMFUNCTION_LEAVE PreReqCheck

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "gplv2.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
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
	IfErrors dxMissing dxOK
	dxMissing:
		MessageBox MB_YESNO|MB_ICONEXCLAMATION "Your system is missing a DirectX update that ${APPNAME} requires. Would you like to download it?" IDYES dxtrue IDNO dxfalse
		dxtrue:
			ExecShell "open" "http://www.microsoft.com/en-us/download/details.aspx?id=35"
		dxfalse:
		Quit
	dxOK:
	ClearErrors
	
	; Check previous instance
	System::Call 'kernel32::OpenMutexW(i 0x100000, b 0, w "OBSMutex") i .R0'
	IntCmp $R0 0 notRunning
    System::Call 'kernel32::CloseHandle(i $R0)'
    MessageBox MB_OK|MB_ICONEXCLAMATION "${APPNAME} is already running. Please close it first before installing a new version." /SD IDOK
    Quit
notRunning:
	
FunctionEnd

Section "Open Broadcaster Software" Section1

	; Set Section properties
	SetOverwrite on

	; Set Section Files and Shortcuts
	SetOutPath "$INSTDIR\"
	File "..\Release\OBS.exe"
	File "..\x264\libs\32bit\libx264-136.dll"
	File "..\OBSAPI\Release\OBSApi.dll"
	File "..\rundir\services.xconfig"
	File "..\OBSHelp\OBSHelp.chm"
	File "..\rundir\pdb32\stripped\*.pdb"
	File "$%WindowsSDK80Path%Debuggers\x86\dbghelp.dll"
	SetOutPath "$INSTDIR\locale"
	File "..\rundir\locale\*.txt"
	SetOutPath "$INSTDIR\shaders\"
	File "..\rundir\shaders\*.?Shader"
	SetOutPath "$INSTDIR\plugins\"
	File "..\NoiseGate\Release\NoiseGate.dll"
	File "..\DShowPlugin\Release\DShowPlugin.dll"
	File "..\GraphicsCapture\Release\GraphicsCapture.dll"
	File "..\PSVPlugin\Release\PSVPlugin.dll"
	SetOutPath "$INSTDIR\plugins\PSVPlugin\locale\"
	File "..\rundir\plugins\PSVPlugin\locale\*.txt"
	SetOutPath "$INSTDIR\plugins\DShowPlugin\locale\"
	File "..\rundir\plugins\DShowPlugin\locale\*.txt"
	SetOutPath "$INSTDIR\plugins\DShowPlugin\shaders\"
	File "..\rundir\plugins\DShowPlugin\shaders\*.?Shader"
	SetOutPath "$INSTDIR\plugins\GraphicsCapture\"
	File "..\GraphicsCapture\GraphicsCaptureHook\Release\GraphicsCaptureHook.dll"
	File "..\GraphicsCapture\GraphicsCaptureHook\x64\Release\GraphicsCaptureHook64.dll"
	File "..\injectHelper\Release\injectHelper.exe"
	File "..\injectHelper\x64\Release\injectHelper64.exe"
	${if} ${RunningX64}
		SetOutPath "$INSTDIR\64bit\"
		File "..\x64\Release\OBS.exe"
		File "..\x264\libs\64bit\libx264-136.dll"
		File "..\OBSAPI\x64\Release\OBSApi.dll"
		File "..\rundir\services.xconfig"
		File "..\OBSHelp\OBSHelp.chm"
		File "..\rundir\pdb64\stripped\*.pdb"
		File "$%WindowsSDK80Path%Debuggers\x64\dbghelp.dll"
		SetOutPath "$INSTDIR\64bit\locale"
		File "..\rundir\locale\*.txt"
		SetOutPath "$INSTDIR\64bit\shaders\"
		File "..\rundir\shaders\*.?Shader"
		SetOutPath "$INSTDIR\64bit\plugins\"
		File "..\NoiseGate\x64\Release\NoiseGate.dll"
		File "..\DShowPlugin\x64\Release\DShowPlugin.dll"
		File "..\GraphicsCapture\x64\Release\GraphicsCapture.dll"
		File "..\PSVPlugin\x64\Release\PSVPlugin.dll"
		SetOutPath "$INSTDIR\64bit\plugins\PSVPlugin\locale\"
		File "..\rundir\plugins\PSVPlugin\locale\*.txt"
		SetOutPath "$INSTDIR\64bit\plugins\DShowPlugin\locale\"
		File "..\rundir\plugins\DShowPlugin\locale\*.txt"
		SetOutPath "$INSTDIR\64bit\plugins\DShowPlugin\shaders\"
		File "..\rundir\plugins\DShowPlugin\shaders\*.?Shader"
		SetOutPath "$INSTDIR\64bit\plugins\GraphicsCapture\"
		File "..\GraphicsCapture\GraphicsCaptureHook\Release\GraphicsCaptureHook.dll"
		File "..\GraphicsCapture\GraphicsCaptureHook\x64\Release\GraphicsCaptureHook64.dll"
		File "..\injectHelper\Release\injectHelper.exe"
		File "..\injectHelper\x64\Release\injectHelper64.exe"
	${endif}

	WriteUninstaller "$INSTDIR\uninstall.exe"

	SetOutPath "$INSTDIR\"
	CreateShortCut "$DESKTOP\Open Broadcaster Software.lnk" "$INSTDIR\OBS.exe"
	CreateDirectory "$SMPROGRAMS\Open Broadcaster Software"
	CreateShortCut "$SMPROGRAMS\Open Broadcaster Software\Open Broadcaster Software (32bit).lnk" "$INSTDIR\OBS.exe"
	CreateShortCut "$SMPROGRAMS\Open Broadcaster Software\Uninstall.lnk" "$INSTDIR\uninstall.exe"

	${if} ${RunningX64}
		SetOutPath "$INSTDIR\64bit\"
		CreateShortCut "$SMPROGRAMS\Open Broadcaster Software\Open Broadcaster Software (64bit).lnk" "$INSTDIR\64bit\OBS.exe"
	${endif}

	SetOutPath "$INSTDIR\"
SectionEnd

Section -FinishSection

	WriteRegStr HKLM "Software\${APPNAME}" "" "$INSTDIR"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName" "${APPNAME}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString" "$INSTDIR\uninstall.exe"

SectionEnd

; Modern install component descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${Section1} ""
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;Uninstall section
Section Uninstall

	;Remove from registry...
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
	DeleteRegKey HKLM "SOFTWARE\${APPNAME}"

	; Delete self
	Delete "$INSTDIR\uninstall.exe"

	; Delete Shortcuts
	Delete "$DESKTOP\Open Broadcaster Software.lnk"
	Delete "$SMPROGRAMS\Open Broadcaster Software\Open Broadcaster Software (32bit).lnk"
	Delete "$SMPROGRAMS\Open Broadcaster Software\Uninstall.lnk"
	${if} ${RunningX64}
		Delete "$SMPROGRAMS\Open Broadcaster Software\Open Broadcaster Software (64bit).lnk"
	${endif}

	; Clean up Open Broadcaster Software
	Delete "$INSTDIR\OBS.exe"
	Delete "$INSTDIR\libx264-136.dll"
	Delete "$INSTDIR\OBSApi.dll"
	Delete "$INSTDIR\services.xconfig"
	Delete "$INSTDIR\*.chm"
	Delete "$INSTDIR\*.pdb"
	Delete "$INSTDIR\dbghelp.dll"
	Delete "$INSTDIR\locale\*.txt"
	Delete "$INSTDIR\shaders\*.?Shader"
	Delete "$INSTDIR\plugins\DShowPlugin.dll"
	Delete "$INSTDIR\plugins\GraphicsCapture.dll"
	Delete "$INSTDIR\plugins\NoiseGate.dll"
	Delete "$INSTDIR\plugins\PSVPlugin.dll"
	Delete "$INSTDIR\plugins\PSVPlugin\locale\*.txt"
	Delete "$INSTDIR\plugins\DShowPlugin\locale\*.txt"
	Delete "$INSTDIR\plugins\DShowPlugin\shaders\*.?Shader"
	Delete "$INSTDIR\plugins\GraphicsCapture\*.dll"
	Delete "$INSTDIR\plugins\GraphicsCapture\*.exe"
	${if} ${RunningX64}
		Delete "$INSTDIR\64bit\OBS.exe"
		Delete "$INSTDIR\64bit\libx264-136.dll"
		Delete "$INSTDIR\64bit\OBSApi.dll"
		Delete "$INSTDIR\64bit\services.xconfig"
		Delete "$INSTDIR\64bit\*.chm"
		Delete "$INSTDIR\64bit\*.pdb"
		Delete "$INSTDIR\64bit\dbghelp.dll"
		Delete "$INSTDIR\64bit\locale\*.txt"
		Delete "$INSTDIR\64bit\shaders\*.?Shader"
		Delete "$INSTDIR\64bit\plugins\DShowPlugin.dll"
		Delete "$INSTDIR\64bit\plugins\GraphicsCapture.dll"
		Delete "$INSTDIR\64bit\plugins\NoiseGate.dll"
		Delete "$INSTDIR\64bit\plugins\PSVPlugin.dll"
		Delete "$INSTDIR\64bit\plugins\PSVPlugin\locale\*.txt"
		Delete "$INSTDIR\64bit\plugins\DShowPlugin\locale\*.txt"
		Delete "$INSTDIR\64bit\plugins\DShowPlugin\shaders\*.?Shader"
		Delete "$INSTDIR\64bit\plugins\GraphicsCapture\*.dll"
		Delete "$INSTDIR\64bit\plugins\GraphicsCapture\*.exe"
	${endif}

	; Remove remaining directories
	RMDir "$SMPROGRAMS\Open Broadcaster Software"
	RMDir "$INSTDIR\plugins\GraphicsCapture\"
	RMDir "$INSTDIR\plugins\DShowPlugin\shaders\"
	RMDir "$INSTDIR\plugins\DShowPlugin\locale\"
	RMDir "$INSTDIR\plugins\DShowPlugin\"
	RMDir "$INSTDIR\plugins\PSVPlugin\locale\"
	RMDir "$INSTDIR\plugins\PSVPlugin\"
	RMDir "$INSTDIR\plugins"
	RMDir "$INSTDIR\locale"
	RMDir "$INSTDIR\shaders"
	${if} ${RunningX64}
		RMDir "$INSTDIR\64bit\plugins\GraphicsCapture\"
		RMDir "$INSTDIR\64bit\plugins\DShowPlugin\shaders\"
		RMDir "$INSTDIR\64bit\plugins\DShowPlugin\locale\"
		RMDir "$INSTDIR\64bit\plugins\DShowPlugin\"
		RMDir "$INSTDIR\64bit\plugins\PSVPlugin\locale\"
		RMDir "$INSTDIR\64bit\plugins\PSVPlugin\"
		RMDir "$INSTDIR\64bit\plugins"
		RMDir "$INSTDIR\64bit\locale"
		RMDir "$INSTDIR\64bit\shaders"
		RMDir "$INSTDIR\64bit\"
	${endif}
	RMDir "$INSTDIR\"

SectionEnd

; eof