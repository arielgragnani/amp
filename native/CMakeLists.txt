cmake_minimum_required(VERSION 3.22)
project(AltGuitarAmpRack VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")   # no Visual C++ redistributable needed on students' PCs
endif()

option(ALT_ASIO "Build with ASIO support (needs the Steinberg ASIO SDK, see README)" OFF)
set(ASIO_SDK_DIR "" CACHE PATH "Folder containing the ASIO SDK (the one with common/ and host/)")
set(JUCE_LOCAL_DIR "" CACHE PATH "Use a local JUCE checkout instead of downloading it")

include(FetchContent)
if(JUCE_LOCAL_DIR)
  add_subdirectory(${JUCE_LOCAL_DIR} juce EXCLUDE_FROM_ALL)
else()
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.8 GIT_SHALLOW ON)
  FetchContent_MakeAvailable(JUCE)
endif()

juce_add_gui_app(AltGuitarAmpRack PRODUCT_NAME "Alt Guitar Amp Rack" COMPANY_NAME "Alt" VERSION 1.0.0)
target_sources(AltGuitarAmpRack PRIVATE Source/Main.cpp)
target_compile_definitions(AltGuitarAmpRack PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:AltGuitarAmpRack,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="1.0.0" JUCE_REPORT_APP_USAGE=0 JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_WASAPI=1 JUCE_DIRECTSOUND=1 JUCE_WASAPI_EXCLUSIVE=1 JUCE_ASIO=$<BOOL:${ALT_ASIO}>)
if(ALT_ASIO)
  target_include_directories(AltGuitarAmpRack PRIVATE ${ASIO_SDK_DIR}/common ${ASIO_SDK_DIR}/host ${ASIO_SDK_DIR}/host/pc)
endif()
target_link_libraries(AltGuitarAmpRack PRIVATE
  juce::juce_gui_basics juce::juce_audio_utils juce::juce_audio_devices juce::juce_audio_formats juce::juce_data_structures
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

option(ALT_TESTS "Build the headless test programs" OFF)
if(ALT_TESTS)
  juce_add_console_app(rigtest PRODUCT_NAME "rigtest")
  target_sources(rigtest PRIVATE test/rigtest.cpp)
  target_compile_definitions(rigtest PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
  target_link_libraries(rigtest PRIVATE juce::juce_audio_devices juce::juce_audio_utils juce::juce_gui_basics juce::juce_recommended_config_flags)
endif()
