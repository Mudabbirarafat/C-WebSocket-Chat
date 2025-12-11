#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <string>
#include <cstring>
#include <ctime>
#include <fstream>
#pragma comment(lib, "ws2_32.lib")

// Simple SHA1 + Base64
#include <stdint.h>

std::string sha1(const std::string &input);
std::string base64(const std::string &in);

#define PORT 8080

struct Client { SOCKET sock; std::string id; std::string username; };
std::vector<Client> clients;
std::mutex clients_mtx;
std::vector<std::string> messageHistory;
std::mutex history_mtx;
const int MAX_HISTORY = 20;
int nextId = 1;

std::string make_id() { return std::to_string(nextId++); }

std::string get_header_value(const std::string &headers, const std::string &name) {
    std::istringstream ss(headers);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto pos = line.find(':');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0,pos);
        std::string val = line.substr(pos+1);
        // trim
        while (!key.empty() && isspace((unsigned char)key.front())) key.erase(key.begin());
        while (!key.empty() && isspace((unsigned char)key.back())) key.pop_back();
        while (!val.empty() && isspace((unsigned char)val.front())) val.erase(val.begin());
        while (!val.empty() && isspace((unsigned char)val.back())) val.pop_back();
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        if (key == name) return val;
    }
    return "";
}

std::string compute_accept_key(const std::string &key) {
    const std::string GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combo = key + GUID;
    std::string hash = sha1(combo);
    std::string b64 = base64(hash);
    return b64;
}

bool recv_ws_message(SOCKET s, std::string &out) {
    out.clear();
    unsigned char header[2];
    int n = recv(s, (char*)header, 2, 0);
    if (n != 2) return false;
    bool fin = (header[0] & 0x80) != 0;
    unsigned char opcode = header[0] & 0x0F;
    bool masked = (header[1] & 0x80) != 0;
    uint64_t payload_len = header[1] & 0x7F;
    
    if (payload_len == 126) {
        unsigned char ext[2];
        if (recv(s, (char*)ext, 2, 0) != 2) return false;
        payload_len = (ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        unsigned char ext[8];
        if (recv(s, (char*)ext, 8, 0) != 8) return false;
        payload_len = 0;
        for (int i = 0; i < 8; ++i) payload_len = (payload_len << 8) | ext[i];
    }
    
    unsigned char mask_key[4] = {0,0,0,0};
    if (masked) {
        if (recv(s, (char*)mask_key, 4, 0) != 4) return false;
    }
    
    std::string data;
    data.resize(payload_len);
    size_t received = 0;
    while (received < payload_len) {
        int chunk = recv(s, &data[received], (int)(payload_len - received), 0);
        if (chunk <= 0) return false;
        received += chunk;
    }
    
    if (masked) {
        for (uint64_t i = 0; i < payload_len; ++i) data[i] = data[i] ^ mask_key[i % 4];
    }
    
    out = data;
    return true;
}

void send_frame(SOCKET s, const std::string &payload) {
    std::string frame;
    frame.push_back((char)0x81);
    size_t len = payload.size();
    if (len <= 125) {
        frame.push_back((char)len);
    } else if (len <= 65535) {
        frame.push_back((char)126);
        frame.push_back((char)((len >> 8) & 0xFF));
        frame.push_back((char)(len & 0xFF));
    } else {
        frame.push_back((char)127);
        for (int i = 7; i >= 0; --i) frame.push_back((char)((len >> (8*i)) & 0xFF));
    }
    frame += payload;
    send(s, frame.c_str(), (int)frame.size(), 0);
}

void broadcast_frame(const std::string &payload, const std::string &exceptId="") {
    std::lock_guard<std::mutex> lock(clients_mtx);
    for (auto &c : clients) {
        if (c.id == exceptId) continue;
        send_frame(c.sock, payload);
    }
}

void save_to_history(const std::string &msg) {
    std::lock_guard<std::mutex> lock(history_mtx);
    messageHistory.push_back(msg);
    if (messageHistory.size() > MAX_HISTORY) {
        messageHistory.erase(messageHistory.begin());
    }
}

std::string escape_json(const std::string &s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                out += c;
        }
    }
    return out;
}

