# qbm-websocket

## Overview
Official WebSocket module for the qb-io framework, providing a complete implementation compliant with RFC 6455. This module is designed as an extension of the qb-io framework to facilitate real-time bidirectional communication between clients and servers.

## Features
- **RFC 6455 Compliance** - Complete implementation of the WebSocket protocol according to RFC 6455 specification
- **Client and Server Implementations** - Support for both roles in a unified architecture
- **Message Types** - Support for text (UTF-8) and binary messages
- **Control Frames** - Built-in support for ping/pong and connection closure
- **Secure WebSockets** - Support for WSS (WebSocket Secure) over TLS/SSL
- **Event-driven Architecture** - Non-blocking, event-based API
- **Automatic Ping/Pong Mechanism** - Configurable keepalive functionality
- **High Performance** - Optimized for high-frequency exchange applications

## Requirements
- C++17 or higher
- OpenSSL for cryptographic operations and secure connections
- qb-io Framework (qb Actor Framework)
- qbm-http module for HTTP protocol handling (during handshake)

## Quick Start Guide

### Installation

1. Make sure OpenSSL development libraries are installed
2. Include the WebSocket module in your CMake project:
   ```cmake
   add_subdirectory(path/to/qbm/ws)
   target_link_libraries(your_project qbm-websocket)
   ```

### Simple WebSocket Client

Here is a minimal example to create a WebSocket client:

```cpp
#include <qb/io/async.h>
#include "qbm/ws/ws.h"

class SimpleClient {
public:
    // Define WebSocket client with this class as event handler
    qb::http::ws::WebSocket<SimpleClient> ws{*this};
    
    // Connect to WebSocket server
    void start() {
        qb::io::uri server_uri("ws://localhost:8080");
        ws.connect(server_uri);
        
        // Start the event loop
        qb::io::async::run();
    }
    
    // Handler for successful connection event
    void on(typename qb::http::ws::WebSocket<SimpleClient>::connected&&) {
        // Send a text message
        qb::http::ws::MessageText msg;
        msg << "Hello WebSocket Server!";
        ws << msg;
    }
    
    // Handler for received messages
    void on(typename qb::http::ws::WebSocket<SimpleClient>::message&& event) {
        std::string text(event.data, event.size);
        std::cout << "Message received: " << text << std::endl;
    }
};

int main() {
    qb::io::async::init();
    SimpleClient client;
    client.start();
    return 0;
}
```

### Simple WebSocket Server

Minimal example to create a WebSocket server:

```cpp
#include <qb/io/async.h>
#include "qbm/ws/ws.h"

// Forward declaration for circular resolution
class ClientHandler;

// Server implementation
class SimpleServer : public qb::io::use<SimpleServer>::tcp::server<ClientHandler> {
    std::size_t connection_count = 0;
public:
    void start(int port) {
        this->transport().listen(port);
        this->server::start();
        std::cout << "Server started on port " << port << std::endl;
        
        // Start the event loop
        qb::io::async::run();
    }
    
    // Called when a new client connects
    void on(qb::io::IOSession&) {
        ++connection_count;
        std::cout << "New connection - Total: " << connection_count << std::endl;
    }
};

// Server-side client handler
class ClientHandler : public qb::io::use<ClientHandler>::tcp::client<SimpleServer> {
public:
    using Protocol = qb::http::protocol_view<ClientHandler>;
    using WS_Protocol = qb::http::ws::protocol<ClientHandler>;
    
    // Constructor
    explicit ClientHandler(qb::io::IOServer& server)
        : qb::io::use<ClientHandler>::tcp::client<SimpleServer>(server) {}
    
    // Handling WebSocket upgrade request
    void on(Protocol::request&& event) {
        if (this->switch_protocol<WS_Protocol>(*this, event.http)) {
            // Protocol successfully upgraded
            qb::http::ws::MessageText welcome;
            welcome.masked = false; // Server->client, no masking needed
            welcome << "Welcome to the WebSocket server!";
            *this << welcome;
        } else {
            // Upgrade failed
            this->disconnect();
        }
    }
    
    // Handling WebSocket messages
    void on(WS_Protocol::message&& event) {
        std::string message(event.data, event.size);
        std::cout << "Message received: " << message << std::endl;
        
        // Reply with the same message
        event.ws.masked = false; // Server->client, no masking needed
        *this << event.ws;
    }
};

int main() {
    qb::io::async::init();
    SimpleServer server;
    server.start(8080);
    return 0;
}
```

