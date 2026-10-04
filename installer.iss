#define MyAppName "ThumbForge"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Mahtab Jack"
#define MyAppExeName "thumb_forge.exe"

[Setup]
AppId={{D37F2961-A751-4196-9D9A-2EBBC7132194}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=dist
OutputBaseFilename=ThumbForge-Setup-x64
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
AppMutex=ThumbForge_SingleInstance_Mutex_D37F2961
SetupMutex=ThumbForge_Setup_Mutex_D37F2961
CloseApplications=force
RestartApplications=no
SetupIconFile=windows\runner\resources\app_icon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "build\windows\x64\runner\Release\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\ThumbForgeCli.exe"; Parameters: "register --pdf=1 --video=1 --audio=1 --apk=1 --code=1 --dll=""{app}\ThumbForgeProvider.dll"""; Flags: runhidden
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{app}\ThumbForgeCli.exe"; Parameters: "unregister"; Flags: runhidden; RunOnceId: "UnregisterThumbForge"

[Code]
procedure TerminateConflictingProcesses();
var
  ResultCode: Integer;
begin
  // Terminate COM Surrogate (dllhost) so ThumbForgeProvider.dll is not held locked
  Exec('taskkill.exe', '/f /im dllhost.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  // Terminate any running ThumbForge app instance
  Exec('taskkill.exe', '/f /im thumb_forge.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  // Terminate any running CLI companion instance
  Exec('taskkill.exe', '/f /im ThumbForgeCli.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Sleep(400);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  TerminateConflictingProcesses();
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
  begin
    TerminateConflictingProcesses();
  end;
end;

function InitializeUninstall(): Boolean;
begin
  TerminateConflictingProcesses();
  Result := True;
end;
