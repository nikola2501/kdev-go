set(kdevdlv_SRCS
    dlvplugin.cpp
    dlvlauncher.cpp
    dlvsession.cpp
    dapclient.cpp
    dlvbreakpointcontroller.cpp
    dlvframestackmodel.cpp
    dlvvariablecontroller.cpp
    dlvdebug.cpp
)

kdevplatform_add_plugin(kdevdlv JSON kdevdlv.json SOURCES ${kdevdlv_SRCS})

target_link_libraries(kdevdlv
    KDev::Interfaces
    KDev::Debugger
    KDev::OutputView
    KDev::Util
    Qt6::Network
    KF6::TextEditor
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
