set(buildsystem_PART_SRCS
    buildsystem.cpp
    builder.cpp
    buildjob.cpp
    executabletargetitem.cpp
    utils.cpp
)

qt_add_resources(buildsystem_PART_SRCS buildsystem.qrc)
kdevplatform_add_plugin(gobuildsystem JSON buildsystem.json SOURCES ${buildsystem_PART_SRCS})
generate_export_header(gobuildsystem BASE_NAME gobuildsystem EXPORT_MACRO_NAME KDEVGOBUILDSYSTEM_EXPORT)
target_link_libraries(gobuildsystem
    KF6::KIOWidgets
    KDev::Interfaces KDev::Project KDev::Util KDev::Language KDev::OutputView KDev::Shell
)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
