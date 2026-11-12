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

#include <cstdint>
#include <memory>
#include <string>

namespace inputleap {

/** Plays sound that was received from the network on the default output device.

    The server creates one of these for each client that sends audio.
*/
class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    /** Opens the default output device for sound of the given format and starts playing
        (silence, until push() is called).
        Returns false and describes the problem in \p error on failure.
    */
    bool start(std::uint32_t sample_rate, std::uint32_t channels, std::string& error);

    /// Stops playing and closes the output device. Does nothing if not started.
    void stop();

    bool is_running() const;

    /// The format given to start()
    std::uint32_t sample_rate() const;
    std::uint32_t channels() const;

    /** Queues sound to be played. \p pcm is in the format of AudioChunk::data and must contain
        whole frames. Ignored if not started.
    */
    void push(const std::string& pcm);

private:
    // Everything that needs the (very large) miniaudio header lives in this struct, which is
    // only defined in the .cpp file. That keeps miniaudio out of every file that merely uses
    // an AudioPlayer.
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace inputleap
