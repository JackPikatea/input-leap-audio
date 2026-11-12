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
#include <string>

namespace inputleap {

// Terminology used throughout the audio code:
//  - a "sample" is one number describing the speaker position of one channel at one instant.
//  - a "frame" is one sample for every channel (so a stereo frame is 2 samples).
//  - the "sample rate" is how many frames are played per second.

/// The format the client captures audio in. Whatever format the sound card really uses is
/// converted to this, so that the rest of the code only ever deals with one format.
static const std::uint32_t kAudioSampleRate = 48000;
static const std::uint32_t kAudioChannels = 2;

/// How many frames the client puts into a single network message (480 frames = 10 ms).
static const std::uint32_t kAudioChunkFrames = 480;

/// The server refuses audio that is outside of these limits.
static const std::uint32_t kAudioMinSampleRate = 8000;
static const std::uint32_t kAudioMaxSampleRate = 192000;
static const std::uint32_t kAudioMaxChannels = 2;

/// A short piece of captured sound, as it travels from the client to the server.
struct AudioChunk {
    std::uint32_t sample_rate = 0;
    std::uint32_t channels = 0;

    /// Raw sound data: signed 16-bit little-endian samples, channels interleaved
    /// (left, right, left, right, ...).
    std::string data;
};

} // namespace inputleap
