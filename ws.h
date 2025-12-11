/**
 * @file ws.h
 * @brief WebSocket protocol implementation for the qb Actor Framework
 *
 * This module provides WebSocket capabilities conforming to RFC 6455 including:
 * - WebSocket client and server implementations
 * - Support for text and binary messages
 * - Handling of control frames (ping, pong, close)
 * - Built-in security mechanisms for frame validation
 * - Support for secure WebSockets over TLS/SSL
 *
 * The implementation relies on the HTTP module for the initial handshake
 * and requires OpenSSL for security features.
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

#pragma once

// WebSocket protocol requires OpenSSL crypto library
#ifndef QB_HAS_SSL
#error "websocket protocol requires OpenSSL crypto library"
#endif

#include <qb/io/async/tcp/connector.h>
#include <qb/io/crypto.h>
#include <random>
#include <functional>
#include <string_view>
#include "../http/http.h"

/**
 * @namespace qb::http::ws
 * @brief WebSocket protocol implementation within the HTTP namespace
 *
 * This namespace contains classes and functions for WebSocket communication,
 * including message types, connection handling, and protocol operations.
 */
namespace qb::http::ws {

/**
 * @brief Checks if a string view contains valid UTF-8 data.
 * @param sv The string_view to validate.
 * @return True if the data is valid UTF-8, false otherwise.
 */
bool is_utf8(std::string_view sv) noexcept;

/**
 * @enum opcode
 * @brief WebSocket frame opcodes as defined in RFC 6455
 *
 * These values represent the different types of WebSocket frames
 * with their corresponding opcode values (including FIN bit set).
 */
enum opcode : unsigned char {
    Continuation = 0, /**< Continuation frame (opcode 0x0) */
    _Text        = 1, /**< Text frame (opcode 0x1) */
    _Binary      = 2, /**< Binary frame (opcode 0x2) */
    _Close       = 8, /**< Close frame (opcode 0x8) */
    _Ping        = 9, /**< Ping frame (opcode 0x9) */
    _Pong        = 10,/**< Pong frame (opcode 0xA) */

    Text   = 129, /**< Text frame (0x81): FIN bit + opcode 0x1 */
    Binary = 130, /**< Binary frame (0x82): FIN bit + opcode 0x2 */
    Close  = 136, /**< Connection close frame (0x88): FIN bit + opcode 0x8 */
    Ping   = 137, /**< Ping frame (0x89): FIN bit + opcode 0x9 */
    Pong   = 138  /**< Pong frame (0x8A): FIN bit + opcode 0xA */
};

// Forward declaration
namespace qb {
namespace allocator {
template <typename T>
class pipe;
}
} // namespace qb

/**
 * @struct Message
 * @brief Base class for all WebSocket message types
 *
 * Provides core functionality for WebSocket messages, including
 * data storage, frame composition, and state management.
 */
struct Message {
    unsigned char fin_rsv_opcode =
        0; /**< Combined field for FIN bit, RSV bits, and opcode */
    bool masked =
        false; /**< Whether the message should be masked (required for client->server) */
    ::qb::allocator::pipe<char>
        _data; /**< Internal buffer storing the message payload */

    ::qb::allocator::pipe<char>&
    data() noexcept {
        return _data;
    } /**< Pointer to the message data buffer */

    /**
     * @brief Get the size of the message payload
     * @return Size of the message data in bytes
     */
    [[nodiscard]] std::size_t
    size() const noexcept {
        return _data.size();
    }

