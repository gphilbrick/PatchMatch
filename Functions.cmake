# Attempt to find the indicated components (producing targets a la "Qt::${component1}",
# using first Qt6 and then falling back on Qt >=5.15.
#
# find_qt_components
#   COMPONENTS component1 [component2...]
function( find_qt_components )
	set( OPTIONS )
    set( KEYWORDS_ONEVAL )
    set( KEYWORDS_MULTIVAL COMPONENTS )
    cmake_parse_arguments( VAR "${OPTIONS}" "${KEYWORDS_ONEVAL}" "${KEYWORDS_MULTIVAL}" ${ARGN} )

	find_package( Qt6 COMPONENTS ${VAR_COMPONENTS} )
	if( NOT Qt6_FOUND )
		find_package( Qt5 5.15 REQUIRED COMPONENTS ${VAR_COMPONENTS} )
		if( NOT Qt5_FOUND )
			message( FATAL_ERROR "Could not locate Qt 5.15 or greater" )
		endif()
	endif()	
endfunction()

# Inspired by https://discourse.cmake.org/t/copy-resources-to-build-folder/1010/5
#
# ensure_uptodate_runtime_resources( 
#   TARGET target
#	RESOURCE_DIR dir 
#	RESOURCE_DIR_CONTENTS contents1 [contents2...]
#
# Make it so that 'target' is dependent on the up-to-date contents
# of 'dir' (which is relative to CMAKE_CURRENT_SOURCE_DIR) being
# copied to CMAKE_CURRENT_BINARY_DIR/${dir}. 'contents...'
# must specific all the files within 'dir' (relative to 'dir').
#
# The general usage is to make it so that the
# executable 'target' can load, using relative paths,
# up-to-date data within 'dir', _assuming 'target' is 
# executed from the IDE_ (this command does _not_ necessarily
# place runtime resources in the same directory as the executable, just
# in CMAKE_CURRENT_BINARY_DIR).
function( ensure_uptodate_runtime_resources )
	set( OPTIONS )
    set( KEYWORDS_ONEVAL TARGET RESOURCE_DIR )
    set( KEYWORDS_MULTIVAL RESOURCE_DIR_CONTENTS )
    cmake_parse_arguments( VAR "${OPTIONS}" "${KEYWORDS_ONEVAL}" "${KEYWORDS_MULTIVAL}" ${ARGN} )

	if( NOT DEFINED VAR_TARGET )
		message( FATAL_ERROR "TARGET must be defined" )
	endif()
	if( NOT DEFINED VAR_RESOURCE_DIR )
		message( FATAL_ERROR "RESOURCE_DIR must be defined" )
	endif()
	if( NOT DEFINED VAR_RESOURCE_DIR_CONTENTS )
		message( FATAL_ERROR "RESOURCE_DIR_CONTENTS must specify at least one file." )
	endif()

	# relative to CMAKE_CURRENT_SOURCE_DIR
	set( RELDIR_TO_COPY ${VAR_RESOURCE_DIR} )
	set( RELDIR_CONTENTS ${VAR_RESOURCE_DIR_CONTENTS} )

	# sanity check--do the individually named resource files actually exist in the source tree?
	foreach( RESOURCE_FILE ${RELDIR_CONTENTS} )
		set( FULL_PATH_IN_SOURCE_TREE ${CMAKE_CURRENT_SOURCE_DIR}/${RELDIR_TO_COPY}/${RESOURCE_FILE} )
		if( NOT EXISTS ${FULL_PATH_IN_SOURCE_TREE} )
			message( FATAL_ERROR "The indicated in-source-tree resource file ${FULL_PATH_IN_SOURCE_TREE} cannot be found." )
		endif()
	endforeach()
	
	# The resource files in the source tree, prepended with 'dir'.
	list( TRANSFORM RELDIR_CONTENTS
		PREPEND "${RELDIR_TO_COPY}/"
		OUTPUT_VARIABLE SOURCE_FILES_REL
	)
	
	# The resource files' full paths once copied to the binary directory.
	list( TRANSFORM SOURCE_FILES_REL
		PREPEND "${CMAKE_CURRENT_BINARY_DIR}/"
		OUTPUT_VARIABLE DEST_FILES_ABS
	)
	
	set( SOURCE_DIR_ABS ${CMAKE_CURRENT_SOURCE_DIR}/${RELDIR_TO_COPY} )
	set( DEST_DIR_ABS ${CMAKE_CURRENT_BINARY_DIR}/${RELDIR_TO_COPY} )
	add_custom_command( 
		OUTPUT ${DEST_FILES_ABS}
		COMMAND ${CMAKE_COMMAND}
			ARGS
				-E 
				copy_directory_if_different
				"${CMAKE_CURRENT_SOURCE_DIR}/${RELDIR_TO_COPY}" # "" in case of spaces in paths
				"${CMAKE_CURRENT_BINARY_DIR}/${RELDIR_TO_COPY}" #..
		COMMENT "Copying directory ${SOURCE_DIR_ABS} to ${DEST_DIR_ABS} to keep runtime resources for ${VAR_TARGET} up to date."
		COMMAND_EXPAND_LISTS
		DEPENDS ${SOURCE_FILES_REL}
	)
	set( INTERMEDIATE_TARGET ${VAR_TARGET}-RuntimeResources )
	add_custom_target( ${INTERMEDIATE_TARGET}
		DEPENDS ${DEST_FILES_ABS}
	)
	add_dependencies( ${VAR_TARGET} ${INTERMEDIATE_TARGET} )
endfunction()