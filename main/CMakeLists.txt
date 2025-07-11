idf_component_register(
                    SRCS    "main.c"
                    INCLUDE_DIRS ""
                    )

# set(GWINFO "${CMAKE_CURRENT_SOURCE_DIR}/../gwinfo")
# if(EXISTS ${WEB_SRC_DIR}/dist)
#     spiffs_create_partition_image(phy_init ${GWINFO}/dist FLASH_IN_PROJECT)
# else()
#     message(FATAL_ERROR "${WEB_SRC_DIR}/dist doesn't exit. Please run 'npm run build' in ${WEB_SRC_DIR}")
# endif()