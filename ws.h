/*
 * qb - C++ Actor Framework
 * Copyright (c) 2011-2025 qb - isndev (cpp.actor). All rights reserved.
 *
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
 *         limitations under the License.
 */

/**
 * @file ws.h
 * @brief WebSocket Protocol Implementation for QB Actor Framework
 * 
 * This file implements a robust WebSocket protocol client and server foundation using 
 * the actor model of the QB framework. It provides:
 * 
 * - Full RFC 6455 WebSocket Protocol compliance
 * - Secure handshake with cryptographic verification
 * - Text and binary message support
 * - Ping/pong heartbeat mechanism
 * - Fragmented message handling
 * - Automatic masking as required by the specification
 * - Clean connection closure with status codes
 * 
 * The implementation builds on top of the HTTP module for initial handshake
 * and is designed to work with the asynchronous I/O facilities of the QB framework.
 * 
 * @see qb::http
 * @see qb::io::async
 */

#ifndef QB_MODULE_WS_H_
#define QB_MODULE_WS_H_

#ifndef QB_IO_WITH_SSL
#    error "websocket protocol requires OpenSSL crypto library"
#endif

#include "../http/http.h"
#include <qb/io/crypto.h>
#include <qb/io/async/tcp/connector.h>
#include <random>

namespace qb::http::ws {

/**
 * @brief WebSocket frame opcodes as defined in RFC 6455
 * 
 * These values define the type of WebSocket frame being sent or received.
 * The highest bit (0x80) indicates whether the frame is the final fragment.
 */
enum opcode : unsigned char { Text = 129, Binary = 130, Close = 136, Ping = 137, Pong = 138};

/**
 * @brief Base message class for WebSocket communication
 * 
 * Provides functionality common to all WebSocket message types,
 * including message content storage and frame type management.
 */
struct Message {
    unsigned char fin_rsv_opcode = 0; ///< Frame control byte (FIN, RSV1-3, and opcode)
    bool masked = true; ///< Whether the message should be masked
    qb::allocator::pipe<char> _data; ///< Message content buffer

    /**
     * @brief Get the size of the message content
     * 
     * @return std::size_t Size of the message data in bytes
     */
    [[nodiscard]] std::size_t
    size() const noexcept {
        return _data.size();
    }

    /**
     * @brief Append data to the message
     * 
     * @tparam T Type of data to append
     * @param data Data to add to the message
     * @return Message& Reference to this message for chaining
     */
    template <typename T>
    Message &
    operator<<(T const &data) {
        _data << data;
        return *this;
    }

    /**
     * @brief Reset the message to its initial state
     * 
     * Clears the control byte and message content.
     */
    void
    reset() {
        fin_rsv_opcode = 0;
        _data.reset();
    }
};

/**
 * @brief Text message class for WebSocket
 * 
 * Represents a text frame (UTF-8 encoded data) in the WebSocket protocol.
 */
struct MessageText : public Message {
    /**
     * @brief Constructs a text message
     * 
     * Sets the opcode to Text (0x81) to indicate a text frame.
     */
    MessageText() {
        fin_rsv_opcode = opcode::Text;
    }
};

/**
 * @brief Binary message class for WebSocket
 * 
 * Represents a binary frame in the WebSocket protocol.
 */
struct MessageBinary : public Message {
    /**
     * @brief Constructs a binary message
     * 
     * Sets the opcode to Binary (0x82) to indicate a binary frame.
     */
    MessageBinary() {
        fin_rsv_opcode = opcode::Binary;
    }
};

/**
 * @brief Ping message class for WebSocket
 * 
 * Used for WebSocket connection heartbeat and liveness checks.
 * According to the specification, a peer receiving a ping must respond with a pong.
 */
struct MessagePing : public Message {
    /**
     * @brief Constructs a ping message
     * 
     * Sets the opcode to Ping (0x89) and disables masking for server-originated pings.
     */
    MessagePing() {
        fin_rsv_opcode = opcode::Ping;
        masked = false;
    }
};

/**
 * @brief Pong message class for WebSocket
 * 
 * Used as a response to ping messages for connection liveness verification.
 */
struct MessagePong : public Message {
    /**
     * @brief Constructs a pong message
     * 
     * Sets the opcode to Pong (0x8A) to indicate a pong response.
     */
    MessagePong() {
        fin_rsv_opcode = opcode::Pong;
    }
};

/**
 * @brief Close status codes as defined in RFC 6455
 * 
 * These status codes indicate the reason for WebSocket connection closure.
 */
enum CloseStatus : int {
    Normal = 1000,            ///< Normal closure; the connection successfully completed whatever purpose it was created for
    GoingAway = 1001,         ///< Server/client is going away (e.g., server shutting down, browser navigating away)
    ProtocolError = 1002,     ///< Protocol error, terminating the connection
    DataNotAccepted = 1003,   ///< Received data of a type not accepted (e.g., binary vs text)
    zReserved1 = 1004,        ///< Reserved status code
    zReserved2 = 1005,        ///< Reserved status code - MUST NOT be set explicitly
    zReserved3 = 1006,        ///< Reserved status code - MUST NOT be set explicitly
    DataNotConsistent = 1007, ///< Message data is inconsistent (e.g., non-UTF-8 data in a text message)
    PolicyViolation = 1008,   ///< Message received violates policy
    MessageTooBig = 1009,     ///< Message too large to process
    MissingExtension = 1010,  ///< Client expected server to negotiate extensions but server didn't
    UnexpectedReason = 1011,  ///< Server encountered an unexpected condition preventing request fulfillment
    zReserved4 = 1012         ///< Reserved status code
};

/**
 * @brief Close message class for WebSocket
 * 
 * Used to initiate or confirm a WebSocket connection closure.
 * Includes a status code and optional reason text.
 */
struct MessageClose : Message {
    MessageClose() = delete;
    
