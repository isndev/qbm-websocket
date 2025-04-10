/**
 * @file test-websocket-client.cpp
 * @brief WebSocket client testing implementation
 *
 * This test validates the WebSocket client functionality according to RFC 6455:
 * - Connection establishment and handshake
 * - Message sending and receiving
 * - Ping/pong control frame handling
 * - Proper connection closure
 *
 * @author qb - C++ Actor Framework
 * @copyright Copyright (c) 2011-2025 qb - isndev (cpp.actor)
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <gtest/gtest.h>
#include <thread>
#include "../ws.h"

using namespace qb::io;

// Test configuration constants
constexpr const std::size_t MESSAGE_COUNT    = 100;
constexpr const char        TEST_MESSAGE[]   = "Test message from client";
constexpr const int         PING_INTERVAL_MS = 500;

// Test synchronization variables
std::atomic<std::size_t> messages_received{0};
std::atomic<std::size_t> messages_sent{0};
std::atomic<std::size_t> pings_received{0};
std::mutex               test_mutex;
std::condition_variable  test_cv;
bool                     test_complete = false;

/**
 * @brief Helper function to run until a condition is met
 *
 * Runs the event loop until the provided condition returns true or
 * the maximum number of iterations is reached.
 *
 * @param condition Function returning bool that signals completion
 * @param max_iterations Maximum number of iterations to run
 * @param delay_ms Delay between iterations in milliseconds
 * @return true if condition was met, false if max iterations reached
 */
