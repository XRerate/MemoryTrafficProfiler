include_guard(GLOBAL)

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
  message(FATAL_ERROR "QProf is only supported when cross-compiling for Android.")
endif()

if (NOT (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64-v8a"))
  message(FATAL_ERROR "QProf is only supported for Android aarch64.")
endif()

set(_QPROF_HINTS
  $ENV{QPROF_HOME}
  ${QPROF_HOME}
  /opt/qcom/Shared/QualcommProfiler/API
)

# Include dir
find_path(QPROF_INCLUDE_DIR
  NAMES QProfilerApi.h
  HINTS ${_QPROF_HINTS}
  PATH_SUFFIXES include
  NO_CMAKE_FIND_ROOT_PATH
)

# Library (file is libQualcommProfilerApi.so)
find_library(QPROF_LIBRARY
  NAMES QualcommProfilerApi
  HINTS ${_QPROF_HINTS}
  PATH_SUFFIXES target-la/aarch64/libs
  NO_CMAKE_FIND_ROOT_PATH
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QProf
  REQUIRED_VARS QPROF_INCLUDE_DIR QPROF_LIBRARY
)

if (QProf_FOUND)
  add_library(QProf::QProf UNKNOWN IMPORTED)
  set_target_properties(QProf::QProf PROPERTIES
    IMPORTED_LOCATION             "${QPROF_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${QPROF_INCLUDE_DIR}"
  )

  set(QPROF_LIBRARIES "${QPROF_LIBRARY}")
  set(QPROF_INCLUDE_DIRS "${QPROF_INCLUDE_DIR}")
  message(STATUS "Found QProf: ${QPROF_LIBRARY}")
endif()

mark_as_advanced(QPROF_INCLUDE_DIR QPROF_LIBRARY)