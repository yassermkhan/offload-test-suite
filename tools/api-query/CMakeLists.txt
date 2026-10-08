add_offloadtest_tool(api-query
              api-query.cpp
              api-query-mock-helpers.cpp)
target_enable_agility_sdk(api-query)

target_link_libraries(api-query PRIVATE LLVMSupport OffloadTestAPI)
target_include_directories(api-query PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../lib/API/DX")

if (NOT OFFLOADTEST_ENABLE_D3D12)
  target_sources(api-query PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../lib/API/DX/DXFeatures.cpp")
endif()

if (APPLE AND OFFLOADTEST_ENABLE_VULKAN)
  get_filename_component(_Vulkan_LIB_DIR ${Vulkan_LIBRARY} DIRECTORY)
  set_property(TARGET api-query APPEND_STRING PROPERTY
               LINK_FLAGS " -Wl,-rpath,${_Vulkan_LIB_DIR} ")
endif()

if (APPLE)
  get_filename_component(_MetalIRConverter_LIB_DIR ${METAL_IRCONVERTER_LIBRARY} DIRECTORY)
  set_property(TARGET api-query APPEND_STRING PROPERTY
               LINK_FLAGS " -Wl,-rpath,${_MetalIRConverter_LIB_DIR} ")
endif()
