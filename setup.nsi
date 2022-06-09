;--------------------------------
; Includes

!include "MUI2.nsh"
!include "logiclib.nsh"

;--------------------------------
; Custom defines

!define NAME "proxinject"
!define APPFILE "proxinjector.exe"
!define SLUG "${NAME} ${VERSION}"

;--------------------------------
; General

Name "${NAME}"
OutFile "${NAME}Setup.exe"
InstallDir "$Appdata\${NAME}"
InstallDirRegKey HKCU "Software\${NAME}" ""
RequestExecutionLevel user

;--------------------------------
; UI

!define MUI_HEADERIMAGE
!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_UNFINISHPAGE_NOAUTOCLOSE
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "${SLUG} Setup"

;--------------------------------
; Pages

; Installer pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Set UI language
!insertmacro MUI_LANGUAGE "English"

;--------------------------------
; Section - Install App

Section "-hidden app"
    SectionIn RO
    SetOutPath "$INSTDIR"
    File /r "release\*.*" 
    WriteRegStr HKCU "Software\${NAME}" "" $INSTDIR
    WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

;--------------------------------
; Section - Shortcut

Section "Desktop Shortcut" DeskShort
    CreateShortCut "$DESKTOP\${NAME}.lnk" "$INSTDIR\${APPFILE}"
SectionEnd

Section "Start Menu Shortcut" StartShort
    CreateDirectory "$SMPROGRAMS\${NAME}"
    CreateShortCut "$SMPROGRAMS\${NAME}\${NAME}.lnk" "$INSTDIR\${APPFILE}"
    CreateShortCut "$SMPROGRAMS\${NAME}\Uninstall ${NAME}.lnk" "$INSTDIR\uninstall.exe"
SectionEnd

;--------------------------------
; Descriptions

;Language strings
LangString DESC_DeskShort ${LANG_ENGLISH} "Create Shortcut on Dekstop."
LangString DESC_StartShort ${LANG_ENGLISH} "Create Shortcut on Start Menu."

;Assign language strings to sections
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${DeskShort} $(DESC_DeskShort)
    !insertmacro MUI_DESCRIPTION_TEXT ${StartShort} $(DESC_StartShort)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
; Section - Uninstaller

Section "Uninstall"
    Delete "$DESKTOP\${NAME}.lnk"
    RMDir /r "$SMPROGRAMS\${NAME}"
    Delete "$INSTDIR\uninstall.exe"
    RMDir /r "$INSTDIR"
    DeleteRegKey /ifempty HKCU "Software\${NAME}"
SectionEnd
