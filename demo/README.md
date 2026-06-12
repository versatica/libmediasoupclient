# mediasoupclient demo

Connects to a [mediasoup-demo](https://github.com/versatica/mediasoup-demo) server, sends mic audio and camera video, and receives audio/video from other participants.

## Prerequisites

- CMake ≥ 3.14 and a C++20-capable compiler.
- Optionally, a running mediasoup-demo server.

## Build & Run

`run.sh` always does an incremental build before launching the binary.
Pass `rebuild` as the first argument to do a full clean reconfigure first.

```bash
# Incremental build + run
PATH_TO_LIBWEBRTC_SOURCES=/path/to/libwebrtc/src \
PATH_TO_LIBWEBRTC_BINARY=/path/to/libwebrtc/out/Release \
./demo/run.sh --url wss://<server>:4443 --origin https://<server>:4443

# Full clean reconfigure + build + run
PATH_TO_LIBWEBRTC_SOURCES=/path/to/libwebrtc/src \
PATH_TO_LIBWEBRTC_BINARY=/path/to/libwebrtc/out/Release \
./demo/run.sh rebuild --url wss://<server>:4443 --origin https://<server>:4443
```

### Manual build

```bash
cmake . -Bbuild \
  -DLIBWEBRTC_INCLUDE_PATH=/path/to/libwebrtc/src \
  -DLIBWEBRTC_BINARY_PATH=/path/to/libwebrtc/out/Release \
  -DMEDIASOUPCLIENT_BUILD_DEMO=ON

cmake --build build --target mediasoupclient_demo
```

Then run the binary directly (macOS bundle path shown; Linux omits `.app/Contents/MacOS`):

```bash
./build/demo/mediasoupclient_demo.app/Contents/MacOS/mediasoupclient_demo \
  --url    wss://<server>:4443 \
  --origin https://<server>:4443
```

Pass `--help` for a full list of options.

## Options

| Option | Default | Description |
|--------|---------|-------------|
| `--ws-url` | *(required)* | Protoo WebSocket URL, e.g. `wss://localhost:4443` |
| `--origin` | *(required)* | HTTP Origin header sent during the WebSocket handshake |
| `--room-id` | `demo-room` | Room ID to join |

## Example: connect to official mediasoup demo

```bash
 ./run.sh --ws-url wss://v3demo.mediasoup.org:4443 --origin https://v3demo.mediasoup.org --room-id foobar
```
