# C++ WebSocket Chat Server 💬

[![CI](https://github.com/Mudabbirarafat/C-WebSocket-Chat/actions/workflows/ci.yml/badge.svg)](https://github.com/Mudabbirarafat/C-WebSocket-Chat/actions/workflows/ci.yml)

A real-time multi-user chat application built with C++ (Winsock2) backend and modern HTML5/JavaScript frontend with WebSocket support.

## Features ✨

- **Real-time messaging** - Messages broadcast to all connected users instantly
- **User management** - Custom usernames for each client
- **Join/Leave notifications** - See when users connect or disconnect
- **Message history** - New users see the last 20 messages
- **Color-coded usernames** - Each user gets a unique color for easy identification
- **UTF-8 support** - Full support for emojis, special characters, and Unicode
- **Responsive design** - Works on desktop and mobile browsers
- **Beautiful UI** - Modern gradient design with smooth animations

## Project Structure

```
cpp_ws_chat/
├── server.cpp    - WebSocket server (C++ with Winsock2)
├── index.html    - Frontend with embedded CSS & JavaScript
└── README.md     - This file
```

## Requirements

- **Windows OS** (uses Winsock2)
- **GCC/MinGW** with C++17 support
- **Modern web browser** (Chrome, Firefox, Edge, Safari)

## Build Instructions

### Compile the Server

```bash
cd cpp_ws_chat
g++ -std=c++17 -o server.exe server.cpp -lws2_32
```

## Running the Application

### 1. Start the Server
```bash
.\server.exe
```

You should see:
```
[WS] Listening on port 8080
```

### 2. Open in Browser

Visit: **http://localhost:8080**

The server serves `index.html` on HTTP and handles WebSocket upgrades automatically.

## How to Use

1. **Enter your username** in the prompt
2. **Click "Join Chat"** or press Enter
3. **Type messages** and press Enter or click Send
4. **Messages appear instantly** for all connected users
5. **Join/leave notifications** show when users connect or disconnect
6. **Message history** loads automatically for new users

## Multi-User Testing

To test with multiple users:

1. Open the chat in multiple browser windows/tabs
2. Enter different usernames for each
3. Send messages and see them broadcast to all users
4. Watch join/leave notifications appear
5. Check terminal to see all activity

Example terminal output:
```
[WS] Listening on port 8080
[WS] Client connected: 1
[WS] Client 1 set username: Alice
[WS] Client connected: 2
[WS] Client 2 set username: Bob
[Browser→C++] Alice (1): Hello everyone!
[Browser→C++] Bob (2): Hi Alice!
[WS] Client disconnected: 1 (Alice)
```

## Architecture

### Server (C++)
- **Protocol**: WebSocket (RFC 6455)
- **Networking**: Windows Sockets 2 (Winsock2)
- **Threading**: Multi-threaded (one thread per client)
- **Features**:
  - HTTP GET handler for serving index.html
  - WebSocket handshake with SHA1 + Base64 encoding
  - Message frame parsing and construction
  - JSON message formatting
  - UTF-8 text support
  - Message history storage (last 20 messages)
  - User join/leave notifications

### Frontend (HTML5 + JavaScript)
- **WebSocket Connection**: Real-time bidirectional communication
- **JSON Messaging**: Structured message format with types (message, system)
- **Dynamic Colors**: 10 unique colors assigned to users
- **Local Storage**: User color persistence during session
- **Animations**: Smooth slide-in effect for new messages
- **Responsive Layout**: Flexbox-based responsive design

## Message Format

### Client to Server
```json
"plain text message"
```

OR for username:
```json
{"type":"setname","name":"Alice"}
```

### Server to Client

**Regular Message**:
```json
{
  "type": "message",
  "from": "1",
  "nick": "Alice",
  "msg": "Hello world!",
  "serverTs": 1733950000
}
```

**System Message**:
```json
{
  "type": "system",
  "msg": "Alice joined the chat",
  "serverTs": 1733950000
}
```

## Customization

### Change Port
Edit `server.cpp`, line with `#define PORT 8080` to desired port.

### Change Message History Size
Edit `server.cpp`, line with `const int MAX_HISTORY = 20` to store more/fewer messages.

### Add More Username Colors
Edit `index.html` JavaScript, add colors to the `colors` array.

### Modify UI Styling
All CSS is in the `<style>` tag in `index.html` - feel free to customize!

## Limitations

- Local network only (currently runs on localhost)
- Messages stored in memory (lost on server restart)
- No persistence/database
- No user authentication
- No private messaging (broadcasts to all)
- Windows-only (Winsock2 dependency)

## Future Enhancements

- [ ] Cross-platform support (Linux/Mac with BSD sockets)
- [ ] Database persistence
- [ ] User authentication & login
- [ ] Private messages
- [ ] Chat rooms/channels
- [ ] File sharing
- [ ] User typing indicators
- [ ] Message reactions/emojis
- [ ] Admin controls
- [ ] Rate limiting

## Technical Notes

- **SHA1 + Base64**: Used for WebSocket handshake (Sec-WebSocket-Accept header)
- **Mutex Protection**: Thread-safe access to shared client and message history lists
- **Masking**: Incoming WebSocket frames are properly unmasked
- **Frame Format**: Handles both short (≤125 bytes) and extended (126-65535 bytes) payloads
- **UTF-8 Handling**: Full support for multi-byte characters

## Testing

Tested with:
- Windows 10/11
- GCC 11+ (MinGW/WinLibs)
- Chrome, Firefox, Edge browsers
- Multiple concurrent connections (3-4+ clients)

## License

Free to use and modify.

## Author

Built as a demonstration of WebSocket programming in C++ on Windows.

