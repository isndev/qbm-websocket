# QB WebSocket Module (`qbm-ws`)

**RFC 6455 Compliant WebSocket Implementation for the QB Actor Framework**

<p align="center">
  <img src="https://img.shields.io/badge/WebSocket-RFC%206455-blue.svg" alt="WebSocket"/>
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg" alt="C++17"/>
  <img src="https://img.shields.io/badge/Cross--Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg" alt="Cross Platform"/>
  <img src="https://img.shields.io/badge/Arch-x86__64%20%7C%20ARM64-lightgrey.svg" alt="Architecture"/>
  <img src="https://img.shields.io/badge/SSL-WSS-green.svg" alt="SSL/WSS"/>
  <img src="https://img.shields.io/badge/License-Apache%202.0-green.svg" alt="License"/>
</p>

This module extends the `qbm-http` module to provide production-ready WebSocket support, enabling real-time, bidirectional communication over persistent TCP connections. Built on QB's asynchronous I/O foundation, it delivers high-performance WebSocket capabilities without blocking your actors.

## Quick Integration with QB

### Adding to Your QB Project

```bash
# Add both HTTP and WebSocket modules as submodules
git submodule add https://github.com/isndev/qbm-http qbm/http
git submodule add https://github.com/isndev/qbm-ws qbm/ws
```

### CMake Setup

```cmake
# QB framework setup
add_subdirectory(qb)
include_directories(${QB_PATH}/include)

# Load QB modules (automatically discovers both modules)
qb_load_modules("${CMAKE_CURRENT_SOURCE_DIR}/qbm")

# Link against both HTTP and WebSocket modules
target_link_libraries(your_target PRIVATE qbm::http qbm::ws)
```

### Include and Use

```cpp
#include <http/http.h>   // Required for HTTP handshake
#include <ws/ws.h>       // WebSocket protocol implementation
```

## Why Choose `qbm-ws`?

**RFC 6455 Compliance**: Full implementation of the WebSocket protocol including handshake, framing, masking, and all message types.

**HTTP Integration**: Seamless upgrade from HTTP connections using the existing `qbm-http` infrastructure.

**High Performance**: Built on QB's non-blocking I/O for handling thousands of concurrent WebSocket connections.

**Security Ready**: Full support for secure WebSocket connections (WSS) with SSL/TLS encryption.

**Cross-Platform**: Same code runs on Linux, macOS, Windows (x86_64, ARM64) with identical performance.

## Quick Start: WebSocket Echo Server

```cpp
#include <http/http.h>
#include <ws/ws.h>
#include <qb/main.h>

class EchoServer : public qb::Actor, public qb::http::Server<> {
public:
    bool onInit() override {
        // Handle WebSocket upgrade requests
        router().get("/ws", [this](auto ctx) {
            auto& request = ctx->request();
            
            // Validate WebSocket headers
            if (request.header("upgrade") == "websocket" && 
                request.header("connection").find("upgrade") != std::string::npos) {
                
                // Upgrade to WebSocket protocol
                upgrade_to_websocket(ctx);
            } else {
                ctx->response().status() = qb::http::Status::BAD_REQUEST;
                ctx->response().body() = "WebSocket upgrade required";
                ctx->complete();
            }
        });
        
        router().compile();
        
        if (listen({"tcp://0.0.0.0:8080"})) {
            start();
            qb::io::cout() << "WebSocket server running on ws://localhost:8080/ws" << std::endl;
        }
        
        return true;
    }
    
private:
    void upgrade_to_websocket(std::shared_ptr<qb::http::Context<qb::http::DefaultSession>> ctx) {
        // Switch to WebSocket protocol
        ctx->switch_protocol<qb::http::ws::protocol>([this](auto ws_session) {
            // New WebSocket connection established
            qb::io::cout() << "New WebSocket connection established" << std::endl;
            
            // Register WebSocket event handlers
            ws_session->on_message([](const std::string& message) {
                qb::io::cout() << "Received: " << message << std::endl;
                
                // Echo the message back
                qb::http::ws::MessageText reply("Echo: " + message);
                ws_session->send(reply);
            });
            
            ws_session->on_close([](int code, const std::string& reason) {
                qb::io::cout() << "WebSocket closed: " << code << " - " << reason << std::endl;
            });
        });
    }
};

int main() {
    qb::Main engine;
    engine.addActor<EchoServer>(0);
    engine.start();
    return 0;
}
```

