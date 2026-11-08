cmake_minimum_required(VERSION 3.22)

# Static MSVC runtime: the plugin needs no Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

project(AutoTheremin VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ---------------------------------------------------------------- JUCE
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.9
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------- SDL3 (PS5 DualSense / gamepads)
option(AT_WITH_GAMEPAD "Build DualSense / gamepad support (SDL3)" ON)

if(AT_WITH_GAMEPAD)
    foreach(opt SDL_SHARED SDL_TEST_LIBRARY SDL_TESTS SDL_EXAMPLES SDL_INSTALL
                SDL_AUDIO SDL_RENDER SDL_GPU SDL_CAMERA SDL_DIALOG SDL_POWER
                SDL_VULKAN SDL_OPENGL SDL_OPENGLES SDL_WAYLAND SDL_KMSDRM
                SDL_DIRECTX SDL_X11 SDL_IBUS SDL_PIPEWIRE)
        set(${opt} OFF CACHE BOOL "" FORCE)
    endforeach()
    foreach(opt SDL_STATIC SDL_JOYSTICK SDL_HIDAPI SDL_HIDAPI_JOYSTICK SDL_SENSOR SDL_HAPTIC)
        set(${opt} ON CACHE BOOL "" FORCE)
    endforeach()
    # Windows needs SDL's hidden helper window for joystick device messages;
    # elsewhere the video subsystem isn't needed for gamepads.
    if(WIN32)
        set(SDL_VIDEO ON CACHE BOOL "" FORCE)
    else()
        set(SDL_VIDEO OFF CACHE BOOL "" FORCE)
        set(SDL_UNIX_CONSOLE_BUILD ON CACHE BOOL "" FORCE)
    endif()

    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-3.2.10
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(SDL3)
endif()

# ---------------------------------------------------------------- Plugin
juce_add_plugin(AutoTheremin
    COMPANY_NAME            "APT Industrial"
    COMPANY_WEBSITE         ""
    BUNDLE_ID               com.aptindustrial.autotheremin
    PLUGIN_MANUFACTURER_CODE Aptx
    PLUGIN_CODE             AtR1
    FORMATS                 VST3 Standalone
    PRODUCT_NAME            "Auto Theremin"
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES         Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(AutoThereminData
    SOURCES
        Resources/fonts/Michroma-Regular.ttf
        Resources/fonts/ShareTechMono-Regular.ttf
        Resources/fonts/Righteous-Regular.ttf
        Resources/fonts/Monoton-Regular.ttf)

target_sources(AutoTheremin PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/model/CurveModel.cpp
    Source/input/Gamepad.cpp
    Source/ui/LookAndFeel.cpp
    Source/ui/CurveEditor.cpp
    Source/ui/FieldPad.cpp
    Source/ui/Widgets.cpp)

target_include_directories(AutoTheremin PRIVATE Source)

target_compile_definitions(AutoTheremin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)
target_compile_definitions(AutoTheremin PRIVATE AT_WITH_GAMEPAD=$<BOOL:${AT_WITH_GAMEPAD}>)

target_link_libraries(AutoTheremin
    PRIVATE
        AutoThereminData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(AT_WITH_GAMEPAD)
    target_link_libraries(AutoTheremin PRIVATE SDL3::SDL3-static)
endif()

# ---------------------------------------------------------------- Dev tools (not shipped)
option(AT_BUILD_TOOLS "Build offline render / UI snapshot tools" OFF)
if(AT_BUILD_TOOLS)
    set(AT_CORE_SOURCES
        Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/model/CurveModel.cpp
        Source/input/Gamepad.cpp Source/ui/LookAndFeel.cpp Source/ui/CurveEditor.cpp
        Source/ui/FieldPad.cpp Source/ui/Widgets.cpp)
    foreach(tool AtRender AtSnapshot)
        juce_add_console_app(${tool} PRODUCT_NAME "${tool}")
        target_include_directories(${tool} PRIVATE Source)
        target_compile_definitions(${tool} PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 AT_WITH_GAMEPAD=0 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries(${tool} PRIVATE AutoThereminData juce::juce_audio_utils juce::juce_dsp
                              juce::juce_recommended_config_flags)
    endforeach()
    target_sources(AtRender PRIVATE tools/Render.cpp ${AT_CORE_SOURCES})
    target_sources(AtSnapshot PRIVATE tools/Snapshot.cpp ${AT_CORE_SOURCES})
endif()
