# `qbm-ws`: WebSocket Protocol Handler

This document describes the `qb::http::ws::protocol` class (found indirectly via `ws.h`, defined within `qb::protocol::ws_internal::base` and specialized wrappers `ws_server`/`ws_client`), which is responsible for parsing and handling WebSocket frames after a successful handshake.

## Role

Once the initial HTTP handshake is complete and `switch_protocol` has been called, this protocol handler takes over the I/O processing for the connection. Its primary jobs are:

1.  **Frame Parsing:** Reading the incoming byte stream and parsing it according to the WebSocket framing rules (RFC 6455, Section 5).
2.  **Header Interpretation:** Decoding the FIN bit, RSV bits, opcode, payload length, and mask bit.
3.  **Masking/Unmasking:** Applying or removing the XOR mask from the payload data based on the mask bit and the direction of communication (client->server MUST be masked, server->client MUST NOT be masked).
4.  **Payload Assembly:** Collecting the payload data, potentially across multiple continuation frames.
5.  **Event Dispatching:** Triggering specific events based on the frame type (opcode) once a complete message or control frame is received.

## Implementation (`ws_internal::base`)

The core logic resides in `qb::protocol::ws_internal::base<IO_>`. Key aspects:

*   **State Management:** It maintains internal state variables like `_parsed` (bytes parsed in current frame), `_expected_size` (payload size), and `fin_rsv_opcode`.
*   **`getMessageSize()`:** This crucial method is called by the underlying `qb-io` transport layer. It attempts to read the frame header from the input buffer (`_io.in()`) to determine the total size of the next WebSocket frame. It handles the different payload length encodings (7-bit, 7+16 bit, 7+64 bit).
*   **`onMessage(size_t)`:** Called by the transport layer once `getMessageSize()` has returned a non-zero value and enough bytes (`size_t`) are available in the input buffer.
    *   It reads the masking key (if present).
    *   It unmasks the payload data (if masked).
    *   It appends the payload data to the internal `_message._data` buffer.
    *   It checks the FIN bit and opcode to determine the frame type.
    *   It dispatches the appropriate event (`ping`, `pong`, `message`, `close`) to the `IO_` handler (your client/server class).
    *   It resets its internal state if the FIN bit was set, preparing for the next message.
*   **Masking Enforcement (Server-side):** The server-side protocol (`ws_server`) explicitly checks if incoming frames (from the client) have the mask bit set. If not, it considers this a protocol error, sends a Close frame (status 1002), and terminates the connection.

## Events Dispatched

When the protocol handler successfully parses a frame, it triggers an event on the `IO_` handler (the class that *uses* the protocol, e.g., your `StressServerClient` or `WebSocket<MyClient>`). The event types correspond to the frame opcodes:

*   **`on(qb::http::ws::protocol::message&& event)`:** Triggered when a complete data message (Text or Binary) is received (FIN bit = 1). The `event` contains:
    *   `event.size`: Size of the *complete* message payload.
    *   `event.data`: Pointer to the start of the payload data in the internal buffer.
    *   `event.ws`: A reference to the internal `qb::http::ws::Message` object, allowing access to `fin_rsv_opcode` to differentiate between Text (0x81) and Binary (0x82).
*   **`on(qb::http::ws::protocol::ping&& event)`:** Triggered for Ping frames.
    *   `event.size`, `event.data`: Ping payload.
    *   The protocol automatically sends a Pong response with the same payload **before** dispatching this event.
*   **`on(qb::http::ws::protocol::pong&& event)`:** Triggered for Pong frames.
    *   `event.size`, `event.data`: Pong payload.
*   **`on(qb::http::ws::protocol::close&& event)`:** Triggered for Close frames.
    *   `event.size`, `event.data`: Close payload (containing status code and reason).
    *   The protocol handler typically marks itself as `not_ok()` after dispatching this, often leading to connection termination by the underlying transport.

## Usage Context

You generally don't interact with `qb::protocol::ws_internal::base` directly. Instead:

*   **Servers:** You create a class inheriting from `qb::io::use<...>::tcp::client<...>` (or `ssl::client`). Inside its `on(http::protocol::request&&)` handler, you call `this->switch_protocol<qb::http::ws::protocol>(*this, event.http)`.
*   **Clients:** You use the `qb::http::ws::WebSocket<T>` or `WebSocketSecure<T>` class template. It internally manages switching from an initial HTTP protocol handler to the WebSocket protocol handler upon receiving the `101` response.

In both cases, the `qb::http::ws::protocol` operates behind the scenes, translating the raw byte stream into the high-level WebSocket events you handle in your `on(...)` methods. 