That's it! A complete WebSocket server that echoes messages back to clients.

## Real-World Examples

### Chat Room Server

```cpp
#include <http/http.h>
#include <ws/ws.h>
#include <qb/main.h>
#include <qb/json.h>
#include <unordered_set>

class ChatServer : public qb::Actor, public qb::http::Server<> {
    std::unordered_set<qb::http::ws::Session*> _connections;
    
public:
    bool onInit() override {
        // Serve static HTML page
        router().get("/", [](auto ctx) {
            ctx->response().body() = R"(
<!DOCTYPE html>
<html>
<head><title>QB Chat</title></head>
<body>
    <div id="messages"></div>
    <input type="text" id="messageInput" placeholder="Type a message...">
    <button onclick="sendMessage()">Send</button>
    
    <script>
        const ws = new WebSocket('ws://localhost:8080/chat');
        const messages = document.getElementById('messages');
        
        ws.onmessage = function(event) {
            const data = JSON.parse(event.data);
            messages.innerHTML += '<div>' + data.user + ': ' + data.message + '</div>';
        };
        
        function sendMessage() {
            const input = document.getElementById('messageInput');
            if (input.value) {
                ws.send(JSON.stringify({
                    type: 'message',
                    user: 'User',
                    message: input.value
                }));
                input.value = '';
            }
        }
        
        document.getElementById('messageInput').addEventListener('keypress', function(e) {
            if (e.key === 'Enter') sendMessage();
        });
    </script>
</body>
</html>
            )";
            ctx->complete();
        });
        
        // WebSocket chat endpoint
        router().get("/chat", [this](auto ctx) {
            upgrade_to_chat_websocket(ctx);
        });
        
        router().compile();
        
        if (listen({"tcp://0.0.0.0:8080"})) {
            start();
            qb::io::cout() << "Chat server running on http://localhost:8080" << std::endl;
        }
        
        return true;
    }
    
private:
    void upgrade_to_chat_websocket(std::shared_ptr<qb::http::Context<qb::http::DefaultSession>> ctx) {
        ctx->switch_protocol<qb::http::ws::protocol>([this](auto ws_session) {
            // Add to active connections
            _connections.insert(ws_session.get());
            qb::io::cout() << "New chat user connected. Total users: " << _connections.size() << std::endl;
            
            // Send welcome message
            qb::json welcome = {
                {"type", "system"},
                {"message", "Welcome to QB Chat!"},
                {"users", _connections.size()}
            };
            qb::http::ws::MessageText welcome_msg(welcome.dump());
            ws_session->send(welcome_msg);
            
            // Handle incoming messages
            ws_session->on_message([this, ws_session](const std::string& message) {
                try {
                    auto data = qb::json::parse(message);
                    
                    if (data["type"] == "message") {
                        // Broadcast message to all connected clients
                        qb::json broadcast = {
                            {"type", "message"},
                            {"user", data["user"]},
                            {"message", data["message"]},
                            {"timestamp", qb::time::now().to_string()}
                        };
                        
                        broadcast_to_all(broadcast.dump());
                    }
                } catch (const std::exception& e) {
                    qb::io::cout() << "Invalid message format: " << e.what() << std::endl;
                }
            });
            
            // Handle disconnection
            ws_session->on_close([this, ws_session](int code, const std::string& reason) {
                _connections.erase(ws_session.get());
                qb::io::cout() << "User disconnected. Remaining users: " << _connections.size() << std::endl;
                
                // Notify remaining users
                qb::json notification = {
                    {"type", "system"},
                    {"message", "A user has left the chat"},
                    {"users", _connections.size()}
                };
                broadcast_to_all(notification.dump());
            });
        });
    }
    
    void broadcast_to_all(const std::string& message) {
        qb::http::ws::MessageText msg(message);
        
        for (auto* connection : _connections) {
            try {
                connection->send(msg);
            } catch (const std::exception& e) {
                qb::io::cout() << "Failed to send to client: " << e.what() << std::endl;
            }
        }
    }
};

int main() {
    qb::Main engine;
    engine.addActor<ChatServer>(0);
    engine.start();
    return 0;
}
```

