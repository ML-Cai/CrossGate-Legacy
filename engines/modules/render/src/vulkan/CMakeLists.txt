include(${CMAKE_CURRENT_LIST_DIR}/../../../cmake/VulkanTools.cmake)


#-------------------------------------------------------------------------------
# [add shader]
#-------------------------------------------------------------------------------
add_render("backend"
           DEP_TARGET ${LIBRARY_NAME}
           PATH "backend")
