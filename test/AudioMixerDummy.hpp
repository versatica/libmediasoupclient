#ifndef MSC_TEST_AUDIO_MIXER_DUMMY_HPP
#define MSC_TEST_AUDIO_MIXER_DUMMY_HPP

#include "api/audio/audio_mixer.h"

class AudioMixerDummy : public webrtc::AudioMixer
{
public:
	bool AddSource(Source* audio_source) override { return true; };
	void RemoveSource(Source* audio_source) override {};
	void Mix(size_t number_of_channels, webrtc::AudioFrame* audio_frame_for_mixing) override
	{}
};

#endif // MSC_TEST_AUDIO_MIXER_DUMMY_HPP
