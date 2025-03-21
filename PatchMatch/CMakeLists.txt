project( PatchMatch )

set( WRAPFOLDER ${PROJECT_NAME} )

add_library( ${PROJECT_NAME}
    ${WRAPFOLDER}/holefillpatchmatch.h 
    ${WRAPFOLDER}/holefillpatchmatch.cpp 
    ${WRAPFOLDER}/holefillpatchmatchopencl.h 
    ${WRAPFOLDER}/holefillpatchmatchopencl.cpp 	
    ${WRAPFOLDER}/nnf.h 
    ${WRAPFOLDER}/nnf.cpp 
    ${WRAPFOLDER}/patchmatch.h 
    ${WRAPFOLDER}/patchmatch.cpp
    ${WRAPFOLDER}/patchmatchutility.h 
    ${WRAPFOLDER}/patchmatchutility.cpp 	
)

target_include_directories( ${PROJECT_NAME} 
	PUBLIC ${PROJECT_SOURCE_DIR} 
	PRIVATE ${PROJECT_SOURCE_DIR}/${WRAPFOLDER} 
)

target_link_libraries( ${PROJECT_NAME}
	PUBLIC OpenCL
)