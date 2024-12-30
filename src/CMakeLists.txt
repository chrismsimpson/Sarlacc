# Get all project dir
FILE(GLOB projects ${CMAKE_CURRENT_SOURCE_DIR}/*)

# For each project dir, build a target
FOREACH(project ${projects})
    IF(IS_DIRECTORY ${project})
        # Get project name and all sources
        get_filename_component(project-name ${project} NAME)
        FILE(GLOB ${project}-src ${project}/*.cpp)

        include_directories(../lib/sarlacc)

        # Create executable and link target
        add_executable(${project-name} ${${project}-src})

        target_link_libraries(${project-name} Metal Sarlacc)

        message(STATUS "Adding ${project-name}")
    ENDIF()
ENDFOREACH()
