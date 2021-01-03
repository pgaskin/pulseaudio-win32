#!/bin/bash

# script stuff
set -eo pipefail
if [[ "$#" -ne 2 ]]; then
    echo "usage: $0 ZYPPER_MINGW32_REPO BIN_DIR"
    exit 2
fi

# for querying zypper repos
xml() {
    xmlstarlet sel \
        -N p="http://linux.duke.edu/metadata/repo" \
        -N m="http://linux.duke.edu/metadata/common" \
        -N r="http://linux.duke.edu/metadata/rpm" \
        -t -v "$1"
}

# read the repository index
repomd="$(
    wget -qO- "$1/repodata/repomd.xml" |
    xml "/p:repomd/p:data[@type=\"primary\"]/p:location/@href"
)"

# read the package list
repopk="$(
    wget -qO- "$1/$repomd" |
    zcat -
)"

# find all built-in windows dlls
dlls_builtin="$(
    xml "/m:metadata/m:package[m:name=\"mingw32-runtime\"]/m:format/r:provides/r:entry/@name" <<< "$repopk" |
    sed -En "s/mingw32\(lib:(.+)\)/\1.dll/p"
)"

while true; do
    # find all dlls the exes and other dlls depend on
    dlls_deps="$(
        find "$2" -maxdepth 1 \
            \( -name "*.exe" -o -name "*.dll" \) \
            -exec i686-w64-mingw32-objdump --private-headers {} \; |
        sed -En "s/.*DLL Name: ([^ ]+).*/\\1/p" |
        sort |
        uniq
    )"

    # find all dlls we have already
    dlls_have="$(
        find "$2" -maxdepth 1 \
            -name "*.dll" \
            -exec basename {} \;
    )"

    # filter out dlls we have from the dependencies and take the first one we need
    dll="$({ grep -ivxFf <(cat <<< "$dlls_have") -f <(cat <<< "$dlls_builtin") <<< "$dlls_deps" || true; } | head -n1)"

    # if there's none left, we're done
    if [[ -z "$dll" ]]; then
        break
    fi

    # find the package containing the dll
    pkg="$(xml "/m:metadata/m:package[m:format/r:provides/r:entry/@name=\"mingw32($(tr "[A-Z]" "[a-z]" <<< "$dll"))\"][last()]/m:location/@href" <<< "$repopk" || true)"
    if [[ -z "$pkg" ]]; then
        echo "Error: No package for $dll." >&2
        exit 1
    fi

    # download and extract the dll
    wget -qO- "$1/$pkg" |
    rpm2cpio |
    bsdtar -xvf - -s "|.*/||" -C "$2" ./usr/i686-w64-mingw32/sys-root/mingw/bin/$dll
done
