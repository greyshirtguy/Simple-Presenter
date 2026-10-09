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
