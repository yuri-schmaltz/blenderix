cmake_minimum_required(VERSION 3.22)
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../build_environment/cmake/versions.cmake"
  eigen_settings REGEX "^set[(]EIGEN_(VERSION|HASH|HASH_TYPE|FILE) ")
list(JOIN eigen_settings "\n" eigen_settings)
cmake_language(EVAL CODE "${eigen_settings}")

if(NOT IS_ABSOLUTE "${EIGEN_PREFIX}")
  message(FATAL_ERROR "Set EIGEN_PREFIX to an absolute installation path")
endif()

message(STATUS "Installing Eigen revision ${EIGEN_VERSION}")
set(work_dir "${EIGEN_PREFIX}/_build")
file(MAKE_DIRECTORY "${work_dir}")
file(DOWNLOAD
  "https://projects.blender.org/blender/lib-source/media/branch/main/${EIGEN_FILE}"
  "${work_dir}/${EIGEN_FILE}"
  EXPECTED_HASH "${EIGEN_HASH_TYPE}=${EIGEN_HASH}"
  TLS_VERIFY ON
  TIMEOUT 180
)
file(ARCHIVE_EXTRACT INPUT "${work_dir}/${EIGEN_FILE}" DESTINATION "${work_dir}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${work_dir}/eigen-${EIGEN_VERSION}" -B "${work_dir}/configured"
    "-DCMAKE_INSTALL_PREFIX=${EIGEN_PREFIX}" -DEIGEN_BUILD_TESTING=OFF -DEIGEN_BUILD_DOC=OFF
    -DEIGEN_BUILD_BLAS=OFF -DEIGEN_BUILD_LAPACK=OFF
  COMMAND_ERROR_IS_FATAL ANY
)
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${work_dir}/configured" COMMAND_ERROR_IS_FATAL ANY)