    /**
     * @brief Append data to the message payload
     * @tparam T Type of data to append
     * @param data The data to append to the message
     * @return Reference to this message for chaining
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
     * Clears the message data and resets the frame control bits.
     */
    void
    reset() {
        fin_rsv_opcode = 0;
        _data.reset();
    }
};

/**
 * @struct MessageText
 * @brief WebSocket text message
 *
 * Specialization of Message for UTF-8 encoded text data.
 */
struct MessageText : public Message {
    /**
     * @brief Construct a new text message with the appropriate opcode
     */
    MessageText() {
        fin_rsv_opcode = opcode::Text;
    }
};

/**
 * @struct MessageBinary
 * @brief WebSocket binary message
 *
 * Specialization of Message for binary data.
 */
struct MessageBinary : public Message {
    /**
     * @brief Construct a new binary message with the appropriate opcode
     */
    MessageBinary() {
        fin_rsv_opcode = opcode::Binary;
    }
};

/**
 * @struct MessagePing
 * @brief WebSocket ping control message
 *
 * Used to verify that the remote endpoint is still responsive.
 * The recipient must respond with a pong containing the same payload.
 */
struct MessagePing : public Message {
    /**
     * @brief Construct a new ping message with the appropriate opcode
     */
    MessagePing() {
        fin_rsv_opcode = opcode::Ping;
        masked         = false;
    }
};

/**
 * @struct MessagePong
 * @brief WebSocket pong control message
 *
 * Response to a ping message, containing the same payload as the ping.
 */
struct MessagePong : public Message {
    /**
     * @brief Construct a new pong message with the appropriate opcode
     */
    MessagePong() {
        fin_rsv_opcode = opcode::Pong;
    }
};

/**
 * @enum CloseStatus
 * @brief WebSocket close status codes as defined in RFC 6455
 *
 * These values represent standard status codes for WebSocket connection closure.
 */
enum CloseStatus : int {
    Normal =
        1000, /**< Normal closure; the connection successfully completed its purpose */
    GoingAway     = 1001, /**< The endpoint is going away (e.g., server shutdown) */
    ProtocolError = 1002, /**< Protocol error */
    DataNotAccepted =
        1003, /**< Received data cannot be accepted (e.g., invalid data format) */
    zReserved1 = 1004, /**< Reserved status code */
    zReserved2 = 1005, /**< Reserved status code - no status received */
    zReserved3 = 1006, /**< Reserved status code - abnormal closure */
    DataNotConsistent =
        1007, /**< Data is inconsistent with message type (e.g., non-UTF8 in text) */
    PolicyViolation  = 1008, /**< Message violates policy */
    MessageTooBig    = 1009, /**< Message is too large to process */
    MissingExtension = 1010, /**< Client expected server to negotiate an extension */
    UnexpectedReason = 1011, /**< Server encountered an unexpected condition */
    zReserved4       = 1012  /**< Reserved status code */
};

/**
 * @struct MessageClose
 * @brief WebSocket close control message
 *
 * Used to initiate or respond to a connection closure with a status code and reason.
 */
struct MessageClose : Message {
    MessageClose() = delete;

    /**
     * @brief Construct a new close message with status code and reason
     * @param status The close status code
     * @param reason A human-readable explanation for the closure
     */
    explicit MessageClose(int                status = CloseStatus::Normal,
                          std::string_view reason = "closed normally") {
        fin_rsv_opcode = opcode::Close;

        // Truncate reason if it makes the payload exceed 123 bytes (125 total - 2 for status)
        if (reason.length() > 123) {
            reason = reason.substr(0, 123);
        }

        this->_data << static_cast<unsigned char>(status >> 8)
                    << static_cast<unsigned char>(status % 256) << reason;
    }
};

/**
 * @brief Generate a random WebSocket key for handshake
 * @return Base64-encoded random 16-byte value
 *
 * This function creates a secure random key for use in the WebSocket opening handshake.
 */
std::string generateKey() noexcept;

} // namespace qb::http::ws

/**
 * @namespace qb::http
 * @brief HTTP protocol related functionality
 */
namespace qb::http {

/**
 * @struct WebSocketRequest
 * @brief HTTP request specifically formatted for a WebSocket upgrade
 *
 * Extends the standard HTTP Request to include headers required for a WebSocket
 * handshake.
 */
struct WebSocketRequest : public Request {
    WebSocketRequest() = delete;

    /**
     * @brief Construct a WebSocket upgrade request with the specified key
     * @param key The WebSocket key for the handshake
     */
    explicit WebSocketRequest(std::string const &key) {
        method() = qb::http::Method::GET;
        _headers["Upgrade"].emplace_back("websocket");
        _headers["Connection"].emplace_back("Upgrade");
        _headers["Sec-WebSocket-Key"].emplace_back(key);
        _headers["Sec-WebSocket-Version"].emplace_back("13");
    }
};
} // namespace qb::http

/**
 * @namespace qb::protocol
 * @brief Protocol implementations for network communication
 */
namespace qb::protocol {

/**
 * @namespace qb::protocol::ws_internal
 * @brief Internal implementation details for WebSocket protocol
 */
namespace ws_internal {

/**
 * @namespace qb::protocol::ws_internal::rfc
 * @brief Constants related to the WebSocket RFC 6455 specification.
 */
namespace rfc {
constexpr uint8_t     FIN_BIT_MASK                 = 0x80;
constexpr uint8_t     RSV_BITS_MASK                = 0x70;
constexpr uint8_t     OPCODE_MASK                  = 0x0F;
constexpr uint8_t     MASK_BIT_MASK                = 0x80;
constexpr size_t      MAX_CONTROL_FRAME_PAYLOAD_SIZE = 125;
constexpr uint8_t     PAYLOAD_LEN_16_BIT           = 126;
constexpr uint8_t     PAYLOAD_LEN_64_BIT           = 127;
} // namespace rfc

/**
 * @class base
 * @brief Base implementation of the WebSocket protocol
 *
 * Provides core functionality for both client and server WebSocket endpoints,
 * including frame parsing, message handling, and event dispatching.
 *
 * @tparam IO_ The I/O handler type
 */
template <typename IO_>
class base : public qb::io::async::AProtocol<IO_> {
    std::size_t   _parsed = 0; /**< Number of bytes parsed from the current frame */
    std::size_t   _expected_size = 0; /**< Expected payload size based on frame header */
    unsigned char _fin_rsv_opcode = 0; /**< Current frame's FIN, RSV, and opcode bits */
    unsigned char _data_opcode    = 0; /**< Opcode for the current fragmented data message */
    ::qb::http::ws::Message _message;   /**< Current message being assembled */
    size_t _max_payload_size = 0;       /**< Max allowed payload size, 0 for unlimited */

