# Changelog


### 3.4.2

* Fix explicit codec selection ([#164](https://github.com/versatica/libmediasoupclient/pull/164)). Thanks @fedulvtubudul.


### 3.4.1

* Clear the stored transceivers before closing the PeerConnection ([#156](https://github.com/versatica/libmediasoupclient/pull/156)). Thanks @adriancretu.
* Fix non-virtual destructors ([PR #157](https://github.com/versatica/libmediasoupclient/pull/157)). Thanks @adriancretu.
* Fix min max range inclusion. ([PR #158](https://github.com/versatica/libmediasoupclient/pull/158)). Thanks @adriancretu.
* Update libsdptransform to 1.2.10 ([PR #171](https://github.com/versatica/libmediasoupclient/pull/171)). Thanks@copiltembel.


### 3.4.0

* 'maxaveragebitrate' support for Opus. Thanks @PeterCang.
*  Enable VP9 SVC ([#131](https://github.com/versatica/libmediasoupclient/pull/131)). Thanks @harvestsure.
*  Reuse closed m= sections in remote SDP offer ([#99](https://github.com/versatica/libmediasoupclient/pull/99)).
*  Allow forcing local DTLS role ([#133](https://github.com/versatica/libmediasoupclient/pull/133)).
*  Add cbr config option for opus ([#138](https://github.com/versatica/libmediasoupclient/pull/138)). Thanks @GEverding.

### 3.3.0

* Update to libwebrtc M94/4606.

### 3.2.0

* Do not auto generate the stream ID for the receiving dataChannel,
  but provide it via API. Fixes ([#126](https://github.com/versatica/libmediasoupclient/pull/126)).


### 3.1.5

* Fix profile-id codec parameter by converting parsed value into integer. Fixes ([#115](https://github.com/versatica/libmediasoupclient/pull/115))


### 3.1.4

* Convert `RecvTransport::Listener` to a subclass. Thanks @maxweisel.
* Fix ambiguous cast error when compiling with MSVC. Thanks @maxweisel.


### 3.1.3

* Fix H264 `profile-level-id` value in SDP answer.
  - Same as in https://github.com/versatica/mediasoup-client/issues/148


### 3.1.2

* Fix memory leak ([#105](https://github.com/versatica/libmediasoupclient/pull/105)). Thanks @ploverlake.


### 3.1.1

* Update `libsdptransform` dep to 1.2.8.


### 3.1.0

* DataChannel support.
