cmake_minimum_required(VERSION 3.14)

if(pybind11_FOUND)
  message(STATUS "Building Python binding (synapse_vsomeip)")

  pybind11_add_module(synapse_vsomeip synapse_vsomeip.cpp)

  target_include_directories(synapse_vsomeip PRIVATE
      ${PROJECT_SOURCE_DIR}
      ${PROJECT_SOURCE_DIR}/common
  )

  target_link_libraries(synapse_vsomeip
      PRIVATE
      ${VSOMEIP_LIB}
      ${VSOMEIP_CFG_LIB}
      ${VSOMEIP_SD_LIB}
      ${VSOMEIP_E2E_LIB}
      ${BOOST_SYSTEM_LIB}
      pthread dl stdc++fs
  )

  set_target_properties(synapse_vsomeip PROPERTIES
      CXX_STANDARD 17
      CXX_STANDARD_REQUIRED YES
  )
else()
  message(WARNING "pybind11 not available; skipping Python module build.")
endif()
