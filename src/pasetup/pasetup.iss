[Setup]
AppName=PulseAudio
AppVerName=PulseAudio {#AppVersion}
AppVersion={#AppVersion}
AppId=github.com/pgaskin/pulseaudio-win32
UninstallDisplayName=PulseAudio
AppPublisher=Patrick Gaskin
AppPublisherURL=https://github.com/pgaskin/pulseaudio-win32
AppSupportURL=https://github.com/pgaskin/pulseaudio-win32
DefaultDirName={autopf}\PulseAudio
DisableDirPage=yes
DisableProgramGroupPage=yes
MinVersion=6.1
OutputBaseFilename=pasetup
PrivilegesRequired=admin
WizardStyle=classic
Uninstallable=WizardIsComponentSelected('uninstall')
UninstallDisplayIcon={app}\bin\pulseaudio.exe

[Types]
Name: full;    Description: "Full installation"
Name: minimal; Description: "Minimal installation"
Name: custom;  Description: "Custom installation"; Flags: iscustom

[Components]
Name: pulseaudio;    Description: "PulseAudio";     Types: full minimal custom; Flags: fixed
Name: documentation; Description: "Documentation";  Types: full
Name: service;       Description: "System service"; Types: full
Name: uninstall;     Description: "Uninstaller";    Types: full minimal custom

[Tasks]
Name: firewall;              Description: "Add a firewall rule (4713/tcp)";                          GroupDescription: Service; Components: service
Name: firewall/deny;         Description: "Deny all external connections";                           Flags: exclusive
Name: firewall/allow;        Description: "Allow external connections on private networks";          Flags: exclusive unchecked
Name: firewall/allowall;     Description: "Allow all external connections";                          Flags: exclusive unchecked
Name: firewall/allowalledge; Description: "Allow all external connections including edge traversal"; Flags: exclusive unchecked
Name: svcmodload;            Description: "Allow module loading";                                    GroupDescription: Service; Flags: unchecked; Components: Service

[Files]
Source: "pulseaudio\*";          DestDir: "{app}";     Flags: recursesubdirs ignoreversion; Components: pulseaudio;
Source: "padoc\*";               DestDir: "{app}";     Flags: recursesubdirs ignoreversion; Components: documentation
Source: "pasvc\bin\pasvc.exe";   DestDir: "{app}\bin"; Flags: recursesubdirs ignoreversion; Components: service
Source: "pasvc\bin\pasvcfw.exe"; DestDir: "{app}\bin"; Flags: recursesubdirs ignoreversion; Components: service; Tasks: firewall

[Dirs]
Name: "{app}\etc\pulse\default.pa.d"
Name: "{app}\etc\pulse\system.pa.d"
Name: "{app}\etc\pulse\daemon.conf.d"
Name: "{app}\etc\pulse\client.conf.d"
Name: "{commonappdata}\PulseAudio"; Permissions: service-full

[InstallDelete]
Name: "{commonappdata}\PulseAudio\pulseaudio.log"; Type: files

[UninstallDelete]
Name: "{commonappdata}\PulseAudio"; Type: filesandordirs

[Run]
; TODO: maybe fail the installation if a command fails?
Filename: "{app}\bin\pasvcfw.exe"; Parameters: "install /PaExe:""{app}\bin\pulseaudio.exe"" /Mode:deny";         StatusMsg: "Installing firewall rule...";   Flags: runhidden; Components: service; Tasks: firewall/deny
Filename: "{app}\bin\pasvcfw.exe"; Parameters: "install /PaExe:""{app}\bin\pulseaudio.exe"" /Mode:allow";        StatusMsg: "Installing firewall rule...";   Flags: runhidden; Components: service; Tasks: firewall/allow
Filename: "{app}\bin\pasvcfw.exe"; Parameters: "install /PaExe:""{app}\bin\pulseaudio.exe"" /Mode:allowall";     StatusMsg: "Installing firewall rule...";   Flags: runhidden; Components: service; Tasks: firewall/allowall
Filename: "{app}\bin\pasvcfw.exe"; Parameters: "install /PaExe:""{app}\bin\pulseaudio.exe"" /Mode:allowalledge"; StatusMsg: "Installing firewall rule...";   Flags: runhidden; Components: service; Tasks: firewall/allowalledge
Filename: "{app}\bin\pasvc.exe";   Parameters: "stop";                                                           StatusMsg: "Stopping service if exists..."; Flags: runhidden; Components: service
Filename: "{app}\bin\pasvc.exe";   Parameters: "delete";                                                         StatusMsg: "Deleting service if exists..."; Flags: runhidden; Components: service
Filename: "{app}\bin\pasvc.exe";   Parameters: "install";                                                        StatusMsg: "Installing service...";         Flags: runhidden; Components: service; Tasks: not svcmodload
Filename: "{app}\bin\pasvc.exe";   Parameters: "install /AllowModuleLoad";                                       StatusMsg: "Installing service...";         Flags: runhidden; Components: service; Tasks: svcmodload
Filename: "{app}\bin\pasvc.exe";   Parameters: "start";                                                          StatusMsg: "Starting service...";           Flags: runhidden; Components: service

[UninstallRun]
Filename: "{app}\bin\pasvc.exe";   Parameters: "stop";                  RunOnceId: "StopSvc";     Flags: runhidden; Components: service
Filename: "{app}\bin\pasvc.exe";   Parameters: "delete";                RunOnceId: "DeleteSvc";   Flags: runhidden; Components: service
Filename: "{sys}\taskkill.exe";    Parameters: "/F /IM pulseaudio.exe"; RunOnceId: "KillPA";      Flags: runhidden; Components: service
Filename: "{sys}\taskkill.exe";    Parameters: "/F /IM pasvc.exe";      RunOnceId: "KillPASvc";   Flags: runhidden; Components: service
Filename: "{app}\bin\pasvcfw.exe"; Parameters: "delete";                RunOnceId: "DeleteSvcFw"; Flags: runhidden; Components: service; Tasks: firewall

[Code]

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
    ResultCode: Integer;
begin
    Exec(ExpandConstant('{app}\bin\pasvc.exe'), 'stop', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM pulseaudio.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM pasvc.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;
