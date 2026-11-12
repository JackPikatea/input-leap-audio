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

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace inputleap {

/** A queue of sound that sits between the network and the sound card.

    Sound arrives from the network in bursts and with irregular timing, but the sound card needs
    a perfectly steady supply. This class smooths that out: the network side push()es samples in
    whenever they arrive and the sound card side pop()s them out at its own pace.

    - Nothing is handed out until \c target samples are queued. That head start is what absorbs
      irregular arrival times. It is also the delay this class adds to the sound.
    - If the queue runs dry, silence is handed out and the head start is built up again.
    - If the queue would grow beyond its capacity the oldest sound is thrown away, so that the
      delay can never keep growing.

    push() and pop() may be called from different threads.
*/
class AudioJitterBuffer {
public:
    /** Both sizes are in samples, not frames. To keep the channels from getting swapped they
        must be a multiple of the channel count, as must every count given to push() and pop().
    */
    AudioJitterBuffer(std::size_t target_samples, std::size_t capacity_samples);

    /// Adds samples to the end of the queue.
    void push(const std::int16_t* samples, std::size_t count);

    /// Takes \p count samples from the front of the queue. Whatever is not available is filled
    /// with silence, so \p out is always completely written.
    void pop(std::int16_t* out, std::size_t count);

    /// Number of samples currently queued.
    std::size_t size() const;

    /// True while pop() hands out silence because it is waiting for the queue to fill up.
    bool is_buffering() const;

private:
    mutable std::mutex mutex_;

    // The queue is a "ring": a fixed block of memory in which the queued samples start at head_
    // and wrap around to the beginning when they reach the end.
    std::vector<std::int16_t> buffer_;
    std::size_t target_;
    std::size_t head_ = 0;
    std::size_t size_ = 0;
    bool buffering_ = true;
};

} // namespace inputleap
