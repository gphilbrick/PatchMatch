project( HoleFillApplication )

add_executable( ${PROJECT_NAME} 
	main.cpp 
)

target_link_libraries( ${PROJECT_NAME} 
	PRIVATE View )

target_include_directories( ${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR} )

# Copy OpenCL runtime files to where the exe can find them.
ensure_uptodate_runtime_resources(
	TARGET ${PROJECT_NAME}
	RESOURCE_DIR runtimeResources
	RESOURCE_DIR_CONTENTS
		images/pexels-photo-5712934.jpeg
		openCLPrograms/bleedProgram.cl
		openCLPrograms/patches/holeFillPatchMatch.cl
		openCLPrograms/utility/utility.cl
)
