/**
 * @file ws.cpp
 * @brief Implementation of the WebSocket protocol for the qb Actor Framework
 *
 * This file contains the implementation of core WebSocket functionality including:
 * - Secure random key generation for handshakes
 * - Frame construction with proper masking
 * - Serialization of different message types
 *
 * The implementation follows RFC 6455 (The WebSocket Protocol).
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

#include "ws.h"

namespace qb {
namespace http {
namespace ws {

/**
 * @brief Generate a random WebSocket key for handshake
 * @return Base64-encoded random 16-byte value
 *
 * Creates a cryptographically secure random key for use in the WebSocket
 * opening handshake. The key is a 16-byte nonce that is Base64-encoded
 * as specified in RFC 6455 Section 4.1.
 */
std::string
generateKey() noexcept {
    // Make random 16-byte nonce
    char                                          nonce[16] = "";
    std::uniform_int_distribution<unsigned short> dist(0, 255);
    std::random_device                            rd;
    for (char &i : nonce)
        i = static_cast<char>(dist(rd));
    return crypto::base64::encode({nonce, 16});
}

} // namespace ws
} // namespace http
} // namespace qb

namespace qb {
namespace allocator {

/**
 * @brief Create an unmasked WebSocket frame in the output buffer
 * @param pipe The output buffer to write the frame to
 * @param msg The WebSocket message to format
 *
 * Formats a WebSocket message as an unmasked frame according to RFC 6455.
 * This is typically used for server-to-client communication where masking
 * is not required.
 */
static void
fill_unmasked_message(pipe<char> &pipe, const http::ws::Message &msg) {
    std::size_t length = msg.size();
    pipe.reserve(length + 10); // Reserve space for header and payload

    // Write FIN, RSV, and opcode
    pipe << static_cast<char>(msg.fin_rsv_opcode);

    // Write payload length with appropriate format based on size
    if (length >= 126) {
        std::size_t num_bytes;
        if (length > 0xffff) {
            // For lengths >= 65536, use 8-byte length format
            num_bytes = 8;
            pipe << static_cast<char>(127); // 127 indicates 8-byte length
        } else {
            // For lengths >= 126 but < 65536, use 2-byte length format
            num_bytes = 2;
            pipe << static_cast<char>(126); // 126 indicates 2-byte length
        }

        // Write the length bytes in network byte order (big-endian)
        for (std::size_t c = num_bytes - 1; c != static_cast<std::size_t>(-1); --c)
            pipe << static_cast<char>(
                (static_cast<unsigned long long>(length) >> (8 * c)) % 256);
    } else {
        // For lengths < 126, use 1-byte length format
        pipe << static_cast<char>(length);
    }

    // Append the message payload
    pipe << msg._data;
}

/**
 * @brief Create a masked WebSocket frame in the output buffer
 * @param pipe The output buffer to write the frame to
 * @param msg The WebSocket message to format
 *
 * Formats a WebSocket message as a masked frame according to RFC 6455.
 * This is required for client-to-server communication to prevent certain
 * types of attacks on proxies and intermediaries.
 */
static void
fill_masked_message(pipe<char> &pipe, const http::ws::Message &msg) {
    // Create a random 4-byte mask as required by the protocol
    std::array<unsigned char, 4>                  mask{};
    std::uniform_int_distribution<unsigned short> dist(0, 255);
    std::random_device                            rd;
    for (std::size_t c = 0; c < 4; ++c)
        mask[c] = static_cast<unsigned char>(dist(rd));

    std::size_t length = msg.size();
    pipe.reserve(length + 14); // Reserve space for header, mask, and payload

    // Write FIN, RSV, and opcode
    pipe << static_cast<char>(msg.fin_rsv_opcode);

    // Write payload length with appropriate format based on size
    // Set the mask bit (0x80) in the first length byte
    if (length >= 126) {
        std::size_t num_bytes;
        if (length > 0xffff) {
            // For lengths >= 65536, use 8-byte length format
            num_bytes = 8;
            pipe << static_cast<char>(127 + 128); // 127 + mask bit
        } else {
            // For lengths >= 126 but < 65536, use 2-byte length format
            num_bytes = 2;
            pipe << static_cast<char>(126 + 128); // 126 + mask bit
        }

        // Write the length bytes in network byte order (big-endian)
        for (std::size_t c = num_bytes - 1; c != static_cast<std::size_t>(-1); --c)
            pipe << static_cast<char>(
                (static_cast<unsigned long long>(length) >> (8 * c)) % 256);
    } else {
        // For lengths < 126, use 1-byte length format
        pipe << static_cast<char>(length + 128); // length + mask bit
    }

    // Write the 4-byte mask
    pipe.write(reinterpret_cast<char *>(mask.data()), 4);

    // Apply the mask to the payload and write it
    const auto msg_begin = msg._data.cbegin();
    auto       out_begin = pipe.allocate_back(length);
    for (std::size_t i = 0; i < length; ++i)
        out_begin[i] = static_cast<char>(msg_begin[i] ^ mask[i % 4]);
}

/**
 * @brief Specialization to serialize a WebSocket Message into a pipe
 * @param msg The WebSocket message to serialize
 * @return Reference to this pipe for chaining
 *
 * Formats the WebSocket message according to the protocol specification,
 * applying masking if required by the message's masked flag.
 */
template <>
pipe<char> &
pipe<char>::put<http::ws::Message>(const http::ws::Message &msg) {
    if (msg.masked)
        fill_masked_message(*this, msg);
    else
        fill_unmasked_message(*this, msg);
    return *this;
}

/**
 * @brief Specialization to serialize a WebSocket Ping message
 * @param msg The Ping message to serialize
 * @return Reference to this pipe for chaining
 */
template <>
pipe<char> &
pipe<char>::put<http::ws::MessagePing>(const http::ws::MessagePing &msg) {
    return put(static_cast<http::ws::Message const &>(msg));
}

/**
 * @brief Specialization to serialize a WebSocket Pong message
 * @param msg The Pong message to serialize
 * @return Reference to this pipe for chaining
 */
template <>
pipe<char> &
pipe<char>::put<http::ws::MessagePong>(const http::ws::MessagePong &msg) {
    return put(static_cast<http::ws::Message const &>(msg));
}

/**
 * @brief Specialization to serialize a WebSocket Text message
 * @param msg The Text message to serialize
 * @return Reference to this pipe for chaining
 */
template <>
pipe<char> &
pipe<char>::put<http::ws::MessageText>(const http::ws::MessageText &msg) {
    return put(static_cast<http::ws::Message const &>(msg));
}

/**
 * @brief Specialization to serialize a WebSocket Binary message
 * @param msg The Binary message to serialize
 * @return Reference to this pipe for chaining
 */
template <>
pipe<char> &
pipe<char>::put<http::ws::MessageBinary>(const http::ws::MessageBinary &msg) {
    return put(static_cast<http::ws::Message const &>(msg));
}

/**
 * @brief Specialization to serialize a WebSocket Close message
 * @param msg The Close message to serialize
 * @return Reference to this pipe for chaining
 */
template <>
pipe<char> &
pipe<char>::put<http::ws::MessageClose>(const http::ws::MessageClose &msg) {
    return put(static_cast<http::ws::Message const &>(msg));
}

/**
 * @brief Specialization to serialize a WebSocket handshake request
 * @param msg The WebSocket handshake request to serialize
 * @return Reference to this pipe for chaining
 *
 * Converts a WebSocketRequest to an HTTP Request for the initial handshake.
 */
template <>
pipe<char> &
pipe<char>::put<http::WebSocketRequest>(const http::WebSocketRequest &msg) {
    return put(static_cast<const http::Request &>(msg));
}

} // namespace allocator
} // namespace qb

// #include <qb/io/async.h>