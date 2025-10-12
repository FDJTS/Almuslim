; Inno Setup script for Almuslim
; Build with: ISCC.exe /DAppSource="<staging-app-folder>" /DAppVersion="0.1.0" installer.iss

#define MyAppName "Almuslim"
#ifndef AppVersion
#define AppVersion "0.1.0"
#endif
#ifndef AppSource
#define AppSource "app"
#endif

[Setup]
AppId={{2E9C5A21-3C1D-4A3E-9C2B-9A0D3F6EC0B1}
AppName={#MyAppName}
AppVersion={#AppVersion}
AppPublisher=Almuslim
DefaultDirName={localappdata}\Programs\{#MyAppName}
DisableDirPage=yes
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=.
OutputBaseFilename={#MyAppName}-Setup-{#AppVersion}
Compression=lzma
SolidCompression=yes
PrivilegesRequired=lowest
ArchitecturesInstallIn64BitMode=x64
SetupIconFile=..\..\App\Assets\logo\al-muslim.ico

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked

[Files]
; main executable
Source: "{#AppSource}\\al-muslim.exe"; DestDir: "{app}"; Flags: ignoreversion
; data directory (cities.csv)
Source: "{#AppSource}\\data\\*"; DestDir: "{app}\\data"; Flags: ignoreversion recursesubdirs createallsubdirs
; app icon for shortcuts
Source: "{#AppSource}\\al-muslim.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
; Start Menu
Name: "{group}\Almuslim"; Filename: "{app}\al-muslim.exe"; WorkingDir: "{app}"; IconFilename: "{app}\al-muslim.ico"
Name: "{group}\Almuslim (Setup City)"; Filename: "{app}\al-muslim.exe"; Parameters: "--setup"; WorkingDir: "{app}"; IconFilename: "{app}\al-muslim.ico"
; Desktop (optional)
Name: "{autodesktop}\Almuslim"; Filename: "{app}\al-muslim.exe"; Tasks: desktopicon; WorkingDir: "{app}"; IconFilename: "{app}\al-muslim.ico"

[Run]
Filename: "{app}\\al-muslim.exe"; Description: "Run Almuslim now (setup city)"; Parameters: "--setup"; Flags: nowait postinstall skipifsilent
