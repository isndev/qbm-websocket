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

# SSL Certificate Generation for WebSocket Tests
if (OPENSSL_FOUND)
    # Le répertoire de sortie des tests est ${CMAKE_BINARY_DIR}/bin/qbm/${PROJECT_NAME}/tests
    set(TEST_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/qbm-websocket/tests")

    # Assurez-vous que le répertoire existe
    file(MAKE_DIRECTORY ${TEST_OUTPUT_DIR})

    # Define output paths for certificates directly in the test output directory
    set(SSL_CERT_OUTPUT "${TEST_OUTPUT_DIR}/cert.pem")
    set(SSL_KEY_OUTPUT "${TEST_OUTPUT_DIR}/key.pem")

    # Function to check if OpenSSL is available
    find_program(OPENSSL_EXECUTABLE openssl)

    if (OPENSSL_EXECUTABLE)
        # Check OpenSSL version to determine command options
        execute_process(
                COMMAND ${OPENSSL_EXECUTABLE} version -v
                OUTPUT_VARIABLE OPENSSL_VERSION_OUTPUT
                ERROR_VARIABLE OPENSSL_VERSION_ERROR
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" OPENSSL_VERSION_MATCH "${OPENSSL_VERSION_OUTPUT}")
        set(OPENSSL_VERSION_MAJOR ${CMAKE_MATCH_1})
        set(OPENSSL_VERSION_MINOR ${CMAKE_MATCH_2})
        set(OPENSSL_VERSION_PATCH ${CMAKE_MATCH_3})

        # For OpenSSL >= 1.1.1, use -addext option
        if ((OPENSSL_VERSION_MAJOR GREATER 1) OR
        (OPENSSL_VERSION_MAJOR EQUAL 1 AND OPENSSL_VERSION_MINOR GREATER 1) OR
        (OPENSSL_VERSION_MAJOR EQUAL 1 AND OPENSSL_VERSION_MINOR EQUAL 1 AND OPENSSL_VERSION_PATCH GREATER_EQUAL 1))
            set(OPENSSL_CERT_COMMAND
                    ${OPENSSL_EXECUTABLE} req -x509 -newkey rsa:2048 -keyout ${SSL_KEY_OUTPUT}
                    -out ${SSL_CERT_OUTPUT} -days 365 -nodes
                    -subj "/CN=localhost/O=QB Tests/C=US"
                    -addext "subjectAltName = DNS:localhost"
            )
        else ()
            # Older OpenSSL versions don't support -addext
            set(OPENSSL_CERT_COMMAND
                    ${OPENSSL_EXECUTABLE} req -x509 -newkey rsa:2048 -keyout ${SSL_KEY_OUTPUT}
                    -out ${SSL_CERT_OUTPUT} -days 365 -nodes
                    -subj "/CN=localhost/O=QB Tests/C=US"
            )
        endif ()

        # Define command to generate SSL certificates
        add_custom_command(
                OUTPUT ${SSL_CERT_OUTPUT} ${SSL_KEY_OUTPUT}
                COMMAND ${OPENSSL_CERT_COMMAND}
                COMMENT "Generating self-signed SSL certificate for WebSocket tests"
                VERBATIM
        )

        # Define custom target for certificate generation
        add_custom_target(generate_websocket_ssl_certs DEPENDS ${SSL_CERT_OUTPUT} ${SSL_KEY_OUTPUT})

        # Make all SSL-dependent tests depend on certificate generation
        foreach (TEST ${WEBSOCKET_TESTS})
            add_dependencies(qbm-websocket-gtest-${TEST} generate_websocket_ssl_certs)
        endforeach ()

        # Tests using SSL will look for certificates in their runtime directory
        message(STATUS "[WebSocket Tests] SSL certificate generation enabled (OpenSSL ${OPENSSL_VERSION_OUTPUT})")
        message(STATUS "[WebSocket Tests] Certificates will be placed in ${TEST_OUTPUT_DIR}")
    else ()
        message(STATUS "[WebSocket Tests] OpenSSL executable not found, skipping certificate generation")
        # If OpenSSL executable not found but we have the resources, copy them to the correct directory
        if (QB_SSL_RESOURCES)
            file(COPY ${QB_SSL_RESOURCES} DESTINATION ${TEST_OUTPUT_DIR})
            message(STATUS "[WebSocket Tests] Copying SSL resources to ${TEST_OUTPUT_DIR}")
        endif ()
    endif ()
else ()
    message(STATUS "[WebSocket Tests] OpenSSL not found, skipping SSL certificate generation")
endif ()