set(BSATOOL
	bsatool.cpp
)
source_group(apps\\bsatool FILES ${BSATOOL})

# Main executable
openmw_add_executable(bsatool
	${BSATOOL}
)

target_link_libraries(bsatool
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  ${Boost_FILESYSTEM_LIBRARY}
  components
)

if (BUILD_WITH_CODE_COVERAGE)
  add_definitions (--coverage)
  target_link_libraries(bsatool gcov)
endif()

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16 AND MSVC)
    target_precompile_headers(bsatool PRIVATE
        <filesystem>
        <fstream>
        <vector>
    )
endif()
