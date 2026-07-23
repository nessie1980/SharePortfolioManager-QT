; Inno Setup Skript für Share Portfolio Manager (Qt/C++ Port)
; Erwartet, dass der CI-Workflow vor dem Aufruf von ISCC bereits einen
; vollständigen windeployqt-Output im Verzeichnis "deploy/" (Repo-Root)
; bereitgestellt hat.
;
; AppId bewusst fest vergeben (nicht bei jedem Lauf neu generieren!) —
; ein stabiler AppId ist nötig, damit spätere Versionen als Update über
; die vorherige Installation erkannt werden, statt eine zweite parallele
; Installation anzulegen. Falls jemals bewusst ein Bruch gewünscht ist
; (z. B. Namenswechsel), per Inno Setup IDE über Tools > Generate GUID
; einen neuen erzeugen.

#define MyAppName "Share Portfolio Manager"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "nessie1980"
#define MyAppExeName "SharePortfolioManager.exe"
#define MyAppIcon "..\..\app\resources\icons\app\app_icon.ico"

[Setup]
AppId={{B1E1D8E4-3F5A-4C9B-9E2D-8A1F6C7D9E10}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename=SharePortfolioManager-Setup
SetupIconFile={#MyAppIcon}
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
; Installer-Sprache ist bewusst nur Englisch/Standard (Default Dialog) —
; ein deutsches Sprachpaket (German.isl) ist in der Standard-CI-Installation
; von Inno Setup nicht enthalten und müsste extra nachgeladen werden. Die
; Anwendung selbst läuft davon unabhängig immer auf Deutsch. Bei Bedarf
; später nachrüstbar.

[Files]
Source: "..\..\deploy\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs
Source: "{#MyAppIcon}"; DestDir: "{app}"; DestName: "app_icon.ico"

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\app_icon.ico"
Name: "{group}\Deinstallieren"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\app_icon.ico"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Desktop-Symbol erstellen"; GroupDescription: "Zusätzliche Symbole:"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{#MyAppName} starten"; Flags: nowait postinstall skipifsilent
