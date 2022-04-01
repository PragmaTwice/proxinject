cmake_minimum_required(VERSION 3.20)

project(proxinject)

include(FetchContent)

FetchContent_Declare(minhook
	GIT_REPOSITORY https://github.com/TsudaKageyu/minhook
	GIT_TAG 4a455528f61b5a375b1f9d44e7d296d47f18bb18
)

FetchContent_MakeAvailable(minhook)

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

file(GLOB INJECTEE_SRCS src/injectee/*.cpp)

add_library(proxinject_injectee SHARED ${INJECTEE_SRCS})
target_compile_features(proxinject_injectee PUBLIC cxx_std_20)
target_link_libraries(proxinject_injectee PUBLIC minhook ws2_32)
target_include_directories(proxinject_injectee PUBLIC ${asio_SOURCE_DIR}/asio/include src/common)

file(GLOB INJECTOR_SRCS src/injector/*.cpp)

add_executable(proxinject_injector ${INJECTOR_SRCS})
target_compile_features(proxinject_injector PUBLIC cxx_std_20)
target_include_directories(proxinject_injector PUBLIC ${asio_SOURCE_DIR}/asio/include src/common)
