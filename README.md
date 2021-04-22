# pulseaudio-win32

An up-to-date PulseAudio build for Windows with an installer, service, and other
improvements.

### Features

- Various fixes and improvements.
- Standalone.
- Optional installer.
- Can run as a Windows Service.
- Working `pactl`/`pacat`.
- Working modules.
- Working auto-connection to user and system instances.
- Working unix sockets for both user and system instances on Windows 10.
- HTML documentation.
- Works on Windows Vista and later.

### Limitations

- There are still some hard-coded paths which don't automatically relocate to
  the installation directory. This mainly affects the documentation.
- Locale files aren't currently included.

### Building

```bash
mkdir -p ./build &&
docker build --tag pulseaudio-win32 --build-arg "PAW32_VERSION=$(echo "$(git describe --always --tags --dirty || echo unknown)" | sed -e "s/^v//g")" ./src &&
docker save pulseaudio-win32 |
tar -xOf - --wildcards '*/layer.tar' |
tar -xvf - -C ./build
```

### Troubleshooting

- **Service**
  - **Error: The process terminated unexpectedly**<br>
    Check PulseAudio's log. Note that it gets truncated every time PulseAudio
    starts.
  - **Error: The environment is incorrect**<br>
    Something went wrong when finding the AppData folder. This error shouldn't
    happen.
  - **Error: The specified service has been marked for deletion**<br>
    If you have `services.msc` open, close it and try again (it prevents service
    deletions from working properly). Otherwise, try killing `pulseaudio.exe`
    and `pasvc.exe`.
  - **Permission errors for ProgramData**<br>
    If you attempted to run pulseaudio with the `--system` option manually or
    otherwise created conflicting files in ProgramData, delete them and start
    the service again.
  - **Invalid argument errors when attempting to create the Unix socket for the system service**
    This is caused by file permission errors when PulseAudio cannot delete or
    write to the socket file or the directory it's in. Try deleting the
    ProgramData folder for PulseAudio and restarting the service.
- **Installer**
  - **The service does not get installed and/or started**
    Try installing it manually from the command line with `pasvc install`.
  - **The firewall rules do not get installed**
    Try installing it manually from the command line with `pasvcfw install`.
- **PulseAudio**
  - **Error: cannot create wakeup pipe**<br>
    If you are starting `pulseaudio.exe` from another process, ensure the
    `SystemRoot` environment variable is defined.
  - **Audio is much louder than other apps at the same volume level or is distorted even when relatively quiet**
    Try disabling audio effects on the output devices. Dolby Audio (e.g. the one
    preloaded on many devices) can cause this to happen if it is enabled even if
    all individual effects are turned off.

### Silent installation

The installer uses Inno Setup 6 and supports the usual flags (e.g. `/SILENT`,
`/VERYSILENT`).

The following components are available (all are selected by default) and can be
passed as a comma separated list to `/COMPONENTS=`:

- `pulseaudio` (required) - PulseAudio daemon, modules, and tools
- `documentation` - HTML documentation.
- `service` - Windows Service.
- `uninstaller` - Uninstallation support.

The following tasks are available when the `service` component is selected and
can be passed as a comma separated list to `/TASKS=`:

- `firewall` - Add a firewall rule (4713/tcp).
  - `firewall/deny` - Deny all external connections.
  - `firewall/allow` - Allow external connections on private networks.
  - `firewall/allowall` - Allow all external connections.
  - `firewall/allowalledge` - Allow all external connections including edge
    traversal.
- `svcmodload` - Allow loading additional modules (note that this is a potential
  security risk).

The included configuration files in the installation directory will be
overwritten on install and deleted on uninstall. To preserve your changes, place
your custom configuration files in `*.pa.d\*.pa` and `*.conf.d\*.conf`.

Uninstalling will remove everything except for the log and state data for the
service if used.

### Notes

While this repository is licensed under the MIT license, the icon and build
output is licensed under the LGPL like PulseAudio itself (the PulseAudio build
isn't linked to GPL libraries).
