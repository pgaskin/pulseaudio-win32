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
- Works on Windows 7 and later.

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

### Usage

See the [website](https://pgaskin.net/pulseaudio-win32/#readme).

### Notes

While this repository is licensed under the MIT license, the icon and build
output is licensed under the LGPL like PulseAudio itself (the PulseAudio build
isn't linked to GPL libraries).
