; This file is part of Pazpar2.
; Copyright (C) Index Data
; See the file LICENSE for details.

Unicode true

!include version.nsi

!include "MUI.nsh"

Name "Pazpar2"

!include "..\m4\common.nsi"

RequestExecutionLevel admin

SetCompressor bzip2

Caption "Index Data Pazpar2 ${VERSION} Setup"
OutFile "pazpar2_${VERSION}.exe"

LicenseText "You must read the following license before installing:"
LicenseData license.txt

ComponentText "This will install Pazpar2 on your computer:"
InstType "Full (w/ Source)"
InstType "Lite (w/o Source)"

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
	File ..\README.md
	File ..\NEWS
	SetOutPath $INSTDIR\etc
	File /r ..\etc\*.xml
	File /r ..\etc\*.xsl
	File /r ..\etc\*.mmap
	File /oname=pazpar2.cfg ..\etc\pazpar2.cfg.dist 
	SetOutPath $INSTDIR\log

SectionEnd ; end of default section

Section "Pazpar2 Runtime" Pazpar2_Runtime
	SectionIn 1 2
	IfFileExists "$INSTDIR\bin\pazpar2.exe" 0 Noservice
	ExecWait '"$INSTDIR\bin\pazpar2.exe" -remove'
Noservice:
	SetOutPath $INSTDIR\bin
!if "${VS_REDIST_FULL}" != ""
	File "${VS_REDIST_FULL}"
	ReadRegDword $1 HKLM "${VS_REDIST_KEY}" "Version"
	${If} $1 == ""
	  ExecWait '"$INSTDIR\bin\${VS_REDIST_EXE}" /passive /nostart'
	${endif}
	Delete "$INSTDIR\bin\${VS_REDIST_EXE}"
!endif
	File ..\bin\*.dll
	File ..\bin\*.exe
	SetOutPath $SMPROGRAMS\Pazpar2
	SetOutPath $SMPROGRAMS\Pazpar2\Server
 	CreateShortCut "$SMPROGRAMS\Pazpar2\Server\Server on console.lnk" \
                 "$INSTDIR\bin\pazpar2.exe" '-f"$INSTDIR\etc\pazpar2.cfg"'
  	CreateShortCut "$SMPROGRAMS\Pazpar2\Server\Install Pazpar2 service.lnk" \
                 "$INSTDIR\bin\pazpar2.exe" '-install -l"$INSTDIR\log\pazpar2.log" -f"$INSTDIR\etc\pazpar2.cfg"'
 	CreateShortCut "$SMPROGRAMS\Pazpar2\Server\Remove Pazpar2 service.lnk" \
                 "$INSTDIR\bin\pazpar2.exe" '-remove'
SectionEnd

Section "Pazpar2 Documentation" Pazpar2_Documentation
	SectionIn 1 2
	SetOutPath $INSTDIR\doc
	File /nonfatal /r ..\doc\*.css
	File /nonfatal /r ..\doc\*.ent
	File /nonfatal /r ..\doc\*.html
	File /r ..\doc\*.xml
	File /r ..\doc\*.png
	File /nonfatal /r ..\doc\*.xsl
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
	SetOutPath $INSTDIR\m4
	File ..\m4\*.m4
	File ..\m4\*.tcl
	File ..\m4\*.nsi
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
