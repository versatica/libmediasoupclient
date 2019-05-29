#include "sdptransform.hpp"

namespace sdptransform
{
	namespace grammar
	{
		bool hasValue(const json& o, const std::string& key);

		const std::map<char, std::vector<Rule>> rulesMap =
		{
			{
				'v',
				{
					// v=0
					{
						// name:
						"version",
						// push:
						"",
						// reg:
						std::regex("^(\\d*)$"),
						// names:
						{ },
						// types:
						{ 'd' },
						// format:
						"%d"
					}
				}
			},

			{
				'o',
				{
					// o=- 20518 0 IN IP4 203.0.113.1
					{
						// name:
						"origin",
						// push:
						"",
						// reg:
						std::regex("^(\\S*) (\\d*) (\\d*) (\\S*) IP(\\d) (\\S*)"),
						// names:
						{ "username", "sessionId", "sessionVersion", "netType", "ipVer", "address" },
						// types:
						{ 's', 'd', 'd', 's', 'd', 's' },
						// format:
						"%s %d %d %s IP%d %s"
					}
				}
			},

			{
				's',
				{
					// s=-
					{
						// name:
						"name",
						// push:
						"",
						// reg:
						std::regex("(.*)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"%s"
					}
				}
			},

			{
				'i',
				{
					// i=foo
					{
						// name:
						"description",
						// push:
						"",
						// reg:
						std::regex("(.*)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"%s"
					}
				}
			},

			{
				'u',
				{
					// u=https://foo.com
					{
						// name:
						"uri",
						// push:
						"",
						// reg:
						std::regex("(.*)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"%s"
					}
				}
			},

			{
				'e',
				{
					// e=alice@foo.com
					{
						// name:
						"email",
						// push:
						"",
						// reg:
						std::regex("(.*)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"%s"
					}
				}
			},

			{
				'p',
				{
					// p=+12345678
					{
						// name:
						"phone",
						// push:
						"",
						// reg:
						std::regex("(.*)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"%s"
					}
				}
			},

			{
				'z',
				{
					{
						// name:
						"timezones",
						// push:
						"",
						// reg:
						std::regex("(.*)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"%s"
					}
				}
			},

			{
				'r',
				{
					{
						// name:
						"repeats",
						// push:
						"",
						// reg:
						std::regex("(.*)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"%s"
					}
				}
			},

			{
				't',
				{
					// t=0 0
					{
						// name:
						"timing",
						// push:
						"",
						// reg:
						std::regex("^(\\d*) (\\d*)"),
						// names:
						{ "start", "stop" },
						// types:
						{ 'd', 'd' },
						// format:
						"%d %d"
					}
				}
			},

			{
				'c',
				{
					// c=IN IP4 10.47.197.26
					{
						// name:
						"connection",
						// push:
						"",
						// reg:
						std::regex("^IN IP(\\d) ([^\\\\S/]*)(?:/(\\d*))?"),
						// names:
						{ "version", "ip" , "ttl"},
						// types:
						{ 'd', 's', 'd'},
						// format:
						"",
						// formatFunc:
						[](const json& o)
						{
							return hasValue(o, "ttl")
								? "IN IP%d %s/%d"
								: "IN IP%d %s";
						}
					}
				}
			},

			{
				'b',
				{
					// b=AS:4000
					{
						// name:
						"",
						// push:
						"bandwidth",
						// reg:
						std::regex("^(TIAS|AS|CT|RR|RS):(\\d*)"),
						// names:
						{ "type", "limit" },
						// types:
						{ 's', 'd' },
						// format:
						"%s:%d"
					}
				}
			},

			{
				'm',
				{
					// m=video 51744 RTP/AVP 126 97 98 34 31
					{
						// name:
						"",
						// push:
						"",
						// reg:
						std::regex("^(\\w*) (\\d*)(?:/(\\d*))? ([\\w\\/]*)(?: (.*))?"),
						// names:
						{ "type", "port", "numPorts", "protocol", "payloads" },
						// types:
						{ 's', 'd', 'd', 's', 's' },
						// format:
						"",
						// formatFunc:
						[](const json& o)
						{
							return hasValue(o, "numPorts")
								? "%s %d/%d %s %s"
								: "%s %d%v %s %s";
						}
					}
				}
			},

			{
				'a',
				{
					// a=rtpmap:110 opus/48000/2
					{
						// name:
						"",
						// push:
						"rtp",
						// reg:
						std::regex("^rtpmap:(\\d*) ([\\w\\-\\.]*)(?:\\s*\\/(\\d*)(?:\\s*\\/(\\S*))?)?"),
						// names:
						{ "payload", "codec", "rate", "encoding" },
						// types:
						{ 'd', 's', 'd', 's' },
						// format:
						"",
						// formatFunc:
						[](const json& o)
						{
							return hasValue(o, "encoding")
								? "rtpmap:%d %s/%s/%s"
								: hasValue(o, "rate")
									? "rtpmap:%d %s/%s"
									: "rtpmap:%d %s";
						}
					},

					// a=fmtp:108 profile-level-id=24;object=23;bitrate=64000
					// a=fmtp:111 minptime=10; useinbandfec=1
					{
						// name:
						"",
						// push:
						"fmtp",
						// reg:
						std::regex("^fmtp:(\\d*) (.*)"),
						// names:
						{ "payload", "config" },
						// types:
						{ 'd', 's' },
						// format:
						"fmtp:%d %s"
					},

					// a=control:streamid=0
					{
						// name:
						"control",
						// push:
						"",
						// reg:
						std::regex("^control:(.*)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"control:%s"
					},

					// a=rtcp:65179 IN IP4 193.84.77.194
					{
						// name:
						"rtcp",
						// push:
						"",
						// reg:
						std::regex("^rtcp:(\\d*)(?: (\\S*) IP(\\d) (\\S*))?"),
						// names:
						{ "port", "netType", "ipVer", "address" },
						// types:
						{ 'd', 's', 'd', 's' },
						// format:
						"",
						// formatFunc:
						[](const json& o)
						{
							return hasValue(o, "address")
								? "rtcp:%d %s IP%d %s"
								: "rtcp:%d";
						}
					},

					// a=rtcp-fb:98 trr-int 100
					{
						// name:
						"",
						// push:
						"rtcpFbTrrInt",
						// reg:
						std::regex("^rtcp-fb:(\\*|\\d*) trr-int (\\d*)"),
						// names:
						{ "payload", "value" },
						// types:
						{ 's', 'd' },
						// format:
						"rtcp-fb:%s trr-int %d"
					},

					// a=rtcp-fb:98 nack rpsi
					{
						// name:
						"",
						// push:
						"rtcpFb",
						// reg:
						std::regex("^rtcp-fb:(\\*|\\d*) ([\\w\\-_]*)(?: ([\\w\\-_]*))?"),
						// names:
						{ "payload", "type", "subtype" },
						// types:
						{ 's', 's', 's' },
						// format:
						"",
						// formatFunc:
						[](const json& o)
						{
							return hasValue(o, "subtype")
								? "rtcp-fb:%s %s %s"
								: "rtcp-fb:%s %s";
						}
					},

					// a=extmap:2 urn:ietf:params:rtp-hdrext:toffset
					// a=extmap:1/recvonly URI-gps-string
					// a=extmap:3 urn:ietf:params:rtp-hdrext:encrypt urn:ietf:params:rtp-hdrext:smpte-tc 25@600/24
					{
						// name:
						"",
						// push:
						"ext",
						// reg:
						std::regex("^extmap:(\\d+)(?:\\/(\\w+))?(?: (urn:ietf:params:rtp-hdrext:encrypt))? (\\S*)(?: (\\S*))?"),
						// names:
						{ "value", "direction", "encrypt-uri", "uri", "config" },
						// types:
						{ 'd', 's', 's', 's', 's' },
						// format:
						"",
						// formatFunc:
						[](const json& o)
						{
							return std::string("extmap:%d") +
								(hasValue(o, "direction") ? "/%s" : "%v") +
								(hasValue(o, "encrypt-uri") ? " %s" : "%v") +
								" %s" +
								(hasValue(o, "config") ? " %s" : "");
						}
					},

					// a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:PS1uQCVeeCFCanVmcjkpPywjNWhcYD0mXXtxaVBR|2^20|1:32
					{
						// name:
						"",
						// push:
						"crypto",
						// reg:
						std::regex("^crypto:(\\d*) ([\\w_]*) (\\S*)(?: (\\S*))?"),
						// names:
						{ "id", "suite", "config", "sessionConfig" },
						// types:
						{ 'd', 's', 's', 's' },
						// format:
						"",
						// formatFunc:
						[](const json& o)
						{
							return hasValue(o, "sessionConfig")
								? "crypto:%d %s %s %s"
								: "crypto:%d %s %s";
						}
					},

					// a=setup:actpass
					{
						// name:
						"setup",
						// push:
						"",
						// reg:
						std::regex("^setup:(\\w*)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"setup:%s"
					},

					// a=mid:1
					{
						// name:
						"mid",
						// push:
						"",
						// reg:
						std::regex("^mid:([^\\s]*)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"mid:%s"
					},

					// a=msid:0c8b064d-d807-43b4-b434-f92a889d8587 98178685-d409-46e0-8e16-7ef0db0db64a
					{
						// name:
						"msid",
						// push:
						"",
						// reg:
						std::regex("^msid:(.*)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"msid:%s"
					},

					// a=ptime:20
					{
						// name:
						"ptime",
						// push:
						"",
						// reg:
						std::regex("^ptime:(\\d*)"),
						// names:
						{ },
						// types:
						{ 'd' },
						// format:
						"ptime:%d"
					},

					// a=maxptime:60
					{
						// name:
						"maxptime",
						// push:
						"",
						// reg:
						std::regex("^maxptime:(\\d*)"),
						// names:
						{ },
						// types:
						{ 'd' },
						// format:
						"maxptime:%d"
					},

					// a=sendrecv
					{
						// name:
						"direction",
						// push:
						"",
						// reg:
						std::regex("^(sendrecv|recvonly|sendonly|inactive)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"%s"
					},

					// a=ice-lite
					{
						// name:
						"icelite",
						// push:
						"",
						// reg:
						std::regex("^(ice-lite)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"%s"
					},

					// a=ice-ufrag:F7gI
					{
						// name:
						"iceUfrag",
						// push:
						"",
						// reg:
						std::regex("^ice-ufrag:(\\S*)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"ice-ufrag:%s"
					},

					// a=ice-pwd:x9cml/YzichV2+XlhiMu8g
					{
						// name:
						"icePwd",
						// push:
						"",
						// reg:
						std::regex("^ice-pwd:(\\S*)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"ice-pwd:%s"
					},

					// a=fingerprint:SHA-1 00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33
					{
						// name:
						"fingerprint",
						// push:
						"",
						// reg:
						std::regex("^fingerprint:(\\S*) (\\S*)"),
						// names:
						{ "type", "hash" },
						// types:
						{ 's', 's' },
						// format:
						"fingerprint:%s %s"
					},

					// a=candidate:0 1 UDP 2113667327 203.0.113.1 54400 typ host
					// a=candidate:1162875081 1 udp 2113937151 192.168.34.75 60017 typ host generation 0 network-id 3 network-cost 10
					// a=candidate:3289912957 2 udp 1845501695 193.84.77.194 60017 typ srflx raddr 192.168.34.75 rport 60017 generation 0 network-id 3 network-cost 10
					// a=candidate:229815620 1 tcp 1518280447 192.168.150.19 60017 typ host tcptype active generation 0 network-id 3 network-cost 10
					// a=candidate:3289912957 2 tcp 1845501695 193.84.77.194 60017 typ srflx raddr 192.168.34.75 rport 60017 tcptype passive generation 0 network-id 3 network-cost 10
					{
						// name:
						"",
						// push:
						"candidates",
						// reg:
						std::regex("^candidate:(\\S*) (\\d*) (\\S*) (\\d*) (\\S*) (\\d*) typ (\\S*)(?: raddr (\\S*) rport (\\d*))?(?: tcptype (\\S*))?(?: generation (\\d*))?(?: network-id (\\d*))?(?: network-cost (\\d*))?"),
						// names:
						{ "foundation", "component", "transport", "priority", "ip", "port", "type", "raddr", "rport", "tcptype", "generation", "network-id", "network-cost" },
						// types:
						{ 's', 'd', 's', 'd', 's', 'd', 's', 's', 'd', 's', 'd', 'd', 'd', 'd' },
						// format:
						"",
						// formatFunc:
						[](const json& o)
						{
							std::string str = "candidate:%s %d %s %d %s %d typ %s";

							str += hasValue(o, "raddr") ? " raddr %s rport %d" : "%v%v";

							// NOTE: candidate has three optional chunks, so %void middles one if it's
							// missing.
							str += hasValue(o, "tcptype") ? " tcptype %s" : "%v";

							if (hasValue(o, "generation"))
								str += " generation %d";

							str += hasValue(o, "network-id") ? " network-id %d" : "%v";
							str += hasValue(o, "network-cost") ? " network-cost %d" : "%v";

							return str;
						}
					},

					// a=end-of-candidates
					{
						// name:
						"endOfCandidates",
						// push:
						"",
						// reg:
						std::regex("^(end-of-candidates)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"%s"
					},

					// a=remote-candidates:1 203.0.113.1 54400 2 203.0.113.1 54401
					{
						// name:
						"remoteCandidates",
						// push:
						"",
						// reg:
						std::regex("^remote-candidates:(.*)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"remote-candidates:%s"
					},

					// a=ice-options:google-ice
					{
						// name:
						"iceOptions",
						// push:
						"",
						// reg:
						std::regex("^ice-options:(\\S*)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"ice-options:%s"
					},

					// a=ssrc:2566107569 cname:t9YU8M1UxTF8Y1A1
					{
						// name:
						"",
						// push:
						"ssrcs",
						// reg:
						std::regex("^ssrc:(\\d*) ([\\w_-]*)(?::(.*))?"),
						// names:
						{ "id", "attribute", "value" },
						// types:
						{ 'd', 's', 's' },
						// format:
						"",
						// formatFunc:
						[](const json& o)
						{
							std::string str = "ssrc:%d";

							if (hasValue(o, "attribute"))
							{
								str += " %s";

								if (hasValue(o, "value"))
									str += ":%s";
							}

							return str;
						}
					},

					// a=ssrc-group:FEC 1 2
					// a=ssrc-group:FEC-FR 3004364195 1080772241
					{
						// name:
						"",
						// push:
						"ssrcGroups",
						// reg:
						std::regex("^ssrc-group:([\x21\x23\x24\x25\x26\x27\x2A\x2B\x2D\x2E\\w]*) (.*)"),
						// names:
						{ "semantics", "ssrcs" },
						// types:
						{ 's', 's' },
						// format:
						"ssrc-group:%s %s"
					},

					// a=msid-semantic: WMS Jvlam5X3SX1OP6pn20zWogvaKJz5Hjf9OnlV
					{
						// name:
						"msidSemantic",
						// push:
						"",
						// reg:
						std::regex("^msid-semantic:\\s?(\\w*) (\\S*)"),
						// names:
						{ "semantic", "token" },
						// types:
						{ 's', 's' },
						// format:
						"msid-semantic: %s %s" // Space after ':' is not accidental.
					},

					// a=group:BUNDLE audio video
					{
						// name:
						"",
						// push:
						"groups",
						// reg:
						std::regex("^group:(\\w*) (.*)"),
						// names:
						{ "type", "mids" },
						// types:
						{ 's', 's' },
						// format:
						"group:%s %s"
					},

					// a=rtcp-mux
					{
						// name:
						"rtcpMux",
						// push:
						"",
						// reg:
						std::regex("^(rtcp-mux)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"%s"
					},

					// a=rtcp-rsize
					{
						// name:
						"rtcpRsize",
						// push:
						"",
						// reg:
						std::regex("^(rtcp-rsize)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"%s"
					},

					// a=sctpmap:5000 webrtc-datachannel 1024
					{
						// name:
						"sctpmap",
						// push:
						"",
						// reg:
						std::regex("^sctpmap:(\\d+) (\\S*)(?: (\\d*))?"),
						// names:
						{ "sctpmapNumber", "app", "maxMessageSize" },
						// types:
						{ 'd', 's', 'd' },
						// format:
						"",
						// formatFunc:
						[](const json& o)
						{
							return hasValue(o, "maxMessageSize")
								? "sctpmap:%s %s %s"
								: "sctpmap:%s %s";
						}
					},

					// a=x-google-flag:conference
					{
						// name:
						"xGoogleFlag",
						// push:
						"",
						// reg:
						std::regex("x-google-flag:([^\\s]*)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"x-google-flag:%s"
					},

					// a=rid:1 send max-width=1280;max-height=720;max-fps=30;depend=0
					{
						// name:
						"",
						// push:
						"rids",
						// reg:
						std::regex("^rid:([\\d\\w]+) (\\w+)(?: (.*))?"),
						// names:
						{ "id", "direction", "params" },
						// types:
						{ 's', 's', 's' },
						// format:
						"",
						// formatFunc:
						[](const json& o)
						{
							return hasValue(o, "params")
								? "rid:%s %s %s"
								: "rid:%s %s";
						}
					},

					// a=imageattr:97 send [x=800,y=640,sar=1.1,q=0.6] [x=480,y=320] recv [x=330,y=250]
					// a=imageattr:* send [x=800,y=640] recv *
					// a=imageattr:100 recv [x=320,y=240]
					{
						// name:
						"",
						// push:
						"imageattrs",
						// reg:
						std::regex(
							std::string() +
							// a=imageattr:97
							"^imageattr:(\\d+|\\*)" +
							// send [x=800,y=640,sar=1.1,q=0.6] [x=480,y=320]
							// send *
							"[\\s\\t]+(send|recv)[\\s\\t]+(\\*|\\[\\S+\\](?:[\\s\\t]+\\[\\S+\\])*)" +
							// recv [x=330,y=250]
							// recv *
							"(?:[\\s\\t]+(recv|send)[\\s\\t]+(\\*|\\[\\S+\\](?:[\\s\\t]+\\[\\S+\\])*))?"
						),
						// names:
						{ "pt", "dir1", "attrs1", "dir2", "attrs2" },
						// types:
						{ 's', 's', 's', 's', 's' },
						// format:
						"",
						// formatFunc:
						[](const json& o)
						{
							return std::string("imageattr:%s %s %s") +
								(hasValue(o, "dir2") ? " %s %s" : "");
						}
					},

					// a=simulcast:send 1,2,3;~4,~5 recv 6;~7,~8
					// a=simulcast:recv 1;4,5 send 6;7
					{
						// name:
						"simulcast",
						// push:
						"",
						// reg:
						std::regex(
							std::string() +
							// a=simulcast:
							"^simulcast:" +
							// send 1,2,3;~4,~5
							"(send|recv) ([a-zA-Z0-9\\-_~;,]+)" +
							// space + recv 6;~7,~8
							"(?:\\s?(send|recv) ([a-zA-Z0-9\\-_~;,]+))?" +
							// end
							"$"
						),
						// names:
						{ "dir1", "list1", "dir2", "list2" },
						// types:
						{ 's', 's', 's', 's' },
						// format:
						"",
						// formatFunc:
						[](const json& o)
						{
							return std::string("simulcast:%s %s") +
								(hasValue(o, "dir2") ? " %s %s" : "");
						}
					},

					// Old simulcast draft 03 (implemented by Firefox).
					//   https://tools.ietf.org/html/draft-ietf-mmusic-sdp-simulcast-03
					// a=simulcast: recv pt=97;98 send pt=97
					// a=simulcast: send rid=5;6;7 paused=6,7
					{
						// name:
						"simulcast_03",
						// push:
						"",
						// reg:
						std::regex("^simulcast: (.+)$"),
						// names:
						{ "value" },
						// types:
						{ 's' },
						// format:
						"simulcast: %s"
					},

					// a=framerate:25
					// a=framerate:29.97
					{
						// name:
						"framerate",
						// push:
						"",
						// reg:
						std::regex("^framerate:(\\d+(?:$|\\.\\d+))"),
						// names:
						{ },
						// types:
						{ 'f' },
						// format:
						"framerate:%s"
					},

					// a=source-filter: incl IN IP4 239.5.2.31 10.1.15.5
					{
						// name:
						"sourceFilter",
						// push:
						"",
						// reg:
						std::regex("^source-filter:[\\s\\t]+(excl|incl) (\\S*) (IP4|IP6|\\*) (\\S*) (.*)"),
						// names:
						{ "filterMode", "netType", "addressTypes", "destAddress", "srcList" },
						// types:
						{ 's', 's', 's', 's', 's' },
						// format:
						"source-filter: %s %s %s %s %s"
					},

					// a=ts-refclk:ptp=IEEE1588-2008:00-50-C2-FF-FE-90-04-37:0
					{
						// name:
						"tsRefclk",
						// push:
						"",
						// reg:
						std::regex("^ts-refclk:(.*)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"ts-refclk:%s"
					},

					// a=mediaclk:direct=0
					{
						// name:
						"mediaclk",
						// push:
						"",
						// reg:
						std::regex("^mediaclk:(.*)"),
						// names:
						{ },
						// types:
						{ 's' },
						// format:
						"mediaclk:%s"
					},

					// Any a= that we don't understand is kepts verbatim on media.invalid.
					{
						// name:
						"",
						// push:
						"invalid",
						// reg:
						std::regex("(.*)"),
						// names:
						{ "value" },
						// types:
						{ 's' },
						// format:
						"%s"
					},
				}
			}
		};

		bool hasValue(const json& o, const std::string& key)
		{
			auto it = o.find(key);

			if (it == o.end())
				return false;

			if (it->is_string())
			{
				return !it->get<std::string>().empty();
			}
			else if (it->is_number())
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}
}
