; Script generated by the HM NIS Edit Script Wizard.

!addPluginDir "nsis_plugins"

; Use the 7zip-like compressor
SetCompressor /FINAL /SOLID lzma


!include "springsettings.nsh"
!include "LogicLib.nsh"
!include "Sections.nsh"
!include "WordFunc.nsh"
!insertmacro VersionCompare

; this registry entry is deprecated (march 2011, use HKLM\Software\Spring\SpringEngine[Helper] instead)
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\spring.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"
!define PRODUCT_ROOT_KEY "HKLM"
!define PRODUCT_KEY "Software\Spring"

; MUI 1.67 compatible ------
!include "MUI.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "graphics\InstallerIcon.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "graphics\SideBanner.bmp"
;!define MUI_COMPONENTSPAGE_SMALLDESC ;puts description on the bottom, but much shorter.
!define MUI_COMPONENTSPAGE_TEXT_TOP "Some of these components must be downloaded during the install process."


; Welcome page
!insertmacro MUI_PAGE_WELCOME
; Licensepage
!insertmacro MUI_PAGE_LICENSE "..\gpl-2.0.txt"

; Components page
!insertmacro MUI_PAGE_COMPONENTS

; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES

; Finish page

!define MUI_FINISHPAGE_SHOWREADME "http://springrts.com/wiki/Read_Me_First"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Open $\"Read Me First$\" Webpage"
!define MUI_FINISHPAGE_RUN "$INSTDIR\springsettings.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Configure ${PRODUCT_NAME} settings now"
!define MUI_FINISHPAGE_TEXT "${PRODUCT_NAME} version ${PRODUCT_VERSION} has been successfully installed or updated from a previous version.  You should configure Spring settings now if this is a fresh installation.  If you did not install spring to C:\Program Files\Spring you will need to point the settings program to the install location."

!define MUI_FINISHPAGE_LINK "The ${PRODUCT_NAME} website"
!define MUI_FINISHPAGE_LINK_LOCATION ${PRODUCT_WEB_SITE}
!define MUI_FINISHPAGE_NOREBOOTSUPPORT

!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; MUI end ------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"

!define SP_OUTSUFFIX1 ""

; if present this should hold defines with custom mingwlibs location, etc.
!include /NONFATAL "custom_defines.nsi"

; set output filename for installer
OutFile "${SP_BASENAME}${SP_OUTSUFFIX1}.exe"
InstallDir "$PROGRAMFILES\Spring"
InstallDirRegKey ${PRODUCT_ROOT_KEY} "${PRODUCT_DIR_REGKEY}" "Path"

VAR REGISTRY ; if 1 registry values are written

!include "include\echo.nsh"
!include "include\fileassoc.nsh"
!include "include\fileExistChecks.nsh"
!include "include\fileMisc.nsh"
!ifndef SLIM
!include "include\checkrunning.nsh"
!endif
!include "include\aiHelpers.nsh"
!include "include\getParameterValue.nsh"

;!include "sections\ensureDotNet.nsh" ; deprecated, setup of zero-k installs .Net itself


${!echonow} ""
${!echonow} "Base dir:   <engine-source-root>/installer/"

; Set default values for undefined vars
!ifndef CONTENT_DIR
	!define CONTENT_DIR "..\cont"
!endif
${!defineifdirexists} CONTENT_DIR_EXISTS "${CONTENT_DIR}"
!ifndef CONTENT_DIR_EXISTS
	!error "Could not find the content dir at '${CONTENT_DIR}', try setting CONTENT_DIR manually."
	!undef CONTENT_DIR_EXISTS
!endif
${!echonow} "Using CONTENT_DIR:   ${CONTENT_DIR}"
!ifndef DOC_DIR
	!define DOC_DIR "..\doc"
!endif
${!defineifdirexists} DOC_DIR_EXISTS "${DOC_DIR}"
!ifndef DOC_DIR_EXISTS
	!error "Could not find the documentation dir at '${DOC_DIR}', try setting DOC_DIR manually."
	!undef DOC_DIR_EXISTS
!endif
${!echonow} "Using DOC_DIR:       ${DOC_DIR}"
!ifndef MINGWLIBS_DIR
	!define MINGWLIBS_DIR "..\mingwlibs"
