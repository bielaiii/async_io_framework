#cmake_minimum_required(VERSION 3.20)
#project(Test)
#aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src src_files)
set(UT_TIMER TimerTest)
add_executable(${UT_TIMER} ${CMAKE_CURRENT_LIST_DIR}/test_timer.cpp)


set(UT_TIMEEVENT TimerEvent)
set(UT_CONNECT Connect)
set(UT_WRITEREAD WriteRead)
set(UT_TIMEOUT_READ TimeoutRead)
add_executable(${UT_TIMEEVENT} ${CMAKE_CURRENT_LIST_DIR}/test_timeout_event.cpp)
add_executable(${UT_CONNECT} ${CMAKE_CURRENT_LIST_DIR}/test_connect.cpp)
add_executable(${UT_WRITEREAD} ${CMAKE_CURRENT_LIST_DIR}/test_write_read.cpp)
add_executable(${UT_TIMEOUT_READ} ${CMAKE_CURRENT_LIST_DIR}/test_write_read_withtimeout.cpp)
set(COMMON_SETTING COMMON_SETTING)
add_library(${COMMON_SETTING} INTERFACE)

set(EXPORT_COMPILE_COMMANDS ON)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
add_compile_options(-stdlib=libc++)
add_link_options(-stdlib=libc++)
endif()
target_include_directories(${COMMON_SETTING} INTERFACE ${CMAKE_SOURCE_DIR}/include/async_frame)
set_target_properties(${COMMON_SETTING}
                       PROPERTIES
                       CXX_STANDARD 23
                       CXX_STANDARD_REQUIRED ON
)
target_compile_options(${COMMON_SETTING} INTERFACE 
                       $<$<CXX_COMPILER_ID:GNU>:
                       $<$<CONFIG:Debug>:-Wall -pedantic -g>
                       $<$<CONFIG:Release>:-O3>
                       >
                       $<$<CXX_COMPILER_ID:Clang>:
                       $<$<CONFIG:Debug>: -g -Wno-c++98-compat >
                       $<$<CONFIG:Release>:-O3>
                       >
)

set_target_properties(${UT_TIMER}
                       PROPERTIES
                       CXX_STANDARD 23
                       CXX_STANDARD_REQUIRED ON
)
set_target_properties(${UT_TIMEEVENT}
                       PROPERTIES
                       CXX_STANDARD 23
                       CXX_STANDARD_REQUIRED ON
)

set_target_properties(${UT_CONNECT}
                       PROPERTIES
                       CXX_STANDARD 23
                       CXX_STANDARD_REQUIRED ON
)
set_target_properties(${UT_WRITEREAD}
                       PROPERTIES
                       CXX_STANDARD 23
                       CXX_STANDARD_REQUIRED ON
)
set_target_properties(${UT_TIMEOUT_READ}
                       PROPERTIES
                       CXX_STANDARD 23
                       CXX_STANDARD_REQUIRED ON
)

target_link_libraries(${UT_TIMER} PRIVATE ${COMMON_SETTING})
target_link_libraries(${UT_TIMEEVENT} PRIVATE ${COMMON_SETTING})

target_link_libraries(${UT_CONNECT} PRIVATE ${COMMON_SETTING})
target_link_libraries(${UT_WRITEREAD} PRIVATE ${COMMON_SETTING})
target_link_libraries(${UT_TIMEOUT_READ} PRIVATE ${COMMON_SETTING})