# libmediasoupclient

C++ version of the [mediasoup-client](https://github.com/versatica/mediasoup-client/) JavaScript library using [WebRTC Native Code](https://webrtc.org/native-code).

## Usage

Once installed (see *Installation* below):

```c++
#include "libmediasoupclient/mediasoupclient.hpp"
```

The **libmediasoupclient** API is exposed under the `mediasoupclient` C++ namespace.

**libmediasoupclient** integrates the [JSON for Modern C++](https://github.com/nlohmann/json/) library.


## Dependencies

### [libsdptransform](https://github.com/ibc/libsdptransform)

Automatically added during *Installation*.

### [libwebrtc](https://webrtc.org)

**libwebrtc** must be downloaded and compiled in the system. Follow the [official instructions](https://webrtc.org/native-code/development/).

Make sure branch `remotes/branch-heads/72` is checked-out and compiled.

## Developement dependencies

### [Catch2](https://github.com/catchorg/Catch2)

Already integrated in this repository.

## Installation

```bash
git clone https://github.com/jmillan/libmediasoupclient.git

cd libmediasoupclient/

cmake . -Bbuild \
-DLIBWEBRTC_INCLUDE_PATH:PATH=${PATH_TO_LIBWEBRTC_SOURCES} \
-DLIBWEBRTC_BINARY_PATH:PATH=${PATH_TO_LIBWEBRTC_BINARY}

# Compile.
make -C build/ # or: cd build/ && make

# Optionally install.
make install -C build/ # or: cd build/ && make install
```

Depending on the host, it will generate the following static lib and header files:

```
-- Installing: /usr/local/lib/libmediasoupclient.a
-- Up-to-date: /usr/local/include/mediasoupclient/mediasoupclient.hpp
```


## Testing

```bash
./scripts/test.sh
```


## Authors

* José Luis Millán [[website](https://jssip.net)|[github](https://github.com/jmillan/)]
* Iñaki Baz Castillo [[website](https://inakibaz.me)|[github](https://github.com/ibc/)]


## License

[MIT](LICENSE)