### Real-time Data Stream

```cpp
#include <http/http.h>
#include <ws/ws.h>
#include <qb/main.h>
#include <qb/json.h>

class DataStreamServer : public qb::Actor, public qb::http::Server<> {
    std::vector<qb::http::ws::Session*> _subscribers;
    qb::io::timer _data_timer;
    
public:
    bool onInit() override {
        // WebSocket endpoint for real-time data
        router().get("/stream", [this](auto ctx) {
            auto& request = ctx->request();
            
            if (is_websocket_request(request)) {
                upgrade_to_data_stream(ctx);
            } else {
                ctx->response().status() = qb::http::Status::BAD_REQUEST;
                ctx->response().body() = "WebSocket required";
                ctx->complete();
            }
        });
        
        router().compile();
        
        if (listen({"tcp://0.0.0.0:8080"})) {
            start();
            start_data_generation();
            qb::io::cout() << "Data stream server running on ws://localhost:8080/stream" << std::endl;
        }
        
        return true;
    }
    
private:
    bool is_websocket_request(const qb::http::Request& request) {
        return request.header("upgrade") == "websocket" && 
               request.header("connection").find("upgrade") != std::string::npos;
    }
    
    void upgrade_to_data_stream(std::shared_ptr<qb::http::Context<qb::http::DefaultSession>> ctx) {
        ctx->switch_protocol<qb::http::ws::protocol>([this](auto ws_session) {
            _subscribers.push_back(ws_session.get());
            qb::io::cout() << "New data subscriber. Total: " << _subscribers.size() << std::endl;
            
            // Send initial data
            send_current_stats(ws_session.get());
            
            ws_session->on_close([this, ws_session](int code, const std::string& reason) {
                auto it = std::find(_subscribers.begin(), _subscribers.end(), ws_session.get());
                if (it != _subscribers.end()) {
                    _subscribers.erase(it);
                    qb::io::cout() << "Subscriber disconnected. Remaining: " << _subscribers.size() << std::endl;
                }
            });
        });
    }
    
    void start_data_generation() {
        // Generate and broadcast data every second
        _data_timer.schedule(id(), 1000_ms, [this]() {
            generate_and_broadcast_data();
            start_data_generation(); // Schedule next update
        });
    }
    
    void generate_and_broadcast_data() {
        // Generate sample data
        qb::json data = {
            {"timestamp", qb::time::now().to_string()},
            {"cpu_usage", rand() % 100},
            {"memory_usage", rand() % 100},
            {"active_connections", _subscribers.size()},
            {"requests_per_second", rand() % 1000 + 100}
        };
        
        qb::http::ws::MessageText message(data.dump());
        
        // Broadcast to all subscribers
        auto it = _subscribers.begin();
        while (it != _subscribers.end()) {
            try {
                (*it)->send(message);
                ++it;
            } catch (const std::exception& e) {
                qb::io::cout() << "Removing disconnected subscriber" << std::endl;
                it = _subscribers.erase(it);
            }
        }
    }
    
    void send_current_stats(qb::http::ws::Session* session) {
        qb::json stats = {
            {"type", "welcome"},
            {"message", "Connected to real-time data stream"},
            {"update_interval", "1 second"}
        };
        
        qb::http::ws::MessageText welcome(stats.dump());
        session->send(welcome);
    }
};

int main() {
    qb::Main engine;
    engine.addActor<DataStreamServer>(0);
    engine.start();
    return 0;
}
```

### WebSocket Client

