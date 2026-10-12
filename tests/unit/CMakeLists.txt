# The unit tests: one small program for each part of the app that is plain rules, built
# from that part's sources alone, with nothing of the windows in it. `ctest` runs them.
find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(tst_showstate
    tst_showstate.cpp
    ${PROJECT_SOURCE_DIR}/src/showstate.h
    ${PROJECT_SOURCE_DIR}/src/showstate.cpp
)
target_include_directories(tst_showstate PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_showstate PRIVATE Qt6::Core Qt6::Test)
add_test(NAME showstate COMMAND tst_showstate)

qt_add_executable(tst_screenfile
    tst_screenfile.cpp
    ${PROJECT_SOURCE_DIR}/src/screenfile.h
    ${PROJECT_SOURCE_DIR}/src/screenfile.cpp
)
target_include_directories(tst_screenfile PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_screenfile PRIVATE Qt6::Core Qt6::Test proproto)
add_test(NAME screenfile COMMAND tst_screenfile)

qt_add_executable(tst_ndisetup
    tst_ndisetup.cpp
    ${PROJECT_SOURCE_DIR}/src/ndisetup.h
    ${PROJECT_SOURCE_DIR}/src/ndisetup.cpp
)
target_include_directories(tst_ndisetup PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_ndisetup PRIVATE Qt6::Core Qt6::Test)
add_test(NAME ndisetup COMMAND tst_ndisetup)

qt_add_executable(tst_searchindex
    tst_searchindex.cpp
    ${PROJECT_SOURCE_DIR}/src/searchindex.h
    ${PROJECT_SOURCE_DIR}/src/searchindex.cpp
)
target_include_directories(tst_searchindex PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_searchindex PRIVATE Qt6::Core Qt6::Test)
add_test(NAME searchindex COMMAND tst_searchindex)

qt_add_executable(tst_lookfile
    tst_lookfile.cpp
    ${PROJECT_SOURCE_DIR}/src/lookfile.h
    ${PROJECT_SOURCE_DIR}/src/lookfile.cpp
)
target_include_directories(tst_lookfile PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_lookfile PRIVATE Qt6::Core Qt6::Test proproto)
add_test(NAME lookfile COMMAND tst_lookfile)

qt_add_executable(tst_thememath
    tst_thememath.cpp
    ${PROJECT_SOURCE_DIR}/src/thememath.h
    ${PROJECT_SOURCE_DIR}/src/thememath.cpp
)
target_include_directories(tst_thememath PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_thememath PRIVATE Qt6::Core Qt6::Test)
add_test(NAME thememath COMMAND tst_thememath)

qt_add_executable(tst_chords
    tst_chords.cpp
    ${PROJECT_SOURCE_DIR}/src/chords.h
    ${PROJECT_SOURCE_DIR}/src/chords.cpp
)
target_include_directories(tst_chords PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_chords PRIVATE Qt6::Core Qt6::Test)
add_test(NAME chords COMMAND tst_chords)

qt_add_executable(tst_groupedit
    tst_groupedit.cpp
    ${PROJECT_SOURCE_DIR}/src/groupedit.h
    ${PROJECT_SOURCE_DIR}/src/groupedit.cpp
)
target_include_directories(tst_groupedit PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_groupedit PRIVATE Qt6::Core Qt6::Test proproto)
add_test(NAME groupedit COMMAND tst_groupedit)

qt_add_executable(tst_labelfile
    tst_labelfile.cpp
    ${PROJECT_SOURCE_DIR}/src/labelfile.h
    ${PROJECT_SOURCE_DIR}/src/labelfile.cpp
)
target_include_directories(tst_labelfile PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_labelfile PRIVATE Qt6::Core Qt6::Test proproto)
add_test(NAME labelfile COMMAND tst_labelfile)

qt_add_executable(tst_clearfile
    tst_clearfile.cpp
    ${PROJECT_SOURCE_DIR}/src/clearfile.h
    ${PROJECT_SOURCE_DIR}/src/clearfile.cpp
)
target_include_directories(tst_clearfile PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_clearfile PRIVATE Qt6::Core Qt6::Test proproto)
add_test(NAME clearfile COMMAND tst_clearfile)

qt_add_executable(tst_decodingchoice
    tst_decodingchoice.cpp
    ${PROJECT_SOURCE_DIR}/src/decodingchoice.h
    ${PROJECT_SOURCE_DIR}/src/decodingchoice.cpp
)
target_include_directories(tst_decodingchoice PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_decodingchoice PRIVATE Qt6::Core Qt6::Test)
add_test(NAME decodingchoice COMMAND tst_decodingchoice)

qt_add_executable(tst_decodingguard
    tst_decodingguard.cpp
    ${PROJECT_SOURCE_DIR}/src/decodingguard.h
    ${PROJECT_SOURCE_DIR}/src/decodingguard.cpp
)
target_include_directories(tst_decodingguard PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_decodingguard PRIVATE Qt6::Core Qt6::Test)
add_test(NAME decodingguard COMMAND tst_decodingguard)

qt_add_executable(tst_slidenumber
    tst_slidenumber.cpp
    ${PROJECT_SOURCE_DIR}/src/slidenumber.h
    ${PROJECT_SOURCE_DIR}/src/slidenumber.cpp
)
target_include_directories(tst_slidenumber PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_slidenumber PRIVATE Qt6::Core Qt6::Test)
add_test(NAME slidenumber COMMAND tst_slidenumber)

# (Held to cases that are kept beside it: tests/unit/data, which the test is told of.)
qt_add_executable(tst_rtfstandins
    tst_rtfstandins.cpp
    ${PROJECT_SOURCE_DIR}/src/rtfstandins.h
    ${PROJECT_SOURCE_DIR}/src/rtfstandins.cpp
)
target_include_directories(tst_rtfstandins PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(tst_rtfstandins PRIVATE VECTORS="${CMAKE_CURRENT_SOURCE_DIR}/data")
target_link_libraries(tst_rtfstandins PRIVATE Qt6::Core Qt6::Test)
add_test(NAME rtfstandins COMMAND tst_rtfstandins)

qt_add_executable(tst_audiocue
    tst_audiocue.cpp
    ${PROJECT_SOURCE_DIR}/src/audiocue.h
    ${PROJECT_SOURCE_DIR}/src/audiocue.cpp
)
target_include_directories(tst_audiocue PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_audiocue PRIVATE Qt6::Core Qt6::Test proproto)
add_test(NAME audiocue COMMAND tst_audiocue)

qt_add_executable(tst_audiolevel
    tst_audiolevel.cpp
    ${PROJECT_SOURCE_DIR}/src/audiolevel.h
    ${PROJECT_SOURCE_DIR}/src/audiolevel.cpp
)
target_include_directories(tst_audiolevel PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_audiolevel PRIVATE Qt6::Core Qt6::Test)
add_test(NAME audiolevel COMMAND tst_audiolevel)

qt_add_executable(tst_clocktext
    tst_clocktext.cpp
    ${PROJECT_SOURCE_DIR}/src/clocktext.h
    ${PROJECT_SOURCE_DIR}/src/clocktext.cpp
)
target_include_directories(tst_clocktext PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_clocktext PRIVATE Qt6::Core Qt6::Test)
add_test(NAME clocktext COMMAND tst_clocktext)
