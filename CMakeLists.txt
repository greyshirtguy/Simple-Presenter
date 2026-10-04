cmake_minimum_required(VERSION 3.21)
project(SimplePresenter VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.9 REQUIRED COMPONENTS Quick Multimedia ShaderTools)
find_package(Protobuf REQUIRED)
find_package(Fontconfig REQUIRED)
qt_standard_project_setup(REQUIRES 6.9)

# ProPresenter 7 document schema: presentation.proto and everything it imports.
set(PRO_PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ProPresenter7-Proto/autogen-proto)
set(PRO_PROTO_NAMES
    action alignmentGuide alphaType applicationInfo background collectionElementType color cue
    customOptions digitalAudio effects fileProperties font graphicsData groups hotKey input
    intRange layers messages musicKeyScale presentation presentationSlide propSlide rvtimestamp
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
    src/prodocument.cpp
    src/prodocument.h
    src/richtext.h
    src/rtf.cpp
    src/rtf.h
    src/thumbnailprovider.cpp
    src/thumbnailprovider.h
)
target_include_directories(SimplePresenter PRIVATE src)

qt_add_qml_module(SimplePresenter
    # Not the program's own name: the module is built into a folder named after its URI,
    # which would collide with the executable.
    URI SimplePresenterApp
    VERSION 1.0
    QML_FILES
        qml/AppButton.qml
        qml/AppComboBox.qml
        qml/AppSlider.qml
        qml/AppTextField.qml
        qml/AuxWindow.qml
        qml/KineticWheel.qml
        qml/Main.qml
        qml/Output.qml
        qml/Slide.qml
        qml/MediaContent.qml
        qml/ResizeGrips.qml
        qml/SettingsScreen.qml
        qml/SidebarList.qml
        qml/Stage.qml
        qml/StageView.qml
        qml/ToolbarIcon.qml
        qml/TransitionLayer.qml
    SOURCES
        src/catalog.h
        src/catalog.cpp
        src/framerelay.h
        src/framerelay.cpp
        src/strokedtext.h
        src/strokedtext.cpp
)

# Compiled by qsb for every RHI backend; loaded as qrc:/shaders/<name>.frag.qsb
qt_add_shaders(SimplePresenter "shaders"
    PREFIX "/"
    FILES
        shaders/dissolve.frag
        shaders/ripple.frag
)

target_link_libraries(SimplePresenter PRIVATE Qt6::Quick Qt6::Multimedia proproto Fontconfig::Fontconfig)
