kdeveloppgqt_generate(go_parser_SRC go NAMESPACE go DEBUG_VISITOR TOKEN_TEXT GENERATE_LEXER
      "${CMAKE_CURRENT_SOURCE_DIR}/go.g")

set(go_parser_lib_SRC 
    parsesession.cpp
    gomodule.cpp
)

add_library(kdevgoparser SHARED ${go_parser_SRC} ${go_parser_lib_SRC})
include(GenerateExportHeader)
generate_export_header(kdevgoparser BASE_NAME kdevgoparser EXPORT_MACRO_NAME KDEVGOPARSER_EXPORT)
target_link_libraries( kdevgoparser LINK_PRIVATE
    KDev::Language
)
target_link_libraries(kdevgoparser PUBLIC KDevelopPGQt::Utils)

add_executable(go_parser main.cpp)
target_link_libraries(go_parser
    kdevgoparser
    KDev::Language
)
install(TARGETS kdevgoparser DESTINATION ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
