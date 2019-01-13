# libmediasoupclient

C++ version of the [mediasoup-client](https://github.com/versatica/mediasoup-client/) JavaScript library using [WebRTC Native Code](https://webrtc.org/native-code).

## Usage

Once installed (see *Installation* below):

```c++
#include "libmediasoupclient/mediasoupclient.hpp"
```

The **libmediasoupclient** API is exposed in the `mediasoupclient` C++ namespace.

**libmediasoupclient** integrates the [JSON for Modern C++](https://github.com/nlohmann/json/) library and exposes it under the `json` C++ namespace.


## Dependencies

### [libsdptransform](https://github.com/ibc/libsdptransform)

```bash
# Retrive the code.
scripts/get-dep.sh libsdptransform

# Install.
cd deps/libsdptransform
cmake . -Bbuild
cmake --build build
```

### [libwebrtc](https://webrtc.org)

Follow the [official instructions](https://webrtc.org/native-code/development/).

Make sure you compile the branch `remotes/branch-heads/70`.

Copy or link the header files into `deps/libwebrtc/include`:

```bash
mkdir -p ${libmediasoupclient_path}/deps/libwebrtc/include
ln -s ${webrtc-checkout}/src ${libmediasoupclient_path}/deps/libwebrtc/include
```

Copy or link the binary file into `deps/libwebrtc/lib`:

```bash
mkdir -p ${libmediasoupclient_path}/deps/libwebrtc/lib
ln -s ${webrtc-checkout}/src/out/${target}/obj/libwebrtc.a ${libmediasoupclient_path}/deps/libwebrtc/lib
```

## Developement dependencies

### [Catch2](https://github.com/catchorg/Catch2)

```bash
# Retrive the code (single header).
scripts/get-dep.sh catch
```

## Installation

```bash
git clone https://github.com/jmillan/libmediasoupclient.git
cd libmediasoupclient/
cmake . -Bbuild
make install -C build/ # or: cd build/ && make install
```

Depending on the host, it will generate the following static lib and header files:

```
-- Installing: /usr/local/lib/libmediasoupclient.a
-- Up-to-date: /usr/local/include/mediasoupclient/mediasoupclient.hpp
-- Up-to-date: /usr/local/include/mediasoupclient/json.hpp
```


## Development

* Build the lib:

```bash
$ cmake . -Bbuild
```

* Run test units:

```bash
$ ./scripts/run-test.sh
```


## Authors

* José Luis Millán [[website](https://jssip.net)|[github](https://github.com/jmillan/)]
* Iñaki Baz Castillo [[website](https://inakibaz.me)|[github](https://github.com/ibc/)]


## License

[MIT](LICENSE)
