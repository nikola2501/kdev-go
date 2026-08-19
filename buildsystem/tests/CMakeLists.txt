ecm_add_test(test_goutils.cpp ../utils.cpp
        TEST_NAME goutils
        LINK_LIBRARIES
        Qt6::Test
        KDev::Project
        KDev::Util
        )
target_include_directories(goutils PRIVATE .. ${CMAKE_CURRENT_BINARY_DIR}/..)
