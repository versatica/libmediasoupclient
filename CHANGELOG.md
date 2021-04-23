# Changelog


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
