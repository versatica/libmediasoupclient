# Changelog


### NOT RELEASED


### 3.4.0

* 'maxaveragebitrate' support for Opus. Thanks @PeterCang.
*  Enable VP9 SVC (#131). Thanks @harvestsure.
*  Reuse closed m= sections in remote SDP offer (#99).
*  Allow forcing local DTLS role (#133).
*  Add cbr config option for opus (#138). Thanks @GEverding.

### 3.3.0

* Update to libwebrtc M94/4606.

### 3.2.0

* Do not auto generate the stream ID for the receiving dataChannel,
  but provide it via API. Fixes #126.


### 3.1.5

* Fix profile-id codec parameter by converting parsed value into integer. Fixes #115


### 3.1.4

* Convert `RecvTransport::Listener` to a subclass. Thanks @maxweisel.
* Fix ambiguous cast error when compiling with MSVC. Thanks @maxweisel.


### 3.1.3

* Fix H264 `profile-level-id` value in SDP answer.
  - Same as in https://github.com/versatica/mediasoup-client/issues/148


### 3.1.2

* Fix memory leak (#105). Thanks @ploverlake.


### 3.1.1

* Update `libsdptransform` dep to 1.2.8.


### 3.1.0

* DataChannel support.
