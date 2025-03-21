project (Core)

set( WRAPFOLDER ${PROJECT_NAME} )

add_library( ${PROJECT_NAME} 
    ${WRAPFOLDER}/exceptions/runtimeerror.cpp 
    ${WRAPFOLDER}/exceptions/runtimeerror.h
    ${WRAPFOLDER}/image/imagetypes.h
	${WRAPFOLDER}/image/imageutility.h
	${WRAPFOLDER}/image/imageutility.cpp
    ${WRAPFOLDER}/utility/boundingbox.cpp 
    ${WRAPFOLDER}/utility/boundingbox.h 
    ${WRAPFOLDER}/utility/boundinginterval.cpp 
    ${WRAPFOLDER}/utility/boundinginterval.h 
    ${WRAPFOLDER}/utility/casts.h 
    ${WRAPFOLDER}/utility/intcoord.cpp 
    ${WRAPFOLDER}/utility/intcoord.h 
    ${WRAPFOLDER}/utility/linesegment.h 
    ${WRAPFOLDER}/utility/linesegment.cpp 
    ${WRAPFOLDER}/utility/mathutility.cpp 
    ${WRAPFOLDER}/utility/mathutility.h 
    ${WRAPFOLDER}/utility/twodarray.h 
    ${WRAPFOLDER}/utility/vector2.cpp 
    ${WRAPFOLDER}/utility/vector2.h 
    ${WRAPFOLDER}/utility/vector3.cpp 
    ${WRAPFOLDER}/utility/vector3.h 
    ${WRAPFOLDER}/utility/vector4.cpp 
    ${WRAPFOLDER}/utility/vector4.h 
)

target_compile_features( ${PROJECT_NAME} PUBLIC cxx_std_17 )

target_include_directories( ${PROJECT_NAME}
    PUBLIC	${PROJECT_SOURCE_DIR}
	PRIVATE ${PROJECT_SOURCE_DIR}/Core
)

find_package( Boost REQUIRED )

target_link_libraries( ${PROJECT_NAME} 
	PUBLIC Boost::boost
)