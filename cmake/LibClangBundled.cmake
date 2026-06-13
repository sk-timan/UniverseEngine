# Bundled libclang from OpenSpecTest/thirdparty/libclang (build-time only).
set(LIBCLANG_BUNDLED_ROOT
  "${CMAKE_SOURCE_DIR}/OpenSpecTest/thirdparty/libclang"
  CACHE PATH "Vendored libclang root directory")

set(_LibClangBundled_IncludeDir "${LIBCLANG_BUNDLED_ROOT}/include")
set(_LibClangBundled_ImportLib_Debug "${LIBCLANG_BUNDLED_ROOT}/lib/Debug/libclang.lib")
set(_LibClangBundled_ImportLib_Release "${LIBCLANG_BUNDLED_ROOT}/lib/Release/libclang.lib")
set(_LibClangBundled_Dll_Debug "${LIBCLANG_BUNDLED_ROOT}/bin/Debug/libclang.dll")
set(_LibClangBundled_Dll_Release "${LIBCLANG_BUNDLED_ROOT}/bin/Release/libclang.dll")
set(_LibClangBundled_IndexHeader "${_LibClangBundled_IncludeDir}/clang-c/Index.h")

if (NOT EXISTS "${_LibClangBundled_IndexHeader}")
  message(FATAL_ERROR
    "Bundled libclang headers not found at ${_LibClangBundled_IndexHeader}. "
    "Populate OpenSpecTest/thirdparty/libclang before building ReflectionGenerator.")
endif()

if (NOT EXISTS "${_LibClangBundled_ImportLib_Release}")
  message(FATAL_ERROR
    "Bundled libclang import library not found at ${_LibClangBundled_ImportLib_Release}")
endif()

if (NOT EXISTS "${_LibClangBundled_Dll_Release}")
  message(FATAL_ERROR
    "Bundled libclang runtime not found at ${_LibClangBundled_Dll_Release}")
endif()

if (NOT EXISTS "${_LibClangBundled_ImportLib_Debug}")
  set(_LibClangBundled_ImportLib_Debug "${_LibClangBundled_ImportLib_Release}")
endif()

if (NOT EXISTS "${_LibClangBundled_Dll_Debug}")
  set(_LibClangBundled_Dll_Debug "${_LibClangBundled_Dll_Release}")
endif()

if (NOT TARGET bundled_libclang)
  add_library(bundled_libclang SHARED IMPORTED GLOBAL)
  set_target_properties(bundled_libclang PROPERTIES
    # Plain include path; ReflectionGenerator adds this explicitly to avoid /external:I.
    INTERFACE_INCLUDE_DIRECTORIES ""
    IMPORTED_IMPLIB_DEBUG "${_LibClangBundled_ImportLib_Debug}"
    IMPORTED_IMPLIB_RELEASE "${_LibClangBundled_ImportLib_Release}"
    IMPORTED_IMPLIB_RELWITHDEBINFO "${_LibClangBundled_ImportLib_Release}"
    IMPORTED_IMPLIB_MINSIZEREL "${_LibClangBundled_ImportLib_Release}"
    IMPORTED_LOCATION_DEBUG "${_LibClangBundled_Dll_Debug}"
    IMPORTED_LOCATION_RELEASE "${_LibClangBundled_Dll_Release}"
    IMPORTED_LOCATION_RELWITHDEBINFO "${_LibClangBundled_Dll_Release}"
    IMPORTED_LOCATION_MINSIZEREL "${_LibClangBundled_Dll_Release}"
  )
  add_library(LibClang::LibClang ALIAS bundled_libclang)
endif()

function(libclang_bundled_deploy_runtime InTarget)
  if (NOT TARGET "${InTarget}")
    message(FATAL_ERROR "libclang_bundled_deploy_runtime: target '${InTarget}' does not exist.")
  endif()

  add_custom_command(TARGET "${InTarget}" POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<IF:$<CONFIG:Debug>,${_LibClangBundled_Dll_Debug},${_LibClangBundled_Dll_Release}>"
      "$<TARGET_FILE_DIR:${InTarget}>"
    COMMENT "Deploy bundled libclang.dll next to ${InTarget}"
    VERBATIM
  )
endfunction()
