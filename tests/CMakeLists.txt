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
include(CTest)

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
    qb_register_module_test(
        MODULE_NAME websocket
        TEST_NAME ${TEST}
        SOURCES test-${TEST}.cpp
    )
endforeach ()

# SSL resources are automatically copied by qb_setup_test_resources()
# which is called in qb/CMakeLists.txt. Tests will find them in their working directory.