## Detailed Guide

### Message Types

The module provides several message types for different WebSocket operations:

```cpp
// Text message (UTF-8)
qb::http::ws::MessageText text_msg;
text_msg << "Text content";

// Binary message
qb::http::ws::MessageBinary binary_msg;
binary_msg << std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04};
// Or directly with raw data
const char* binary_data = "\x01\x02\x03\x04";
binary_msg << std::string(binary_data, 4);

// Ping message to check if the recipient is still active
qb::http::ws::MessagePing ping_msg;
ping_msg << "ping payload"; // Optional payload

// Pong message in response to a ping
qb::http::ws::MessagePong pong_msg;
pong_msg << "same payload as the ping"; // Must contain the same payload as the ping

// Connection close message
qb::http::ws::MessageClose close_msg(1000, "Normal closure");
// Close status codes available in qb::http::ws::CloseStatus
```

### WebSocket Client in Detail

Based on the implementation in `test-client.cpp`, here is a more detailed example:

```cpp
class DetailedClient {
private:
    std::atomic<bool> connected{false};
    std::atomic<std::size_t> messages_received{0};

public:
    // Define WebSocket client with this class as event handler
    qb::http::ws::WebSocket<DetailedClient> ws_client{*this};
    
    // Constructor - Client configuration
    DetailedClient() {
        // Configure ping interval (ms)
        ws_client.set_ping_interval(30000); // 30 seconds
    }
    
    // Connection method
    void connect(const std::string& host, int port) {
        qb::io::uri uri("ws://" + host + ":" + std::to_string(port) + "/");
        std::cout << "Connecting to " << uri.str() << std::endl;
        ws_client.connect(uri);
    }
    
    // Send a text message
    void send_text(const std::string& text) {
        if (!connected) {
            std::cerr << "Not connected - unable to send" << std::endl;
            return;
        }
        
        qb::http::ws::MessageText msg;
        msg << text;
        ws_client << msg;
        std::cout << "Message sent: " << text << std::endl;
    }
    
    // Send a binary message
    void send_binary(const std::vector<uint8_t>& data) {
        if (!connected) {
            std::cerr << "Not connected - unable to send" << std::endl;
            return;
        }
        
        qb::http::ws::MessageBinary msg;
        // Convert vector to string for sending
        std::string binary_data(reinterpret_cast<const char*>(data.data()), data.size());
        msg << binary_data;
        ws_client << msg;
        std::cout << "Binary message sent: " << data.size() << " bytes" << std::endl;
    }
    
    // Clean connection closure
    void close(int code = 1000, const std::string& reason = "Normal closure") {
        if (!connected) {
            std::cerr << "Already disconnected" << std::endl;
            return;
        }
        
        qb::http::ws::MessageClose msg(code, reason);
        ws_client << msg;
        std::cout << "Close request sent: " << reason << std::endl;
    }
    
    // === WebSocket Event Handlers ===
    
    // WebSocket request customization event (optional)
    void on(typename qb::http::ws::WebSocket<DetailedClient>::sending_http_request&& event) {
        // You can customize the request before it's sent
        // For example, add additional headers
        event.request.headers()["CustomHeader"].emplace_back("CustomValue");
    }
    
    // Successful connection event
    void on(typename qb::http::ws::WebSocket<DetailedClient>::connected&&) {
        connected = true;
        std::cout << "WebSocket connection established" << std::endl;
    }
    
    // Connection error event
    void on(typename qb::http::ws::WebSocket<DetailedClient>::error&&) {
        std::cerr << "WebSocket connection error" << std::endl;
    }
    
    // Message reception event
    void on(typename qb::http::ws::WebSocket<DetailedClient>::message&& event) {
        // Determine if it's a text or binary message via the opcode
        bool is_text = (event.ws.fin_rsv_opcode & 0x0F) == 0x01;
        
        if (is_text) {
            std::string text(event.data, event.size);
            std::cout << "Text message received: " << text << std::endl;
        } else {
            std::cout << "Binary message received: " << event.size << " bytes" << std::endl;
        }
        
        ++messages_received;
    }
    
    // Ping reception event
    void on(typename qb::http::ws::WebSocket<DetailedClient>::ping&& event) {
        std::cout << "Ping received (" << event.size << " bytes)" << std::endl;
        // The module automatically responds with a pong
    }
    
    // Pong reception event
    void on(typename qb::http::ws::WebSocket<DetailedClient>::pong&& event) {
        std::cout << "Pong received (" << event.size << " bytes)" << std::endl;
    }
    
    // Connection closure event
    void on(typename qb::http::ws::WebSocket<DetailedClient>::closed&& event) {
        connected = false;
        
        // Parse the closure code and reason
        if (event.size >= 2) {
            int status_code = (static_cast<unsigned char>(event.data[0]) << 8) | 
                               static_cast<unsigned char>(event.data[1]);
            
            std::string reason;
            if (event.size > 2) {
                reason = std::string(event.data + 2, event.size - 2);
            }
            
            std::cout << "Connection closed: code=" << status_code 
                      << ", reason=" << reason << std::endl;
        } else {
            std::cout << "Connection closed" << std::endl;
        }
    }
    
    // TCP disconnection event
    void on(qb::io::async::event::disconnected&&) {
        connected = false;
        std::cout << "TCP disconnection" << std::endl;
    }
};
```

