#
# qb - C++ Actor Framework
# Copyright (c) 2011-2025 qb - isndev (cpp.actor). All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
#         limitations under the License.
#

cmake_minimum_required(VERSION 3.10)
project(qbm-websocket)

# Force OpenSSL to be enabled
set(QB_IO_WITH_SSL ON CACHE BOOL "Enable SSL support" FORCE)
set(OPENSSL_FOUND TRUE CACHE BOOL "Force OpenSSL to be found" FORCE)

if (OPENSSL_FOUND)
    qb_register_module(
            NAME websocket
            VERSION 1.0.0
            DEPENDENCIES qbm-http
            SOURCES ws.cpp
    )
    if (QB_BUILD_TEST)
        add_subdirectory(tests)
    endif ()
else ()
    message(INFO: qbm-websocket will not be loaded, missing openssl)
endif ()