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

option(PROXINJECTEE_ONLY "only build proxyinjectee" OFF)

if(NOT WIN32)
	message(FATAL_ERROR "support Windows only")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(minhook
	GIT_REPOSITORY https://github.com/TsudaKageyu/minhook
	GIT_TAG 4a455528f61b5a375b1f9d44e7d296d47f18bb18
)

set(BUILD_TESTS OFF CACHE BOOL "")
FetchContent_Declare(protopuf
	GIT_REPOSITORY https://github.com/PragmaTwice/protopuf
	GIT_TAG v2.2.0
)

FetchContent_Declare(argparse
	GIT_REPOSITORY https://github.com/p-ranav/argparse
	GIT_TAG v2.4
)

FetchContent_Declare(spdlog
	GIT_REPOSITORY https://github.com/gabime/spdlog
	GIT_TAG v1.10.0
)

FetchContent_MakeAvailable(minhook protopuf)
if(NOT PROXINJECTEE_ONLY)
	FetchContent_MakeAvailable(argparse spdlog)
endif()

FetchContent_Declare(asio
	GIT_REPOSITORY https://github.com/chriskohlhoff/asio
	GIT_TAG asio-1-22-1
)

FetchContent_GetProperties(asio)
if(NOT asio_POPULATED)
  FetchContent_Populate(asio)
endif()

if(NOT PROXINJECTEE_ONLY)
	set(ELEMENTS_ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include CACHE STRING "")
	set(ELEMENTS_BUILD_EXAMPLES OFF CACHE BOOL "")
	FetchContent_Declare(elements
		GIT_REPOSITORY https://github.com/PragmaTwice/elements
		GIT_TAG 0d603d169702428a2fa23c757e141da2cd7d228c
		GIT_SUBMODULES_RECURSE ON
	)

	FetchContent_MakeAvailable(elements)
endif()

set(WIN32_VERSION 0x0A00)

execute_process(COMMAND git describe --tags
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	RESULT_VARIABLE GIT_DESCRIBE_EXITCODE
	OUTPUT_VARIABLE PROXINJECT_VERSION)

if(NOT GIT_DESCRIBE_EXITCODE EQUAL 0)
	message(FATAL_ERROR "error occurred while dumping version via `git describe`")
endif()

string(STRIP ${PROXINJECT_VERSION} PROXINJECT_VERSION)
configure_file(src/common/version.hpp.in src/common/version.hpp)

add_library(proxinject_common INTERFACE)
target_include_directories(proxinject_common INTERFACE src/common ${CMAKE_BINARY_DIR}/src/common)
target_compile_definitions(proxinject_common INTERFACE -D_WIN32_WINNT=${WIN32_VERSION} -DUNICODE)

file(GLOB INJECTEE_SRCS src/injectee/*.cpp)

add_library(proxinjectee SHARED ${INJECTEE_SRCS})
target_compile_features(proxinjectee PUBLIC cxx_std_20)
target_link_libraries(proxinjectee PUBLIC minhook ws2_32 protopuf proxinject_common)
target_include_directories(proxinjectee PUBLIC ${asio_SOURCE_DIR}/asio/include)

if(NOT PROXINJECTEE_ONLY)
	file(GLOB INJECTOR_GUI_SRCS src/injector/injector_gui.cpp src/injector/ui_elements/*.cpp)

	set(ELEMENTS_APP_PROJECT proxinjector)
	set(ELEMENTS_APP_SOURCES ${INJECTOR_GUI_SRCS})

	set(ELEMENTS_APP_ICON "resources/proxinject.rc")
	list(APPEND ELEMENTS_APP_SOURCES ${ELEMENTS_APP_ICON})

	set(ELEMENTS_ROOT ${elements_SOURCE_DIR})
	set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${ELEMENTS_ROOT}/cmake")
	include(ElementsConfigApp)

	target_compile_features(proxinjector PUBLIC cxx_std_20)
	target_link_libraries(proxinjector PUBLIC protopuf proxinject_common)
	target_include_directories(proxinjector PUBLIC ${asio_SOURCE_DIR}/asio/include)

	file(GLOB INJECTOR_CLI_SRCS src/injector/injector_cli.cpp)

	add_executable(proxinjector-cli ${INJECTOR_CLI_SRCS})
	target_compile_features(proxinjector-cli PUBLIC cxx_std_20)
	target_link_libraries(proxinjector-cli PUBLIC protopuf argparse spdlog proxinject_common)
	target_include_directories(proxinjector-cli PUBLIC ${asio_SOURCE_DIR}/asio/include)
endif()

if(PROXINJECTEE_ONLY)
	add_executable(wow64-address-dumper src/wow64/address_dumper.cpp)
endif()
