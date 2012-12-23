; Script generated with the Venis Install Wizard

; Define your application name
!define APPNAME "Open Broadcaster Software"
!define APPNAMEANDVERSION "Open Broadcaster Software 0.452a"

; Main Install settings
Name "${APPNAMEANDVERSION}"
InstallDir "$PROGRAMFILES\OBS"
InstallDirRegKey HKLM "Software\${APPNAME}" ""
OutFile "OBS_Installer.exe"

; Use compression
SetCompressor LZMA

; Need Admin
RequestExecutionLevel admin

; Modern interface settings
!include "MUI.nsh"

; Additional script dependencies
!include WinVer.nsh

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
		nsexec::exectostack "wmic qfe where HotFixID='KB971512' get HotFixID /Format:list"
		pop $0
		pop $0
		strcpy $1 $0 17 6
		strcmps $1 "HotFixID=KB971512" gotPatch
			MessageBox MB_YESNO|MB_ICONEXCLAMATION "${APPNAME} requires the Windows Vista Platform Update. Would you like to download it?" IDYES putrue IDNO pufalse
			putrue:
				ExecShell "open" "http://www.microsoft.com/en-us/download/details.aspx?id=3274"
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
		MessageBox MB_YESNO|MB_ICONEXCLAMATION "Your system is missing a DirectX update that {$APPNAME} requires. Would you like to download it?" IDYES dxtrue IDNO dxfalse
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
	File "..\rundir\OBS.exe"
	File "..\rundir\OBS.exe"
	File "..\rundir\OBSApi.dll"
	File "..\rundir\services.xconfig"
	SetOutPath "$INSTDIR\locale"
	File "..\rundir\locale\bg.txt"
	File "..\rundir\locale\br.txt"
	File "..\rundir\locale\de.txt"
	File "..\rundir\locale\el.txt"
	File "..\rundir\locale\en.txt"
	File "..\rundir\locale\es.txt"
	File "..\rundir\locale\et.txt"
	File "..\rundir\locale\fi.txt"
	File "..\rundir\locale\fr.txt"
	File "..\rundir\locale\ja.txt"
	File "..\rundir\locale\nb.txt"
	File "..\rundir\locale\pl.txt"
	File "..\rundir\locale\ru.txt"
	File "..\rundir\locale\tw.txt"
	SetOutPath "$INSTDIR\plugins\"
	File "..\rundir\plugins\DShowPlugin.dll"
	File "..\rundir\plugins\GraphicsCapture.dll"
	SetOutPath "$INSTDIR\plugins\DShowPlugin\locale\"
	File "..\rundir\plugins\DShowPlugin\locale\bg.txt"
	File "..\rundir\plugins\DShowPlugin\locale\br.txt"
	File "..\rundir\plugins\DShowPlugin\locale\de.txt"
	File "..\rundir\plugins\DShowPlugin\locale\el.txt"
	File "..\rundir\plugins\DShowPlugin\locale\en.txt"
	File "..\rundir\plugins\DShowPlugin\locale\es.txt"
	File "..\rundir\plugins\DShowPlugin\locale\et.txt"
	File "..\rundir\plugins\DShowPlugin\locale\fi.txt"
	File "..\rundir\plugins\DShowPlugin\locale\ja.txt"
	File "..\rundir\plugins\DShowPlugin\locale\nb.txt"
	File "..\rundir\plugins\DShowPlugin\locale\ru.txt"
	File "..\rundir\plugins\DShowPlugin\locale\tw.txt"
	SetOutPath "$INSTDIR\plugins\DShowPlugin\shaders\"
	File "..\rundir\plugins\DShowPlugin\shaders\ChromaKey_HDYCToRGB.pShader"
	File "..\rundir\plugins\DShowPlugin\shaders\ChromaKey_RGB.pShader"
	File "..\rundir\plugins\DShowPlugin\shaders\ChromaKey_UYVToRGB.pShader"
	File "..\rundir\plugins\DShowPlugin\shaders\ChromaKey_YUVToRGB.pShader"
	File "..\rundir\plugins\DShowPlugin\shaders\ChromaKey_YUXVToRGB.pShader"
	File "..\rundir\plugins\DShowPlugin\shaders\ChromaKey_YVUToRGB.pShader"
	File "..\rundir\plugins\DShowPlugin\shaders\ChromaKey_YVXUToRGB.pShader"
	File "..\rundir\plugins\DShowPlugin\shaders\HDYCToRGB.pShader"
	File "..\rundir\plugins\DShowPlugin\shaders\UYVToRGB.pShader"
	File "..\rundir\plugins\DShowPlugin\shaders\YUVToRGB.pShader"
	File "..\rundir\plugins\DShowPlugin\shaders\YUXVToRGB.pShader"
	File "..\rundir\plugins\DShowPlugin\shaders\YVUToRGB.pShader"
	File "..\rundir\plugins\DShowPlugin\shaders\YVXUToRGB.pShader"
	SetOutPath "$INSTDIR\plugins\GraphicsCapture\"
	File "..\rundir\plugins\GraphicsCapture\GraphicsCaptureHook.dll"
	SetOutPath "$INSTDIR\shaders\"
	File "..\rundir\shaders\AlphaIgnore.pShader"
	File "..\rundir\shaders\ColorKey_RGB.pShader"
	File "..\rundir\shaders\DownscaleYUV1.5.pShader"
	File "..\rundir\shaders\DownscaleYUV2.25.pShader"
	File "..\rundir\shaders\DownscaleYUV2.pShader"
	File "..\rundir\shaders\DownscaleYUV3.pShader"
	File "..\rundir\shaders\DrawSolid.pShader"
	File "..\rundir\shaders\DrawSolid.vShader"
	File "..\rundir\shaders\DrawTexture.pShader"
	File "..\rundir\shaders\DrawTexture.vShader"
	File "..\rundir\shaders\DrawYUVTexture.pShader"
	File "..\rundir\shaders\InvertTexture.pShader"
	CreateShortCut "$DESKTOP\Open Broadcaster Software.lnk" "$INSTDIR\OBS.exe"
	CreateDirectory "$SMPROGRAMS\Open Broadcaster Software"
	CreateShortCut "$SMPROGRAMS\Open Broadcaster Software\Open Broadcaster Software.lnk" "$INSTDIR\OBS.exe"
	CreateShortCut "$SMPROGRAMS\Open Broadcaster Software\Uninstall.lnk" "$INSTDIR\uninstall.exe"

