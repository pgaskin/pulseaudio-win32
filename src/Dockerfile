### Base images ###

FROM opensuse/tumbleweed AS mingw32
RUN zypper --non-interactive addrepo https://download.opensuse.org/repositories/windows:mingw:win32/openSUSE_Tumbleweed/windows:mingw:win32.repo && \
    zypper --non-interactive --gpg-auto-import-keys refresh
RUN zypper --non-interactive install \
        wget m4 git-core meson bsdtar gzip xmlstarlet \
        mingw32-filesystem mingw32-cross-gcc mingw32-cross-gcc-c++ mingw32-cross-binutils mingw32-cross-pkgconf && \
    zypper clean
RUN mkdir -p /usr/local/share/meson/cross && \
    wget -qO /usr/local/share/meson/cross/mingw32 https://raw.githubusercontent.com/mesonbuild/meson/ca30550e065991f9b2438e2a6fabc64fd2248806/cross/linux-mingw-w64-32bit.txt

FROM alpine AS inkscape
RUN apk add --no-cache inkscape imagemagick xvfb

FROM i386/alpine AS wine
ENV WINEDEBUG="-all" \
    WINEDLLOVERRIDES="mscoree,mshtml=" \
    WINEARCH="win32" \
    WINEPREFIX="/wine"
RUN apk add --no-cache wine ncurses freetype xvfb unzip wget ca-certificates

FROM wine AS reshack-install
RUN wget -qO /tmp/reshack.nupkg https://chocolatey.org/api/v2/package/reshack/5.1.8 && \
    unzip -j /tmp/reshack.nupkg tools/reshacker_setup.exe -d /tmp && \
    rm /tmp/reshack.nupkg
RUN set -x; \
    export DISPLAY=:0; \
    Xvfb :0 -screen 0 1024x768x16 & until xmodmap -display :0 &> /dev/null; do sleep .1; done && \
    wineboot --init && \
    wine "Z:\\tmp\\reshacker_setup.exe" /VERYSILENT && \
    wineboot --shutdown && \
    pkill Xvfb && while pkill -0 Xvfb &> /dev/null; do sleep .1; done

FROM wine AS reshack
RUN touch /bin/ResourceHacker && \
    chmod +x /bin/ResourceHacker && \
    echo '#!/bin/sh' >> /bin/ResourceHacker && \
    echo 'export DISPLAY=:0' >> /bin/ResourceHacker && \
    echo 'Xvfb :0 -screen 0 1024x768x16 & until xmodmap -display :0 &> /dev/null; do sleep .1; done' >> /bin/ResourceHacker && \
    echo 'wine "C:\\Program Files\\Resource Hacker\\ResourceHacker.exe" "$@"; e=$?' >> /bin/ResourceHacker && \
    echo 'pkill Xvfb && while pkill -0 Xvfb &> /dev/null; do sleep .1; done' >> /bin/ResourceHacker && \
    echo 'exit $e' >> /bin/ResourceHacker
COPY --from=reshack-install ${WINEPREFIX} ${WINEPREFIX}

FROM wine AS inno-install
RUN wget -qO /tmp/is.exe http://files.jrsoftware.org/is/6/innosetup-6.1.2.exe
RUN set -x; \
    export DISPLAY=:0; \
    Xvfb :0 -screen 0 1024x768x16 & until xmodmap -display :0 &> /dev/null; do sleep .1; done && \
    wineboot --init && \
    wine "Z:\\tmp\\is.exe" /SP- /VERYSILENT /ALLUSERS /SUPPRESSMSGBOXES && \
    wineboot --shutdown && \
    pkill Xvfb && while pkill -0 Xvfb &> /dev/null; do sleep .1; done

FROM wine AS inno
RUN touch /bin/iscc && \
    chmod +x /bin/iscc && \
    echo '#!/bin/sh' >> /bin/iscc && \
    echo 'wine "C:\\Program Files\\Inno Setup 6\\ISCC.exe" "$@"' >> /bin/iscc && \
    echo 'exit $?' >> /bin/iscc
