cmake_minimum_required(VERSION 3.20)
project(Chat_server)

find_package(TBB REQUIRED)
include(CTest)

set(SERVER SERVER_END)
set(CLIENT CLIENT_END)
add_executable(${SERVER} src/server.cpp)

add_executable(${CLIENT} test_client.cpp)
target_include_directories(${SERVER} PRIVATE include/async_frame)
target_include_directories(${CLIENT} PRIVATE include/async_frame)
target_link_libraries(${SERVER} PRIVATE TBB::tbb)
target_link_libraries(${CLIENT} PRIVATE TBB::tbb)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)



if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(${SERVER} PRIVATE -stdlib=libc++ -std=c++23)
	target_compile_options(${CLIENT} PRIVATE -stdlib=libc++ -std=c++23)
	if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.13")
		target_link_options(${SERVER} PRIVATE -stdlib=libc++)
		target_link_options(${CLIENT} PRIVATE -stdlib=libc++)
	else()
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
	endif()
else()
	target_compile_features(${SERVER} PRIVATE cxx_std_23)
	target_compile_features(${CLIENT} PRIVATE cxx_std_23)
endif()

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
