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

#include "audio/AudioJitterBuffer.h"

#include <algorithm>
#include <cstring>

namespace inputleap {

AudioJitterBuffer::AudioJitterBuffer(std::size_t target_samples, std::size_t capacity_samples) :
    buffer_(std::max<std::size_t>(std::max(target_samples, capacity_samples), 1)),
    target_{target_samples}
{
}

void AudioJitterBuffer::push(const std::int16_t* samples, std::size_t count)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const std::size_t capacity = buffer_.size();

    if (size_ + count > capacity) {
        // The sound card is not keeping up with the network. Skip ahead in time so that only
        // the target amount is queued once the new samples have been added. Dropping this much
        // in one go gives a single audible skip instead of a constant stutter.
        if (count >= target_) {
            // the new samples alone are enough: keep only the newest of them
            samples += count - target_;
            count = target_;
            head_ = 0;
            size_ = 0;
        } else {
            const std::size_t keep = target_ - count;
            head_ = (head_ + (size_ - keep)) % capacity;
            size_ = keep;
        }
    }

    // copy in, in two parts if we run past the end of the ring
    const std::size_t tail = (head_ + size_) % capacity;
    const std::size_t first = std::min(count, capacity - tail);
    std::memcpy(buffer_.data() + tail, samples, first * sizeof(std::int16_t));
    std::memcpy(buffer_.data(), samples + first, (count - first) * sizeof(std::int16_t));
    size_ += count;
}

void AudioJitterBuffer::pop(std::int16_t* out, std::size_t count)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (buffering_) {
        if (size_ < target_) {
            // still collecting our head start
            std::memset(out, 0, count * sizeof(std::int16_t));
            return;
        }
        buffering_ = false;
    }

    // copy out, in two parts if we run past the end of the ring
    const std::size_t capacity = buffer_.size();
    const std::size_t available = std::min(count, size_);
    const std::size_t first = std::min(available, capacity - head_);
    std::memcpy(out, buffer_.data() + head_, first * sizeof(std::int16_t));
    std::memcpy(out + first, buffer_.data(), (available - first) * sizeof(std::int16_t));
    head_ = (head_ + available) % capacity;
    size_ -= available;

    if (available < count) {
        // ran dry: pad with silence and collect a new head start before playing again
        std::memset(out + available, 0, (count - available) * sizeof(std::int16_t));
        buffering_ = true;
    }
}

std::size_t AudioJitterBuffer::size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return size_;
}

bool AudioJitterBuffer::is_buffering() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return buffering_;
}

} // namespace inputleap
