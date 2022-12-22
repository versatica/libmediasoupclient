# libsdptransform

C++ version of the [sdp-transform](https://github.com/clux/sdp-transform/) JavaScript library exposing the same API.

**libsdptransform** is a simple parser and writer of SDP. Defines internal grammar based on [RFC4566 - SDP](http://tools.ietf.org/html/rfc4566), [RFC5245 - ICE](http://tools.ietf.org/html/rfc5245), and many more.


## Usage

Once installed (see *Installation* below):

```c++
#include "sdptransform/sdptransform.hpp"
```

The **libsdptransform** API is exposed in the `sdptransform` C++ namespace.

**libsdptransform** integrates the [JSON for Modern C++](https://github.com/nlohmann/json/) library and exposes it under the `json` C++ namespace.


### This is not JavaScript!

It's important to recall that this is not JavaScript but C++. Operations that are safe on a JavaScript `Object` may not be safe in a C++ JSON object.

So, before reading a JSON value, make sure that its corresponding `key` **does exist** and also check its type (`int`, `std::string`, `nullptr`, etc.) before assigning it to a C++ variable.

* For example, assuming that the parsed SDP `session` does NOT have a `s=` line (name), the following code would crash:

```c++
std::string sdpName = session.at("name");
// =>
// terminating with uncaught exception of type nlohmann::detail::out_of_range:
// [json.exception.out_of_range.403] key 'name' not found
```

* The safe way is:

```c++
std::string sdpName;

if (session.find("name") != session.end())
{
  // NOTE: The API guarantees that the SDP name is a string (otherwise this
  // would crash).
  sdpName = session.at("name");
}
```

* And a more efficient way is:

```c++
std::string sdpName;
auto it = session.find("name");

if (it != session.end())
{
  // NOTE: The API guarantees that the SDP name is a string (otherwise this
  // would crash).
  sdpName = it->get<std::string>();
  // or just:
  sdpName = *it;
}
```

* Also, as in C++ maps, using the `[]` operator on a JSON object for reading the value of a given `key` will insert such a `key` in the `json` object with value `nullptr` if it did not exist before.

* So, when using `parseParams()` or `parseImageAttributes()` exposed API, the application should do some checks before reading a value of a supposed type. So, for instance, let's assume that the first `a=fmtp` line in a `video` media section is `a=fmtp:97 profile-level-id=4d0028;packetization-mode=1`. The safe way to read its values is:

```c++
auto h264Fmtp = sdptransform::parseParams(video.at("fmtp")[0].at("config"));
std::string profileLevelId;
int packetizationMode;

if (
  h264Fmtp.find("profile-level-id") != h264Fmtp.end() &&
  h264Fmtp["profile-level-id"].is_string()
)
{
  profileLevelId = h264Fmtp.at("profile-level-id");
}

if (
  h264Fmtp.find("packetization-mode") != h264Fmtp.end() &&
  h264Fmtp["packetization-mode"].is_number_unsigned()
)
{
  packetizationMode = h264Fmtp.at("packetization-mode");
}
```

* And much more efficient:

```c++
auto h264Fmtp = sdptransform::parseParams(video.at("fmtp")[0].at("config"));
std::string profileLevelId;
int packetizationMode;

auto profileLevelIdIterator = h264Fmtp.find("profile-level-id");

if (
  profileLevelIdIterator != h264Fmtp.end() &&
  profileLevelIdIterator->is_string()
)
{
  profileLevelId = *profileLevelIdIterator;
}

auto packetizationModeIterator = h264Fmtp.find("packetization-mode");

if (
  packetizationModeIterator != h264Fmtp.end() &&
  packetizationModeIterator->is_number_unsigned()
)
{
  packetizationMode = *packetizationModeIterator;
}
```

It's **strongly** recommended to read the [JSON documentation](https://github.com/nlohmann/json/) and, before reading a parsed SDP, check whether the desired field exists and it has the desired type (string, integer, float, etc).


## Usage - Parser


### parse()

```c++
json parse(const std::string& sdp)
```

Parses an unprocessed SDP string and returns a JSON object. SDP lines can be terminated on `\r\n` (as per specification) or just `\n`.

The syntax of the generated SDP object and each SDP line is documented [here](doc/Grammar.md).

```c++
std::string sdpStr = R"(v=0
o=- 20518 0 IN IP4 203.0.113.1
s=
t=0 0
c=IN IP4 203.0.113.1
a=ice-ufrag:F7gI
a=ice-pwd:x9cml/YzichV2+XlhiMu8g
a=fingerprint:sha-1 42:89:c5:c6:55:9d:6e:c8:e8:83:55:2a:39:f9:b6:eb:e9:a3:a9:e7
m=audio 54400 RTP/SAVPF 0 96
a=rtpmap:0 PCMU/8000
a=rtpmap:96 opus/48000
a=ptime:20
a=sendrecv
a=candidate:0 1 UDP 2113667327 203.0.113.1 54400 typ host
a=candidate:1 2 UDP 2113667326 203.0.113.1 54401 typ host
m=video 55400 RTP/SAVPF 97 98
a=rtcp-fb:* nack
a=rtpmap:97 H264/90000
a=fmtp:97 profile-level-id=4d0028;packetization-mode=1
a=rtcp-fb:97 trr-int 100
a=rtcp-fb:97 nack rpsi
a=rtpmap:98 VP8/90000
a=rtcp-fb:98 trr-int 100
a=rtcp-fb:98 nack rpsi
a=sendrecv
a=candidate:0 1 UDP 2113667327 203.0.113.1 55400 typ host
a=candidate:1 2 UDP 2113667326 203.0.113.1 55401 typ host
a=ssrc:1399694169 foo:bar
a=ssrc:1399694169 baz
)";

json session = sdptransform::parse(sdpStr);
```

Resulting `session` is a JSON object as follows:

```json
{
  "connection": {
    "ip": "203.0.113.1",
    "version": 4
  },
  "fingerprint": {
    "hash": "42:89:c5:c6:55:9d:6e:c8:e8:83:55:2a:39:f9:b6:eb:e9:a3:a9:e7",
    "type": "sha-1"
  },
  "icePwd": "x9cml/YzichV2+XlhiMu8g",
  "iceUfrag": "F7gI",
  "media": [
    {
      "candidates": [
        {
          "component": 1,
          "foundation": "0",
          "ip": "203.0.113.1",
          "port": 54400,
          "priority": 2113667327,
          "transport": "UDP",
          "type": "host"
        },
        {
          "component": 2,
          "foundation": "1",
          "ip": "203.0.113.1",
          "port": 54401,
          "priority": 2113667326,
          "transport": "UDP",
          "type": "host"
        }
      ],
      "direction": "sendrecv",
      "fmtp": [],
      "payloads": "0 96",
      "port": 54400,
      "protocol": "RTP/SAVPF",
      "ptime": 20,
      "rtp": [
        {
          "codec": "PCMU",
          "payload": 0,
          "rate": 8000
        },
        {
          "codec": "opus",
          "payload": 96,
          "rate": 48000
        }
      ],
      "type": "audio"
    },
    {
      "candidates": [
        {
          "component": 1,
          "foundation": "0",
          "ip": "203.0.113.1",
          "port": 55400,
          "priority": 2113667327,
          "transport": "UDP",
          "type": "host"
        },
        {
          "component": 2,
          "foundation": "1",
          "ip": "203.0.113.1",
          "port": 55401,
          "priority": 2113667326,
          "transport": "UDP",
          "type": "host"
        }
      ],
      "direction": "sendrecv",
      "fmtp": [
        {
          "config": "profile-level-id=4d0028;packetization-mode=1",
          "payload": 97
        }
      ],
      "payloads": "97 98",
      "port": 55400,
      "protocol": "RTP/SAVPF",
      "rtcpFb": [
        {
          "payload": "*",
          "type": "nack"
        },
        {
          "payload": "97",
          "subtype": "rpsi",
          "type": "nack"
        },
        {
          "payload": "98",
          "subtype": "rpsi",
          "type": "nack"
        }
      ],
      "rtcpFbTrrInt": [
        {
          "payload": "97",
          "value": 100
        },
        {
          "payload": "98",
          "value": 100
        }
      ],
      "rtp": [
        {
          "codec": "H264",
          "payload": 97,
          "rate": 90000
        },
        {
          "codec": "VP8",
          "payload": 98,
          "rate": 90000
        }
      ],
      "ssrcs": [
        {
          "attribute": "foo",
          "id": 1399694169,
          "value": "bar"
        },
        {
          "attribute": "baz",
          "id": 1399694169
        }
      ],
      "type": "video"
    }
  ],
  "name": "",
  "origin": {
    "address": "203.0.113.1",
    "ipVer": 4,
    "netType": "IN",
    "sessionId": 20518,
    "sessionVersion": 0,
    "username": "-"
  },
  "timing": {
    "start": 0,
    "stop": 0
  },
  "version": 0
}
```


### Parser postprocessing

No excess parsing is done to the raw strings because the writer is built to be the inverse of the parser. That said, a few helpers have been built in:


#### parseParams()

```c++
json parseParams(const std::string& str)
```

Parses `fmtp.at("config")` and others such as `rid.at("params")` and returns an object with all the params in a key/value fashion.

NOTE: The type of each value is auto-detected, so it can be a string, integer or float. Do **NOT** assume the type of a value! (read the **This is not JavaScript!** section above).

```c++
// a=fmtp:97 profile-level-id=4d0028;packetization-mode=1

json params =
  sdptransform::parseParams(session.at("media")[1].at("fmtp")[0].at("config"));
```

Resulting `params` is a JSON object as follows:

```json
{
  "packetization-mode": 1,
  "profile-level-id": "4d0028"
}
```


#### parsePayloads()

```c++
std::vector<int> parsePayloads(const std::string& str)
```

Returns a vector with all the payload advertised in the corresponding m-line.

```c++
// m=video 55400 RTP/SAVPF 97 98

json payloads =
  sdptransform::parsePayloads(session.at("media")[1].at("payloads"));
```

Resulting `payloads` is a C++ vector of `int` elements as follows:

```json
[ 97, 98 ]
```


#### parseImageAttributes()

```c++
json parseImageAttributes(const std::string& str)
```

Parses [Generic Image Attributes](https://tools.ietf.org/html/rfc6236). Must be provided with the `attrs1` or `attrs2` string of a `a=imageattr` line. Returns an array of key/value objects.

NOTE: The type of each value is auto-detected, so it can be a string, integer or float. Do **NOT** assume the type of a value! (read the **This is not JavaScript!** section above).

```c++
// a=imageattr:97 send [x=1280,y=720] recv [x=1280,y=720] [x=320,y=180]

std::string imageAttributesStr = "[x=1280,y=720] [x=320,y=180]";

json imageAttributes = sdptransform::parseImageAttributes(imageAttributesStr);
```

Resulting `imageAttributes` is a JSON array as follows:

```json
[
  { "x": 1280, "y": 720 },
  { "x":  320, "y": 180 }
]
```


#### parseSimulcastStreamList()

```c++
json parseSimulcastStreamList(const std::string& str)
```

Parses [simulcast](https://tools.ietf.org/html/draft-ietf-mmusic-sdp-simulcast) streams/formats. Must be provided with the `attrs1` or `attrs2` string of the `a=simulcast` line.

Returns an array of simulcast streams. Each entry is an array of alternative simulcast formats, which are objects with two keys:

* `scid`: Simulcast identifier (string)
* `paused`: Whether the simulcast format is paused (boolean)

```c++
// // a=simulcast:send 1,~4;2;3 recv c
std::string simulcastAttributesStr = "1,~4;2;3";

json simulcastAttributes =
  sdptransform::parseSimulcastStreamList(simulcastAttributesStr);
```

Resulting `simulcastAttributes` is a JSON array as follows:

```json
[
  [ { "scid": "1", "paused": false }, { "scid": "4", "paused": true } ],
  [ { "scid": "2", "paused": false } ],
  [ { "scid": "3", "paused": false } ]
]
```


## Usage - Writer


### write()

```c++
std::string write(json& session)
```

The writer is the inverse of the parser, and will need a struct equivalent to the one returned by it.

```c++
std::string newSdpStr = sdptransform::write(session); // session parsed above
```

Resulting `newSdpStr` is a string as follows:

```
v=0
o=- 20518 0 IN IP4 203.0.113.1
s=
c=IN IP4 203.0.113.1
t=0 0
a=ice-ufrag:F7gI
a=ice-pwd:x9cml/YzichV2+XlhiMu8g
a=fingerprint:sha-1 42:89:c5:c6:55:9d:6e:c8:e8:83:55:2a:39:f9:b6:eb:e9:a3:a9:e7
m=audio 54400 RTP/SAVPF 0 96
a=rtpmap:0 PCMU/8000
a=rtpmap:96 opus/48000
a=ptime:20
a=sendrecv
a=candidate:0 1 UDP 2113667327 203.0.113.1 54400 typ host
a=candidate:1 2 UDP 2113667326 203.0.113.1 54401 typ host
m=video 55400 RTP/SAVPF 97 98
a=rtpmap:97 H264/90000
a=rtpmap:98 VP8/90000
a=fmtp:97 profile-level-id=4d0028;packetization-mode=1
a=rtcp-fb:97 trr-int 100
a=rtcp-fb:98 trr-int 100
a=rtcp-fb:* nack
a=rtcp-fb:97 nack rpsi
a=rtcp-fb:98 nack rpsi
a=sendrecv
a=candidate:0 1 UDP 2113667327 203.0.113.1 55400 typ host
a=candidate:1 2 UDP 2113667326 203.0.113.1 55401 typ host
a=ssrc:1399694169 foo:bar
a=ssrc:1399694169 baz
```

The only thing different from the original input is we follow the order specified by the SDP RFC, and we will always do so.


## Installation

```bash
git clone https://github.com/ibc/libsdptransform.git
cd libsdptransform/
cmake . -Bbuild
make install -C build/ # or: cd build/ && make install
```

Depending on the host, it will generate the following static lib and header files:

```
-- Installing: /usr/local/lib/libsdptransform.a
-- Up-to-date: /usr/local/include/sdptransform/sdptransform.hpp
-- Up-to-date: /usr/local/include/sdptransform/json.hpp
```


## Development

* Build the lib:

```bash
$ cmake . -Bbuild
```

* Run test units:

```bash
$ ./scripts/test.sh
```


## Author

Iñaki Baz Castillo [[website](https://inakibaz.me)|[github](https://github.com/ibc/)]

Special thanks to [Eirik Albrigtsen](https://github.com/clux), the author of the [sdp-transform](https://github.com/clux/sdp-transform/) JavaScript library.


## License

[MIT](LICENSE)