SectionEnd

Section -FinishSection

	WriteRegStr HKLM "Software\${APPNAME}" "" "$INSTDIR"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName" "${APPNAME}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString" "$INSTDIR\uninstall.exe"
	WriteUninstaller "$INSTDIR\uninstall.exe"

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
	Delete "$SMPROGRAMS\Open Broadcaster Software\Open Broadcaster Software.lnk"
	Delete "$SMPROGRAMS\Open Broadcaster Software\Uninstall.lnk"

	; Clean up Open Broadcaster Software
	Delete "$INSTDIR\OBS.exe"
	Delete "$INSTDIR\OBSApi.dll"
	Delete "$INSTDIR\services.xconfig"
	Delete "$INSTDIR\locale\bg.txt"
	Delete "$INSTDIR\locale\br.txt"
	Delete "$INSTDIR\locale\de.txt"
	Delete "$INSTDIR\locale\el.txt"
	Delete "$INSTDIR\locale\en.txt"
	Delete "$INSTDIR\locale\es.txt"
	Delete "$INSTDIR\locale\et.txt"
	Delete "$INSTDIR\locale\fi.txt"
	Delete "$INSTDIR\locale\fr.txt"
	Delete "$INSTDIR\locale\ja.txt"
	Delete "$INSTDIR\locale\nb.txt"
	Delete "$INSTDIR\locale\pl.txt"
	Delete "$INSTDIR\locale\ru.txt"
	Delete "$INSTDIR\locale\tw.txt"
	Delete "$INSTDIR\plugins\DShowPlugin.dll"
	Delete "$INSTDIR\plugins\GraphicsCapture.dll"
	Delete "$INSTDIR\plugins\DShowPlugin\locale\bg.txt"
	Delete "$INSTDIR\plugins\DShowPlugin\locale\br.txt"
	Delete "$INSTDIR\plugins\DShowPlugin\locale\de.txt"
	Delete "$INSTDIR\plugins\DShowPlugin\locale\el.txt"
	Delete "$INSTDIR\plugins\DShowPlugin\locale\en.txt"
	Delete "$INSTDIR\plugins\DShowPlugin\locale\es.txt"
	Delete "$INSTDIR\plugins\DShowPlugin\locale\et.txt"
	Delete "$INSTDIR\plugins\DShowPlugin\locale\fi.txt"
	Delete "$INSTDIR\plugins\DShowPlugin\locale\ja.txt"
	Delete "$INSTDIR\plugins\DShowPlugin\locale\nb.txt"
	Delete "$INSTDIR\plugins\DShowPlugin\locale\ru.txt"
	Delete "$INSTDIR\plugins\DShowPlugin\locale\tw.txt"
	Delete "$INSTDIR\plugins\DShowPlugin\shaders\ChromaKey_HDYCToRGB.pShader"
	Delete "$INSTDIR\plugins\DShowPlugin\shaders\ChromaKey_RGB.pShader"
	Delete "$INSTDIR\plugins\DShowPlugin\shaders\ChromaKey_UYVToRGB.pShader"
	Delete "$INSTDIR\plugins\DShowPlugin\shaders\ChromaKey_YUVToRGB.pShader"
	Delete "$INSTDIR\plugins\DShowPlugin\shaders\ChromaKey_YUXVToRGB.pShader"
	Delete "$INSTDIR\plugins\DShowPlugin\shaders\ChromaKey_YVUToRGB.pShader"
	Delete "$INSTDIR\plugins\DShowPlugin\shaders\ChromaKey_YVXUToRGB.pShader"
	Delete "$INSTDIR\plugins\DShowPlugin\shaders\HDYCToRGB.pShader"
	Delete "$INSTDIR\plugins\DShowPlugin\shaders\UYVToRGB.pShader"
	Delete "$INSTDIR\plugins\DShowPlugin\shaders\YUVToRGB.pShader"
	Delete "$INSTDIR\plugins\DShowPlugin\shaders\YUXVToRGB.pShader"
	Delete "$INSTDIR\plugins\DShowPlugin\shaders\YVUToRGB.pShader"
	Delete "$INSTDIR\plugins\DShowPlugin\shaders\YVXUToRGB.pShader"
	Delete "$INSTDIR\plugins\GraphicsCapture\GraphicsCaptureHook.dll"
	Delete "$INSTDIR\plugins\GraphicsCapture\AlphaIgnore.pShader"
	Delete "$INSTDIR\plugins\GraphicsCapture\ColorKey_RGB.pShader"
	Delete "$INSTDIR\plugins\GraphicsCapture\DownscaleYUV1.5.pShader"
	Delete "$INSTDIR\plugins\GraphicsCapture\DownscaleYUV2.25.pShader"
	Delete "$INSTDIR\plugins\GraphicsCapture\DownscaleYUV2.pShader"
	Delete "$INSTDIR\plugins\GraphicsCapture\DownscaleYUV3.pShader"
	Delete "$INSTDIR\plugins\GraphicsCapture\DrawSolid.pShader"
	Delete "$INSTDIR\plugins\GraphicsCapture\DrawSolid.vShader"
	Delete "$INSTDIR\plugins\GraphicsCapture\DrawTexture.pShader"
	Delete "$INSTDIR\plugins\GraphicsCapture\DrawTexture.vShader"
	Delete "$INSTDIR\plugins\GraphicsCapture\DrawYUVTexture.pShader"
	Delete "$INSTDIR\plugins\GraphicsCapture\InvertTexture.pShader"

	; Remove remaining directories
	RMDir "$SMPROGRAMS\Open Broadcaster Software"
	RMDir "$INSTDIR\plugins\GraphicsCapture\"
	RMDir "$INSTDIR\plugins\DShowPlugin\shaders\"
	RMDir "$INSTDIR\plugins\DShowPlugin\locale\"
	RMDir "$INSTDIR\plugins\DShowPlugin\"
	RMDir "$INSTDIR\"

SectionEnd

; eof