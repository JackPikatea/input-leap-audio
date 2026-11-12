# InputLeap -- mouse and keyboard sharing utility
# Copyright (C) InputLeap contributors
#
# This package is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# found in the file LICENSE that should have accompanied this file.
#
# This package is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

file(GLOB headers "*.h")
file(GLOB sources "*.cpp" "*.c")

if(INPUTLEAP_ADD_HEADERS)
    list(APPEND sources ${headers})
endif()

add_library(audio STATIC ${sources})

# miniaudio is a third party, single header audio library. SYSTEM stops the compiler from
# reporting warnings for code that is not ours.
target_include_directories(audio SYSTEM PRIVATE "${CMAKE_SOURCE_DIR}/ext/miniaudio")

if (UNIX)
    # On Linux and macOS miniaudio loads the system's audio libraries when the program runs, so
    # no audio development packages are needed to build. It only needs these basics.
    target_link_libraries(audio ${CMAKE_DL_LIBS} m Threads::Threads)
endif()