    /**
     * @brief Fails the WebSocket connection by sending a Close frame.
     * @param status The CloseStatus code.
     * @param reason The reason for closing.
     * @return 0 to indicate to the IO layer that the message processing is done, but in an error state.
     */
    std::size_t
    fail_connection(::qb::http::ws::CloseStatus status, std::string_view reason) {
        if (this->ok()) {
            ::qb::http::ws::MessageClose close_msg(status, std::string(reason));
            if constexpr (!IO_::has_server) {
                close_msg.masked = true;
            }
            this->_io << close_msg;
            this->not_ok();
        }
        // Consume whatever is in the buffer to prevent further processing
        return this->_io.in().size();
    }

    void
    processControlFrame(unsigned char frame_opcode, ::qb::http::ws::Message &current_frame_message) {
        // Handle control frames immediately and separately.
        current_frame_message.fin_rsv_opcode = _fin_rsv_opcode;
        if constexpr (IO_::has_server)
            current_frame_message.masked = false;
        else
            current_frame_message.masked = true; // For client sending pong reply etc.

        if (frame_opcode == ::qb::http::ws::opcode::_Close) {
            this->_io.out().reset();
            if constexpr (has_method_on<IO_, void, close>::value) {
                this->_io.on(
                    close{current_frame_message.size(),
                          current_frame_message._data.cbegin(),
                          current_frame_message});
            } else {
                // Default behavior: echo the close frame back
                this->_io << current_frame_message;
            }
            this->not_ok();
        } else if (frame_opcode == ::qb::http::ws::opcode::_Ping) {
            if constexpr (has_method_on<IO_, void, ping>::value) {
                this->_io.on(
                    ping{current_frame_message.size(),
                         current_frame_message._data.cbegin(),
                         current_frame_message});
            }
            // Send pong automatically
            current_frame_message.fin_rsv_opcode = ::qb::http::ws::opcode::Pong;
            this->_io << current_frame_message;
        } else if (frame_opcode == ::qb::http::ws::opcode::_Pong) {
            if constexpr (has_method_on<IO_, void, pong>::value) {
                this->_io.on(
                    pong{current_frame_message.size(),
                         current_frame_message._data.cbegin(),
                         current_frame_message});
            }
        }
    }

    void
    processDataFrame(unsigned char frame_opcode, bool is_final_frame, ::qb::http::ws::Message &current_frame_message) {
        // Handle data frames (Continuation, Text, Binary).
        if (frame_opcode != ::qb::http::ws::opcode::Continuation) {
            if (_data_opcode != 0) {
                // New data message started before previous one finished.
                fail_connection(
                    ::qb::http::ws::CloseStatus::ProtocolError,
                    "Received new data message before previous was complete");
            } else {
                _data_opcode = frame_opcode;
                _message.reset();
            }
        } else { // Continuation frame
            if (_data_opcode == 0) {
                // Continuation frame received without a prior data frame.
                fail_connection(::qb::http::ws::CloseStatus::ProtocolError,
                                "Received continuation frame without initial data frame");
            }
        }

        if (this->ok()) {
            // Append the payload of the current frame to the main reassembly buffer
            _message._data << current_frame_message._data;

            if (is_final_frame) {
                if (_data_opcode == 0) {
                    // This can happen if a single-frame message is sent with opcode 0
                    fail_connection(
                        ::qb::http::ws::CloseStatus::ProtocolError,
                        "Received final continuation frame with no initial data frame");
                } else {
                    // Complete message has been reassembled.
                    _message.fin_rsv_opcode = _data_opcode | rfc::FIN_BIT_MASK;

                    if (_data_opcode == ::qb::http::ws::opcode::_Text &&
                        !::qb::http::ws::is_utf8(
                            {_message._data.cbegin(), _message.size()})) {
                        fail_connection(
                            ::qb::http::ws::CloseStatus::DataNotConsistent,
                            "Invalid UTF-8 in text message");
                    } else {
                        if constexpr (IO_::has_server)
                            _message.masked = false;
                        else
                            _message.masked = true;

                        this->_io.on(
                            message{_message.size(), _message._data.cbegin(), _message});
                    }

                    // Reset for the next message.
                    _message.reset();
                    _data_opcode = 0;
                }
            }
        }
    }

public:
    /**
     * @struct close
     * @brief Event triggered when a connection close frame is received
     * @warning The data pointer and message reference in this event are only valid for the duration of the 'on' callback.
     */
    struct close {
        const std::size_t      size; /**< Size of the close message payload */
        const char            *data; /**< Pointer to the close message payload */
        qb::http::ws::Message &ws;   /**< Reference to the original message */
    };