    /**
     * @brief Constructs a close message with status and reason
     * 
     * @param status WebSocket close status code
     * @param reason Text description of the closure reason
     */
    explicit MessageClose(int status = CloseStatus::Normal,
                          std::string const &reason = "closed normally") {
        fin_rsv_opcode = opcode::Close;
        _data << static_cast<unsigned char>(status >> 8)
              << static_cast<unsigned char>(status % 256) << reason;
    }
};

/**
 * @brief Generate a cryptographically secure key for WebSocket handshake
 * 
 * Creates a Base64-encoded secure random key for establishing WebSocket connections.
 * 
 * @return std::string The generated WebSocket key
 */
std::string generateKey() noexcept;

} // namespace qb::http::ws

namespace qb::http {

/**
 * @brief HTTP request specialized for WebSocket upgrade
 * 
 * Extends the base HTTP request to include the headers necessary for
 * initiating a WebSocket connection according to RFC 6455.
 */
struct WebSocketRequest : public Request<> {
    WebSocketRequest() = delete;
    
    /**
     * @brief Constructs a WebSocket upgrade request
     * 
     * @param key The WebSocket key to use for the handshake
     */
    explicit WebSocketRequest(std::string const &key) {
        _headers["Upgrade"].emplace_back("websocket");
        _headers["Connection"].emplace_back("Upgrade");
        _headers["Sec-WebSocket-Key"].emplace_back(key);
        _headers["Sec-WebSocket-Version"].emplace_back("13");
    }
};
} // namespace qb::http

namespace qb::protocol {

namespace ws_internal {

/**
 * @brief Base protocol implementation for WebSocket
 * 
 * Handles WebSocket frame parsing, message assembly, and protocol
 * state management according to RFC 6455.
 * 
 * @tparam IO_ I/O handler type
 */
template <typename IO_>
class base : public io::async::AProtocol<IO_> {
    std::size_t _parsed = 0;
    std::size_t _expected_size = 0;
    unsigned char fin_rsv_opcode = 0;
    qb::http::ws::Message _message;

public:
    // shared events
    /**
     * @brief Close event structure
     * 
     * Passed to handlers when a WebSocket close frame is received.
     */
    struct close {
        const std::size_t size;
        const char *data;
        qb::http::ws::Message &ws;
    };
    
    /**
     * @brief Ping event structure
     * 
     * Passed to handlers when a WebSocket ping frame is received.
     */
    struct ping {
        const std::size_t size;
        const char *data;
        qb::http::ws::Message &ws;
    };
    
    /**
     * @brief Pong event structure
     * 
     * Passed to handlers when a WebSocket pong frame is received.
     */
    struct pong {
        const std::size_t size;
        const char *data;
        qb::http::ws::Message &ws;
    };

