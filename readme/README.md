# `qbm-websocket` - Detailed Documentation

Welcome to the detailed documentation for the `qbm-websocket` WebSocket module.

This section provides in-depth information on the concepts, protocol implementation, and usage of WebSockets within the QB C++ Actor Framework. Please refer to the main [qbm-websocket README](../README.md) for a general overview.

## Table of Contents

*   **[Core Concepts](./concepts.md):** Explains the fundamentals of the WebSocket protocol as implemented in this module, including the handshake process, frame types (Text, Binary, Ping, Pong, Close), opcodes, and the masking mechanism.
*   **[Handshake Process](./handshake.md):** Details the HTTP Upgrade mechanism, the roles of `Sec-WebSocket-Key`, `Sec-WebSocket-Accept`, and how `qbm-http` and `qbm-websocket` interact during connection establishment.
*   **[WebSocket Protocol](./protocol.md):** Describes the `qb::http::ws::protocol` class, its role in parsing frames, handling fragmentation (partially), managing masking, and dispatching events (`ws::message`, `ws::ping`, etc.).
*   **[Usage Guide](./usage.md):** Provides practical examples and guidance on integrating WebSocket functionality into your `qb-io` applications, covering both client and server implementations, sending/receiving messages, and handling lifecycle events. 