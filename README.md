# QB WebSocket Module (`qbm-ws`)

This module extends the `qbm-http` module to provide support for the WebSocket protocol (RFC 6455), enabling real-time, bidirectional communication over a single TCP connection. It integrates seamlessly with the QB C++ Actor Framework's asynchronous I/O capabilities.

## Core Philosophy

*   **RFC 6455 Compliance:** Implements the core WebSocket protocol including handshake, framing, masking, and opcodes.
*   **Integration with HTTP:** Leverages `qbm-http` for the initial handshake process before upgrading the connection.
*   **Asynchronous & Event-Driven:** Built on `qb-io` for non-blocking operation and efficient event handling.
*   **Flexibility:** Designed to be used within actor-based applications or standalone `qb-io` projects.

## Key Features

*   **WebSocket Handshake:** Handles the HTTP Upgrade request/response sequence.
    *   Server-side validation of client requests (`Sec-WebSocket-Key`, etc.).
    *   Client-side generation of handshake requests (`qb::http::WebSocketRequest`).
*   **Framing Protocol (`qb::http::ws::protocol::WebSocket`):**
    *   Parses incoming WebSocket frames (Text, Binary, Ping, Pong, Close).
    *   Handles fragmentation (though full reassembly might require application logic).
    *   Manages masking/unmasking of payloads.
*   **Message Representation (`qb::http::ws::Message`):** Base class and derived types (`MessageText`, `MessageBinary`, `MessagePing`, `MessagePong`, `MessageClose`) for easy handling of different frame types.
*   **Asynchronous Integration:** Designed to work with `qb::io::async` components (often via `qb::io::use<>`). Events like `ws::ping`, `ws::pong`, `ws::message`, `ws::closed` signal WebSocket-specific occurrences.
*   **Server & Client Support:** Enables building both WebSocket servers and clients.

## Relationship with `qbm-http`

`qbm-ws` is an **extension** of `qbm-http`. The initial connection is established using HTTP, and the handshake process involves specific HTTP headers. Once the handshake is successful, the underlying protocol typically switches from an HTTP protocol to the `qb::http::ws::protocol::WebSocket` protocol to handle the binary framing of WebSocket messages.

## Documentation

For detailed usage and concepts, please refer to the documentation within the `readme/` directory:

*   **[Introduction](./readme/README.md)**
*   **[Core Concepts](./readme/concepts.md)**: Handshake, Frames, Opcodes, Masking.
*   **[WebSocket Protocol](./readme/protocol.md)**: Details on the `protocol::WebSocket` implementation.
*   **[Handshake Process](./readme/handshake.md)**: How the HTTP Upgrade works.
*   **[Usage Guide](./readme/usage.md)**: How to integrate WebSockets into clients and servers.

## Examples

Refer to the tests in `qbm/ws/tests/` for practical examples:
*   `test-session.cpp`: Basic client/server communication.
*   `test-client.cpp`: Client-specific tests.
*   `test-robustness.cpp`: Handling different message types and scenarios.
*   `test-security.cpp`: Handshake validation tests.
*   `test-stress.cpp`: High-load scenarios.