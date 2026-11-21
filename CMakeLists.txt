cmake_minimum_required(VERSION 3.24)
project(x50-patch-switcher VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(X50_BUILD_TESTS "Build the Qt Test suite" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Test)

include(FetchContent)
set(RTMIDI_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(RTMIDI_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
# Do not depend on whether libjack happens to be installed.
set(RTMIDI_API_JACK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(rtmidi
  GIT_REPOSITORY https://github.com/thestk/rtmidi.git
  GIT_TAG 6.0.0)
FetchContent_MakeAvailable(rtmidi)

function(x50_target_options target)
  target_compile_definitions(${target} PRIVATE QT_NO_CAST_FROM_ASCII)
  if(MSVC)
    # /utf-8: sources hold UTF-8 literals (check marks in CLI output).
    target_compile_options(${target} PRIVATE /W4 /utf-8 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra)
  endif()
endfunction()

# core: no GUI, no MIDI. Placeholder-free content arrives with T1.1.
add_library(x50core STATIC src/core/Version.cpp src/core/Version.h
  src/core/SoundAddress.cpp src/core/SoundAddress.h src/core/BankMap.h
  src/core/Catalogue.cpp src/core/Catalogue.h
  src/core/SearchQuery.cpp src/core/SearchQuery.h
  src/core/ConfigLocation.cpp src/core/ConfigLocation.h
  src/core/SetModel.cpp src/core/SetModel.h
  src/core/Library.cpp src/core/Library.h
  data/x50data.qrc)
set_target_properties(x50core PROPERTIES AUTORCC ON)
target_include_directories(x50core PUBLIC src)
target_link_libraries(x50core PUBLIC Qt6::Core)
x50_target_options(x50core)

add_library(x50midi STATIC src/midi/MidiBackend.h src/midi/RtMidiBackend.cpp src/midi/RtMidiBackend.h
  src/midi/FakeX50.cpp src/midi/FakeX50.h src/midi/X50Link.cpp src/midi/X50Link.h
  src/midi/X50Protocol.cpp src/midi/X50Protocol.h
  src/midi/SysExAssembler.cpp src/midi/SysExAssembler.h)
target_include_directories(x50midi PUBLIC src)
target_link_libraries(x50midi PUBLIC x50core Qt6::Core rtmidi)
x50_target_options(x50midi)

add_library(x50ui STATIC src/ui/MainWindow.cpp src/ui/MainWindow.h
  src/ui/Theme.cpp src/ui/Theme.h src/ui/DisplayStrip.cpp src/ui/DisplayStrip.h
  src/ui/BrowseView.cpp src/ui/BrowseView.h src/ui/Glyphs.cpp src/ui/Glyphs.h
  src/ui/PanelFrame.cpp src/ui/PanelFrame.h src/ui/ResultModel.cpp src/ui/ResultModel.h
  src/ui/ResultDelegate.cpp src/ui/ResultDelegate.h
  src/ui/SearchField.cpp src/ui/SearchField.h src/ui/KindToggle.cpp src/ui/KindToggle.h
  src/ui/ResultTable.cpp src/ui/ResultTable.h src/ui/ResultHeader.cpp src/ui/ResultHeader.h
  src/ui/CategoryDelegate.cpp src/ui/CategoryDelegate.h
  src/ui/ProbeChecklist.cpp src/ui/ProbeChecklist.h src/ui/SettingsDialog.cpp src/ui/SettingsDialog.h
  src/ui/ConnectionSettings.cpp src/ui/ConnectionSettings.h
  src/ui/SetEditor.cpp src/ui/SetEditor.h src/ui/SlotCell.cpp src/ui/SlotCell.h
  src/ui/LiveView.cpp src/ui/LiveView.h src/ui/NameFit.cpp src/ui/NameFit.h
  src/ui/PixelRun.cpp src/ui/PixelRun.h
  src/ui/x50ui.qrc)
set_target_properties(x50ui PROPERTIES AUTORCC ON)
target_include_directories(x50ui PUBLIC src)
target_link_libraries(x50ui PUBLIC x50core x50midi Qt6::Widgets)
x50_target_options(x50ui)

add_executable(x50-patch-switcher
  src/app/main.cpp src/app/CommandLine.cpp src/app/CommandLine.h
  src/app/CliRunner.cpp src/app/CliRunner.h
  src/app/DiagnoseRunner.cpp src/app/DiagnoseRunner.h
  src/app/ScreenshotRunner.cpp src/app/ScreenshotRunner.h
  src/app/LoggingBackend.cpp src/app/LoggingBackend.h)
target_link_libraries(x50-patch-switcher PRIVATE x50ui x50midi x50core Qt6::Widgets)
x50_target_options(x50-patch-switcher)
if(WIN32)
  set_target_properties(x50-patch-switcher PROPERTIES WIN32_EXECUTABLE OFF)
  target_link_libraries(x50-patch-switcher PRIVATE Shell32)
endif()

if(X50_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
