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

#include <gtest/gtest.h>

#include <numeric>
#include <vector>

namespace inputleap {

namespace {

// returns { first, first + 1, first + 2, ... }
std::vector<std::int16_t> counting(std::size_t count, std::int16_t first)
{
    std::vector<std::int16_t> samples(count);
    std::iota(samples.begin(), samples.end(), first);
    return samples;
}

std::vector<std::int16_t> pop(AudioJitterBuffer& buffer, std::size_t count)
{
    // pre-fill with something other than silence to see that everything gets overwritten
    std::vector<std::int16_t> out(count, -1);
    buffer.pop(out.data(), count);
    return out;
}

} // namespace

TEST(AudioJitterBufferTests, pop_belowTarget_returnsSilenceAndKeepsData)
{
    AudioJitterBuffer buffer(8, 16);
    auto in = counting(4, 1);
    buffer.push(in.data(), in.size());

    EXPECT_EQ(std::vector<std::int16_t>(4, 0), pop(buffer, 4));
    EXPECT_EQ(4u, buffer.size());
    EXPECT_TRUE(buffer.is_buffering());
}

TEST(AudioJitterBufferTests, pop_targetReached_returnsDataInOrder)
{
    AudioJitterBuffer buffer(8, 16);
    auto in = counting(8, 1);
    buffer.push(in.data(), in.size());

    EXPECT_EQ(counting(4, 1), pop(buffer, 4));
    EXPECT_EQ(counting(4, 5), pop(buffer, 4));
    EXPECT_EQ(0u, buffer.size());
    EXPECT_FALSE(buffer.is_buffering());
}

TEST(AudioJitterBufferTests, pop_moreThanAvailable_padsWithSilenceAndBuffersAgain)
{
    AudioJitterBuffer buffer(4, 16);
    auto in = counting(6, 1);
    buffer.push(in.data(), in.size());

    std::vector<std::int16_t> expected = { 1, 2, 3, 4, 5, 6, 0, 0 };
    EXPECT_EQ(expected, pop(buffer, 8));
    EXPECT_TRUE(buffer.is_buffering());

    // has to reach the target again before anything is handed out
    auto more = counting(2, 7);
    buffer.push(more.data(), more.size());
    EXPECT_EQ(std::vector<std::int16_t>(2, 0), pop(buffer, 2));

    buffer.push(more.data(), more.size());
    EXPECT_EQ(counting(2, 7), pop(buffer, 2));
}

TEST(AudioJitterBufferTests, pushAndPop_wrapAroundEndOfStorage_keepsOrder)
{
    AudioJitterBuffer buffer(2, 8);
    std::int16_t next_in = 0;
    std::int16_t next_out = 0;

    auto first = counting(6, next_in);
    buffer.push(first.data(), first.size());
    next_in += 6;

    // moving 6 samples at a time through storage for 8 wraps around in most rounds
    for (int round = 0; round < 20; ++round) {
        EXPECT_EQ(counting(6, next_out), pop(buffer, 6));
        next_out += 6;

        auto in = counting(6, next_in);
        buffer.push(in.data(), in.size());
        next_in += 6;
    }
}

TEST(AudioJitterBufferTests, push_overflow_skipsAheadToTarget)
{
    AudioJitterBuffer buffer(4, 8);
    auto in = counting(8, 1);
    buffer.push(in.data(), in.size());

    // doesn't fit: the oldest samples are dropped so that 4 remain after adding these 2
    auto more = counting(2, 9);
    buffer.push(more.data(), more.size());

    EXPECT_EQ(4u, buffer.size());
    EXPECT_EQ(counting(4, 7), pop(buffer, 4));
}

TEST(AudioJitterBufferTests, push_moreThanCapacity_keepsNewestSamples)
{
    AudioJitterBuffer buffer(4, 8);
    auto in = counting(20, 1);
    buffer.push(in.data(), in.size());

    EXPECT_EQ(4u, buffer.size());
    EXPECT_EQ(counting(4, 17), pop(buffer, 4));
}

} // namespace inputleap
