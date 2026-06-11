# libmediasoupclient Tests

## Building and running

```bash
./scripts/test.sh          # build and run
./scripts/test.sh rebuild  # clean, rebuild and run
```

Tests can be filtered using Catch2 tags:

```bash
./build/test/test.app/Contents/MacOS/test "[Smoke]"
./build/test/test.app/Contents/MacOS/test "[ortc]"
```

## Notes

- All tests compile with `NDEBUG` to match the release-mode `libwebrtc.a`.
- `MediaStreamTrackFactory` uses only public WebRTC API (no `pc/test/` headers).
  Audio uses the platform default ADM (`nullptr`); video uses a `NullVideoTrackSource`
  that overrides `AddOrUpdateSink`/`RemoveSink` as no-ops.