    /**
     * @struct ping
     * @brief Event triggered when a ping frame is received
     * @warning The data pointer and message reference in this event are only valid for the duration of the 'on' callback.
     */
    struct ping {
        const std::size_t      size; /**< Size of the ping payload */
        const char            *data; /**< Pointer to the ping payload */
        qb::http::ws::Message &ws;   /**< Reference to the original message */
    };

    /**
     * @struct pong
     * @brief Event triggered when a pong frame is received
     * @warning The data pointer and message reference in this event are only valid for the duration of the 'on' callback.
     */
    struct pong {
        const std::size_t      size; /**< Size of the pong payload */
        const char            *data; /**< Pointer to the pong payload */
        qb::http::ws::Message &ws;   /**< Reference to the original message */
    };

    /**
     * @struct message
     * @brief Event triggered when a data frame (text or binary) is received
     * @warning The data pointer and message reference in this event are only valid for the duration of the 'on' callback.
     */
    struct message {
        const std::size_t      size; /**< Size of the message payload */
        const char            *data; /**< Pointer to the message payload */
        qb::http::ws::Message &ws;   /**< Reference to the original message */
    };

    base() = delete;

    /**
     * @brief Construct a WebSocket protocol handler
     * @param io Reference to the I/O handler
     */
    explicit base(IO_ &io)
        : qb::io::async::AProtocol<IO_>(io) {}

    /**
     * @brief Sets the maximum allowed payload size for a single message.
     * @param size The maximum size in bytes. 0 means no limit.
     */
    void
    set_max_payload_size(size_t size) {
        _max_payload_size = size;
    }

