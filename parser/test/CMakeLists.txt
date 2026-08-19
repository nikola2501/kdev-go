ecm_add_test(parsertest.cpp
        LINK_LIBRARIES
        kdevgoparser
        Qt6::Test
        Qt6::Core
        KDev::Language
        KDev::Tests
        )