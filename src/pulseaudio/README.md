# pulseaudio

Files related to building PulseAudio for Windows with MinGW using Meson.

This mainly consists of patches for fixing bugs or compiler warnings (which I am
planning to upstream), but there a few pulseaudio-win32-specific patches for
better integration (see [here](https://pgaskin.net/pulseaudio-win32#patches) for
a list of patches).

Also includes a script, [mingw32-deps.sh](./minw32-deps.sh), for recursively
resolving and downloading MinGW DLLs which a binary depends on from the OpenSUSE
MinGW 32-bit repository.
