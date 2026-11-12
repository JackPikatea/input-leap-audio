/*  InputLeap -- mouse and keyboard sharing utility
    Copyright (C) InputLeap contributors

    This package is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    found in the file LICENSE that should have accompanied this file.

    This package is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "audio/AudioPlayer.h"

#include "audio/AudioJitterBuffer.h"
#include "audio/miniaudio_wrapper.h"

#include <vector>

namespace inputleap {

namespace {

// How much sound we collect before starting to play. A bigger head start survives bigger
// network hiccups, but delays the sound by the same amount.
const std::uint32_t kTargetBufferMs = 60;

// The most sound we are willing to queue before skipping ahead. See AudioJitterBuffer.
const std::uint32_t kMaxBufferMs = 250;

// How often the sound card asks us for more sound.
const std::uint32_t kPeriodMs = 10;

} // namespace

struct AudioPlayer::Impl {
    ma_device device;
    bool device_open = false;
    std::uint32_t sample_rate = 0;
    std::uint32_t channels = 0;
    std::unique_ptr<AudioJitterBuffer> buffer;

    // reused by push() so that it does not have to allocate memory for every message
    std::vector<std::int16_t> decoded;

    // The sound card calls this, on a thread of its own, every time it needs more sound.
    // \p output has room for frame_count frames in the format we asked for in start().
    static void data_callback(ma_device* device, void* output, const void*, ma_uint32 frame_count)
    {
        auto* impl = static_cast<Impl*>(device->pUserData);
        impl->buffer->pop(static_cast<std::int16_t*>(output), frame_count * impl->channels);
    }
};

AudioPlayer::AudioPlayer() :
    impl_{std::make_unique<Impl>()}
{
}

AudioPlayer::~AudioPlayer()
{
    stop();
}

bool AudioPlayer::start(std::uint32_t sample_rate, std::uint32_t channels, std::string& error)
{
    stop();

    if (sample_rate == 0 || channels == 0) {
        error = "invalid audio format";
        return false;
    }

    impl_->sample_rate = sample_rate;
    impl_->channels = channels;

    // work in whole frames first, so that both sizes are a multiple of the channel count
    const std::size_t target_frames = static_cast<std::size_t>(sample_rate) * kTargetBufferMs / 1000;
    const std::size_t max_frames = static_cast<std::size_t>(sample_rate) * kMaxBufferMs / 1000;
    impl_->buffer = std::make_unique<AudioJitterBuffer>(target_frames * channels,
                                                        max_frames * channels);

    // We always hand the sound over in the format the client sent it in. If the sound card
    // wants something else miniaudio converts it for us.
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_s16;
    config.playback.channels = channels;
    config.sampleRate = sample_rate;
    config.periodSizeInMilliseconds = kPeriodMs;
    config.dataCallback = &Impl::data_callback;
    config.pUserData = impl_.get();

    ma_result result = ma_device_init(nullptr, &config, &impl_->device);
    if (result != MA_SUCCESS) {
        error = std::string("could not open audio output device: ") +
                ma_result_description(result);
        return false;
    }
    impl_->device_open = true;

    result = ma_device_start(&impl_->device);
    if (result != MA_SUCCESS) {
        error = std::string("could not start audio output device: ") +
                ma_result_description(result);
        stop();
        return false;
    }
    return true;
}

void AudioPlayer::stop()
{
    if (impl_->device_open) {
        // this waits for the sound card thread to finish, so data_callback() is never running
        // once we get past this line
        ma_device_uninit(&impl_->device);
        impl_->device_open = false;
    }
}

bool AudioPlayer::is_running() const
{
    return impl_->device_open;
}

std::uint32_t AudioPlayer::sample_rate() const
{
    return impl_->sample_rate;
}

std::uint32_t AudioPlayer::channels() const
{
    return impl_->channels;
}

void AudioPlayer::push(const std::string& pcm)
{
    if (!impl_->device_open) {
        return;
    }

    // Turn pairs of bytes back into 16 bit numbers. An incomplete frame at the end, which a
    // well behaved client never sends, is ignored so the channels can't get swapped.
    const std::size_t frame_count = pcm.size() / (2 * impl_->channels);
    const std::size_t sample_count = frame_count * impl_->channels;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(pcm.data());

    impl_->decoded.resize(sample_count);
    for (std::size_t i = 0; i < sample_count; ++i) {
        impl_->decoded[i] = static_cast<std::int16_t>(
                static_cast<std::uint16_t>(bytes[2 * i]) |
                static_cast<std::uint16_t>(bytes[2 * i + 1] << 8));
    }

    impl_->buffer->push(impl_->decoded.data(), sample_count);
}

} // namespace inputleap
