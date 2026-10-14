cmake_minimum_required(VERSION 3.22)
project(Freeze VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

option(GF_BUILD_PLUGIN "Build the JUCE plugin and tools" ON)

# JUCE-free DSP core
file(GLOB GF_DSP_SOURCES CONFIGURE_DEPENDS source/dsp/*.cpp)
if(GF_DSP_SOURCES)
    add_library(freeze_dsp STATIC ${GF_DSP_SOURCES})
    target_include_directories(freeze_dsp PUBLIC source)
    set_target_properties(freeze_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)
else()
    add_library(freeze_dsp INTERFACE)
    target_include_directories(freeze_dsp INTERFACE source)
endif()

# JUCE-free unit tests
enable_testing()
file(GLOB GF_TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
add_executable(freeze_tests ${GF_TEST_SOURCES})
target_link_libraries(freeze_tests PRIVATE freeze_dsp)
add_test(NAME freeze_tests COMMAND freeze_tests)

if(GF_BUILD_PLUGIN)
    add_subdirectory(external/JUCE)
endif()
