; This file is part of Pazpar2.
; Copyright (C) 2006-2009 Index Data

!include version.nsi

; Microsoft runtime CRT 
; Uncomment exactly ONE section of the three below
; 1: MSVC 6
!define VS_RUNTIME_DLL ""
!define VS_RUNTIME_MANIFEST ""

; 2: VS 2003
; !define VS_RUNTIME_DLL "c:\Program Files\Microsoft Visual Studio .NET 2003\SDK\v1.1\Bin\msvcr71.dll"
;!define VS_RUNTIME_MANIFEST ""

; 3: VS 2005
;!define VS_RUNTIME_DLL      "c:\Program Files\Microsoft Visual Studio 8\VC\redist\x86\Microsoft.VC80.CRT\msvcr80.dll"
;!define VS_RUNTIME_MANIFEST "c:\Program Files\Microsoft Visual Studio 8\VC\redist\x86\Microsoft.VC80.CRT\Microsoft.VC80.CRT.manifest"


!include "MUI.nsh"

SetCompressor bzip2

Name "Pazpar2"
Caption "Index Data Pazpar2 ${VERSION} Setup"
OutFile "pazpar2_${VERSION}.exe"

LicenseText "You must read the following license before installing:"
LicenseData license.txt

ComponentText "This will install Pazpar2 on your computer:"
InstType "Full (w/ Source)"
InstType "Lite (w/o Source)"

InstallDir "$PROGRAMFILES\Pazpar2"
InstallDirRegKey HKLM "SOFTWARE\Index Data\Pazpar2" ""


;----------------------------
; Pages


  !insertmacro MUI_PAGE_LICENSE "license.txt"
  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
; Page components
; Page directory
; Page instfiles

; UninstPage uninstConfirm
; UninstPage instfiles

;--------------------------------
;Languages
 
!insertmacro MUI_LANGUAGE "English"

;--------------------------------

Section "" ; (default section)
	SetOutPath "$INSTDIR"
	; add files / whatever that need to be installed here.
	WriteRegStr HKLM "SOFTWARE\Index Data\Pazpar2" "" "$INSTDIR"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Pazpar2" "DisplayName" "Pazpar2 ${VERSION} (remove only)"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Pazpar2" "UninstallString" '"$INSTDIR\uninst.exe"'
	; write out uninstaller
	WriteUninstaller "$INSTDIR\uninst.exe"
	SetOutPath $SMPROGRAMS\Pazpar2
 	CreateShortCut "$SMPROGRAMS\Pazpar2\Pazpar2 Program Directory.lnk" \
                 "$INSTDIR"
	WriteINIStr "$SMPROGRAMS\Pazpar2\Pazpar2 Home page.url" \
              "InternetShortcut" "URL" "http://www.indexdata.dk/pazpar2/"
	CreateShortCut "$SMPROGRAMS\Pazpar2\Uninstall Pazpar2.lnk" \
		"$INSTDIR\uninst.exe"
	SetOutPath $INSTDIR
	File LICENSE.txt
	File ..\README
	File ..\NEWS
	SetOutPath $INSTDIR/etc
	File /r ..\etc\*.xml
	File /r ..\etc\*.xsl
	File /r ..\etc\*.dist

SectionEnd ; end of default section

Section "Pazpar2 Runtime" Pazpar2_Runtime
	SectionIn 1 2
	IfFileExists "$INSTDIR\bin\pazpar2.exe" 0 Noservice
	ExecWait '"$INSTDIR\bin\pazpar2.exe" -remove'
Noservice:
	SetOutPath $INSTDIR\bin
	File "${VS_RUNTIME_DLL}"
	File "${VS_RUNTIME_MANIFEST}"
	File ..\bin\*.dll
	File ..\bin\*.exe
	SetOutPath $SMPROGRAMS\Pazpar2
	SetOutPath $SMPROGRAMS\Pazpar2\Server
 	CreateShortCut "$SMPROGRAMS\Pazpar2\Server\Server on console.lnk" \
                 "$INSTDIR\bin\pazpar2.exe" '-f"$INSTDIR\etc\pazpar2.cfg"'
  	CreateShortCut "$SMPROGRAMS\Pazpar2\Server\Install Z39.50 service.lnk" \
                  "$INSTDIR\bin\pazpar2.exe" '-installa -f"$INSTDIR\etc\pazpar2.cfg"'
 	CreateShortCut "$SMPROGRAMS\Pazpar2\Server\Remove Pazpar2 service.lnk" \
                 "$INSTDIR\bin\pazpar2.exe" '-remove'
SectionEnd

Section "Pazpar2 Documentation" Pazpar2_Documentation
	SectionIn 1 2
	SetOutPath $INSTDIR\doc
	File /r ..\doc\*.css
	File /r ..\doc\*.ent
	File /r ..\doc\*.html
	File /r ..\doc\*.xml
	File /r ..\doc\*.png
	File /r ..\doc\*.xsl
	SetOutPath $SMPROGRAMS\Pazpar2
	CreateShortCut "$SMPROGRAMS\Pazpar2\HTML Documentation.lnk" \
                 "$INSTDIR\doc\index.html"
SectionEnd

Section "Pazpar2 Source" Pazpar2_Source
	SectionIn 1
	SetOutPath $INSTDIR
	File /r ..\*.c
	File /r ..\*.h
	SetOutPath $INSTDIR\win
	File makefile
	File *.nsi
SectionEnd

; begin uninstall settings/section
UninstallText "This will uninstall Pazpar2 ${VERSION} from your system"

Section Uninstall
; add delete commands to delete whatever files/registry keys/etc you installed here.
	Delete "$INSTDIR\uninst.exe"
	DeleteRegKey HKLM "SOFTWARE\Index Data\Pazpar2"
	DeleteRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Pazpar2"
	ExecWait '"$INSTDIR\bin\pazpar2" -remove'
	RMDir /r $SMPROGRAMS\Pazpar2
	RMDir /r $INSTDIR
        IfFileExists $INSTDIR 0 Removed 
		MessageBox MB_OK|MB_ICONEXCLAMATION \
                 "Note: $INSTDIR could not be removed."
Removed:
SectionEnd

;--------------------------------
;Descriptions

  ;Language strings
LangString DESC_Pazpar2_Runtime ${LANG_ENGLISH} "Pazpar2 runtime files needed in order for it to run, such as DLLs."
LangString DESC_Pazpar2_Documentation ${LANG_ENGLISH} "Pazpar2 Users' guide and reference in HTML."
LangString DESC_Pazpar2_Source ${LANG_ENGLISH} "Source code of Pazpar2. Required if you need to rebuild Pazpar2 (for debugging purposes)."

;Assign language strings to sections
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT ${Pazpar2_Runtime} $(DESC_Pazpar2_Runtime)
!insertmacro MUI_DESCRIPTION_TEXT ${Pazpar2_Documentation} $(DESC_Pazpar2_Documentation)
!insertmacro MUI_DESCRIPTION_TEXT ${Pazpar2_Source} $(DESC_Pazpar2_Source)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; eof
