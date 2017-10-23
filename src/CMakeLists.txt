###########################################################################
# Copyright 2015, 2016, 2017 IoT.bzh
#
# author: Loïc Collignon <loic.collignon@iot.bzh>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###########################################################################

CMAKE_MINIMUM_REQUIRED(VERSION 3.3)

PROJECT_TARGET_ADD(nfc-binding)

set(NFC_BINDING_SOURCES api.c nfc-binding.c)

if (USE_LIBNFC)
	set(NFC_BINDING_SOURCES ${NFC_BINDING_SOURCES} libnfc_reader.c)
	add_definitions(-DUSE_LIBNFC=1)
endif()
message(STATUS "libnfc enabled: ${USE_LIBNFC}")

add_library(${TARGET_NAME} MODULE ${NFC_BINDING_SOURCES})
target_link_libraries(${TARGET_NAME} ${link_libraries})

add_custom_command(TARGET ${TARGET_NAME}
	PRE_BUILD
	COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/../package/htdocs
	COMMAND cp -rv ${CMAKE_CURRENT_SOURCE_DIR}/../htdocs ${CMAKE_CURRENT_BINARY_DIR}/../package/)

SET_TARGET_PROPERTIES(${TARGET_NAME} PROPERTIES
	PREFIX "afb-"
	LABELS "BINDING"
	LINK_FLAGS ${BINDINGS_LINK_FLAG}
	OUTPUT_NAME ${TARGET_NAME})
	
