cmake_minimum_required(VERSION 3.22)
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../build_environment/cmake/versions.cmake"
  oiio_settings REGEX "^set[(]OPENIMAGEIO_(VERSION|HASH|HASH_TYPE|FILE) ")
list(JOIN oiio_settings "\n" oiio_settings)
cmake_language(EVAL CODE "${oiio_settings}")

if(NOT IS_ABSOLUTE "${OIIO_PREFIX}")
  message(FATAL_ERROR "Set OIIO_PREFIX to an absolute installation path")
endif()

message(STATUS "Installing OpenImageIO ${OPENIMAGEIO_VERSION}")
set(work_dir "${OIIO_PREFIX}/_build")
file(MAKE_DIRECTORY "${work_dir}")
file(DOWNLOAD
  "https://projects.blender.org/blender/lib-source/media/branch/main/${OPENIMAGEIO_FILE}"
  "${work_dir}/${OPENIMAGEIO_FILE}"
  EXPECTED_HASH "${OPENIMAGEIO_HASH_TYPE}=${OPENIMAGEIO_HASH}"
  TLS_VERIFY ON
  TIMEOUT 180
)
file(ARCHIVE_EXTRACT INPUT "${work_dir}/${OPENIMAGEIO_FILE}" DESTINATION "${work_dir}")
string(REGEX REPLACE "^v" "" oiio_version "${OPENIMAGEIO_VERSION}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${work_dir}/OpenImageIO-${oiio_version}" -B "${work_dir}/configured"
    -G Ninja "-DCMAKE_INSTALL_PREFIX=${OIIO_PREFIX}" -DCMAKE_INSTALL_LIBDIR=lib
    -DCMAKE_BUILD_TYPE=Release "-DCMAKE_INSTALL_RPATH=$ORIGIN"
    -DOIIO_BUILD_TESTS=OFF -DBUILD_TESTING=OFF -DBUILD_DOCS=OFF -DINSTALL_DOCS=OFF
    -DUSE_PYTHON=OFF -DUSE_QT=OFF -DUSE_OPENVDB=OFF -DUSE_OPENCV=OFF -DUSE_FFMPEG=OFF
    "-DOpenColorIO_DIR=/usr/share/cmake"
  COMMAND_ERROR_IS_FATAL ANY
)
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${work_dir}/configured" --parallel COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${work_dir}/configured" COMMAND_ERROR_IS_FATAL ANY)
file(INSTALL
  "${work_dir}/OpenImageIO-${oiio_version}/LICENSE.md"
  DESTINATION "${OIIO_PREFIX}/share/licenses/OpenImageIO"
)
