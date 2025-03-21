project( OpenCL )

set( WRAPFOLDER ${PROJECT_NAME} )

add_library( ${PROJECT_NAME}
    ${WRAPFOLDER}/device.h 
    ${WRAPFOLDER}/device.cpp 
    ${WRAPFOLDER}/platform.h 
    ${WRAPFOLDER}/platform.cpp
    ${WRAPFOLDER}/opencldist.h 
    ${WRAPFOLDER}/opencldist.cpp
    ${WRAPFOLDER}/openclgpuhost.h 
    ${WRAPFOLDER}/openclgpuhost.cpp
    ${WRAPFOLDER}/opencltypes.h
)

target_include_directories( ${PROJECT_NAME} 
	PUBLIC ${PROJECT_SOURCE_DIR} 
	PRIVATE ${PROJECT_SOURCE_DIR}/${WRAPFOLDER} 
)

find_package(OpenCL REQUIRED)

target_link_libraries( ${PROJECT_NAME}
	PUBLIC Core OpenCL::OpenCL
)