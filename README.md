# memstore

A lightweight, non-blocking in-memory key-value store implemented in C++ using an event-driven `poll()` loop and a custom binary protocol.

## Features

- **Event-Driven Architecture**: Non-blocking I/O using POSIX `poll()` supporting concurrent client connections.
- **Custom Length-Prefixed Binary Protocol**: Binary-safe serialization for requests and responses with pipelining support.
- **In-Memory Operations**: Fast associative store supporting basic CRUD commands.

## Supported Commands

| Command | Syntax | Description |
|---|---|---|
| `SET` | `SET <key> <value>` | Stores or updates the key with the given value. |
| `GET` | `GET <key>` | Retrieves the value associated with the key. |
| `DELETE` | `DELETE <key>` | Removes the specified key from the store. |

### Status Codes

- `200` (OK): Command succeeded.
- `400` (Bad Request): Malformed command or invalid argument count.
- `404` (Not Found): Key not found.
- `500` (Internal Error): Unexpected server error.

## Protocol Specification

### Request Frame
```
+-------------------+-------------------+--------------------+--------------+-----+
| Total Length (4B) | Arg Count (4B)    | Arg 1 Length (4B)  | Arg 1 Data   | ... |
+-------------------+-------------------+--------------------+--------------+-----+
```

### Response Frame
```
+-------------------+--------------------+------------------------+
| Total Length (4B) | Status Code (4B)   | Response Payload (var) |
+-------------------+--------------------+------------------------+
```

## Build and Run

### Prerequisites
- C++17 compliant compiler (`g++` or `clang++`)
- GNU Make

### Building
```bash
make
```

### Running Server
```bash
./server
```
The server listens on `127.0.0.1:1234` by default.

### Running Client
In a separate terminal:
```bash
./client
```

### Example Usage
```text
Enter your message: SET username alice
Status Code: 200
server says: Data saved successfully

Enter your message: GET username
Status Code: 200
server says: alice

Enter your message: DELETE username
Status Code: 200
server says: Deleted successfully

Enter your message: GET username
Status Code: 404
server says: No such entry found
```