; AURA Bass Preamp — instalador para Windows
; Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
; Licença: ver LICENSE  |  https://github.com/ledixplugins/Drive
;
; Monta um .exe que instala o plugin VST3 e o programa autônomo.
; Uso: makensis -DSRC=<pasta com os arquivos> -DVERSAO=<x.y.z> aura-bass-preamp.nsi

!include "MUI2.nsh"
!include "x64.nsh"

!ifndef VERSAO
  !define VERSAO "1.0.0"
!endif
!ifndef SRC
  !define SRC "..\dist\win"
!endif

Name "AURA Bass Preamp ${VERSAO}"
OutFile "AURA-Bass-Preamp-Windows.exe"
Unicode True
RequestExecutionLevel admin
InstallDir "$PROGRAMFILES64\AURA Bass Preamp"
ShowInstDetails show
ShowUninstDetails show

VIProductVersion "${VERSAO}.0"
VIAddVersionKey "ProductName" "AURA Bass Preamp"
VIAddVersionKey "CompanyName" "Thiago Galvao"
VIAddVersionKey "LegalCopyright" "Copyright (c) 2026 Thiago Galvao"
VIAddVersionKey "FileDescription" "Instalador do AURA Bass Preamp"
VIAddVersionKey "FileVersion" "${VERSAO}"

!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "AURA Bass Preamp"
!define MUI_WELCOMEPAGE_TEXT "Este instalador coloca o AURA Bass Preamp no seu computador.$\r$\n$\r$\nVoce pode usar o plugin VST3 na sua DAW (Reaper, Cubase, Studio One, Ableton) e o programa autonomo, que funciona sozinho, sem DAW.$\r$\n$\r$\nO teste e de 15 dias, com todos os recursos liberados."
!define MUI_FINISHPAGE_TITLE "Pronto"
!define MUI_FINISHPAGE_TEXT "O AURA Bass Preamp foi instalado.$\r$\n$\r$\nNa sua DAW, procure o plugin pelo nome AURA Bass Preamp. Se ele nao aparecer, mande a DAW reescanear os plugins VST3."
!define MUI_FINISHPAGE_RUN "$INSTDIR\AURA Bass Preamp.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Abrir o AURA Bass Preamp agora"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SRC}\EULA.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "PortugueseBR"

Section "Plugin VST3" SEC_VST3
  SectionIn RO
  SetOutPath "$COMMONFILES64\VST3\AURA Bass Preamp.vst3"
  File /r "${SRC}\VST3\AURA Bass Preamp.vst3\*.*"
  WriteRegStr HKLM "Software\AURA Bass Preamp" "VST3" "$COMMONFILES64\VST3\AURA Bass Preamp.vst3"
SectionEnd

Section "Programa autonomo" SEC_APP
  SetOutPath "$INSTDIR"
  File "${SRC}\AURA Bass Preamp.exe"
  File "${SRC}\EULA.txt"
  CreateDirectory "$SMPROGRAMS\AURA Bass Preamp"
  CreateShortcut "$SMPROGRAMS\AURA Bass Preamp\AURA Bass Preamp.lnk" "$INSTDIR\AURA Bass Preamp.exe"
  CreateShortcut "$SMPROGRAMS\AURA Bass Preamp\Desinstalar o AURA Bass Preamp.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section "-Pos"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AURABassPreamp" "DisplayName" "AURA Bass Preamp"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AURABassPreamp" "DisplayVersion" "${VERSAO}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AURABassPreamp" "Publisher" "Thiago Galvao"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AURABassPreamp" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AURABassPreamp" "DisplayIcon" "$\"$INSTDIR\AURA Bass Preamp.exe$\""
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AURABassPreamp" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AURABassPreamp" "NoRepair" 1
SectionEnd

LangString DESC_VST3 ${LANG_PORTUGUESEBR} "O plugin para usar dentro da sua DAW. Vai para a pasta padrao de VST3 do Windows."
LangString DESC_APP  ${LANG_PORTUGUESEBR} "O AURA Bass Preamp como programa, para tocar sem abrir uma DAW."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_VST3} $(DESC_VST3)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_APP}  $(DESC_APP)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  RMDir /r "$COMMONFILES64\VST3\AURA Bass Preamp.vst3"
  Delete "$INSTDIR\AURA Bass Preamp.exe"
  Delete "$INSTDIR\EULA.txt"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\AURA Bass Preamp\AURA Bass Preamp.lnk"
  Delete "$SMPROGRAMS\AURA Bass Preamp\Desinstalar o AURA Bass Preamp.lnk"
  RMDir "$SMPROGRAMS\AURA Bass Preamp"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AURABassPreamp"
  DeleteRegKey HKLM "Software\AURA Bass Preamp"
SectionEnd
