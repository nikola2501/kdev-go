ecm_add_test(testduchain.cpp
        TEST_NAME duchain
        LINK_LIBRARIES
        Qt6::Test
        KDev::Language
        KDev::Tests
        kdevgoduchain
        kdevgoparser
)