!endif
${!defineifdirexists} MINGWLIBS_DIR_EXISTS "${MINGWLIBS_DIR}"
!ifndef MINGWLIBS_DIR_EXISTS
	!error "Could not find the MinGW libraries dir at '${MINGWLIBS_DIR}', try setting MINGWLIBS_DIR manually."
	!undef MINGWLIBS_DIR_EXISTS
!endif
${!echonow} "Using MINGWLIBS_DIR: ${MINGWLIBS_DIR}"
!ifndef BUILD_DIR
	!ifndef DIST_DIR
		!error "Neither BUILD_DIR nor DIST_DIR are defined. Define only one of the two, depending on whether you want to generate the installer from the install- or the build-directory."
	!endif
	${!defineifdirexists} DIST_DIR_EXISTS "${DIST_DIR}"
	!ifndef DIST_DIR_EXISTS
		!error "Could not find the distribution dir at '${DIST_DIR}'. Make sure you defined DIST_DIR correctly."
		!undef DIST_DIR_EXISTS
	!endif
	${!echonow} "Using DIST_DIR:      ${DIST_DIR}"
	!define BUILD_OR_DIST_DIR "${DIST_DIR}"
!endif
!ifdef BUILD_DIR
	!ifdef DIST_DIR
		!error "Both BUILD_DIR and DIST_DIR are defined. Define only one of the two, depending on whether you want to generate the installer from the install- or the build-directory."
	!endif
	${!defineifdirexists} BUILD_DIR_EXISTS "${BUILD_DIR}"
	!ifndef BUILD_DIR_EXISTS
		!error "Could not find the build dir at '${BUILD_DIR}'. Make sure you defined BUILD_DIR correctly."
		!undef BUILD_DIR_EXISTS
	!endif
	${!echonow} "Using BUILD_DIR:     ${BUILD_DIR}"
	; This allows us to easily use build products from an out of source build,
	; without the need to run 'make install'
	!define USE_BUILD_DIR
	!define BUILD_OR_DIST_DIR "${BUILD_DIR}"
!endif

!ifndef PORTABLE_ARCHIVE
	!error "PORTABLE_ARCHIVE undefined: please specifiy where portable 7z-archive which contains the spring-engine is"
!else
	${!echonow} "Using PORTABLE_ARCHIVE:      ${PORTABLE_ARCHIVE}"
!endif

!ifndef ARCHIVEMOVER
	!warning "ARCHIVEMOVER not defined"
!else
	${!echonow} "Using ARCHIVEMOVER:      ${ARCHIVEMOVER}"
!endif


SectionGroup /e "!Engine"
	Section "Main application (req)" SEC_MAIN
		; make this section read-only -> user can not deselect it
		SectionIn RO

		!define INSTALL
			${!echonow} "Processing: main"
			!include "sections\main.nsh"
			${!echonow} "Processing: springsettings"
			!include "sections\springsettings.nsh"
			${!echonow} "Processing: deprecated"
		        !include "sections\deprecated.nsh"
		!undef INSTALL
	SectionEnd
SectionGroupEnd


!ifndef SLIM
SectionGroup "Multiplayer battlerooms"
	Section "SpringLobby" SEC_SPRINGLOBBY
		!define INSTALL
			${!echonow} "Processing: springlobby"
			!include "sections\springlobby.nsh"
		!undef INSTALL
	SectionEnd

	Section "Zero-K lobby" SEC_ZERO_K_LOBBY
		!define INSTALL
			${!echonow} "Processing: zeroK"
			!include "sections\zeroK.nsh"
		!undef INSTALL
	SectionEnd
SectionGroupEnd


SectionGroup "Multiplayer lobby servers"
	Section "TASServer" SEC_TASSERVER
		!define INSTALL
			${!echonow} "Processing: TASServer"
			!include "sections\tasServer.nsh"
		!undef INSTALL
	SectionEnd
SectionGroupEnd

Section "Desktop shortcuts" SEC_DESKTOP
	${If} ${SectionIsSelected} ${SEC_SPRINGLOBBY}
		!define INSTALL
			${!echonow} "Processing: shortcuts - Desktop"
			!include "sections\shortcuts_desktop.nsh"
		!undef INSTALL
	${EndIf}
SectionEnd
!endif

