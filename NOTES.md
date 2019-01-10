
## Install libwebrtc

```
git clone https://github.com/aisouard/libwebrtc.git
cd libwebrtc

mkdir out
cd out

cmake -DWEBRTC_BRANCH_HEAD=refs/branch-heads/70 ..
make
```

## Move required header files info to deps/libwebrtc/webrtc/include

```
cd out/webrtc/src

rsync -R `find logging -name \*.h*` $MSC_PATH/deps/libwebrtc/include/webrtc
rsync -R `find api/ -name \*.h*` $MSC_PATH/deps/libwebrtc/include/webrtc/api
rsync -R `find rtc_base/ -name \*.h*` $MSC_PATH/deps/libwebrtc/include/webrtc/rtc_base
rsync -R `find third_party/abseil-cpp/absl -name \*.h*` $MSC_PATH/deps/libwebrtc/include/webrtc/absl
rsync -R `find modules -name \*.h*` $MSC_PATH/deps/libwebrtc/include/webrtc/modules
rsync common_types.h $MSC_PATH/deps/libwebrtc/include/webrtc/
rsync -R `find logging -name \*.h*` $MSC_PATH/deps/libwebrtc/include/webrtc/logging
rsync -R `find media -name \*.h*` $MSC_PATH/deps/libwebrtc/include/webrtc/media
rsync -R `find p2p -name \*.h*` $MSC_PATH/deps/libwebrtc/include/webrtc/p2p
```

## Move libwebrtc binary file info to deps/libwebrtc/webrtc/lib

```
cp lib/libwebrtc.a $MSC_PATH/deps/libwebrtc/lib/
```
