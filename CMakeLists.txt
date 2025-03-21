cmake_minimum_required( VERSION 3.26 ) # for copy_directory_if_different

project( PatchMatch )

include( Functions.cmake )

add_subdirectory( Core )
add_subdirectory( CoreQt )
add_subdirectory( PatchMatch )
add_subdirectory( OpenCL )
add_subdirectory( View )
add_subdirectory( HoleFillApplication )
