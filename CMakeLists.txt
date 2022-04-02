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
	GIT_TAG 9fd6668220f60a1523f2e83b610558b8a164f24d
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

add_library(proxinject_injectee SHARED ${INJECTEE_SRCS})
target_compile_features(proxinject_injectee PUBLIC cxx_std_20)
target_link_libraries(proxinject_injectee PUBLIC minhook ws2_32 protopuf)
target_include_directories(proxinject_injectee PUBLIC ${asio_SOURCE_DIR}/asio/include src/common)
target_compile_definitions(proxinject_injectee PUBLIC -D_WIN32_WINNT=${version})

file(GLOB INJECTOR_SRCS src/injector/*.cpp)

add_executable(proxinject_injector ${INJECTOR_SRCS})
target_compile_features(proxinject_injector PUBLIC cxx_std_20)
target_link_libraries(proxinject_injector PUBLIC protopuf)
target_include_directories(proxinject_injector PUBLIC ${asio_SOURCE_DIR}/asio/include src/common)
target_compile_definitions(proxinject_injector PUBLIC -D_WIN32_WINNT=${version})