// SHA1 and base64 same as earlier
typedef uint32_t u32;
typedef uint64_t u64;
std::string sha1(const std::string &input) {
    u64 ml = input.size() * 8ULL;
    std::string data = input;
    data.push_back((char)0x80);
    while ((data.size() % 64) != 56) data.push_back(0);
    for (int i = 7; i >= 0; --i) data.push_back((char)((ml >> (i*8)) & 0xFF));
    u32 h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;
    for (size_t chunk = 0; chunk < data.size(); chunk += 64) {
        u32 w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = ((unsigned char)data[chunk + i*4] << 24) | ((unsigned char)data[chunk + i*4 +1] << 16)
                 | ((unsigned char)data[chunk + i*4 +2] << 8) | ((unsigned char)data[chunk + i*4 +3]);
        }
        for (int i = 16; i < 80; ++i) {
            u32 val = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
            w[i] = (val << 1) | (val >> 31);
        }
        u32 a=h0,b=h1,c=h2,d=h3,e=h4;
        for (int i=0;i<80;i++){
            u32 f,k;
            if (i<20){ f=(b&c)|((~b)&d); k=0x5A827999; }
            else if (i<40){ f=b^c^d; k=0x6ED9EBA1; }
            else if (i<60){ f=(b&c)|(b&d)|(c&d); k=0x8F1BBCDC; }
            else { f=b^c^d; k=0xCA62C1D6; }
            u32 temp = ((a<<5)|(a>>27)) + f + e + k + w[i];
            e=d; d=c; c=(b<<30)|(b>>2); b=a; a=temp;
        }
        h0+=a; h1+=b; h2+=c; h3+=d; h4+=e;
    }
    std::ostringstream os;
    for (u32 v : {h0,h1,h2,h3,h4}) {
        os << (char)((v>>24)&0xFF) << (char)((v>>16)&0xFF) << (char)((v>>8)&0xFF) << (char)(v&0xFF);
    }
    return os.str();
}
static const std::string b64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
std::string base64(const std::string &in) {
    std::string out;
    int val=0,valb=-6;
    for (unsigned char c : in) {
        val = (val<<8) + c;
        valb += 8;
        while (valb>=0){ out.push_back(b64_chars[(val>>valb)&0x3F]); valb-=6; }
    }
    if (valb>-6) out.push_back(b64_chars[((val<<8)>>(valb+8))&0x3F]);
    while (out.size()%4) out.push_back('=');
    return out;
}

