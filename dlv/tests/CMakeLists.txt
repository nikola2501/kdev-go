ecm_add_test(test_dapclient.cpp ../dapclient.cpp ../dlvdebug.cpp
        TEST_NAME dapclient
        LINK_LIBRARIES
        Qt6::Test
        Qt6::Network
        Qt6::Core
        )
target_include_directories(dapclient PRIVATE ..)
