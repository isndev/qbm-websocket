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


# Define test files and their corresponding test names
set(WEBSOCKET_TESTS
        session
        client
        robustness
        stress
        security
)

# Register each test
foreach (TEST ${WEBSOCKET_TESTS})
    qb_register_module_gtest(
            NAME websocket
            TESTNAME ${TEST}
            SOURCES test-${TEST}.cpp
    )
endforeach ()

# Copy SSL resources if needed
if (OPENSSL_FOUND AND QB_SSL_RESOURCES)
    set(TEST_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/qbm-websocket/tests")
    file(MAKE_DIRECTORY ${TEST_OUTPUT_DIR})
    file(COPY ${QB_SSL_RESOURCES} DESTINATION ${TEST_OUTPUT_DIR})
    message(STATUS "[WebSocket Tests] Copying SSL resources to ${TEST_OUTPUT_DIR}")
endif ()