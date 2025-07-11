#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := gateway-esp32
EXTRA_COMPONENT_DIRS := $(abspath boards)
include $(IDF_PATH)/make/project.mk