### WebSocket Server in Detail

Based on the echo server in the tests:

```cpp
// Forward declaration for circular resolution
class DetailedHandler;

// Server implementation
class DetailedServer : public qb::io::use<DetailedServer>::tcp::server<DetailedHandler> {
private:
    std::size_t connection_count = 0;
    std::atomic<std::size_t> message_count{0};
    
public:
    DetailedServer() {
        std::cout << "WebSocket server created" << std::endl;
    }
    
    ~DetailedServer() {
        std::cout << "WebSocket server destroyed" << std::endl;
        std::cout << "Final statistics:" << std::endl;
        std::cout << "- Connections: " << connection_count << std::endl;
        std::cout << "- Messages: " << message_count.load() << std::endl;
    }
    
    // Start the server on a specific port
    bool start(int port) {
        auto status = this->transport().listen_v6(port);
        if (status != qb::io::SocketStatus::Done) {
            std::cerr << "Error listening on port " << port << std::endl;
            return false;
        }
        
        this->server::start();
        std::cout << "WebSocket server started on port " << port << std::endl;
        return true;
    }
    
    // Called when a new client connects
    void on(qb::io::IOSession&) {
        ++connection_count;
        std::cout << "New connection - Total: " << connection_count << std::endl;
    }
    
    // Increment message counter
    void increment_message_count() {
        ++message_count;
    }
    
    // Get connection count
    std::size_t get_connection_count() const {
        return connection_count;
    }
};

// Server-side client handler
class DetailedHandler : public qb::io::use<DetailedHandler>::tcp::client<DetailedServer> {
private:
    std::string client_id;
    bool ws_connected = false;
    
public:
    using Protocol = qb::http::protocol_view<DetailedHandler>;
    using WS_Protocol = qb::http::ws::protocol<DetailedHandler>;
    
    // Constructor
    explicit DetailedHandler(qb::io::IOServer& server)
        : qb::io::use<DetailedHandler>::tcp::client<DetailedServer>(server) {
        // Generate unique client ID
        static std::atomic<int> next_id{0};
        client_id = "client_" + std::to_string(++next_id);
    }
    
    // Destructor - Cleanup verification
    ~DetailedHandler() {
        std::cout << "Client " << client_id << " disconnected" << std::endl;
    }
    
    // Handle HTTP request for WebSocket upgrade
    void on(Protocol::request&& event) {
        std::cout << "WebSocket upgrade request received from " << client_id << std::endl;
        std::cout << "URI: " << event.http.uri().path() << std::endl;
        
        // Check Sec-WebSocket-Key header
        std::string ws_key = event.http.header("Sec-WebSocket-Key");
        if (ws_key.empty()) {
            std::cerr << "Invalid WebSocket request - missing key" << std::endl;
            this->disconnect();
            return;
        }
        
        // Perform upgrade to WebSocket protocol
        if (!this->switch_protocol<WS_Protocol>(*this, event.http)) {
            std::cerr << "Failed to upgrade to WebSocket" << std::endl;
            this->disconnect();
            return;
        }
        
        ws_connected = true;
        std::cout << "Protocol successfully upgraded for " << client_id << std::endl;
        
        // Send welcome message
        qb::http::ws::MessageText welcome;
        welcome.masked = false; // Server->client, no masking needed
        welcome << "Welcome " << client_id << " to the WebSocket server!";
        *this << welcome;
    }
    
    // Handle WebSocket messages
    void on(WS_Protocol::message&& event) {
        bool is_text = (event.ws.fin_rsv_opcode & 0x0F) == 0x01;
        
        if (is_text) {
            std::string text(event.data, event.size);
            std::cout << client_id << " >> " << text << std::endl;
            
            // Process special commands
            if (text == "ping") {
                // Send ping to client
                qb::http::ws::MessagePing ping;
                ping << "ping payload";
                *this << ping;
                return;
            } else if (text == "close") {
                // Close connection properly
                qb::http::ws::MessageClose close(1000, "Closure requested");
                *this << close;
                return;
            }
        } else {
            std::cout << client_id << " >> [Binary message of " << event.size << " bytes]" << std::endl;
        }
        
        // Echo the message back to the client
        event.ws.masked = false; // Server->client, no masking needed
        *this << event.ws;
        
        // Update server statistics
        static_cast<DetailedServer&>(this->server()).increment_message_count();
    }
    
    // Handle pings
    void on(WS_Protocol::ping&& event) {
        std::cout << "Ping received from " << client_id << " (" << event.size << " bytes)" << std::endl;
        // Pong is sent automatically by the protocol
    }
    
    // Handle pongs
    void on(WS_Protocol::pong&& event) {
        std::cout << "Pong received from " << client_id << " (" << event.size << " bytes)" << std::endl;
    }
    
    // Handle connection closures
    void on(WS_Protocol::close&& event) {
        ws_connected = false;
        
        if (event.size >= 2) {
            int status_code = (static_cast<unsigned char>(event.data[0]) << 8) | 
                               static_cast<unsigned char>(event.data[1]);
            
            std::string reason;
            if (event.size > 2) {
                reason = std::string(event.data + 2, event.size - 2);
            }
            
            std::cout << client_id << " closed the connection: code=" << status_code 
                      << ", reason=" << reason << std::endl;
        } else {
            std::cout << client_id << " closed the connection" << std::endl;
        }
    }
};
```