```cpp
#include <ws/ws.h>
#include <qb/main.h>
#include <qb/json.h>

class WebSocketClient : public qb::Actor {
    qb::http::ws::Client _client;
    
public:
    WebSocketClient() : _client("ws://localhost:8080/chat") {}
    
    bool onInit() override {
        // Connect to WebSocket server
        _client.connect([this](bool success, const std::string& error) {
            if (success) {
                qb::io::cout() << "Connected to WebSocket server!" << std::endl;
                setup_handlers();
                send_initial_message();
            } else {
                qb::io::cout() << "Connection failed: " << error << std::endl;
                kill();
            }
        });
        
        return true;
    }
    
private:
    void setup_handlers() {
        _client.on_message([this](const qb::http::ws::Message& message) {
            if (message.is_text()) {
                try {
                    auto data = qb::json::parse(message.get_text());
                    qb::io::cout() << "Received: " << data.dump(2) << std::endl;
                } catch (const std::exception& e) {
                    qb::io::cout() << "Received text: " << message.get_text() << std::endl;
                }
            }
        });
        
        _client.on_close([this](int code, const std::string& reason) {
            qb::io::cout() << "Connection closed: " << code << " - " << reason << std::endl;
            kill();
        });
        
        _client.on_error([this](const std::string& error) {
            qb::io::cout() << "WebSocket error: " << error << std::endl;
        });
    }
    
    void send_initial_message() {
        qb::json message = {
            {"type", "message"},
            {"user", "QB Client"},
            {"message", "Hello from QB WebSocket client!"}
        };
        
        qb::http::ws::MessageText text_msg(message.dump());
        _client.send(text_msg);
        
        // Schedule more messages
        schedule_periodic_messages();
    }
    
    void schedule_periodic_messages() {
        qb::io::async::callback([this]() {
            static int counter = 1;
            
            qb::json message = {
                {"type", "message"},
                {"user", "QB Client"},
                {"message", "Automated message #" + std::to_string(counter++)}
            };
            
            qb::http::ws::MessageText text_msg(message.dump());
            _client.send(text_msg);
            
            if (counter <= 5) {
                schedule_periodic_messages(); // Send 5 messages total
            } else {
                // Close connection after sending messages
                qb::io::async::callback([this]() {
                    _client.close();
                }, 2.0); // Wait 2 seconds before closing
            }
        }, 3.0); // Send every 3 seconds
    }
};

int main() {
    qb::Main engine;
    engine.addActor<WebSocketClient>(0);
    engine.start();
    return 0;
}
```

## Secure WebSockets (WSS)

```cpp
#include <http/http.h>
#include <ws/ws.h>
#include <qb/main.h>

class SecureWebSocketServer : public qb::Actor, public qb::http::ssl::Server<> {
public:
    SecureWebSocketServer(const std::string& cert_path, const std::string& key_path) 
        : _cert_path(cert_path), _key_path(key_path) {}
    
    bool onInit() override {
        // Configure SSL
        auto ssl_ctx = qb::io::ssl::create_server_context();
        if (!configure_ssl_context(ssl_ctx)) {
            return false;
        }
        
        // WebSocket endpoint
        router().get("/secure", [this](auto ctx) {
            upgrade_to_secure_websocket(ctx);
        });
        
        router().compile();
        
        if (listen({"https://0.0.0.0:8443"}, ssl_ctx)) {
            start();
            qb::io::cout() << "Secure WebSocket server running on wss://localhost:8443/secure" << std::endl;
        }
        
        return true;
    }
    
private:
    std::string _cert_path, _key_path;
    
    bool configure_ssl_context(SSL_CTX* ctx) {
        if (SSL_CTX_use_certificate_file(ctx, _cert_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
            qb::io::cout() << "Failed to load certificate" << std::endl;
            return false;
        }
        
        if (SSL_CTX_use_PrivateKey_file(ctx, _key_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
            qb::io::cout() << "Failed to load private key" << std::endl;
            return false;
        }
        
        return true;
    }
    
    void upgrade_to_secure_websocket(std::shared_ptr<qb::http::Context<qb::http::ssl::DefaultSession>> ctx) {
        ctx->switch_protocol<qb::http::ws::protocol>([](auto ws_session) {
            qb::io::cout() << "Secure WebSocket connection established" << std::endl;
            
            ws_session->on_message([](const std::string& message) {
                qb::io::cout() << "Secure message received: " << message << std::endl;
                
                // Echo back with security info
                qb::http::ws::MessageText reply("Secure echo: " + message);
                ws_session->send(reply);
            });
        });
    }
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <cert.pem> <key.pem>" << std::endl;
        return 1;
    }
    
    qb::Main engine;
    engine.addActor<SecureWebSocketServer>(0, argv[1], argv[2]);
    engine.start();
    return 0;
}
```

