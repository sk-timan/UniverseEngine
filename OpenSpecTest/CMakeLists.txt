# CMakeList.txt: OpenSpecTest 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#

# Qt Widgets（用于编辑/显示 UI 框架）。
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

# 固定 Qt 安装路径，避免依赖命令行传入。
set(OPEN_SPEC_TEST_QT_ROOT "D:/Qt/6.10.2/msvc2022_64" CACHE PATH "Qt installation root")
if (EXISTS "${OPEN_SPEC_TEST_QT_ROOT}/lib/cmake/Qt6")
  list(PREPEND CMAKE_PREFIX_PATH "${OPEN_SPEC_TEST_QT_ROOT}")
  set(Qt6_DIR "${OPEN_SPEC_TEST_QT_ROOT}/lib/cmake/Qt6")
endif()

find_package(Qt6 COMPONENTS Widgets QUIET)
if (Qt6_FOUND)
  set(OPEN_SPEC_TEST_QT_TARGET Qt6::Widgets)
else()
  find_package(Qt5 COMPONENTS Widgets QUIET)
  if (Qt5_FOUND)
    set(OPEN_SPEC_TEST_QT_TARGET Qt5::Widgets)
  else()
    message(FATAL_ERROR "Qt Widgets not found. Please install Qt6 or Qt5 with Widgets module.")
  endif()
endif()

# nlohmann/json (header-only) integration from thirdparty/json.
set(OPEN_SPEC_TEST_NLOHMANN_JSON_ROOT
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/json"
)
set(OPEN_SPEC_TEST_NLOHMANN_JSON_HEADER
  "${OPEN_SPEC_TEST_NLOHMANN_JSON_ROOT}/nlohmann/json.hpp"
)
if (NOT EXISTS "${OPEN_SPEC_TEST_NLOHMANN_JSON_HEADER}")
  message(FATAL_ERROR
    "nlohmann/json not found at ${OPEN_SPEC_TEST_NLOHMANN_JSON_HEADER}"
  )
endif()

add_library(openspec_nlohmann_json INTERFACE)
add_library(nlohmann_json::nlohmann_json ALIAS openspec_nlohmann_json)
target_include_directories(openspec_nlohmann_json INTERFACE
  "${OPEN_SPEC_TEST_NLOHMANN_JSON_ROOT}"
)

# stb (header-only) integration from thirdparty/stb.
set(OPEN_SPEC_TEST_STB_ROOT
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/stb"
)
set(OPEN_SPEC_TEST_STB_IMAGE_HEADER
  "${OPEN_SPEC_TEST_STB_ROOT}/stb_image.h"
)
if (NOT EXISTS "${OPEN_SPEC_TEST_STB_IMAGE_HEADER}")
  message(FATAL_ERROR
    "stb_image.h not found at ${OPEN_SPEC_TEST_STB_IMAGE_HEADER}"
  )
endif()

add_library(openspec_stb INTERFACE)
add_library(stb::stb ALIAS openspec_stb)
target_include_directories(openspec_stb INTERFACE
  "${OPEN_SPEC_TEST_STB_ROOT}"
)

# Use Qt deployment tool so runtime DLLs/plugins are available next to the exe.
find_program(OPEN_SPEC_TEST_WINDEPLOYQT_EXECUTABLE
  NAMES windeployqt
  HINTS "${OPEN_SPEC_TEST_QT_ROOT}/bin"
)

