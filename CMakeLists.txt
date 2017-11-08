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

option(USE_LIBNFC "Enable or disable the 'libnfc' usage." ON)
option(LIBNFC_POLL_ALL "Enable all modulation when polling." OFF)
option(LIBNFC_POLL_NMT_ISO14443A "Enable ISO-14443A modulation when polling." ON)
option(LIBNFC_POLL_NMT_ISOJEWEL "Enable ISO-JEWEL modulation when polling." OFF)
option(LIBNFC_POLL_NMT_ISO14443B "Enable ISO-14443B modulation when polling." OFF)
option(LIBNFC_POLL_NMT_ISO14443BI "Enable ISO-14443BI modulation when polling." OFF)
option(LIBNFC_POLL_NMT_ISO14443B2SR "Enable ISO-14443B2SR modulation when polling." OFF)
option(LIBNFC_POLL_NMT_ISO14443B2CT "Enable ISO-14443B2CT modulation when polling." OFF)
option(LIBNFC_POLL_NMT_FELICA "Enable Felica modulation when polling." OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/conf.d/cmake/config.cmake)

install(DIRECTORY htdocs/nfc DESTINATION htdocs)
install(DIRECTORY etc DESTINATION ./)