    /**
     * @brief Message event structure
     * 
     * Passed to handlers when a WebSocket data frame (text or binary) is received.
     */
    struct message {
        const std::size_t size;
        const char *data;
        qb::http::ws::Message &ws;
    };
    // !shared events

    base() = delete;
    
    /**
     * @brief Constructs a WebSocket protocol handler
     * 
     * @param io Reference to the I/O handler
     */
    explicit base(IO_ &io)
        : io::async::AProtocol<IO_>(io) {}

    /**
     * @brief Calculate the size of a complete WebSocket message
     * 
     * Parses WebSocket frame headers to determine the total message size.
     * Handles various payload length formats and masking.
     * 
     * @return std::size_t Size of the complete message, or 0 if incomplete
     */
    std::size_t
    getMessageSize() noexcept final {
        if (!this->ok())
            return 0;

        auto &buffer = this->_io.in();
        const auto buffer_size = buffer.size();
        auto first_bytes = reinterpret_cast<const unsigned char *>(buffer.cbegin());
        if (!_parsed) {
            if (buffer_size < 2u)
                return 0;
            fin_rsv_opcode = first_bytes[0];
            _message.masked = (first_bytes[1] >= 128u);
            // only server side
            if constexpr (IO_::has_server) {
                if (!_message.masked) {
                    // close if client has sent unmasked message
                    _message.reset();
                    int status = 1002;
                    _message.fin_rsv_opcode = 136u;
                    _message._data << static_cast<unsigned char>(status >> 8)
                                   << static_cast<unsigned char>(status % 256)
                                   << "message from client not masked";
                    this->_io << _message;
                    this->not_ok();
                    return 0u;
                }
            }

            _parsed += 2u;
        }
        if (!_expected_size) {
            std::size_t length = (first_bytes[1] & 127u);
            // 2 or 8 next bytes is the size of content
            std::size_t num_bytes = length == 126u ? 2u : (length == 127u ? 8u : 0u);
            if (num_bytes) {
                if (buffer_size < (num_bytes + 2u))
                    return 0u;
                // position after 2 firt bytes
                auto length_bytes =
                    reinterpret_cast<const unsigned char *>(buffer.cbegin() + 2u);
                length = 0u;
                for (std::size_t c = 0u; c < num_bytes; c++)
                    length += static_cast<std::size_t>(length_bytes[c])
                              << (8u * (num_bytes - 1u - c));
            }
            _expected_size = length;
            _parsed += num_bytes;
        }

        const auto full_size =
            _expected_size + _parsed + (first_bytes[1] >= 128u ? 4u : 0u);
        if (buffer_size < full_size)
            return 0;

        return full_size;
    }

    void
    onMessage(std::size_t) noexcept final {
        if (!this->ok())
            return;

        auto &buffer = this->_io.in();

        // If fragmented message
        if ((fin_rsv_opcode & 0x80u) == 0 || (fin_rsv_opcode & 0x0fu) == 0) {
            if (!_message.size()) {
                _message.fin_rsv_opcode = fin_rsv_opcode | 0x80u;
            }
        } else
            _message.fin_rsv_opcode = fin_rsv_opcode;

        if (_message.masked) {
            // Read mask
            auto mask =
                reinterpret_cast<const unsigned char *>(buffer.cbegin() + _parsed);
            auto begin_buffer_data = buffer.begin() + _parsed + 4;
            auto begin_data = _message._data.allocate_back(_expected_size);
            for (auto i = 0u; i < _expected_size; ++i)
                begin_data[i] = begin_buffer_data[i] ^ mask[i % 4];
        } else {
            std::memcpy(_message._data.allocate_back(_expected_size),
                        buffer.begin() + _parsed, _expected_size);
        }

        // reply in condition
        if constexpr (IO_::has_server)
            _message.masked = false;
        else
            _message.masked = true;

        // If connection close
        if ((fin_rsv_opcode & 0x0f) == 8) {
            this->_io.out().reset();
            if constexpr (has_method_on<IO_, void, close>::value) {
                this->_io.on(close{_message.size(), _message._data.cbegin(), _message});
            } else {
                _message.fin_rsv_opcode = 136u;
                this->_io << _message;
            }
            this->not_ok();
        }
        // If ping
        else if ((fin_rsv_opcode & 0x0f) == 9) {
            if constexpr (has_method_on<IO_, void, ping>::value) {
                this->_io.on(ping{_message.size(), _message._data.cbegin(), _message});
            }
            // Send pong
            _message.fin_rsv_opcode = fin_rsv_opcode + 1;
            this->_io << _message;
        }
        // If pong
        else if ((fin_rsv_opcode & 0x0f) == 10) {
            if constexpr (has_method_on<IO_, void, pong>::value) {
                this->_io.on(pong{_message.size(), _message._data.cbegin(), _message});
            }
        }
        // If fragmented message and not final fragment
        else if ((fin_rsv_opcode & 0x80) == 0) {
            // next message no reset
        } else {
            this->_io.on(message{_message.size(), _message._data.cbegin(), _message});
            // next message + reset
        }

        if ((fin_rsv_opcode & 0x80) != 0)
            _message.reset();
        _expected_size = _parsed = fin_rsv_opcode = 0;
    }

