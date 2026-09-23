; Inno Setup 6/7 — GEDI Process Desktop (C++ / Qt6)
; 1) 先构建并 windeployqt:  cpp\tools\build.bat
; 2) 编译安装包:
;    "D:\software\Inno Setup 7\ISCC.exe" packaging\gedi_cpp_setup.iss
; 输出: packaging\installer_output\GEDIProcessDesktopCpp-1.0.0-Setup.exe

#define MyAppName "GEDI Process Desktop"
#define MyAppNameZh "GEDI 处理桌面"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "GEDI Process Desktop Contributors"
#define MyAppExeName "GEDIProcessDesktopCpp.exe"
; 相对本 iss：仓库根的 cpp\build（已部署 Qt DLL）
#ifndef BuildDir
  #define BuildDir "..\cpp\build"
#endif

[Setup]
AppId={{8F2C1A64-3E9B-4D5A-9C7E-GEDICPP0001}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppCopyright=Copyright (C) GEDI Process Desktop Contributors
DefaultDirName={commonpf32}\GEDIProcessDesktopCpp
DefaultGroupName={#MyAppName}
OutputDir=installer_output
OutputBaseFilename=GEDIProcessDesktopCpp-{#MyAppVersion}-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
; install under C:\Program Files (x86) on 64-bit Windows
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupIconFile=setup.ico
WizardSmallImageFile=wizard_small.png
WizardImageFile=wizard_image.png
LicenseFile=..\LICENSE

[Languages]
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "fileassoc_h5"; Description: "关联 .h5 / .hdf5（可选）"; GroupDescription: "文件关联:"; Flags: unchecked

[Files]
; Qt/MinGW 已部署产物 + 运行库
Source: "{#BuildDir}\GEDIProcessDesktopCpp.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\generic\*"; DestDir: "{app}\generic"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs createallsubdirs
; GeoTIFF 预览 / 瓦片金字塔脚本（运行时可选）
Source: "..\cpp\tools\*.py"; DestDir: "{app}\tools"; Flags: ignoreversion
; 许可与说明
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\cpp\README.md"; DestDir: "{app}"; Flags: ignoreversion
; 图标（占位 logo，可直接替换 cpp\assets\app.ico）
Source: "..\cpp\assets\app.ico"; DestDir: "{app}\assets"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{#MyAppNameZh}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\卸载 {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.h5"; ValueType: string; ValueData: "GEDIProcessDesktop.h5"; Flags: uninsdeletevalue; Tasks: fileassoc_h5
Root: HKA; Subkey: "Software\Classes\.hdf5"; ValueType: string; ValueData: "GEDIProcessDesktop.h5"; Flags: uninsdeletevalue; Tasks: fileassoc_h5
Root: HKA; Subkey: "Software\Classes\GEDIProcessDesktop.h5"; ValueType: string; ValueData: "GEDI HDF5"; Flags: uninsdeletekey; Tasks: fileassoc_h5
Root: HKA; Subkey: "Software\Classes\GEDIProcessDesktop.h5\DefaultIcon"; ValueType: string; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc_h5
Root: HKA; Subkey: "Software\Classes\GEDIProcessDesktop.h5\shell\open\command"; ValueType: string; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc_h5

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
