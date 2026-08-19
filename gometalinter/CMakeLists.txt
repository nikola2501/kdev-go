set(gometalinter_SRCS plugin.cpp job.cpp problemmodel.cpp)

qt_add_resources(gometalinter_SRCS
    gometalinter.qrc
)
kdevplatform_add_plugin(gometalinter
    JSON gometalinter.json
    SOURCES ${gometalinter_SRCS}
)

target_link_libraries(gometalinter
    KDev::Language
    KDev::Project
    KDev::Shell
    KF6::ItemViews
)