# 自动收集 Shader 目录下的所有 HLSL 文件。
file(GLOB OPEN_SPEC_TEST_SHADER_FILES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Shader/*.hlsl"
)

# 自动收集 src 目录下的 C++ 源码与头文件。
file(GLOB_RECURSE OPEN_SPEC_TEST_SRC_FILES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)
list(FILTER OPEN_SPEC_TEST_SRC_FILES EXCLUDE REGEX ".*/src/tests/.*\\.cpp$")

file(GLOB OPEN_SPEC_TEST_REFLECTION_GEN_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Generated/Reflection/*.gen.cpp"
)

set(OPEN_SPEC_TEST_REFLECTION_GENERATED_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/Generated/Reflection"
)

# 将源代码添加到此项目的可执行文件。
add_executable (OpenSpecTest WIN32
  "OpenSpecTest.cpp"
  "OpenSpecTest.h"
  ${OPEN_SPEC_TEST_SRC_FILES}
  ${OPEN_SPEC_TEST_REFLECTION_GEN_SOURCES}
  ${OPEN_SPEC_TEST_SHADER_FILES}
)

target_include_directories(OpenSpecTest PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty"
  "${OPEN_SPEC_TEST_REFLECTION_GENERATED_DIR}"
)

if (TARGET ReflectionGenerator)
  set(OPEN_SPEC_TEST_COMPILE_COMMANDS "${CMAKE_BINARY_DIR}/compile_commands.json")
  if (EXISTS "${OPEN_SPEC_TEST_COMPILE_COMMANDS}")
    set(OPEN_SPEC_TEST_REFLECTION_STAMP "${OPEN_SPEC_TEST_REFLECTION_GENERATED_DIR}/_reflection.stamp")
    add_custom_command(
      OUTPUT "${OPEN_SPEC_TEST_REFLECTION_STAMP}"
      COMMAND ReflectionGenerator
        --compile-commands "${OPEN_SPEC_TEST_COMPILE_COMMANDS}"
        --scan-dir "${CMAKE_CURRENT_SOURCE_DIR}/src"
        --output-dir "${OPEN_SPEC_TEST_REFLECTION_GENERATED_DIR}"
        --module OpenSpecTest
      COMMAND ${CMAKE_COMMAND} -E touch "${OPEN_SPEC_TEST_REFLECTION_STAMP}"
      DEPENDS ReflectionGenerator
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      COMMENT "Generating reflection registration code"
      VERBATIM
    )
    add_custom_target(OpenSpecTest_ReflectionCodegen DEPENDS "${OPEN_SPEC_TEST_REFLECTION_STAMP}")
    add_dependencies(OpenSpecTest OpenSpecTest_ReflectionCodegen)
  else()
    message(STATUS "compile_commands.json not found; using checked-in Generated/Reflection outputs.")
  endif()
endif()

if (OPEN_SPEC_TEST_SHADER_FILES)
  source_group("Shader Files" FILES ${OPEN_SPEC_TEST_SHADER_FILES})
  # HLSL is compiled at runtime by Dx12Renderer; disable VS build-time shader compilation.
  set_source_files_properties(${OPEN_SPEC_TEST_SHADER_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
endif()

target_link_libraries(OpenSpecTest PRIVATE
  d3d12
  dxgi
  dxguid
  d3dcompiler
  nlohmann_json::nlohmann_json
  stb::stb
  ${OPEN_SPEC_TEST_QT_TARGET}
)

set(OPEN_SPEC_TEST_THIRDPARTY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty")
set(OPEN_SPEC_TEST_RUNTIME_BIN_DIRS)
if (EXISTS "${OPEN_SPEC_TEST_QT_ROOT}/bin" AND IS_DIRECTORY "${OPEN_SPEC_TEST_QT_ROOT}/bin")
  list(APPEND OPEN_SPEC_TEST_RUNTIME_BIN_DIRS "${OPEN_SPEC_TEST_QT_ROOT}/bin")
endif()
if (EXISTS "${OPEN_SPEC_TEST_THIRDPARTY_ROOT}" AND IS_DIRECTORY "${OPEN_SPEC_TEST_THIRDPARTY_ROOT}")
  file(GLOB OPEN_SPEC_TEST_THIRDPARTY_ITEMS
    RELATIVE "${OPEN_SPEC_TEST_THIRDPARTY_ROOT}"
    "${OPEN_SPEC_TEST_THIRDPARTY_ROOT}/*"
  )

  foreach(thirdparty_item IN LISTS OPEN_SPEC_TEST_THIRDPARTY_ITEMS)
    if (thirdparty_item STREQUAL "libclang")
      continue()
    endif()

    set(thirdparty_item_dir "${OPEN_SPEC_TEST_THIRDPARTY_ROOT}/${thirdparty_item}")
    if (NOT IS_DIRECTORY "${thirdparty_item_dir}")
      continue()
    endif()

    set(thirdparty_lib_root "${thirdparty_item_dir}/lib")
    if (NOT EXISTS "${thirdparty_lib_root}" OR NOT IS_DIRECTORY "${thirdparty_lib_root}")
      continue()
    endif()

    file(GLOB thirdparty_debug_libs "${thirdparty_lib_root}/Debug/*.lib")
    file(GLOB thirdparty_release_libs "${thirdparty_lib_root}/Release/*.lib")
    file(GLOB thirdparty_flat_libs "${thirdparty_lib_root}/*.lib")
    file(GLOB thirdparty_debug_dlls "${thirdparty_item_dir}/bin/Debug/*.dll")
    file(GLOB thirdparty_release_dlls "${thirdparty_item_dir}/bin/Release/*.dll")

    if (thirdparty_debug_libs OR thirdparty_release_libs)
      foreach(debug_lib IN LISTS thirdparty_debug_libs)
        target_link_libraries(OpenSpecTest PRIVATE debug "${debug_lib}")
      endforeach()
      foreach(release_lib IN LISTS thirdparty_release_libs)
        target_link_libraries(OpenSpecTest PRIVATE optimized "${release_lib}")
      endforeach()
    elseif(thirdparty_flat_libs)
      target_link_libraries(OpenSpecTest PRIVATE ${thirdparty_flat_libs})
    endif()

    if (EXISTS "${thirdparty_item_dir}/bin/Debug" AND IS_DIRECTORY "${thirdparty_item_dir}/bin/Debug")
      list(APPEND OPEN_SPEC_TEST_RUNTIME_BIN_DIRS "${thirdparty_item_dir}/bin/Debug")
    endif()
    if (EXISTS "${thirdparty_item_dir}/bin/Release" AND IS_DIRECTORY "${thirdparty_item_dir}/bin/Release")
      list(APPEND OPEN_SPEC_TEST_RUNTIME_BIN_DIRS "${thirdparty_item_dir}/bin/Release")
    endif()
    
    # Auto-link import libs that correspond to thirdparty bin/<config> DLLs.
    # Note: CMake/Linker cannot directly link a .dll; it links the matching .lib.
    foreach(debug_dll IN LISTS thirdparty_debug_dlls)
      get_filename_component(debug_base "${debug_dll}" NAME_WE)
      set(debug_import_lib_candidates
        "${thirdparty_item_dir}/bin/Debug/${debug_base}.lib"
        "${thirdparty_item_dir}/lib/Debug/${debug_base}.lib"
      )
      foreach(debug_import_lib IN LISTS debug_import_lib_candidates)
        if (EXISTS "${debug_import_lib}")
          target_link_libraries(OpenSpecTest PRIVATE debug "${debug_import_lib}")
          break()
        endif()
      endforeach()
    endforeach()
    
    foreach(release_dll IN LISTS thirdparty_release_dlls)
      get_filename_component(release_base "${release_dll}" NAME_WE)
      set(release_import_lib_candidates
        "${thirdparty_item_dir}/bin/Release/${release_base}.lib"
        "${thirdparty_item_dir}/lib/Release/${release_base}.lib"
      )
      foreach(release_import_lib IN LISTS release_import_lib_candidates)
        if (EXISTS "${release_import_lib}")
          target_link_libraries(OpenSpecTest PRIVATE optimized "${release_import_lib}")
          break()
        endif()
      endforeach()
    endforeach()

    # Deploy third-party runtime DLLs next to executable to avoid load failures.
    foreach(debug_dll IN LISTS thirdparty_debug_dlls)
      add_custom_command(TARGET OpenSpecTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${debug_dll}"
          "$<TARGET_FILE_DIR:OpenSpecTest>"
        VERBATIM
      )
    endforeach()
    foreach(release_dll IN LISTS thirdparty_release_dlls)
      add_custom_command(TARGET OpenSpecTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${release_dll}"
          "$<TARGET_FILE_DIR:OpenSpecTest>"
        VERBATIM
      )
    endforeach()
  endforeach()
endif()

list(REMOVE_DUPLICATES OPEN_SPEC_TEST_RUNTIME_BIN_DIRS)
if (OPEN_SPEC_TEST_RUNTIME_BIN_DIRS)
  string(JOIN ";" OPEN_SPEC_TEST_RUNTIME_PATH_SUFFIX ${OPEN_SPEC_TEST_RUNTIME_BIN_DIRS})
  set(OPEN_SPEC_TEST_DEBUGGER_ENV "PATH=%PATH%;${OPEN_SPEC_TEST_RUNTIME_PATH_SUFFIX}")
  if (EXISTS "${OPEN_SPEC_TEST_QT_ROOT}/plugins/platforms")
    string(APPEND OPEN_SPEC_TEST_DEBUGGER_ENV
      "\nQT_QPA_PLATFORM_PLUGIN_PATH=${OPEN_SPEC_TEST_QT_ROOT}/plugins/platforms")
  endif()
  set_property(TARGET OpenSpecTest PROPERTY VS_DEBUGGER_ENVIRONMENT
    "${OPEN_SPEC_TEST_DEBUGGER_ENV}")
endif()

if (OPEN_SPEC_TEST_WINDEPLOYQT_EXECUTABLE)
  add_custom_command(TARGET OpenSpecTest POST_BUILD
    COMMAND "${OPEN_SPEC_TEST_WINDEPLOYQT_EXECUTABLE}"
      "$<$<CONFIG:Debug>:--debug>$<$<NOT:$<CONFIG:Debug>>:--release>"
      --no-translations
      --dir "$<TARGET_FILE_DIR:OpenSpecTest>"
      "$<TARGET_FILE:OpenSpecTest>"
    COMMENT "Deploying Qt runtime next to OpenSpecTest.exe"
    VERBATIM
  )
endif()

add_custom_command(TARGET OpenSpecTest POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/data"
    "$<TARGET_FILE_DIR:OpenSpecTest>/data"
)

add_custom_command(TARGET OpenSpecTest POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/Shader"
    "$<TARGET_FILE_DIR:OpenSpecTest>/Shader"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET OpenSpecTest PROPERTY CXX_STANDARD 20)
endif()

enable_testing()

add_executable(OpenSpecTest_ConfigStoreTests
  "src/tests/GameplayConfigStoreTests.cpp"
  "src/data/GameplayConfigStore.cpp"
)

target_include_directories(OpenSpecTest_ConfigStoreTests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty"
)

target_link_libraries(OpenSpecTest_ConfigStoreTests PRIVATE
  nlohmann_json::nlohmann_json
)

set_property(TARGET OpenSpecTest_ConfigStoreTests PROPERTY CXX_STANDARD 20)

add_test(NAME OpenSpecTest_ConfigStoreTests COMMAND OpenSpecTest_ConfigStoreTests)

add_executable(OpenSpecTest_ObjectRegistryTests
  "src/tests/ObjectRegistryTests.cpp"
  "src/core/UClass.cpp"
  "src/core/UObject.cpp"
  "src/core/ObjectRegistry.cpp"
)

target_include_directories(OpenSpecTest_ObjectRegistryTests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty"
)

target_link_libraries(OpenSpecTest_ObjectRegistryTests PRIVATE
  nlohmann_json::nlohmann_json
)

add_definitions(-DNOMINMAX)

set_property(TARGET OpenSpecTest_ObjectRegistryTests PROPERTY CXX_STANDARD 20)

add_test(NAME OpenSpecTest_ObjectRegistryTests COMMAND OpenSpecTest_ObjectRegistryTests)

add_executable(OpenSpecTest_ComponentSystemTests
  "src/tests/ComponentSystemTests.cpp"
  "src/core/UClass.cpp"
  "src/core/UObject.cpp"
  "src/core/ObjectRegistry.cpp"
  "src/components/ActorComponent.cpp"
  "src/components/SceneComponent.cpp"
  "src/components/PrimitiveComponent.cpp"
  "src/components/MeshComponent.cpp"
  "src/components/StaticMeshComponent.cpp"
  "src/components/SkinnedMeshComponent.cpp"
  "src/components/SkeletalMeshComponent.cpp"
  "src/world/Actor.cpp"
  "src/render/RenderCollector.cpp"
  "src/render/ResourceRegistry.cpp"
  "src/render/asset/StreamableRenderAsset.cpp"
  "src/render/asset/StaticMesh.cpp"
  "src/render/asset/SkinnedAsset.cpp"
  "src/render/asset/SkeletalMesh.cpp"
  "src/serialization/ComponentSerializer.cpp"
  "src/math/FVector3.cpp"
  "src/math/FVector2D.cpp"
  "src/math/FVector4.cpp"
  "src/math/FRotator3.cpp"
  "src/math/FTransform.cpp"
)

target_include_directories(OpenSpecTest_ComponentSystemTests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty"
)

target_link_libraries(OpenSpecTest_ComponentSystemTests PRIVATE
  nlohmann_json::nlohmann_json
)

set_property(TARGET OpenSpecTest_ComponentSystemTests PROPERTY CXX_STANDARD 20)

add_test(NAME OpenSpecTest_ComponentSystemTests COMMAND OpenSpecTest_ComponentSystemTests)

add_executable(OpenSpecTest_ReflectionRuntimeTests
  "src/tests/ReflectionRuntimeTests.cpp"
  "Generated/Reflection/USceneComponent.gen.cpp"
  "src/core/UClass.cpp"
  "src/core/UObject.cpp"
  "src/core/ObjectRegistry.cpp"
  "src/components/ActorComponent.cpp"
  "src/components/SceneComponent.cpp"
  "src/world/Actor.cpp"
  "src/reflection/Field.cpp"
  "src/reflection/Property.cpp"
  "src/reflection/ScriptStruct.cpp"
  "src/reflection/Enum.cpp"
  "src/reflection/Function.cpp"
  "src/reflection/ReflectionRegistry.cpp"
  "src/reflection/GeneratedRegistration.cpp"
  "src/math/FVector3.cpp"
  "src/math/FVector2D.cpp"
  "src/math/FVector4.cpp"
  "src/math/FRotator3.cpp"
  "src/math/FTransform.cpp"
)

target_include_directories(OpenSpecTest_ReflectionRuntimeTests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty"
  "${OPEN_SPEC_TEST_REFLECTION_GENERATED_DIR}"
)

target_link_libraries(OpenSpecTest_ReflectionRuntimeTests PRIVATE
  nlohmann_json::nlohmann_json
)

set_property(TARGET OpenSpecTest_ReflectionRuntimeTests PROPERTY CXX_STANDARD 20)

add_test(NAME OpenSpecTest_ReflectionRuntimeTests COMMAND OpenSpecTest_ReflectionRuntimeTests)

add_executable(OpenSpecTest_WorldLifecycleTests
  "src/tests/WorldLifecycleTests.cpp"
  "src/core/UClass.cpp"
  "src/core/UObject.cpp"
  "src/core/ObjectRegistry.cpp"
  "src/components/ActorComponent.cpp"
  "src/components/SceneComponent.cpp"
  "src/components/PrimitiveComponent.cpp"
  "src/components/MeshComponent.cpp"
  "src/components/StaticMeshComponent.cpp"
  "src/components/SkinnedMeshComponent.cpp"
  "src/components/SkeletalMeshComponent.cpp"
  "src/world/Actor.cpp"
  "src/world/StaticMeshActor.cpp"
  "src/world/SkeletalMeshActor.cpp"
  "src/world/Level.cpp"
  "src/world/World.cpp"
  "src/render/RenderCollector.cpp"
  "src/render/ResourceRegistry.cpp"
  "src/render/asset/StreamableRenderAsset.cpp"
  "src/render/asset/StaticMesh.cpp"
  "src/render/asset/SkinnedAsset.cpp"
  "src/render/asset/SkeletalMesh.cpp"
  "src/serialization/ComponentSerializer.cpp"
  "src/data/AssimpCoordinate.cpp"
  "src/data/MeshImporter.cpp"
  "src/math/FVector2D.cpp"
  "src/math/FVector3.cpp"
  "src/math/FVector4.cpp"
  "src/math/FRotator3.cpp"
  "src/math/FTransform.cpp"
)

target_include_directories(OpenSpecTest_WorldLifecycleTests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty"
)

target_link_libraries(OpenSpecTest_WorldLifecycleTests PRIVATE
  nlohmann_json::nlohmann_json
)

if (OPEN_SPEC_TEST_THIRDPARTY_ROOT)
  foreach(thirdparty_item IN LISTS OPEN_SPEC_TEST_THIRDPARTY_ITEMS)
    set(thirdparty_item_dir "${OPEN_SPEC_TEST_THIRDPARTY_ROOT}/${thirdparty_item}")
    if (NOT IS_DIRECTORY "${thirdparty_item_dir}")
      continue()
    endif()

    set(thirdparty_lib_root "${thirdparty_item_dir}/lib")
    if (NOT EXISTS "${thirdparty_lib_root}" OR NOT IS_DIRECTORY "${thirdparty_lib_root}")
      continue()
    endif()

    file(GLOB thirdparty_debug_libs "${thirdparty_lib_root}/Debug/*.lib")
    file(GLOB thirdparty_release_libs "${thirdparty_lib_root}/Release/*.lib")
    file(GLOB thirdparty_flat_libs "${thirdparty_lib_root}/*.lib")
    if (thirdparty_debug_libs OR thirdparty_release_libs)
      foreach(debug_lib IN LISTS thirdparty_debug_libs)
        target_link_libraries(OpenSpecTest_WorldLifecycleTests PRIVATE debug "${debug_lib}")
      endforeach()
      foreach(release_lib IN LISTS thirdparty_release_libs)
        target_link_libraries(OpenSpecTest_WorldLifecycleTests PRIVATE optimized "${release_lib}")
      endforeach()
    elseif(thirdparty_flat_libs)
      target_link_libraries(OpenSpecTest_WorldLifecycleTests PRIVATE ${thirdparty_flat_libs})
    endif()
  endforeach()
endif()

set_property(TARGET OpenSpecTest_WorldLifecycleTests PROPERTY CXX_STANDARD 20)

add_test(NAME OpenSpecTest_WorldLifecycleTests COMMAND OpenSpecTest_WorldLifecycleTests)

add_executable(OpenSpecTest_LevelSaveLoadTests
  "src/tests/LevelSaveLoadTests.cpp"
  "src/core/UClass.cpp"
  "src/core/UObject.cpp"
  "src/core/ObjectRegistry.cpp"
  "src/components/ActorComponent.cpp"
  "src/components/SceneComponent.cpp"
  "src/components/PrimitiveComponent.cpp"
  "src/components/MeshComponent.cpp"
  "src/components/StaticMeshComponent.cpp"
  "src/components/SkinnedMeshComponent.cpp"
  "src/components/SkeletalMeshComponent.cpp"
  "src/world/Actor.cpp"
  "src/world/StaticMeshActor.cpp"
  "src/world/SkeletalMeshActor.cpp"
  "src/world/Level.cpp"
  "src/render/RenderCollector.cpp"
  "src/render/ResourceRegistry.cpp"
  "src/render/asset/StreamableRenderAsset.cpp"
  "src/render/asset/StaticMesh.cpp"
  "src/render/asset/SkinnedAsset.cpp"
  "src/render/asset/SkeletalMesh.cpp"
  "src/serialization/ComponentSerializer.cpp"
  "src/data/AssimpCoordinate.cpp"
  "src/data/MeshImporter.cpp"
  "src/math/FVector2D.cpp"
  "src/math/FVector3.cpp"
  "src/math/FVector4.cpp"
  "src/math/FRotator3.cpp"
  "src/math/FTransform.cpp"
)

target_include_directories(OpenSpecTest_LevelSaveLoadTests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty"
)

target_link_libraries(OpenSpecTest_LevelSaveLoadTests PRIVATE
  nlohmann_json::nlohmann_json
)

if (OPEN_SPEC_TEST_THIRDPARTY_ROOT)
  foreach(thirdparty_item IN LISTS OPEN_SPEC_TEST_THIRDPARTY_ITEMS)
    set(thirdparty_item_dir "${OPEN_SPEC_TEST_THIRDPARTY_ROOT}/${thirdparty_item}")
    if (NOT IS_DIRECTORY "${thirdparty_item_dir}")
      continue()
    endif()

    set(thirdparty_lib_root "${thirdparty_item_dir}/lib")
    if (NOT EXISTS "${thirdparty_lib_root}" OR NOT IS_DIRECTORY "${thirdparty_lib_root}")
      continue()
    endif()

    file(GLOB thirdparty_debug_libs "${thirdparty_lib_root}/Debug/*.lib")
    file(GLOB thirdparty_release_libs "${thirdparty_lib_root}/Release/*.lib")
    file(GLOB thirdparty_flat_libs "${thirdparty_lib_root}/*.lib")
    if (thirdparty_debug_libs OR thirdparty_release_libs)
      foreach(debug_lib IN LISTS thirdparty_debug_libs)
        target_link_libraries(OpenSpecTest_LevelSaveLoadTests PRIVATE debug "${debug_lib}")
      endforeach()
      foreach(release_lib IN LISTS thirdparty_release_libs)
        target_link_libraries(OpenSpecTest_LevelSaveLoadTests PRIVATE optimized "${release_lib}")
      endforeach()
    elseif(thirdparty_flat_libs)
      target_link_libraries(OpenSpecTest_LevelSaveLoadTests PRIVATE ${thirdparty_flat_libs})
    endif()
  endforeach()
endif()

set_property(TARGET OpenSpecTest_LevelSaveLoadTests PROPERTY CXX_STANDARD 20)

add_test(NAME OpenSpecTest_LevelSaveLoadTests COMMAND OpenSpecTest_LevelSaveLoadTests)

add_executable(OpenSpecTest_AssetSerializerTests
  "src/tests/AssetSerializerTests.cpp"
  "src/core/UClass.cpp"
  "src/core/UObject.cpp"
  "src/asset/AssetPackageHeader.cpp"
  "src/asset/AssetSerializer.cpp"
  "src/asset/ProjectPaths.cpp"
  "src/asset/SoftObjectPath.cpp"
  "src/render/asset/StreamableRenderAsset.cpp"
  "src/render/asset/StaticMesh.cpp"
  "src/render/asset/SkinnedAsset.cpp"
  "src/render/asset/SkeletalMesh.cpp"
  "src/math/FVector2D.cpp"
  "src/math/FVector3.cpp"
  "src/math/FVector4.cpp"
  "src/math/FRotator3.cpp"
  "src/math/FTransform.cpp"
)

target_include_directories(OpenSpecTest_AssetSerializerTests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty"
)

target_link_libraries(OpenSpecTest_AssetSerializerTests PRIVATE
  nlohmann_json::nlohmann_json
)

set_property(TARGET OpenSpecTest_AssetSerializerTests PROPERTY CXX_STANDARD 20)

add_test(NAME OpenSpecTest_AssetSerializerTests COMMAND OpenSpecTest_AssetSerializerTests)

add_executable(OpenSpecTest_TextureSerializerTests
  "src/tests/TextureSerializerTests.cpp"
  "src/tests/Dx12RendererTestStub.cpp"
  "src/core/UClass.cpp"
  "src/core/UObject.cpp"
  "src/asset/AssetPackageHeader.cpp"
  "src/asset/AssetSerializer.cpp"
  "src/asset/AssetManager.cpp"
  "src/asset/AssetRegistry.cpp"
  "src/asset/AssetReferenceResolver.cpp"
  "src/asset/AssetRedirectStore.cpp"
  "src/asset/ProjectPaths.cpp"
  "src/asset/SoftObjectPath.cpp"
  "src/render/ResourceRegistry.cpp"
  "src/render/asset/StreamableRenderAsset.cpp"
  "src/render/asset/StaticMesh.cpp"
  "src/render/asset/SkinnedAsset.cpp"
  "src/render/asset/SkeletalMesh.cpp"
  "src/render/asset/Texture.cpp"
  "src/render/asset/Texture2D.cpp"
  "src/render/asset/TextureResource.cpp"
  "src/math/FVector2D.cpp"
  "src/math/FVector3.cpp"
  "src/math/FVector4.cpp"
  "src/math/FRotator3.cpp"
  "src/math/FTransform.cpp"
)

target_include_directories(OpenSpecTest_TextureSerializerTests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty"
)

target_link_libraries(OpenSpecTest_TextureSerializerTests PRIVATE
  nlohmann_json::nlohmann_json
  d3d12
  dxguid
)

set_property(TARGET OpenSpecTest_TextureSerializerTests PROPERTY CXX_STANDARD 20)

add_test(NAME OpenSpecTest_TextureSerializerTests COMMAND OpenSpecTest_TextureSerializerTests)

# TODO: 如有需要，请添加测试并安装目标。
