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

#include "audio/AudioCapture.h"

#include "audio/miniaudio_wrapper.h"

#include <algorithm>
#include <cctype>
#include <chrono>

namespace inputleap {

namespace {

const std::size_t kChunkBytes = kAudioChunkFrames * kAudioChannels * sizeof(std::int16_t);

// How often the sound card hands us what it has recorded.
const std::uint32_t kPeriodMs = 10;

// If the sound card has not handed us anything for this long, nothing was playing in between.
const std::chrono::milliseconds kGapThreshold{100};

} // namespace

struct AudioCapture::Impl {
    ma_context context;
    bool context_open = false;
    ma_device device;
    bool device_open = false;

    ChunkCallback callback;

    // The members below are only touched by the sound card thread.

    // sound that was recorded but is not yet enough for a complete chunk
    std::string pending;
    bool pending_has_sound = false;
    std::chrono::steady_clock::time_point last_data_time;

    // The sound card calls this, on a thread of its own, every time it has recorded something.
    // \p input holds frame_count frames in the format we asked for in start().
    static void data_callback(ma_device* device, void*, const void* input, ma_uint32 frame_count)
    {
        auto* impl = static_cast<Impl*>(device->pUserData);
        if (input == nullptr) {
            return;
        }

        // While nothing is playing the sound card may stop calling us. A leftover from before
        // such a pause is old news and must not be glued in front of the new sound.
        const auto now = std::chrono::steady_clock::now();
        if (now - impl->last_data_time > kGapThreshold) {
            impl->pending.clear();
            impl->pending_has_sound = false;
        }
        impl->last_data_time = now;

        const auto* samples = static_cast<const std::int16_t*>(input);
        const std::size_t sample_count = static_cast<std::size_t>(frame_count) * kAudioChannels;

        for (std::size_t i = 0; i < sample_count; ++i) {
            const std::int16_t sample = samples[i];
            if (sample != 0) {
                impl->pending_has_sound = true;
            }

            // Store each 16 bit number as two bytes, lowest byte first ("little-endian"), so
            // that both computers agree on the meaning of the bytes.
            impl->pending.push_back(static_cast<char>(sample & 0xff));
            impl->pending.push_back(static_cast<char>((sample >> 8) & 0xff));

            if (impl->pending.size() == kChunkBytes) {
                impl->emit_chunk();
            }
        }
    }

    void emit_chunk()
    {
        // Pure silence is not worth sending. The player on the other side fills gaps with
        // silence on its own.
        if (pending_has_sound) {
            AudioChunk chunk;
            chunk.sample_rate = kAudioSampleRate;
            chunk.channels = kAudioChannels;
            chunk.data = std::move(pending);
            callback(std::move(chunk));
        }
        pending.clear();
        pending.reserve(kChunkBytes);
        pending_has_sound = false;
    }
};

AudioCapture::AudioCapture() :
    impl_{std::make_unique<Impl>()}
{
}

AudioCapture::~AudioCapture()
{
    stop();
}

bool AudioCapture::start(ChunkCallback callback, std::string& error)
{
    stop();

    impl_->callback = std::move(callback);
    impl_->pending.clear();
    impl_->pending.reserve(kChunkBytes);
    impl_->pending_has_sound = false;
    impl_->last_data_time = std::chrono::steady_clock::now();

    ma_result result = MA_SUCCESS;

#if defined(_WIN32)
    // Windows can record what is being played on an output device directly. This is called
    // "loopback" and is a feature of WASAPI, the standard Windows audio system. Not naming a
    // device means "the default output device".
    ma_device_config config = ma_device_config_init(ma_device_type_loopback);
#else
    // Elsewhere there is no loopback, so we look for a recording device that does the same job.
    // PulseAudio and PipeWire create one for every output and call it "Monitor of <output>".
    result = ma_context_init(nullptr, 0, nullptr, &impl_->context);
    if (result != MA_SUCCESS) {
        error = std::string("could not initialize audio system: ") +
                ma_result_description(result);
        return false;
    }
    impl_->context_open = true;

    ma_device_info* capture_devices = nullptr;
    ma_uint32 capture_device_count = 0;
    result = ma_context_get_devices(&impl_->context, nullptr, nullptr,
                                    &capture_devices, &capture_device_count);
    if (result != MA_SUCCESS) {
        error = std::string("could not list audio devices: ") + ma_result_description(result);
        stop();
        return false;
    }

    ma_device_id monitor_id;
    bool monitor_found = false;
    for (ma_uint32 i = 0; i < capture_device_count && !monitor_found; ++i) {
        std::string name = capture_devices[i].name;
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (name.find("monitor") != std::string::npos) {
            monitor_id = capture_devices[i].id;
            monitor_found = true;
        }
    }
    if (!monitor_found) {
        error = "no recording device with \"monitor\" in its name was found, so there is "
                "nothing to record the speaker output from";
        stop();
        return false;
    }

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.pDeviceID = &monitor_id;
#endif

    // Whatever format the sound card really uses, miniaudio converts it to this one for us.
    config.capture.format = ma_format_s16;
    config.capture.channels = kAudioChannels;
    config.sampleRate = kAudioSampleRate;
    config.periodSizeInMilliseconds = kPeriodMs;
    config.dataCallback = &Impl::data_callback;
    config.pUserData = impl_.get();

#if defined(_WIN32)
    const ma_backend backends[] = { ma_backend_wasapi };
    result = ma_device_init_ex(backends, 1, nullptr, &config, &impl_->device);
#else
    result = ma_device_init(&impl_->context, &config, &impl_->device);
#endif
    if (result != MA_SUCCESS) {
        error = std::string("could not open audio capture device: ") +
                ma_result_description(result);
        stop();
        return false;
    }
    impl_->device_open = true;

    result = ma_device_start(&impl_->device);
    if (result != MA_SUCCESS) {
        error = std::string("could not start audio capture device: ") +
                ma_result_description(result);
        stop();
        return false;
    }
    return true;
}

void AudioCapture::stop()
{
    if (impl_->device_open) {
        // this waits for the sound card thread to finish, so data_callback() is never running
        // once we get past this line
        ma_device_uninit(&impl_->device);
        impl_->device_open = false;
    }
    if (impl_->context_open) {
        ma_context_uninit(&impl_->context);
        impl_->context_open = false;
    }
}

bool AudioCapture::is_running() const
{
    return impl_->device_open;
}

} // namespace inputleap