    /**
     * @brief Calculate the expected size of the incoming WebSocket message
     * @return Expected total size of the current frame in bytes, or 0 if incomplete
     *
     * This method parses WebSocket frame headers to determine the total expected
     * size of the frame, including header and payload.
     */
    std::size_t
    getMessageSize() noexcept final {
        if (!this->ok())
            return 0;

        auto      &buffer      = this->_io.in();
        const auto buffer_size = buffer.size();
        auto first_bytes = reinterpret_cast<const unsigned char *>(buffer.cbegin());
        if (!_parsed) {
            if (buffer_size < 2u)
                return 0;

            _fin_rsv_opcode  = first_bytes[0];
            const auto frame_opcode = _fin_rsv_opcode & rfc::OPCODE_MASK;

            // RFC 5.2: RSV bits MUST be 0 unless an extension is negotiated.
            if ((_fin_rsv_opcode & rfc::RSV_BITS_MASK) != 0) {
                return fail_connection(::qb::http::ws::CloseStatus::ProtocolError, "RSV bits must be 0");
            }

            const bool is_control_frame = frame_opcode >= ::qb::http::ws::opcode::_Close;
            if (is_control_frame) {
                // RFC 5.5: Control frames MUST NOT be fragmented.
                if ((_fin_rsv_opcode & rfc::FIN_BIT_MASK) == 0) {
                    return fail_connection(::qb::http::ws::CloseStatus::ProtocolError, "Control frame cannot be fragmented");
                }
            }

            _message.masked = (first_bytes[1] & rfc::MASK_BIT_MASK) != 0;
            // only server side
            if constexpr (IO_::has_server) {
                if (!_message.masked) {
                    // RFC 5.1: A server MUST close the connection if it receives an unmasked frame.
                    return fail_connection(::qb::http::ws::CloseStatus::ProtocolError, "Message from client not masked");
                }
            }
            _parsed += 2u;
        }
        if (!_expected_size) {
            const auto payload_indicator = first_bytes[1] & 127u;
            std::size_t length = payload_indicator;
            const bool is_control_frame = (_fin_rsv_opcode & rfc::OPCODE_MASK) >= ::qb::http::ws::opcode::_Close;

            if (is_control_frame && length > rfc::MAX_CONTROL_FRAME_PAYLOAD_SIZE) {
                 return fail_connection(::qb::http::ws::CloseStatus::ProtocolError, "Control frame payload cannot exceed 125 bytes");
            }

            // 2 or 8 next bytes is the size of content
            std::size_t num_bytes = 0;
            if (payload_indicator == rfc::PAYLOAD_LEN_16_BIT) {
                num_bytes = 2;
            } else if (payload_indicator == rfc::PAYLOAD_LEN_64_BIT) {
                num_bytes = 8;
            }

            if (num_bytes) {
                if (is_control_frame) {
                    return fail_connection(::qb::http::ws::CloseStatus::ProtocolError, "Control frame cannot have extended payload length");
                }
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

            // Enforce max payload size for data frames
            if (!is_control_frame && _max_payload_size > 0 && _expected_size > _max_payload_size) {
                return fail_connection(::qb::http::ws::CloseStatus::MessageTooBig, "Payload size exceeds configured limit");
            }
            _parsed += num_bytes;
        }

        const auto full_size =
            _expected_size + _parsed + (_message.masked ? 4u : 0u);
        if (buffer_size < full_size)
            return 0;

        return full_size;
    }

    void
    onMessage(std::size_t) noexcept final {
        if (!this->ok())
            return;

        auto      &buffer      = this->_io.in();
        const auto frame_opcode      = _fin_rsv_opcode & rfc::OPCODE_MASK;
        const bool is_final_frame = (_fin_rsv_opcode & rfc::FIN_BIT_MASK) != 0;

        // Create a temporary message to hold the current frame's payload
        // This avoids corrupting the main reassembly buffer (_message) with control frames.
        ::qb::http::ws::Message current_frame_message;
        current_frame_message.masked = _message.masked;

        if (current_frame_message.masked) {
            auto mask = reinterpret_cast<const unsigned char *>(buffer.cbegin() + _parsed);
            auto begin_buffer_data = buffer.begin() + _parsed + 4;
            auto begin_data = current_frame_message._data.allocate_back(_expected_size);
            for (auto i = 0u; i < _expected_size; ++i)
                begin_data[i] = begin_buffer_data[i] ^ mask[i % 4];
        } else {
            std::memcpy(current_frame_message._data.allocate_back(_expected_size),
                        buffer.begin() + _parsed, _expected_size);
        }

        const bool is_control_frame = frame_opcode >= ::qb::http::ws::opcode::_Close;
        if (is_control_frame) {
            processControlFrame(frame_opcode, current_frame_message);
        } else {
            processDataFrame(frame_opcode, is_final_frame, current_frame_message);
        }

        // Reset per-frame state
        _expected_size = _parsed = _fin_rsv_opcode = 0;
    }

    void
    reset() noexcept final {
        _message.reset();
        _expected_size = _parsed = _fin_rsv_opcode = 0;
    }
};

} // namespace ws_internal

template <typename IO_>
class ws_server : public ws_internal::base<IO_> {
    std::string endpoint;

public:
    // server side event
    struct sending_http_response {
        qb::http::Response &response;
    };
    // !server side event

