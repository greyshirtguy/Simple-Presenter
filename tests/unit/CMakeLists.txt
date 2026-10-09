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