!ifndef SLIM
SectionGroup "Tools"
	Section "Easy content installation" SEC_ARCHIVEMOVER
		!define INSTALL
			${!echonow} "Processing: archivemover"
			!include "sections\archivemover.nsh"
		!undef INSTALL
	SectionEnd

	Section "Simple spring-rapid downloader" SEC_RAPID
		!define INSTALL
			${!echonow} "Processing: rapid"
			!include "sections\rapid.nsh"
		!undef INSTALL
	SectionEnd
SectionGroupEnd

!endif

Section "Start menu shortcuts" SEC_START
	!define INSTALL
		${!echonow} "Processing: shortcuts - Start menu"
		!include "sections\shortcuts_startMenu.nsh"
	!undef INSTALL
SectionEnd


Section /o "Portable" SEC_PORTABLE
	!define INSTALL
		${!echonow} "Processing: Portable"
		!include "sections\portable.nsh"
	!undef INSTALL
SectionEnd


!include "sections\sectiondesc.nsh"

!ifndef SLIM
Section -Documentation
	!define INSTALL
		${!echonow} "Processing: docs"
		!include "sections\docs.nsh"
	!undef INSTALL
SectionEnd
!endif

Section -Post
	${!echonow} "Processing: Registry entries"
	IntOp $R0 ${SEC_PORTABLE} & ${SF_SELECTED} ; check if in normal mode
	${If} $R0 != ${SF_SELECTED}
		WriteUninstaller "$INSTDIR\uninst.exe"
	${EndIf}
	${If} $REGISTRY = 1
		WriteRegStr ${PRODUCT_ROOT_KEY} "${PRODUCT_DIR_REGKEY}" "@" "$INSTDIR\spring.exe"
		WriteRegStr ${PRODUCT_ROOT_KEY} "${PRODUCT_DIR_REGKEY}" "Path" "$INSTDIR"
		WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
		WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
		WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\spring.exe"
		WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
		WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
		WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
	${EndIf}
SectionEnd

Function .onInit
	${!echonow} ""
!ifndef SLIM
	IfSilent skiprunchecks ; don't check for running apps, the calling app has to do it
	; check if we need to exit some processes which may be using unitsync
	${CheckExecutableRunning} "TASClient.exe" "TASClient"
	${CheckExecutableRunning} "springlobby.exe" "Spring Lobby"
	${CheckExecutableRunning} "Zero-K.exe" "Zero-K Lobby"
	${CheckExecutableRunning} "CADownloader.exe" "CA Downloader"
	${CheckExecutableRunning} "springsettings.exe" "Spring Settings"
	skiprunchecks:

	; The core cannot be deselected
	IntOp $0 ${SEC_MAIN} | ${SF_RO}
	SectionSetFlags ${SEC_MAIN} $0 ; make the core section read only

!endif
	; enable/disable sections depending on parameters
	!include "sections/SetupSections.nsh"
FunctionEnd

Function un.onUninstSuccess
	IfSilent +3
	HideWindow
	MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was successfully removed from your computer."
FunctionEnd

Function un.onInit
	IfSilent +3
	MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you want to completely remove $(^Name) and all of its components?" IDYES +2
	Abort
FunctionEnd


Section Uninstall
	${!echonow} "Processing: Uninstall"

	!include "sections\main.nsh"
	!include "sections\deprecated.nsh"
	!include "sections\springsettings.nsh"

	Delete "$INSTDIR\spring-multithreaded.exe"

	!include "sections\docs.nsh"
	!include "sections\shortcuts_startMenu.nsh"
	!include "sections\shortcuts_desktop.nsh"
!ifndef SLIM
	!include "sections\archivemover.nsh"
	!include "sections\portable.nsh"
	!include "sections\zeroK.nsh"
	!include "sections\tasServer.nsh"
	!include "sections\rapid.nsh"
!endif
	!insertmacro DeleteSkirmishAI "AAI"
	!insertmacro DeleteSkirmishAI "KAIK"
	!insertmacro DeleteSkirmishAI "RAI"
	!insertmacro DeleteSkirmishAI "E323AI"
!ifndef SLIM
	!include "sections\springlobby.nsh"
!endif
	; All done
	RMDir "$INSTDIR"

	DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
	DeleteRegKey ${PRODUCT_ROOT_KEY} "${PRODUCT_DIR_REGKEY}"
	SetAutoClose true
SectionEnd