    void
    reset() noexcept final {
        _message.reset();
        _expected_size = _parsed = fin_rsv_opcode = 0;
    }
};

} // namespace ws_internal

/**
 * @brief WebSocket server protocol implementation
 * 
 * Handles server-side behavior during WebSocket connection establishment
 * and communication according to RFC 6455.
 * 
 * @tparam IO_ I/O handler type
 */
template <typename IO_>
class ws_server : public ws_internal::base<IO_> {
    std::string endpoint;

public:
    /**
     * @brief HTTP response sending event
     * 
     * Passed to handlers before sending the WebSocket upgrade response.
     * Allows customization of the response.
     */
    struct sending_http_response {
        qb::http::Response &response;
    };

    ws_server() = delete;
    
    /**
     * @brief Constructs a WebSocket server protocol handler
     * 
     * Processes the upgrade request and sends the appropriate response
     * with the calculated accept key according to RFC 6455.
     * 
     * @tparam HttpRequest Type of HTTP request
     * @param io Reference to the I/O handler
     * @param http HTTP request containing the WebSocket upgrade request
     */
    template <typename HttpRequest>
    ws_server(IO_ &io, HttpRequest const &http)
        : ws_internal::base<IO_>(io) {
        static auto ws_magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        if (http.upgrade) {
            std::string ws_key(http.header("Sec-WebSocket-Key"));
            if (!ws_key.empty()) {
                ws_key += ws_magic_string;
                qb::http::Response res;
                res.status_code = HTTP_STATUS_SWITCHING_PROTOCOLS;
                res.status = "Web Socket Protocol Handshake";
                res.headers()["Upgrade"].emplace_back("websocket");
                res.headers()["Connection"].emplace_back("Upgrade");
                res.headers()["Sec-WebSocket-Accept"].emplace_back(
                    crypto::base64::encode(crypto::sha1(ws_key)));

                if constexpr (has_method_on<IO_, void, sending_http_response>::value) {
                    this->_io.on(sending_http_response{res});
                }

                this->_io << res;
                endpoint = http.uri().path();
                return;
            }
            // error
        }
        this->not_ok();
    }
};

/**
 * @brief WebSocket client protocol implementation
 * 
 * Handles client-side behavior during WebSocket connection establishment
 * and communication according to RFC 6455.
 * 
 * @tparam IO_ I/O handler type
 */
template <typename IO_>
class ws_client : public ws_internal::base<IO_> {
public:
    ws_client() = delete;
    
    /**
     * @brief Constructs a WebSocket client protocol handler
     * 
     * Verifies the server's handshake response by checking the accept key
     * according to RFC 6455.
     * 
     * @tparam HttpResponse Type of HTTP response
     * @param io Reference to the I/O handler
     * @param http HTTP response from the server
     * @param key Original client key sent in the upgrade request
     */
    template <typename HttpResponse>
    ws_client(IO_ &io, HttpResponse const &http, std::string const &key)
        : ws_internal::base<IO_>(io) {
        static const auto ws_magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        if (http.upgrade) {
            if (http.status_code == HTTP_STATUS_SWITCHING_PROTOCOLS) {
                const auto &res_key = http.header("Sec-WebSocket-Accept");
                if (!res_key.empty()) {
                    if (crypto::base64::decode(std::string(res_key)) ==
                        crypto::sha1(key + ws_magic_string)) {
                        return;
                    }
                }
            }
            // error
        }
        this->not_ok();
    }
};

} // namespace qb::protocol