void handle_client(SOCKET clientSock, std::string clientId) {
    // receive HTTP request
    char buf[4096];
    int r = recv(clientSock, buf, sizeof(buf), 0);
    if (r <= 0) { closesocket(clientSock); return; }
    std::string headers(buf, buf+r);
    
    // Check if this is a WebSocket upgrade request
    std::string key = get_header_value(headers, "sec-websocket-key");
    
    if (key.empty()) {
        // Handle HTTP GET request for index.html
        if (headers.find("GET") == 0) {
            std::ifstream file("index.html", std::ios::binary);
            if (file.good()) {
                std::string html((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                file.close();
                std::ostringstream reply;
                reply << "HTTP/1.1 200 OK\r\n";
                reply << "Content-Type: text/html; charset=utf-8\r\n";
                reply << "Content-Length: " << html.size() << "\r\n";
                reply << "Connection: close\r\n\r\n";
                std::string rep = reply.str();
                send(clientSock, rep.c_str(), (int)rep.size(), 0);
                send(clientSock, html.c_str(), (int)html.size(), 0);
            } else {
                std::ostringstream reply;
                reply << "HTTP/1.1 404 Not Found\r\n\r\n";
                std::string rep = reply.str();
                send(clientSock, rep.c_str(), (int)rep.size(), 0);
            }
        }
        closesocket(clientSock);
        return;
    }
    
    // Handle WebSocket upgrade
    std::string accept = compute_accept_key(key);
    std::ostringstream reply;
    reply << "HTTP/1.1 101 Switching Protocols\r\n";
    reply << "Upgrade: websocket\r\n";
    reply << "Connection: Upgrade\r\n";
    reply << "Sec-WebSocket-Accept: " << accept << "\r\n\r\n";
    std::string rep = reply.str();
    send(clientSock, rep.c_str(), (int)rep.size(), 0);

    {
        std::lock_guard<std::mutex> lock(clients_mtx);
        clients.push_back({clientSock, clientId, ""});
    }
    std::cout << "[WS] Client connected: " << clientId << std::endl;
    
    // Wait for username
    std::string nameMsg;
    if (!recv_ws_message(clientSock, nameMsg)) {
        closesocket(clientSock);
        std::lock_guard<std::mutex> lock(clients_mtx);
        clients.erase(std::remove_if(clients.begin(), clients.end(), [&](const Client &c){ return c.id==clientId; }), clients.end());
        return;
    }
    
    std::string username = "Anonymous";
    try {
        size_t nameStart = nameMsg.find("\"name\":\"");
        if (nameStart != std::string::npos) {
            nameStart += 8;
            size_t nameEnd = nameMsg.find("\"", nameStart);
            if (nameEnd != std::string::npos) {
                username = nameMsg.substr(nameStart, nameEnd - nameStart);
            }
        }
    } catch (...) {}
    
    // Update client username
    {
        std::lock_guard<std::mutex> lock(clients_mtx);
        for (auto &c : clients) {
            if (c.id == clientId) {
                c.username = username;
                break;
            }
        }
    }
    std::cout << "[WS] Client " << clientId << " set username: " << username << std::endl;
    
    // Send message history to new client
    {
        std::lock_guard<std::mutex> lock(history_mtx);
        for (const auto &msg : messageHistory) {
            send_frame(clientSock, msg);
        }
    }
    
    // Broadcast join message
    std::ostringstream joinMsg;
    joinMsg << "{"
            << "\"type\":\"system\","
            << "\"msg\":\"" << escape_json(username) << " joined the chat\","
            << "\"serverTs\":" << (long long)time(NULL)
            << "}";
    std::string joinStr = joinMsg.str();
    broadcast_frame(joinStr, "");
    save_to_history(joinStr);

    // message loop
    while (true) {
        std::string inmsg;
        if (!recv_ws_message(clientSock, inmsg)) break;
        std::cout << "[Browser→C++] " << username << " (" << clientId << "): " << inmsg << std::endl;
        
        std::string safeMsg = escape_json(inmsg);
        std::ostringstream out;
        out << "{"
            << "\"type\":\"message\","
            << "\"from\":\"" << clientId << "\","
            << "\"nick\":\"" << escape_json(username) << "\","
            << "\"msg\":\"" << safeMsg << "\","
            << "\"serverTs\":" << (long long)time(NULL)
            << "}";
        std::string msgStr = out.str();
        broadcast_frame(msgStr, "");
        save_to_history(msgStr);
    }

    // remove client and broadcast left message
    {
        std::lock_guard<std::mutex> lock(clients_mtx);
        for (auto &c : clients) {
            if (c.id == clientId) {
                username = c.username;
                break;
            }
        }
        clients.erase(std::remove_if(clients.begin(), clients.end(), [&](const Client &c){ return c.id==clientId; }), clients.end());
    }
    
    std::ostringstream leftMsg;
    leftMsg << "{"
            << "\"type\":\"system\","
            << "\"msg\":\"" << escape_json(username) << " left the chat\","
            << "\"serverTs\":" << (long long)time(NULL)
            << "}";
    std::string leftStr = leftMsg.str();
    broadcast_frame(leftStr, "");
    save_to_history(leftStr);
    
    closesocket(clientSock);
    std::cout << "[WS] Client disconnected: " << clientId << " (" << username << ")" << std::endl;
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n"; return 1;
    }
    SOCKET serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock == INVALID_SOCKET) { std::cerr << "socket failed\n"; return 1; }
    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(serverSock, (sockaddr*)&addr, sizeof(addr)) != 0) { std::cerr << "bind failed\n"; return 1; }
    if (listen(serverSock, SOMAXCONN) != 0) { std::cerr << "listen failed\n"; return 1; }
    std::cout << "[WS] Listening on port " << PORT << std::endl;
    while (true) {
        sockaddr_in clientAddr; int clientSize = sizeof(clientAddr);
        SOCKET clientSock = accept(serverSock, (sockaddr*)&clientAddr, &clientSize);
        if (clientSock == INVALID_SOCKET) continue;
        std::string id = make_id();
        std::thread t(handle_client, clientSock, id);
        t.detach();
    }
    closesocket(serverSock);
    WSACleanup();
    return 0;
}