    ws_server() = delete;
    template <typename HttpRequest>
    ws_server(IO_ &io, HttpRequest const &http)
        : ws_internal::base<IO_>(io) {
        static const auto ws_magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        if (http.upgrade) {
            std::string ws_key(http.header("Sec-WebSocket-Key"));
            if (!ws_key.empty()) {
                ws_key += ws_magic_string;
                qb::http::Response res;
                res.status() = qb::http::status::SWITCHING_PROTOCOLS;
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

    template <typename HttpRequest, typename HttpResponse>
    ws_server(IO_ &io, HttpRequest const &request, HttpResponse &response)
        : ws_internal::base<IO_>(io) {
        static const auto ws_magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        if (request.upgrade) {
            std::string ws_key(request.header("Sec-WebSocket-Key"));
            if (!ws_key.empty()) {
                ws_key += ws_magic_string;
                response.status() = qb::http::status::SWITCHING_PROTOCOLS;
                response.headers()["Upgrade"].emplace_back("websocket");
                response.headers()["Connection"].emplace_back("Upgrade");
                response.headers()["Sec-WebSocket-Accept"].emplace_back(
                    crypto::base64::encode(crypto::sha1(ws_key)));

                endpoint = request.uri().path();
                return;
            }
            // error
        }
        this->not_ok();
    }

};

template <typename IO_>
class ws_client : public ws_internal::base<IO_> {
public:
    ws_client() = delete;
    template <typename HttpResponse>
    ws_client(IO_ &io, HttpResponse const &http, std::string const &key)
        : ws_internal::base<IO_>(io) {
        static const auto ws_magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        if (http.upgrade) {
            if (http.status() == qb::http::status::SWITCHING_PROTOCOLS) {
                const auto res_key = http.header("Sec-WebSocket-Accept");
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
    using protocol = ::qb::protocol::ws_server<IO_>;
};

template <typename IO_>
struct side<IO_, false> {
    using protocol = ::qb::protocol::ws_client<IO_>;
};

} // namespace internal

template <typename IO_>
using protocol = typename internal::side<IO_>::protocol;

// Forward declare the transport type
namespace qb {
namespace io {
namespace transport {
class tcp;
}
} // namespace io
} // namespace qb

/**
 * @class WebSocket
 * @brief WebSocket client implementation
 *
 * This class provides a complete implementation of a WebSocket client
 * according to RFC 6455. It handles:
 * - Connection establishment and handshake
 * - Message sending and receiving (text and binary)
 * - Control frames (ping/pong for keepalive)
 * - Connection management and cleanup
 * - Event-based callbacks for WebSocket events
 *
 * @tparam T Parent class type that will receive event notifications
 * @tparam Transport Transport layer implementation (TCP by default or secure TCP)
 */
template <typename T, typename Transport = io::transport::tcp>
class WebSocket
    : public io::async::tcp::client<WebSocket<T, Transport>, Transport>
    , public io::use<WebSocket<T, Transport>>::timeout {
    const std::string _ws_key;        /**< WebSocket handshake key */
    int               _ping_interval; /**< Interval for sending ping frames (in ms) */
    io::uri           _remote;        /**< Remote server URI */

private:
    T& derived() noexcept { return *static_cast<T*>(this); }
    const T& derived() const noexcept { return *static_cast<const T*>(this); }

public:
    using http_protocol = http::protocol_view<WebSocket<T, Transport>>;
    using ws_protocol   = http::ws::protocol<WebSocket<T, Transport>>;

    // public events
    /**
     * @struct sending_http_request
     * @brief Event triggered when a WebSocket handshake request is being sent
     */
    struct sending_http_request {
        http::WebSocketRequest
            &request; /**< Reference to the WebSocket handshake request */
    };

    /**
     * @struct connected
     * @brief Event triggered when WebSocket connection is established
     */
    struct connected {};

    /**
     * @struct error
     * @brief Event triggered when a WebSocket error occurs
     */
    struct error {};

    using closed  = typename ws_protocol::close;   /**< Connection closed event */
    using ping    = typename ws_protocol::ping;    /**< Ping message received event */
    using pong    = typename ws_protocol::pong;    /**< Pong message received event */
    using message = typename ws_protocol::message; /**< Data message received event */
    using disconnected = io::async::event::disconnected; /**< TCP disconnection event */
    using timeout      = io::async::event::timeout;      /**< Timeout event for pings */

public:
    /**
     * @brief Constructs a WebSocket client
     *
     * Initializes the WebSocket client with a randomly generated key
     * for the WebSocket handshake.
     */
    explicit WebSocket()
        : _ws_key(http::ws::generateKey())
        , _ping_interval(0)
        {}

    /**
     * @brief Sets the ping interval for keepalive
     * @param ping_interval Interval in milliseconds (0 to disable pings)
     *
     * Configures automatic ping/pong keepalive mechanism.
     * A value of 0 disables automatic pings.
     */
    void
    set_ping_interval(int ping_interval = 0) {
        _ping_interval = ping_interval;
        this->setTimeout(ping_interval);
    }

    /**
     * @brief Connects to a WebSocket server
     * @param remote URI of the remote WebSocket endpoint
     * @param timeout Connection timeout in milliseconds (0 for no timeout)
     *
     * Initiates a connection to the specified WebSocket server.
     * The connection process includes establishing a TCP connection and
     * performing the WebSocket handshake.
     */
    void
    connect(io::uri const &remote, int timeout = 0) {
        this->clear_protocols();
        this->setTimeout(0);
        _remote = remote;
        io::async::tcp::connect<typename Transport::transport_io_type>(
            remote,
            [this](auto &&transport) {
                if (!transport.is_open()) {
                    if constexpr (has_method_on<T, void, error>::value) {
                        derived().on(error{});
                    }
                } else {
                    this->transport() = std::move(transport);
                    this->template switch_protocol<http_protocol>(*this);
                    this->start();

                    http::WebSocketRequest request(_ws_key);
                    request.headers()["host"].emplace_back(std::string(_remote.host()));
                    request.uri() = _remote;

                    if constexpr (has_method_on<T, void, sending_http_request>::value) {
                        derived().on(sending_http_request{request});
                    }

                    *this << request;
                }
            },
            timeout);
    }

    /**
     * @brief Handles HTTP response events during handshake
     * @param event HTTP response event from the server
     *
     * Processes the HTTP response during the WebSocket handshake.
     * Validates the response and switches to the WebSocket protocol
     * if the handshake was successful.
     */
    void
    on(typename http_protocol::response &&event) {
        if (!this->template switch_protocol<ws_protocol>(*this, event, _ws_key)) {
            if constexpr (has_method_on<T, void, error>::value) {
                derived().on(error{});
            }
            this->disconnect();
            return;
        }
        if constexpr (has_method_on<T, void, connected>::value) {
            derived().on(connected{});
            this->setTimeout(_ping_interval);
        }
    }

    /**
     * @brief Handles ping events
     * @param event Ping event containing the ping payload
     *
     * Forwards ping events to the parent class if it has a handler.
     */
    void
    on(ping &&event) {
        if constexpr (has_method_on<T, void, ping>::value) {
            derived().on(std::forward<ping>(event));
        }
    }

    /**
     * @brief Handles pong events
     * @param event Pong event containing the pong payload
     *
     * Forwards pong events to the parent class if it has a handler.
     */
    void
    on(pong &&event) {
        if constexpr (has_method_on<T, void, pong>::value) {
            derived().on(std::forward<pong>(event));
        }
    }

    /**
     * @brief Handles message events
     * @param event Message event containing the data payload
     *
     * Forwards WebSocket message events to the parent class.
     */
    void
    on(message &&event) {
        derived().on(std::forward<message>(event));
    }

    /**
     * @brief Handles close events
     * @param event Close event containing the status code and reason
     *
     * Forwards WebSocket close events to the parent class if it has a handler.
     */
    void
    on(closed &&event) {
        if constexpr (has_method_on<T, void, closed>::value) {
            derived().on(std::forward<closed>(event));
        }
    }

    /**
     * @brief Handles disconnection events
     * @param event Disconnection event
     *
     * Forwards TCP disconnection events to the parent class.
     */
    void
    on(disconnected &&event) {
        derived().on(std::forward<disconnected>(event));
    }

    /**
     * @brief Handles timeout events
     * @param event Timeout event
     *
     * Sends a ping message when a timeout occurs and resets the timer.
     * This is used for the ping/pong keepalive mechanism.
     */
    void
    on(timeout const &) {
        MessagePing msg;
        *this << msg;
        this->setTimeout(_ping_interval);
    }

    /**
     * @brief Sends a message to the WebSocket server
     * @tparam ToSend Type of the message to send
     * @param msg Message to send
     * @return Reference to this object for method chaining
     */
    template <typename ToSend>
    WebSocket &operator<<(ToSend &&msg) {
        if constexpr (std::is_base_of_v<Message, std::decay_t<ToSend>>) {
            // RFC 5.1: Client-to-server frames MUST be masked.
            // RFC 5.5: Control frames MUST NOT be masked.
            // Here we are the client, so we must mask data frames.
            const auto opcode = msg.fin_rsv_opcode & ::qb::protocol::ws_internal::rfc::OPCODE_MASK;
            if (opcode == ::qb::http::ws::opcode::_Text || opcode == ::qb::http::ws::opcode::_Binary || opcode == ::qb::http::ws::opcode::Continuation) {
                msg.masked = true;
            } else {
                msg.masked = false; // Ensure control frames are not masked
            }
        }
        io::async::tcp::client<WebSocket<T, Transport>, Transport>::operator<<(std::forward<ToSend>(msg));
        return *this;
    }
};

/**
 * @typedef WebSocketSecure
 * @brief Secure WebSocket client using TLS/SSL
 *
 * A specialized version of the WebSocket client that uses secure
 * transport (TLS/SSL) for encrypted connections.
 */
template <typename T>
using WebSocketSecure = WebSocket<T, io::transport::stcp>;

/**
 * @class Client
 * @brief WebSocket client implementation using callbacks
 *
 * This class provides a WebSocket client that uses callback functions
 * instead of inheritance for handling WebSocket events. It inherits from
 * WebSocket internally but exposes a callback-based interface.
 *
 * @tparam Transport Transport layer implementation (TCP by default or secure TCP)
 */
template <typename Transport = io::transport::tcp>
class Client : public WebSocket<Client<Transport>, Transport> {
public:
    using base_type = WebSocket<Client<Transport>, Transport>;
    
    // Callback types for each event
    using sending_http_request_callback_t = std::function<void(typename base_type::sending_http_request&)>;
    using connected_callback_t = std::function<void(typename base_type::connected&)>;
    using error_callback_t = std::function<void(typename base_type::error&)>;
    using closed_callback_t = std::function<void(typename base_type::closed&)>;
    using ping_callback_t = std::function<void(typename base_type::ping&)>;
    using pong_callback_t = std::function<void(typename base_type::pong&)>;
    using message_callback_t = std::function<void(typename base_type::message&)>;
    using disconnected_callback_t = std::function<void(typename base_type::disconnected&)>;

private:
    sending_http_request_callback_t _on_sending_http_request;
    connected_callback_t _on_connected;
    error_callback_t _on_error;
    closed_callback_t _on_closed;
    ping_callback_t _on_ping;
    pong_callback_t _on_pong;
    message_callback_t _on_message;
    disconnected_callback_t _on_disconnected;

public:
    /**
     * @brief Constructs a WebSocket client with callback-based event handling
     */
    Client() : base_type() {}

    /**
     * @brief Set callback for HTTP request sending event
     * @param callback Function to call when sending HTTP request
     * @return Reference to this object for method chaining
     */
    Client& on_sending_http_request(sending_http_request_callback_t callback) {
        _on_sending_http_request = std::move(callback);
        return *this;
    }

    /**
     * @brief Set callback for connection established event
     * @param callback Function to call when connection is established
     * @return Reference to this object for method chaining
     */
    Client& on_connected(connected_callback_t callback) {
        _on_connected = std::move(callback);
        return *this;
    }

    /**
     * @brief Set callback for error event
     * @param callback Function to call when an error occurs
     * @return Reference to this object for method chaining
     */
    Client& on_error(error_callback_t callback) {
        _on_error = std::move(callback);
        return *this;
    }

    /**
     * @brief Set callback for connection closed event
     * @param callback Function to call when connection is closed
     * @return Reference to this object for method chaining
     */
    Client& on_closed(closed_callback_t callback) {
        _on_closed = std::move(callback);
        return *this;
    }

    /**
     * @brief Set callback for ping message event
     * @param callback Function to call when ping is received
     * @return Reference to this object for method chaining
     */
    Client& on_ping(ping_callback_t callback) {
        _on_ping = std::move(callback);
        return *this;
    }

    /**
     * @brief Set callback for pong message event
     * @param callback Function to call when pong is received
     * @return Reference to this object for method chaining
     */
    Client& on_pong(pong_callback_t callback) {
        _on_pong = std::move(callback);
        return *this;
    }

    /**
     * @brief Set callback for data message event
     * @param callback Function to call when a message is received
     * @return Reference to this object for method chaining
     */
    Client& on_message(message_callback_t callback) {
        _on_message = std::move(callback);
        return *this;
    }

    /**
     * @brief Set callback for disconnection event
     * @param callback Function to call when disconnected
     * @return Reference to this object for method chaining
     */
    Client& on_disconnected(disconnected_callback_t callback) {
        _on_disconnected = std::move(callback);
        return *this;
    }

    // Event handlers that delegate to callbacks
    void on(typename base_type::sending_http_request&& event) {
        if (_on_sending_http_request) {
            _on_sending_http_request(event);
        }
    }

    void on(typename base_type::connected&& event) {
        if (_on_connected) {
            _on_connected(event);
        }
    }

    void on(typename base_type::error&& event) {
        if (_on_error) {
            _on_error(event);
        }
    }

    void on(typename base_type::closed&& event) {
        if (_on_closed) {
            _on_closed(event);
        }
    }

    void on(typename base_type::ping&& event) {
        if (_on_ping) {
            _on_ping(event);
        }
    }

    void on(typename base_type::pong&& event) {
        if (_on_pong) {
            _on_pong(event);
        }
    }

    void on(typename base_type::message&& event) {
        if (_on_message) {
            _on_message(event);
        }
    }

    void on(typename base_type::disconnected&& event) {
        if (_on_disconnected) {
            _on_disconnected(event);
        }
    }
};

/**
 * @typedef ClientSecure
 * @brief Secure WebSocket client using callbacks and TLS/SSL
 *
 * A specialized version of the Client that uses secure
 * transport (TLS/SSL) for encrypted connections.
 */
using ClientSecure = Client<io::transport::stcp>;

using client = Client<io::transport::tcp>;
using client_secure = Client<io::transport::stcp>;

} // namespace qb::http::ws

namespace qb::allocator {

template <>
pipe<char> &pipe<char>::put<http::ws::Message>(const http::ws::Message &msg);

template <>
pipe<char> &pipe<char>::put<http::ws::MessagePing>(const http::ws::MessagePing &msg);

template <>
pipe<char> &pipe<char>::put<http::ws::MessagePong>(const http::ws::MessagePong &msg);

template <>
pipe<char> &pipe<char>::put<http::ws::MessageText>(const http::ws::MessageText &msg);

template <>
pipe<char> &pipe<char>::put<http::ws::MessageBinary>(const http::ws::MessageBinary &msg);

template <>
pipe<char> &pipe<char>::put<http::ws::MessageClose>(const http::ws::MessageClose &msg);

template <>
pipe<char> &pipe<char>::put<http::WebSocketRequest>(const http::WebSocketRequest &msg);

} // namespace qb::allocator
