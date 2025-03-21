project( View )

set( WRAPFOLDER ${PROJECT_NAME} )

add_library( ${PROJECT_NAME}
    ${WRAPFOLDER}/loadimagesdialog.h 
    ${WRAPFOLDER}/loadimagesdialog.cpp 
    ${WRAPFOLDER}/loadimagesdialog.ui 
	${WRAPFOLDER}/holefilllabel.h
	${WRAPFOLDER}/holefilllabel.cpp
	${WRAPFOLDER}/holefillwindow.h
	${WRAPFOLDER}/holefillwindow.cpp
	${WRAPFOLDER}/holefillwindow.ui	
    ${WRAPFOLDER}/opencldeviceselection.h 
    ${WRAPFOLDER}/opencldeviceselection.cpp 
    ${WRAPFOLDER}/selectopencldevicedialog.h 
    ${WRAPFOLDER}/selectopencldevicedialog.cpp
    ${WRAPFOLDER}/selectopencldevicedialog.ui 
)

target_include_directories( ${PROJECT_NAME} 
	PUBLIC ${PROJECT_SOURCE_DIR} 
	PRIVATE ${PROJECT_SOURCE_DIR}/${WRAPFOLDER} 
)

find_package( Qt6 COMPONENTS Widgets )
if( NOT Qt6_FOUND )
    find_package( Qt5 5.15 REQUIRED COMPONENTS Widgets )
endif()	
set_target_properties( ${PROJECT_NAME} PROPERTIES AUTOMOC ON AUTOUIC ON )

target_link_libraries( ${PROJECT_NAME} 
    PUBLIC 
        CoreQt
        PatchMatch
		Qt::Widgets
		OpenCL
)
