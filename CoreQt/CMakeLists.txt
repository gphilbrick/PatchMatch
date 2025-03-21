project( CoreQt )

set( WRAPFOLDER ${PROJECT_NAME} )

add_library( ${PROJECT_NAME}
    ${WRAPFOLDER}/imageeditlabel.cpp 
    ${WRAPFOLDER}/imageeditlabel.h 
	${WRAPFOLDER}/qdatastreamutility.h
	${WRAPFOLDER}/qdatastreamutility.cpp
    ${WRAPFOLDER}/qtinterfaceutility.cpp
    ${WRAPFOLDER}/qtinterfaceutility.h
)
	
find_package( Qt6 COMPONENTS Widgets )
if( NOT Qt6_FOUND )
    find_package( Qt5 5.15 REQUIRED COMPONENTS Widgets )
endif()	
set_target_properties( ${PROJECT_NAME} PROPERTIES AUTOMOC ON AUTOUIC ON )
	
find_package( OpenMP REQUIRED )

target_link_libraries( ${PROJECT_NAME} 
	PUBLIC 
		Core 
		Qt::Widgets 
		OpenMP::OpenMP_CXX
)

target_include_directories( ${PROJECT_NAME}
	PUBLIC ${PROJECT_SOURCE_DIR}
	PRIVATE ${PROJECT_SOURCE_DIR}/${WRAPFOLDER}
)
