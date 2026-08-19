set(golangcilint_SRCS plugin.cpp job.cpp problemmodel.cpp)

qt_add_resources(golangcilint_SRCS
    golangcilint.qrc
)
kdevplatform_add_plugin(golangcilint
    JSON golangcilint.json
    SOURCES ${golangcilint_SRCS}
)

target_link_libraries(golangcilint
    KDev::Language
    KDev::Project
    KDev::Shell
    KF6::ItemViews
)