namespace qb::http::ws {

namespace internal {

template <typename IO_, bool has_server = IO_::has_server>
struct side {
    using protocol = qb::protocol::ws_server<IO_>;
};

template <typename IO_>
struct side<IO_, false> {
    using protocol = qb::protocol::ws_client<IO_>;
};

} // namespace internal

template <typename IO_>
using protocol = typename internal::side<IO_>::protocol;

/**
 * @brief WebSocket client connection class
 * 
 * Provides a complete client WebSocket implementation that handles connection,
 * protocol switching, message transmission, and heartbeat monitoring.
 * 
 * @tparam T Parent handler type that will receive WebSocket events
 * @tparam Transport Transport layer type (TCP or SSL)
 */
template <typename T, typename Transport = qb::io::transport::tcp>
class WebSocket
        : public qb::io::async::tcp::client<WebSocket<T, Transport>, Transport>
        , public qb::io::use<WebSocket<T, Transport>>::timeout {
    const std::string _ws_key;
    int _ping_interval;
    T &_parent;
    qb::io::uri _remote;
public:
    using http_protocol = qb::http::protocol_view<WebSocket<T, Transport>>;
    using ws_protocol = qb::http::ws::protocol<WebSocket<T, Transport>>;

    // public events
    /**
     * @brief HTTP request sending event
     * 
     * Passed to handlers before sending the WebSocket upgrade request.
     * Allows customization of the request.
     */
    struct sending_http_request {
        qb::http::WebSocketRequest &request;
    };
    
    /**
     * @brief Connection successful event
     * 
     * Signaled when the WebSocket connection is successfully established.
     */
    struct connected {};
    
    /**
     * @brief Connection error event
     * 
     * Signaled when an error occurs during connection establishment.
     */
    struct error {};
    
    // Re-export event types from the protocol
    using closed = typename ws_protocol::close;
    using ping = typename ws_protocol::ping;
    using pong = typename ws_protocol::pong;
    using message = typename ws_protocol::message;
    using disconnected = qb::io::async::event::disconnected;
    using timeout = qb::io::async::event::timeout;
public:

    /**
     * @brief Constructs a WebSocket client
     * 
     * Initializes the WebSocket client with a parent handler that will
     * receive the WebSocket events.
     * 
     * @param parent Reference to the parent handler
     */
    explicit WebSocket(T &parent)
            : _ws_key(qb::http::ws::generateKey())
            , _ping_interval(0)
            , _parent(parent) {
    }

    /**
     * @brief Set the ping interval for heartbeat
     * 
     * Configures the WebSocket to automatically send ping frames at the specified interval
     * to keep the connection alive and detect disconnections.
     * 
     * @param ping_interval Interval in milliseconds (0 to disable)
     */
    void set_ping_interval(int ping_interval = 0) {
        _ping_interval = ping_interval;
        this->setTimeout(ping_interval);
    }

    /**
     * @brief Connect to a WebSocket server
     * 
     * Initiates a WebSocket connection to the specified URI by first establishing
     * a TCP connection and then performing the WebSocket handshake.
     * 
     * @param remote URI of the WebSocket server
     * @param timeout Connection timeout in milliseconds (0 for no timeout)
     */
    void connect(qb::io::uri const &remote, int timeout = 0) {
        this->clear_protocols();
        this->setTimeout(0);
        _remote = remote;
        qb::io::async::tcp::connect<typename Transport::transport_io_type>(
                remote,
                [this](auto &transport) {
                    if (!transport.is_open()) {
                        if constexpr (has_method_on<T, void, error>::value) {
                            _parent.on(error{});
                        }
                    } else {
                        this->transport() = transport;
                        this->template switch_protocol<http_protocol>(*this);
                        this->start();

                        qb::http::WebSocketRequest request(_ws_key);
                        request.headers()["host"].emplace_back(std::string(_remote.host()));
                        request.uri() = _remote;

                        if constexpr (has_method_on<T, void, sending_http_request>::value) {
                            _parent.on(sending_http_request{request});
                        }

                        *this << request;
                    }
                }, timeout);
    }

    /**
     * @brief Handle HTTP response event
     * 
     * Called when the HTTP handshake response is received from the server.
     * Validates the response and completes the WebSocket connection establishment.
     * 
     * @param event HTTP response event containing the server's handshake response
     */
    void
    on(typename http_protocol::response &&event) {
        if (!this->template switch_protocol<ws_protocol>(*this, event.http, _ws_key)) {
            if constexpr (has_method_on<T, void, error>::value) {
                _parent.on(error{});
            }
            this->disconnect();
            return;
        }
        if constexpr (has_method_on<T, void, connected>::value) {
            _parent.on(connected{});
            this->setTimeout(_ping_interval);
        }
    }

    /**
     * @brief Handle ping event
     * 
     * Called when a ping frame is received from the server.
     * Forwards the event to the parent handler if it has a matching handler.
     * 
     * @param event Ping event containing the ping data
     */
    void
    on(ping &&event) {
        if constexpr (has_method_on<T, void, ping>::value) {
            _parent.on(std::forward<ping>(event));
        }
    }

    /**
     * @brief Handle pong event
     * 
     * Called when a pong frame is received from the server.
     * Forwards the event to the parent handler if it has a matching handler.
     * 
     * @param event Pong event containing the pong data
     */
    void
    on(pong &&event) {
        if constexpr (has_method_on<T, void, pong>::value) {
            _parent.on(std::forward<pong>(event));
        }
    }

    /**
     * @brief Handle message event
     * 
     * Called when a data frame (text or binary) is received from the server.
     * Forwards the event to the parent handler.
     * 
     * @param event Message event containing the received data
     */
    void
    on(message &&event) {
        _parent.on(std::forward<message>(event));
    }

    /**
     * @brief Handle close event
     * 
     * Called when a close frame is received from the server.
     * Forwards the event to the parent handler if it has a matching handler.
     * 
     * @param event Close event containing the close status and reason
     */
    void
    on(closed &&event) {
        if constexpr (has_method_on<T, void, closed>::value) {
            _parent.on(std::forward<closed>(event));
        }
    }

    /**
     * @brief Handle disconnection event
     * 
     * Called when the underlying transport connection is closed.
     * Forwards the event to the parent handler.
     * 
     * @param event Disconnection event containing the reason for disconnection
     */
    void
    on(disconnected &&event) {
        _parent.on(std::forward<disconnected>(event));
    }

    /**
     * @brief Handle timeout event
     * 
     * Called when the ping interval timer expires.
     * Sends a ping frame to the server and resets the timer.
     * 
     * @param event Timeout event
     */
    void
    on(timeout const &event) {
        MessagePing msg;
        *this << msg;
        this->setTimeout(_ping_interval);
    }

};

/**
 * @brief Secure WebSocket client connection class
 * 
 * Type alias for WebSocket using SSL/TLS transport for secure connections.
 * 
 * @tparam T Parent handler type that will receive WebSocket events
 */
template <typename T>
using WebSocketSecure = WebSocket<T, qb::io::transport::stcp>;

} // namespace qb::http::ws

namespace qb::allocator {

template <>
pipe<char> &pipe<char>::put<qb::http::ws::Message>(const qb::http::ws::Message &msg);

template <>
pipe<char> &
pipe<char>::put<qb::http::ws::MessagePing>(const qb::http::ws::MessagePing &msg);

template <>
pipe<char> &
pipe<char>::put<qb::http::ws::MessagePong>(const qb::http::ws::MessagePong &msg);

template <>
pipe<char> &
pipe<char>::put<qb::http::ws::MessageText>(const qb::http::ws::MessageText &msg);

template <>
pipe<char> &
pipe<char>::put<qb::http::ws::MessageBinary>(const qb::http::ws::MessageBinary &msg);

template <>
pipe<char> &
pipe<char>::put<qb::http::ws::MessageClose>(const qb::http::ws::MessageClose &msg);

template <>
pipe<char> &
pipe<char>::put<qb::http::WebSocketRequest>(const qb::http::WebSocketRequest &msg);

} // namespace qb::allocator

#endif // QB_MODULE_WS_H_
