cmake_minimum_required(VERSION 3.21)
project(SimplePresenter VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.9 REQUIRED COMPONENTS Quick Multimedia ShaderTools)
find_package(Protobuf REQUIRED)
find_package(Fontconfig REQUIRED)
find_package(ZLIB REQUIRED)
# FFmpeg's libraries, which Qt Multimedia plays video with anyway, are used directly for
# one thing: taking a single frame from a video for its thumbnail (src/videoframe.cpp).
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFmpeg REQUIRED IMPORTED_TARGET libavformat libavcodec libswscale libavutil)
qt_standard_project_setup(REQUIRES 6.9)

# ProPresenter 7 document schema: the presentation and playlist documents and everything
# they import.
set(PRO_PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ProPresenter7-Proto/autogen-proto)
set(PRO_PROTO_NAMES
    action alignmentGuide alphaType applicationInfo background collectionElementType color cue
    customOptions digitalAudio effects fileProperties font graphicsData groups hotKey input
    intRange layers messages musicKeyScale planningCenter playlist presentation
    presentationSlide propresenter propSlide rvtimestamp
    slide stage templateIdentification timers url uuid version
)
list(TRANSFORM PRO_PROTO_NAMES PREPEND ${PRO_PROTO_DIR}/ OUTPUT_VARIABLE PRO_PROTO_FILES)
list(TRANSFORM PRO_PROTO_FILES APPEND .proto)

add_library(proproto STATIC)
set_target_properties(proproto PROPERTIES AUTOMOC OFF POSITION_INDEPENDENT_CODE ON)
protobuf_generate(
    TARGET proproto
    LANGUAGE cpp
    PROTOS ${PRO_PROTO_FILES}
    IMPORT_DIRS ${PRO_PROTO_DIR}
    PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto
)
target_include_directories(proproto PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/proto)
target_link_libraries(proproto PUBLIC protobuf::libprotobuf)

qt_add_executable(SimplePresenter
    src/main.cpp
    src/fontresolver.cpp
    src/fontresolver.h
    src/playlistfile.cpp
    src/playlistfile.h
    src/playlistimport.cpp
    src/playlistimport.h
    src/proconvert.cpp
    src/proconvert.h
    src/prodocument.cpp
    src/prodocument.h
    src/richtext.cpp
    src/richtext.h
    src/rtf.cpp
    src/rtf.h
    src/rtfwriter.cpp
    src/rtfwriter.h
    src/textlayout.cpp
    src/textlayout.h
    src/thumbnailprovider.cpp
    src/thumbnailprovider.h
    src/videoframe.cpp
    src/videoframe.h
    src/workspacefiles.cpp
    src/workspacefiles.h
    src/zipreader.cpp
    src/zipreader.h
)
target_include_directories(SimplePresenter PRIVATE src)

qt_add_qml_module(SimplePresenter
    # Not the program's own name: the module is built into a folder named after its URI,
    # which would collide with the executable.
    URI SimplePresenterApp
    VERSION 1.0
    QML_FILES
        qml/AppButton.qml
        qml/AppCheck.qml
        qml/AppComboBox.qml
        qml/AppSlider.qml
        qml/AppTextField.qml
        qml/AuxWindow.qml
        qml/ColorButton.qml
        qml/Divider.qml
        qml/Editor.qml
        qml/EditorCanvas.qml
        qml/EditorInspector.qml
        qml/EmptyNote.qml
        qml/FontPicker.qml
        qml/IconButton.qml
        qml/KineticWheel.qml
        qml/LiveRing.qml
        qml/Main.qml
        qml/MediaBin.qml
        qml/Output.qml
        qml/PopupMenu.qml
        qml/PreviewPanel.qml
        qml/Slide.qml
        qml/SlideElement.qml
        qml/MediaContent.qml
        qml/NumberField.qml
        qml/ResizeGrips.qml
        qml/RowDrag.qml
        qml/RowIcon.qml
        qml/SectionTitle.qml
        qml/SettingsScreen.qml
        qml/Sidebar.qml
        qml/SidebarList.qml
        qml/SlideGrid.qml
        qml/Stage.qml
        qml/StageView.qml
        qml/Toolbar.qml
        qml/ToolbarIcon.qml
        qml/TransitionLayer.qml
        qml/ZoomButtons.qml
    SOURCES
        src/catalog.h
        src/catalog.cpp
        src/framerelay.h
        src/framerelay.cpp
        src/presentationeditor.h
        src/presentationeditor.cpp
        src/richtextbridge.h
        src/richtextbridge.cpp
        src/strokedtext.h
        src/strokedtext.cpp
)

# Compiled by qsb for every RHI backend; loaded as qrc:/shaders/<name>.frag.qsb
qt_add_shaders(SimplePresenter "shaders"
    PREFIX "/"
    FILES
        shaders/dissolve.frag
        shaders/ripple.frag
        shaders/gl-transitions/circleopen.frag
        shaders/gl-transitions/crosswarp.frag
        shaders/gl-transitions/CrossZoom.frag
        shaders/gl-transitions/directionalwarp.frag
        shaders/gl-transitions/Dreamy.frag
        shaders/gl-transitions/heart.frag
        shaders/gl-transitions/LinearBlur.frag
        shaders/gl-transitions/pinwheel.frag
        shaders/gl-transitions/pixelize.frag
        shaders/gl-transitions/Radial.frag
        shaders/gl-transitions/randomsquares.frag
        shaders/gl-transitions/SimpleZoom.frag
        shaders/gl-transitions/Swirl.frag
        shaders/gl-transitions/WaterDrop.frag
        shaders/gl-transitions/wind.frag
        shaders/gl-transitions/windowslice.frag
        shaders/gl-transitions/wipeDown.frag
        shaders/gl-transitions/wipeLeft.frag
        shaders/gl-transitions/wipeRight.frag
        shaders/gl-transitions/wipeUp.frag
)

target_link_libraries(SimplePresenter PRIVATE Qt6::Quick Qt6::Multimedia proproto Fontconfig::Fontconfig ZLIB::ZLIB
                      PkgConfig::FFmpeg)
