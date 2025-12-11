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

project(qbm-websocket)

# Check if qb framework is available
if(NOT QB_FOUND)
    qb_status_message("qb framework not found, skipping WebSocket module")
    return()
endif()

# Check if SSL support is available
if(NOT QB_HAS_SSL)
    qb_status_message("WebSocket module requires SSL support - skipping")
    return()
endif()

# Register the WebSocket module using the new framework functions
qb_register_module(
    NAME websocket
    VERSION 1.0.0
    DEPENDS qbm-http
    SOURCES
        ws.cpp
    DEFINES
        QBM_WEBSOCKET_VERSION=1.0.0
)

# Add tests if enabled
if(QB_BUILD_TESTS)
    add_subdirectory(tests)
endif()