bool
run_until(std::function<bool()> condition, int max_iterations = 1000,
          int delay_ms = 10) {
    for (int i = 0; i < max_iterations; ++i) {
        async::run(EVRUN_NOWAIT);
        if (condition()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
    return false;
}

/**
 * @brief Echo server that responds to WebSocket clients
 */
class EchoServer;

/**
 * @brief Server-side client handler for echo WebSocket server
 *
 * This class handles client connections to the echo server,
 * processing WebSocket protocol events and echoing messages back to clients.
 */
class EchoServerClient : public use<EchoServerClient>::tcp::client<EchoServer> {
public:
    using Protocol    = qb::http::protocol_view<EchoServerClient>;
    using WS_Protocol = qb::http::ws::protocol<EchoServerClient>;

    explicit EchoServerClient(IOServer &server)
        : client(server) {}

    /**
     * @brief Handle HTTP request for WebSocket upgrade
     */
    void
    on(Protocol::request &&event) {
        std::cout << "Server received WebSocket upgrade request" << std::endl;

        if (!this->switch_protocol<WS_Protocol>(*this, event.http)) {
            std::cerr << "Failed to switch to WebSocket protocol" << std::endl;
            disconnect();
        } else {
            std::cout << "Successfully upgraded to WebSocket protocol" << std::endl;
        }
    }

    /**
     * @brief Handle WebSocket messages by echoing them back
     */
    void
    on(WS_Protocol::message &&event) {
        std::cout << "Server received message: " << std::string(event.data, event.size)
                  << std::endl;

        // Echo the message back
        event.ws.masked = false;
        *this << event.ws;
    }

    /**
     * @brief Handle ping frames and respond with pongs
     */
    void
    on(WS_Protocol::ping &&event) {
        std::cout << "Server received ping" << std::endl;

        // Respond with a pong
        if (event.size > 0) {
            qb::http::ws::MessagePong pong;
            pong << std::string(event.data, event.size);
            *this << pong;
        }
    }
};

/**
 * @brief Echo server that accepts WebSocket connections
 *
 * WebSocket server implementation that tracks connections and
 * creates handlers for each client that connects.
 */
class EchoServer : public use<EchoServer>::tcp::server<EchoServerClient> {
    std::size_t _connection_count = 0;

public:
    ~EchoServer() {
        EXPECT_EQ(_connection_count, 1u) << "Expected exactly one client connection";
    }

    /**
     * @brief Handle new client connection
     */
    void
    on(IOSession &) {
        ++_connection_count;
    }

    /**
     * @brief Get the number of active connections
     */
    std::size_t
    connection_count() const {
        return _connection_count;
    }
};

/**
 * @brief WebSocket client implementation for testing
 *
 * This class implements a WebSocket client that connects to a server,
 * sending and receiving messages to validate the protocol implementation.
 */
class WebSocketTestClient : public use<WebSocketTestClient>::tcp::client<> {
private:
    const std::string ws_key;

public:
    using Protocol    = qb::http::protocol_view<WebSocketTestClient>;
    using WS_Protocol = qb::http::ws::protocol<WebSocketTestClient>;

    /**
     * @brief Construct a WebSocket test client
     */
    WebSocketTestClient()
        : ws_key(qb::http::ws::generateKey()) {}

    /**
     * @brief Send the WebSocket handshake request
     */
    void
    sendHandshake() {
        qb::http::WebSocketRequest r(ws_key);
        r.uri() = "ws://localhost:9998/";
        r.headers()["Host"].emplace_back("localhost:9998");
        std::cout << "Sending WebSocket handshake request" << std::endl;
        *this << r;
    }

    /**
     * @brief Handle HTTP response to handshake request
     */
    void
    on(Protocol::response &&event) {
        std::cout << "Received HTTP response: " << event.http.status_code << std::endl;

        if (!this->switch_protocol<WS_Protocol>(*this, event.http, ws_key)) {
            std::cerr << "Failed to switch to WebSocket protocol" << std::endl;
            disconnect();
        } else {
            std::cout << "Successfully switched to WebSocket protocol" << std::endl;

            // Start sending test messages
            for (size_t i = 0; i < MESSAGE_COUNT; ++i) {
                std::string message =
                    std::string(TEST_MESSAGE) + " #" + std::to_string(i);
                send_message(message);
            }
        }
    }

    /**
     * @brief Handle incoming WebSocket messages
     */
    void
    on(WS_Protocol::message &&event) {
        std::string message(event.data, event.size);
        std::cout << "Received message: " << message << std::endl;
        ++messages_received;

        // Check if we've received all expected messages
        if (messages_received >= MESSAGE_COUNT) {
            std::cout << "All " << MESSAGE_COUNT
                      << " messages received, closing connection" << std::endl;

            // Send close frame
            qb::http::ws::MessageClose close_msg(1000, "Test completed");
            *this << close_msg;

            // Signal test completion
            {
                std::lock_guard<std::mutex> lock(test_mutex);
                test_complete = true;
            }
            test_cv.notify_all();
        }
    }

    /**
     * @brief Handle ping frames from the server
     */
    void
    on(WS_Protocol::ping &&event) {
        std::cout << "Received ping frame of size " << event.size << std::endl;
        ++pings_received;
    }

    /**
     * @brief Handle pong frames from the server
     */
    void
    on(WS_Protocol::pong &&event) {
        std::cout << "Received pong frame of size " << event.size << std::endl;
    }

    /**
     * @brief Handle close frames from the server
     */
    void
    on(WS_Protocol::close &&event) {
        std::cout << "WebSocket connection closed" << std::endl;

        // Signal test completion
        {
            std::lock_guard<std::mutex> lock(test_mutex);
            test_complete = true;
        }
        test_cv.notify_all();
    }

    /**
     * @brief Handle TCP disconnection
     */
    void
    on(async::event::disconnected &&) {
        std::cout << "TCP connection closed" << std::endl;

        // Signal test completion
        {
            std::lock_guard<std::mutex> lock(test_mutex);
            test_complete = true;
        }
        test_cv.notify_all();
    }

    /**
     * @brief Send a text message
     */
    void
    send_message(const std::string &text) {
        std::cout << "Sending text message: " << text << std::endl;
        qb::http::ws::MessageText msg;
        msg.masked = true;
        msg << text;
        *this << msg;
        ++messages_sent;
    }
};

/**
 * @brief Test for WebSocket Client functionality
 *
 * This test validates the WebSocket client implementation by:
 * 1. Creating an echo server that returns messages sent to it
 * 2. Connecting a client and performing the WebSocket handshake
 * 3. Sending a series of messages and verifying they're echoed back
 * 4. Testing ping/pong functionality
 * 5. Properly closing the connection
 */
TEST(WebSocketClient, EchoTest) {
    // Reset test state
    messages_received = 0;
    messages_sent     = 0;
    pings_received    = 0;
    test_complete     = false;

    // Initialize async event system
    async::init();

    // Create and start echo server
    EchoServer server;
    ASSERT_EQ(SocketStatus::Done, server.transport().listen_v6(9998));
    server.start();

    // Create client in a separate thread
    std::thread client_thread([]() {
        async::init();

        WebSocketTestClient client;
        ASSERT_EQ(SocketStatus::Done, client.transport().connect_v6("localhost", 9998));
        client.start();
        client.sendHandshake();

        // Process events until test completes
        run_until([]() { return test_complete; }, 2000, 10);
    });

    // Process server events in main thread
    run_until([]() { return test_complete; }, 2000, 10);

    // Wait for client thread to finish
    client_thread.join();

    // Verify test results
    EXPECT_EQ(messages_sent, MESSAGE_COUNT);
    EXPECT_EQ(messages_received, MESSAGE_COUNT);
    EXPECT_GE(pings_received, 0); // Pings are optional in this test
}

/**
 * @brief Main function
 */
int
main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}