## Features

**RFC 6455 Compliance**: Complete WebSocket protocol implementation with proper handshake, framing, and message types.

**Message Types**: Support for text, binary, ping, pong, and close frames with automatic handling.

**Connection Management**: Automatic connection lifecycle management with proper cleanup.

**Security**: Full SSL/TLS support for secure WebSocket connections (WSS).

**Performance**: Built on QB's asynchronous I/O for handling thousands of concurrent connections.

**Error Handling**: Comprehensive error reporting and connection state management.

## Build Information

### Requirements
- **QB Framework**: This module requires the QB Actor Framework as its foundation
- **qbm-http**: WebSocket depends on HTTP for the initial handshake
- **C++17** compatible compiler
- **CMake 3.14+**

### Optional Dependencies
- **OpenSSL**: For secure WebSocket connections (WSS). Enable with `QB_IO_WITH_SSL=ON` when building QB

### Building with QB
When using the QB project template, simply add both modules as shown in the integration section above. The `qb_load_modules()` function will automatically handle the configuration.

### Manual Build (Advanced)
```cmake
# If building outside QB framework context
find_package(qb REQUIRED)
target_link_libraries(your_target PRIVATE qb::qbm-http qb::qbm-ws)
```

## Advanced Documentation

For comprehensive technical documentation, implementation details, and in-depth guides:

**📖 [Complete WebSocket Module Documentation](./readme/README.md)**

This detailed documentation covers:
- **[Core Concepts](./readme/concepts.md)** - WebSocket fundamentals, connection lifecycle, and actor integration
- **[Protocol Implementation](./readme/protocol.md)** - RFC 6455 compliance, framing, masking, and message types
- **[Handshake Process](./readme/handshake.md)** - HTTP upgrade mechanism, headers validation, and security considerations
- **[Usage Patterns](./readme/usage.md)** - Common implementation patterns, client/server examples, and best practices

The advanced documentation provides:
- **Protocol Details** - Complete RFC 6455 implementation specifics
- **Security Considerations** - WebSocket security, WSS setup, and origin validation
- **Performance Optimization** - Connection pooling, message batching, and memory management
- **Error Handling** - Connection failures, protocol errors, and recovery strategies
- **Integration Patterns** - Combining with HTTP routes, middleware, and authentication
- **Real-time Applications** - Chat systems, live data feeds, and notification services

## Documentation & Examples

For comprehensive examples and detailed usage patterns:

- **[QB Examples Repository](https://github.com/isndev/qb-examples):** Real-world WebSocket integration patterns
- **[Full Module Documentation](./readme/README.md):** Complete API reference and guides

**Example Categories:**
- Basic WebSocket servers and clients
- Real-time chat applications
- Live data streaming
- Secure WebSocket (WSS) setup
- Integration with HTTP servers
- Connection management patterns

## Contributing

We welcome contributions! Please see the main [QB Contributing Guidelines](https://github.com/isndev/qb/blob/master/CONTRIBUTING.md) for details.

## License

Licensed under the Apache License, Version 2.0. See [LICENSE](./LICENSE) for details.

---

**Part of the [QB Actor Framework](https://github.com/isndev/qb) ecosystem - Build the future of concurrent C++ applications.**