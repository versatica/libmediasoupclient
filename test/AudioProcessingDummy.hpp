#ifndef MSC_TEST_AUDIO_PROCESSING_DUMMY_HPP
#define MSC_TEST_AUDIO_PROCESSING_DUMMY_HPP

#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/gain_control_impl.h"
#include "modules/audio_processing/level_estimator_impl.h"
#include "modules/audio_processing/noise_suppression_impl.h"
#include "modules/audio_processing/voice_detection_impl.h"

class AudioProcessingDummy : public webrtc::AudioProcessing
{
public:
	int Initialize() override { return 0; }
	int Initialize(const webrtc::ProcessingConfig& processing_config) override { return 0; }
	int Initialize(
	  int capture_input_sample_rate_hz,
	  int capture_output_sample_rate_hz,
	  int render_sample_rate_hz,
	  ChannelLayout capture_input_layout,
	  ChannelLayout capture_output_layout,
	  ChannelLayout render_input_layout) override
	{
		return 0;
	}
	void ApplyConfig(const Config& config) override {}
	void SetExtraOptions(const webrtc::Config& config) override {}
	int proc_sample_rate_hz() const override { return 44100; }
	int proc_split_sample_rate_hz() const override { return 8000; }
	size_t num_input_channels() const override { return 1; }
	size_t num_proc_channels() const override { return 1; }
	size_t num_output_channels() const override { return 1; }
	size_t num_reverse_channels() const override { return 1; }
	void set_output_will_be_muted(bool muted) override {}
	void SetRuntimeSetting(RuntimeSetting setting) override {}
	int ProcessStream(webrtc::AudioFrame* frame) override { return 0; }
	int ProcessStream(
	  const float* const* src,
	  size_t samples_per_channel,
	  int input_sample_rate_hz,
	  ChannelLayout input_layout,
	  int output_sample_rate_hz,
	  ChannelLayout output_layout,
	  float* const* dest) override
	{
		return 0;
	}
	int ProcessStream(
	  const float* const* src,
	  const webrtc::StreamConfig& input_config,
	  const webrtc::StreamConfig& output_config,
	  float* const* dest) override
	{
		return 0;
	}
	int ProcessReverseStream(webrtc::AudioFrame* frame) override { return 0; }
	int AnalyzeReverseStream(
	  const float* const* data,
	  size_t samples_per_channel,
	  int sample_rate_hz,
	  ChannelLayout layout) override
	{
		return 0;
	}
	int ProcessReverseStream(
	  const float* const* src,
	  const webrtc::StreamConfig& input_config,
	  const webrtc::StreamConfig& output_config,
	  float* const* dest) override
	{
		return 0;
	}
	int set_stream_delay_ms(int delay) override { return 0; }
	int stream_delay_ms() const override { return 0; }
	bool was_stream_delay_set() const override { return false; }
	void set_stream_key_pressed(bool key_pressed) override {}
	void set_delay_offset_ms(int offset) override {}
	int delay_offset_ms() const override { return 0; }
	void AttachAecDump(std::unique_ptr<webrtc::AecDump> aec_dump) override {}
	void DetachAecDump() override {}
	void AttachPlayoutAudioGenerator(
	  std::unique_ptr<webrtc::AudioGenerator> audio_generator) override {}
	void DetachPlayoutAudioGenerator() override {}
	void UpdateHistogramsOnCallEnd() override {}
	webrtc::AudioProcessingStats GetStatistics(bool has_remote_tracks) const override
	{
		webrtc::AudioProcessingStats stats;

		return stats;
	}
	webrtc::GainControl* gain_control() const override
	{
		webrtc::GainControlImpl* gain_control;

		return gain_control;
	}
	webrtc::LevelEstimator* level_estimator() const override
	{
		webrtc::LevelEstimatorImpl* level_estimator;

		return level_estimator;
	}
	webrtc::NoiseSuppression* noise_suppression() const override
	{
		webrtc::NoiseSuppressionImpl* noise_suppression;

		return noise_suppression;
	}
	webrtc::VoiceDetection* voice_detection() const override
	{
		webrtc::VoiceDetectionImpl* voice_detection;

		return voice_detection;
	}
	webrtc::AudioProcessing::Config GetConfig() const override
	{
		AudioProcessing::Config config;

		return config;
	}
};

#endif // MSC_TEST_AUDIO_PROCESSING_DUMMY_HPP
