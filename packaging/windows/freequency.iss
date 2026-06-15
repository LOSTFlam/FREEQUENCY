; FREEQUENCY Windows installer (Inno Setup 6)
; Build: iscc /DBuildDir="..\..\build\src\FREEQUENCY_artefacts\Release" /DAppVersion="0.1.0" freequency.iss

#ifndef BuildDir
  #define BuildDir "..\..\build\src\FREEQUENCY_artefacts\Release"
#endif
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

[Setup]
AppId={{A7B3C4D5-E6F7-4890-ABCD-EF1234567890}
AppName=FREEQUENCY
AppVersion={#AppVersion}
AppVerName=FREEQUENCY {#AppVersion}
AppPublisher=FREEQUENCY
AppPublisherURL=https://github.com/LOSTFlam/FREEQUENCY
AppSupportURL=https://github.com/LOSTFlam/FREEQUENCY/issues
AppUpdatesURL=https://github.com/LOSTFlam/FREEQUENCY/releases
DefaultDirName={autopf}\FREEQUENCY
DefaultGroupName=FREEQUENCY
DisableProgramGroupPage=yes
LicenseFile=
OutputDir=..\..\dist
OutputBaseFilename=FREEQUENCY-Setup-{#AppVersion}-win64
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\FREEQUENCY.exe
SetupLogging=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#BuildDir}\FREEQUENCY.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\assets\icon.png"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\FREEQUENCY"; Filename: "{app}\FREEQUENCY.exe"; Comment: "Hybrid DAW"
Name: "{group}\{cm:UninstallProgram,FREEQUENCY}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\FREEQUENCY"; Filename: "{app}\FREEQUENCY.exe"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Classes\.freq"; ValueType: string; ValueName: ""; ValueData: "freequency.project"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\freequency.project"; ValueType: string; ValueName: ""; ValueData: "FREEQUENCY Project"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\freequency.project\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\FREEQUENCY.exe,0"
Root: HKCU; Subkey: "Software\Classes\freequency.project\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\FREEQUENCY.exe"" ""%1"""

[Run]
Filename: "{app}\FREEQUENCY.exe"; Description: "{cm:LaunchProgram,FREEQUENCY}"; Flags: nowait postinstall skipifsilent
