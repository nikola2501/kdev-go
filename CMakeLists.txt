cmake_minimum_required(VERSION 3.16)
project(kdevgoplugin)

find_package(ECM "6.0.0" REQUIRED NO_MODULE)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMAddTests)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(GenerateExportHeader)
include(FeatureSummary)

set(QT_MIN_VERSION "6.5.0")
find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED Core Widgets Network Test)
set(KF6_DEP_VERSION "6.0.0")
find_package(KF6 ${KF6_DEP_VERSION} REQUIRED COMPONENTS
    ThreadWeaver
    TextEditor
    I18n
    KIO
    ItemViews
)
set(KDEVPLATFORM_DEP_VERSION "6.0")
find_package(KDevPlatform ${KDEVPLATFORM_DEP_VERSION} CONFIG)
set_package_properties(KDevPlatform PROPERTIES
    TYPE REQUIRED
)
find_package(KDevelopPGQt CONFIG)
set_package_properties(KDevelopPGQt PROPERTIES
    TYPE REQUIRED
)

# some distro packages of KDevelop do not ship the KDevPlatformTests library;
# allow pointing at an external build of it via KDEVPLATFORM_TESTS_PREFIX
if(BUILD_TESTING AND NOT TARGET KDev::Tests AND KDEVPLATFORM_TESTS_PREFIX)
    add_library(KDev::Tests SHARED IMPORTED)
    set_target_properties(KDev::Tests PROPERTIES
        IMPORTED_LOCATION "${KDEVPLATFORM_TESTS_PREFIX}/lib/libKDevPlatformTests.so"
        INTERFACE_INCLUDE_DIRECTORIES "${KDEVPLATFORM_TESTS_PREFIX}/include/kdevplatform"
        INTERFACE_LINK_LIBRARIES "KDev::Shell;KDev::Language;KDev::Project"
    )
endif()

add_definitions(-DTRANSLATION_DOMAIN=\"kdevgo\")

# the code base predates QT_NO_FOREACH / QT_NO_CAST_FROM_ASCII hygiene
remove_definitions(
    -DQT_NO_FOREACH
    -DQT_NO_KEYWORDS
    -DQT_NO_CAST_FROM_ASCII
    -DQT_NO_CAST_FROM_BYTEARRAY
)

# needed for parser/ module prefix with includes for some minimal namespacing
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_subdirectory(app_templates)
add_subdirectory(buildsystem)
add_subdirectory(parser)
add_subdirectory(duchain)
add_subdirectory(codecompletion)
add_subdirectory(golangcilint)
add_subdirectory(dlv)

kdevplatform_add_plugin(kdevgoplugin JSON kdevgo.json SOURCES
    kdevgoplugin.cpp
    golangparsejob.cpp
    gohighlighting.cpp
    godebug.cpp
)

target_link_libraries(kdevgoplugin
    KDev::Interfaces
    KDev::Language
    KF6::ThreadWeaver
    KF6::TextEditor
    kdevgoparser
    kdevgoduchain
    kdevgocompletion
)

install(FILES builtins.go DESTINATION ${KDE_INSTALL_DATADIR}/kdev-go)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