COPY --from=inno-install ${WINEPREFIX} ${WINEPREFIX}

### Icon ###

FROM inkscape AS build-icon
COPY ./icon/* /src/
RUN set -x; \
    export DISPLAY=:0; \
    Xvfb :0 -screen 0 1024x768x16 & until xmodmap -display :0 &> /dev/null; do sleep .1; done && \
    mkdir -p /build && \
    inkscape -w 16  -h 16  --export-filename=/build/pulseaudio-flat.16.png /src/pulseaudio-flat-simple.svg && \
    inkscape -w 32  -h 32  --export-filename=/build/pulseaudio-flat.32.png /src/pulseaudio-flat-simple.svg && \
    inkscape -w 48  -h 48  --export-filename=/build/pulseaudio-flat.48.png /src/pulseaudio-flat-simple.svg && \
    inkscape -w 64  -h 64  --export-filename=/build/pulseaudio-flat.64.png /src/pulseaudio-flat.svg && \
    inkscape -w 96  -h 96  --export-filename=/build/pulseaudio-flat.96.png /src/pulseaudio-flat.svg && \
    inkscape -w 128 -h 128 --export-filename=/build/pulseaudio-flat.96.png /src/pulseaudio-flat.svg && \
    convert /build/pulseaudio-flat.*.png /pulseaudio.ico && \
    pkill Xvfb && while pkill -0 Xvfb &> /dev/null; do sleep .1; done

### PulseAudio ###

FROM mingw32 AS build-pulseaudio-stage1
RUN zypper --non-interactive install \
        intltool gettext-tools mingw32-libsndfile-devel mingw32-libintl-devel mingw32-win_iconv-devel mingw32-pcre-devel \
        orc mingw32-libspeex-devel mingw32-liborc-devel mingw32-libtool && \
    zypper clean
RUN mkdir -p /src
RUN git init /src/pulseaudio && \
    git -C /src/pulseaudio remote add origin https://gitlab.freedesktop.org/pulseaudio/pulseaudio.git && \
    git -C /src/pulseaudio fetch origin && \
    git -C /src/pulseaudio checkout 81ebd8ba3f644e95a7f858ede9fc65d340ab06e2
COPY ./pulseaudio/* /src/
RUN git -C /src/pulseaudio apply /src/*.patch
ARG PAW32_VERSION=unknown
RUN git -C /src/pulseaudio describe --always --tags | \
    sed -e "s/^v//g" -e "s/$/-paw32-${PAW32_VERSION}/g" | \
    tee /src/pulseaudio/.tarball-version /src/pulseaudio/.version /pulseaudio.version
RUN meson setup \
        --cross-file mingw32 \
        -Dtests=false \
        -Ddatabase=simple \
        -Drunning-from-build-tree=false \
        -Dlegacy-database-entry-format=false \
        -Dprefix=/pulseaudio \
        -Dmodlibexecdir=/pulseaudio/bin \
        -Dbluez5=false \
        -Dc_args="-D__USE_MINGW_ANSI_STDIO=1 -D_WIN32_WINNT=_WIN32_WINNT_VISTA" \
        /build/pulseaudio /src/pulseaudio
RUN ninja -C /build/pulseaudio
RUN meson install -C /build/pulseaudio
RUN find /pulseaudio \
        # static libraries
        -name '*.a' \
        -delete && \
    rm -r \
        # development files
        /pulseaudio/lib/pkgconfig /pulseaudio/include /pulseaudio/lib/cmake /pulseaudio/share/vala \
        # manual pages
        /pulseaudio/share/man \
        # shell completion
        /pulseaudio/share/bash-completion /pulseaudio/share/zsh \
        # extra locales
        /pulseaudio/share/locale \
        # misc scripts (not compatible with windows)
        /pulseaudio/bin/pa-info && \
    rmdir /pulseaudio/share /pulseaudio/lib
RUN bash /src/mingw32-deps.sh "https://download.opensuse.org/repositories/windows:/mingw:/win32/openSUSE_Tumbleweed" /pulseaudio/bin

FROM reshack AS build-pulseaudio-stage2
COPY --from=build-pulseaudio-stage1 /pulseaudio         /pulseaudio
COPY --from=build-pulseaudio-stage1 /pulseaudio.version /pulseaudio.version
COPY --from=build-icon              /pulseaudio.ico     /pulseaudio.ico
RUN ResourceHacker \
        -open "Z:\\pulseaudio\\bin\\pulseaudio.exe" \
        -save "Z:\\pulseaudio\\bin\\pulseaudio.exe" \
        -action addskip \
        -res "Z:\\pulseaudio.ico" \
        -mask ICONGROUP,MAINICON,

FROM mingw32 AS build-pulseaudio
COPY --from=build-pulseaudio-stage2 /pulseaudio         /pulseaudio
COPY --from=build-pulseaudio-stage2 /pulseaudio.version /
RUN bsdtar -acf /pulseaudio.zip -C / pulseaudio

### pulseaudio (docs) ###

FROM mingw32 as build-padoc
COPY --from=build-pulseaudio-stage1 /build/pulseaudio/man /build/pulseaudio/man
COPY --from=build-pulseaudio-stage1 /src/pulseaudio/man   /src/pulseaudio/man
RUN zypper --non-interactive install libxslt-tools && \
    zypper clean
RUN mkdir -p /padoc && \
    for f in /build/pulseaudio/man/*.xml; do \
        echo "$f"; \
        xsltproc --output "/padoc/doc/$(basename "${f%.xml}.html")" --path /src/pulseaudio/man /src/pulseaudio/man/xmltoman.xsl "$f"; \
    done
RUN bsdtar -acf /padoc.zip -C / padoc

### pasvc ###

FROM mingw32 AS build-pasvc
COPY ./pasvc/* /src/
RUN mkdir -p /pasvc/bin
ARG PAW32_VERSION=unknown
RUN i686-w64-mingw32-gcc -Wall -Wextra -Werror -D_WIN32_WINNT=_WIN32_WINNT_VISTA -DPAW32_VERSION='"'"${PAW32_VERSION}"'"' /src/pasvc.c   -o /pasvc/bin/pasvc.exe   -lshlwapi -lshell32
RUN i686-w64-mingw32-gcc -Wall -Wextra -Werror -D_WIN32_WINNT=_WIN32_WINNT_VISTA -DPAW32_VERSION='"'"${PAW32_VERSION}"'"' /src/pasvcfw.c -o /pasvc/bin/pasvcfw.exe -lshlwapi -lole32 -loleaut32
RUN bsdtar -acf /pasvc.zip -C / pasvc

### pasetup ###

FROM inno AS build-pasetup
COPY --from=build-pulseaudio /pulseaudio         /src/pulseaudio
COPY --from=build-pulseaudio /pulseaudio.version /src/pulseaudio.version
COPY --from=build-padoc      /padoc              /src/padoc
COPY --from=build-pasvc      /pasvc              /src/pasvc
COPY ./pasetup/pasetup.iss                       /src/pasetup.iss
RUN iscc /O"Z:\\" /DAppVersion="$(cat /src/pulseaudio.version)" "Z:\\src\\pasetup.iss"

### output ###

FROM scratch AS output-merge
COPY --from=build-pulseaudio /pulseaudio.zip     /
COPY --from=build-pulseaudio /pulseaudio.version /
COPY --from=build-padoc      /padoc.zip          /
COPY --from=build-pasvc      /pasvc.zip          /
COPY --from=build-pasetup    /pasetup.exe        /
COPY --from=build-icon       /pulseaudio.ico     /

FROM scratch AS output
COPY --from=output-merge / /
