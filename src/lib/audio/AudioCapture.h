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

#include "audio/AudioFormat.h"

#include <functional>
#include <memory>
#include <string>

namespace inputleap {

/** Records whatever this computer is playing through its speakers ("loopback" capture).

    The client uses this to get hold of the sound that it then sends to the server.

    - Windows: records the default output device. No setup is needed.
    - Linux: records the "monitor" device that PulseAudio and PipeWire provide for each output.
    - macOS: has no built in way to record the speakers. A virtual audio device that has
      "monitor" in its name is used if one is installed.
*/
class AudioCapture {
public:
    /// Receives the recorded sound in pieces of kAudioChunkFrames frames.
    using ChunkCallback = std::function<void(AudioChunk&&)>;

    AudioCapture();
    ~AudioCapture();

    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    /** Starts recording. Returns false and describes the problem in \p error on failure.

        \p callback is called on a separate thread that belongs to the sound card, not on the
        thread that called start(). It must return quickly and may only do things that are safe
        to do from another thread. Pieces that are completely silent are not reported.
    */
    bool start(ChunkCallback callback, std::string& error);

    /// Stops recording. The callback is never called again once this returns.
    void stop();

    bool is_running() const;

private:
    // see AudioPlayer.h for why this is here
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace inputleap
