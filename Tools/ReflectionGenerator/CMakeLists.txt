include("${CMAKE_SOURCE_DIR}/cmake/LibClangBundled.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/OstMsvcStdIncludes.cmake")

add_executable(ReflectionGenerator
  Main.cpp
  ReflectionGeneratorApp.cpp
  ClangAstScanner.cpp
  MacroParser.cpp
  CodeEmitter.cpp
)

target_include_directories(ReflectionGenerator PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${LIBCLANG_BUNDLED_ROOT}/include"
)

target_link_libraries(ReflectionGenerator PRIVATE bundled_libclang)

ost_target_add_msvc_std_paths(ReflectionGenerator)

set_property(TARGET ReflectionGenerator PROPERTY CXX_STANDARD 20)

libclang_bundled_deploy_runtime(ReflectionGenerator)
