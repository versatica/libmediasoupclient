# Changelog

### NEXT

- Sync ortc with mediasoup-client ([PR #198](https://github.com/versatica/libmediasoupclient/pull/198)).
- Adapt SCTP/DataChannels API to latest changes in mediasoup ([PR #200](https://github.com/versatica/libmediasoupclient/pull/200)).
- Modernize code ([PR #227](https://github.com/versatica/libmediasoupclient/pull/227)):
  - Fix installation of the public headers: `install()` only shipped `mediasoupclient.hpp`, so including it from an application failed with "'Device.hpp' file not found".
  - The library `include/` directory is now `PUBLIC`, so applications no longer need to add it to their include path by hand.
  - Document the public API.
  - Sync `.clang-format` and `.clang-tidy` with mediasoup and fix every reported warning.
  - Add `doc/Building.md` with the build, test and lint workflow.

### 3.5.1

- Fix deadlock on `PeerConnection` calls ([issue #187](https://github.com/versatica/libmediasoupclient/issues/187)).

### 3.5.0

- Update to libwebrtc M140/7339 ([PR #188](https://github.com/versatica/libmediasoupclient/pull/188), credits to @revidee).

### 3.4.3

- Update to libwebrtc M120/6099 ([PR #173](https://github.com/versatica/libmediasoupclient/pull/173), credits to @Poldraunic, @janreyho and @copiltembel).

### 3.4.2

- Fix explicit codec selection ([PR #164](https://github.com/versatica/libmediasoupclient/pull/164), credits to @fedulvtubudul).

### 3.4.1

- Clear the stored transceivers before closing the `PeerConnection` ([PR #156](https://github.com/versatica/libmediasoupclient/pull/156), credits to @adriancretu).
- Fix non-virtual destructors ([PR #157](https://github.com/versatica/libmediasoupclient/pull/157), credits to @adriancretu).
- Fix min max range inclusion ([PR #158](https://github.com/versatica/libmediasoupclient/pull/158), credits to @adriancretu).
- Update libsdptransform to 1.2.10 ([PR #171](https://github.com/versatica/libmediasoupclient/pull/171), credits to @copiltembel).

### 3.4.0

- `maxaveragebitrate` support for Opus (credits to @PeterCang).
- Enable VP9 SVC ([PR #131](https://github.com/versatica/libmediasoupclient/pull/131), credits to @harvestsure).
- Reuse closed m= sections in remote SDP offer ([PR #99](https://github.com/versatica/libmediasoupclient/pull/99)).
- Allow forcing local DTLS role ([PR #133](https://github.com/versatica/libmediasoupclient/pull/133)).
- Add cbr config option for Opus ([PR #138](https://github.com/versatica/libmediasoupclient/pull/138), credits to @GEverding).

### 3.3.0

- Update to libwebrtc M94/4606.

### 3.2.0

- Do not auto generate the stream id for the receiving DataChannel, but provide it via API ([PR #126](https://github.com/versatica/libmediasoupclient/pull/126)).

### 3.1.5

- Fix `profile-id` codec parameter by converting the parsed value into integer ([PR #115](https://github.com/versatica/libmediasoupclient/pull/115)).

### 3.1.4

- Convert `RecvTransport::Listener` to a subclass (credits to @maxweisel).
- Fix ambiguous cast error when compiling with MSVC (credits to @maxweisel).

### 3.1.3

- Fix H264 `profile-level-id` value in SDP answer.
  - Same as [mediasoup-client issue #148](https://github.com/versatica/mediasoup-client/issues/148).

### 3.1.2

- Fix memory leak ([PR #105](https://github.com/versatica/libmediasoupclient/pull/105), credits to @ploverlake).

### 3.1.1

- Update `libsdptransform` dep to 1.2.8.

### 3.1.0

- DataChannel support.
