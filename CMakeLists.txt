# Copyright 2022 PragmaTwice
#
# Licensed under the Apache License,
# Version 2.0(the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.20)

project(proxinject)

include(FetchContent)

FetchContent_Declare(minhook
	GIT_REPOSITORY https://github.com/TsudaKageyu/minhook
	GIT_TAG 4a455528f61b5a375b1f9d44e7d296d47f18bb18
)

set(BUILD_TESTS OFF)
FetchContent_Declare(protopuf
	GIT_REPOSITORY https://github.com/PragmaTwice/protopuf
	GIT_TAG 6b795c2cafef4cdf85854158a25356c6055ab635
)

FetchContent_MakeAvailable(minhook protopuf)

FetchContent_Declare(asio
	GIT_REPOSITORY https://github.com/chriskohlhoff/asio
	GIT_TAG asio-1-22-1
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
)

FetchContent_GetProperties(asio)
if(NOT asio_POPULATED)
  FetchContent_Populate(asio)
endif()

set(ver ${CMAKE_SYSTEM_VERSION})
string(REPLACE "." "" ver ${ver})
string(REGEX REPLACE "([0-9])" "0\\1" ver ${ver})
set(version "0x${ver}")

file(GLOB INJECTEE_SRCS src/injectee/*.cpp)

add_library(proxinjectee SHARED ${INJECTEE_SRCS})
target_compile_features(proxinjectee PUBLIC cxx_std_20)
target_link_libraries(proxinjectee PUBLIC minhook ws2_32 protopuf)
target_include_directories(proxinjectee PUBLIC ${asio_SOURCE_DIR}/asio/include src/common)
target_compile_definitions(proxinjectee PUBLIC -D_WIN32_WINNT=${version})

file(GLOB INJECTOR_SRCS src/injector/*.cpp)

add_executable(proxinjector ${INJECTOR_SRCS})
target_compile_features(proxinjector PUBLIC cxx_std_20)
target_link_libraries(proxinjector PUBLIC protopuf)
target_include_directories(proxinjector PUBLIC ${asio_SOURCE_DIR}/asio/include src/common)
target_compile_definitions(proxinjector PUBLIC -D_WIN32_WINNT=${version})