## Secure WebSocket (WSS)

For secure WebSocket connections, use the `WebSocketSecure` template. This requires SSL certificates:

```cpp
class SecureClient {
public:
    // Secure WebSocket client with TLS/SSL
    qb::http::ws::WebSocketSecure<SecureClient> wss_client{*this};
    
    void connect_secure(const std::string& url) {
        qb::io::uri server_uri(url); // Use "wss://" instead of "ws://"
        wss_client.connect(server_uri);
    }
    
    // Event handlers identical to those of normal WebSocket client
    void on(typename qb::http::ws::WebSocketSecure<SecureClient>::connected&&) {
        std::cout << "Secure connection established" << std::endl;
    }
    
    void on(typename qb::http::ws::WebSocketSecure<SecureClient>::message&& event) {
        std::string text(event.data, event.size);
        std::cout << "Secure message received: " << text << std::endl;
    }
    
    // etc.
};
```

### SSL Certificates for Tests

To run tests with WSS, certificates are needed. The module automatically generates self-signed certificates during compilation for tests:

```cmake
add_custom_command(
    OUTPUT ${SSL_CERT_OUTPUT} ${SSL_KEY_OUTPUT}
    COMMAND ${OPENSSL_CERT_COMMAND}
    COMMENT "Generating self-signed SSL certificate for WebSocket tests"
    VERBATIM
)
```

## Advanced Features

### Message Fragmentation

The WebSocket protocol supports fragmentation of large messages. The module automatically handles the reception of fragmented messages, but you can also send fragmented messages:

```cpp
// Creating a fragmented text message
qb::http::ws::MessageText fragment1;
fragment1.fin_rsv_opcode = 0x01; // First fragment (no FIN bit)
fragment1 << "First part of the message";
client << fragment1;

qb::http::ws::MessageText fragment2;
fragment2.fin_rsv_opcode = 0x00; // Continuation fragment (no FIN bit)
fragment2 << " second part";
client << fragment2;

qb::http::ws::MessageText fragment3;
fragment3.fin_rsv_opcode = 0x80; // Last fragment (FIN bit)
fragment3 << " last part!";
client << fragment3;
```

### Integration with Other Protocols

The WebSocket module integrates well with other protocols of the qb Actor Framework:

```cpp
// Example of integration with JSON for a WebSocket client that transmits JSON data
class JsonWebSocketClient {
public:
    qb::http::ws::WebSocket<JsonWebSocketClient> ws{*this};
    
    void connect() {
        qb::io::uri uri("ws://localhost:8080/json");
        ws.connect(uri);
    }
    
    void send_json(const qb::json::value& json_data) {
        // Serialize JSON to string
        std::string json_str = qb::json::stringify(json_data);
        
        // Send as WebSocket text message
        qb::http::ws::MessageText msg;
        msg << json_str;
        ws << msg;
    }
    
    void on(typename qb::http::ws::WebSocket<JsonWebSocketClient>::message&& event) {
        // Assume the message is JSON
        std::string json_str(event.data, event.size);
        
        try {
            // Parse the JSON
            qb::json::value json_data = qb::json::parse(json_str);
            process_json(json_data);
        } catch(const std::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
        }
    }
    
    void process_json(const qb::json::value& json) {
        // Process JSON data
        // ...
    }
};
```

### Customization of Event Handlers

The qb-io framework uses C++ template concepts to check for the presence of event handlers. You can therefore declare only the handlers you need:

```cpp
// Minimal client version handling only messages and disconnection
class MinimalClient {
public:
    qb::http::ws::WebSocket<MinimalClient> ws{*this};
    
    // Only these events will be handled
    void on(typename qb::http::ws::WebSocket<MinimalClient>::message&& event) {
        std::string text(event.data, event.size);
        std::cout << "Message received: " << text << std::endl;
    }
    
    void on(qb::io::async::event::disconnected&&) {
        std::cout << "Disconnected" << std::endl;
    }
};
```

### Performance and Optimizations

For high-performance applications:

1. **Message Reuse** - Reuse message objects instead of creating new ones:
   ```cpp
   qb::http::ws::MessageText reusable_msg;
   
   void send_message(const std::string& text) {
       reusable_msg.reset();
       reusable_msg << text;
       ws << reusable_msg;
   }
   ```

2. **Binary Messages** - Use binary messages for non-textual data to avoid conversions:
   ```cpp
   qb::http::ws::MessageBinary binary_msg;
   binary_msg << binary_data;
   ```

3. **Event Loop Parameters** - Adjust event loop parameters:
   ```cpp
   qb::io::async::run(EVRUN_NOWAIT); // Don't block
   ```

## Tests

The module includes comprehensive tests in the `tests/` directory:

- `test-client.cpp` - Tests client functionality with an echo server
- `test-session.cpp` - Tests WebSocket sessions over TCP and SSL
- `test-robustness.cpp` - Tests robustness (error handling, malformed messages)
- `test-security.cpp` - Tests security features
- `test-stress.cpp` - Performance tests under load

To run the tests:
```bash
cd build
cmake --build . --target qbm-websocket-gtest-websocket-client
cmake --build . --target qbm-websocket-gtest-session
cmake --build . --target qbm-websocket-gtest-robustness
cmake --build . --target qbm-websocket-gtest-security
cmake --build . --target qbm-websocket-gtest-stress

# Run a specific test
./bin/qbm-websocket/tests/qbm-websocket-gtest-websocket-client
```

## Troubleshooting

### Messages Not Received
- Check that message masking is correctly configured (client→server: masked, server→client: unmasked)
- Check that the event loop is running

### WebSocket Upgrade Errors
- Check HTTP headers for upgrade
- Ensure the WebSocket key is correctly generated and validated

### Unexpectedly Closed Connections
- Check closure codes in event handlers
- Enable automatic pings to detect connection losses

## License
This module is licensed under the Apache License, Version 2.0.