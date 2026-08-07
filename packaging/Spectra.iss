#ifndef AppVersion
  #define AppVersion "1.0.1"
#endif

#ifndef SourceDir
  #define SourceDir "..\dist\Spectra"
#endif

#ifndef OutputDir
  #define OutputDir "..\artifacts"
#endif

#ifndef OutputBaseFilename
  #define OutputBaseFilename "Spectra-1.0.1-windows-x64-setup"
#endif

[Setup]
AppId={{65D3EEEB-6007-4DA5-84EA-E993A19FC186}
AppName=Spectra
AppVersion={#AppVersion}
AppVerName=Spectra {#AppVersion}
AppPublisher=Fourier Audio Lab
AppPublisherURL=https://github.com/Ethan-H147/Spectra
AppSupportURL=https://github.com/Ethan-H147/Spectra/issues
AppUpdatesURL=https://github.com/Ethan-H147/Spectra/releases
DefaultDirName={localappdata}\Programs\Spectra
DefaultGroupName=Spectra
DisableProgramGroupPage=yes
LicenseFile={#SourceDir}\LICENSE
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBaseFilename}
SetupIconFile=..\assets\spectra.ico
UninstallDisplayIcon={app}\bin\spectra_qt.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
CloseApplications=yes
RestartApplications=no
VersionInfoVersion={#AppVersion}.0
VersionInfoProductName=Spectra
VersionInfoProductVersion={#AppVersion}
VersionInfoCompany=Fourier Audio Lab
VersionInfoDescription=Spectra installer

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Spectra"; Filename: "{app}\bin\spectra_qt.exe"; WorkingDir: "{app}\bin"
Name: "{autodesktop}\Spectra"; Filename: "{app}\bin\spectra_qt.exe"; WorkingDir: "{app}\bin"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\spectra_qt.exe"; Description: "Launch Spectra"; WorkingDir: "{app}\bin"; Flags: nowait postinstall skipifsilent
