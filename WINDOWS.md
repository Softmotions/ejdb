# Windows cross compilation

**Note:** HTTP/Websocket network API is disabled and not yet supported.

Nodejs/Dart bindings not yet ported to Windows.

## Prerequisites

```sh
sudo apt-get install bison flex libtool ruby scons intltool libtool-bin p7zip-full wine wine32 \
                     python-is-python3 python3-mako gperf
```

## Build

### Prepare MXE

```sh
git clone https://github.com/mxe/mxe.git
```

`nano ./mxe/settings.mk`:

```
JOBS := 1
MXE_TARGETS := x86_64-w64-mingw32.static
LOCAL_PKG_LIST := cunit libiberty
.DEFAULT local-pkg-list:
local-pkg-list: $(LOCAL_PKG_LIST)
```

```
cd ./mxe
make
```

### Cross compilation

```bash
mkdir ./build
cd  ./build

export MXE_HOME=<path to project>/mxe

cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_TOOLCHAIN_FILE=../win64-tc.cmake \
         -DENABLE_HTTP=OFF

make package
```