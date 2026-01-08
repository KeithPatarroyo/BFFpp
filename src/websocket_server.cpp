#include "websocket_server.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>

// Platform-specific includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define close closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <arpa/inet.h>
#endif

// SHA1 and Base64 for WebSocket handshake
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

static std::string base64_encode(const unsigned char* input, int length) {
    BIO *bio, *b64;
    BUF_MEM *buffer_ptr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &buffer_ptr);

    std::string result(buffer_ptr->data, buffer_ptr->length);
    BIO_free_all(bio);

    return result;
}

WebSocketServer::WebSocketServer(int port)
    : port(port), server_socket(-1), running(false), paused(false) {
}

WebSocketServer::~WebSocketServer() {
    stop();
}

void WebSocketServer::start() {
    if (running) return;

    running = true;
    server_thread_handle = std::thread(&WebSocketServer::server_thread, this);
}

void WebSocketServer::stop() {
    if (!running) return;

    running = false;

    // Close all client sockets
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (int sock : client_sockets) {
            close(sock);
        }
        client_sockets.clear();
    }

    // Close server socket
    if (server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }

    // Wait for server thread to finish
    if (server_thread_handle.joinable()) {
        server_thread_handle.join();
    }
}

void WebSocketServer::broadcast(const std::string& message) {
    std::vector<uint8_t> frame = create_websocket_frame(message);

    std::lock_guard<std::mutex> lock(clients_mutex);
    auto it = client_sockets.begin();
    while (it != client_sockets.end()) {
        int sock = *it;
        ssize_t sent = send(sock, frame.data(), frame.size(), 0);
        if (sent <= 0) {
            // Client disconnected
            close(sock);
            it = client_sockets.erase(it);
        } else {
            ++it;
        }
    }
}

bool WebSocketServer::has_clients() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(clients_mutex));
    return !client_sockets.empty();
}

int WebSocketServer::get_client_count() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(clients_mutex));
    return client_sockets.size();
}

void WebSocketServer::server_thread() {
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }

    // Set socket options
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Set non-blocking mode
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(server_socket, FIONBIO, &mode);
#else
    fcntl(server_socket, F_SETFL, O_NONBLOCK);
#endif

    // Bind socket
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind to port " << port << std::endl;
        close(server_socket);
        return;
    }

    // Listen
    if (listen(server_socket, 5) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
        close(server_socket);
        return;
    }

    std::cout << "WebSocket server listening on port " << port << std::endl;

    // Accept connections
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);

        if (client_socket >= 0) {
            std::thread(&WebSocketServer::handle_client, this, client_socket).detach();
        } else {
            // No connection pending, sleep briefly
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void WebSocketServer::handle_client(int client_socket) {
    // Read HTTP request
    char buffer[4096];
    ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read <= 0) {
        close(client_socket);
        return;
    }

    buffer[bytes_read] = '\0';
    std::string request(buffer);

    // Check if it's a WebSocket upgrade request
    if (request.find("Upgrade: websocket") != std::string::npos) {
        // Extract Sec-WebSocket-Key
        size_t key_pos = request.find("Sec-WebSocket-Key: ");
        if (key_pos != std::string::npos) {
            key_pos += 19;
            size_t key_end = request.find("\r\n", key_pos);
            std::string key = request.substr(key_pos, key_end - key_pos);

            // Send handshake response
            std::string response = create_handshake_response(key);
            send(client_socket, response.c_str(), response.length(), 0);

            // Add to client list
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                client_sockets.push_back(client_socket);
                std::cout << "WebSocket client connected (" << client_sockets.size() << " total)" << std::endl;
            }

            // Keep reading messages from client
            while (running) {
                std::vector<uint8_t> frame_buffer(4096);
                ssize_t bytes = recv(client_socket, frame_buffer.data(), frame_buffer.size(), 0);

                if (bytes <= 0) {
                    // Client disconnected
                    break;
                }

                frame_buffer.resize(bytes);
                std::string message = parse_websocket_frame(frame_buffer);

                if (!message.empty()) {
                    // Handle command
                    if (message == "pause") {
                        paused = true;
                        std::cout << "Simulation paused by client" << std::endl;
                    } else if (message == "play") {
                        paused = false;
                        std::cout << "Simulation resumed by client" << std::endl;
                    }

                    // Invoke callback if set
                    {
                        std::lock_guard<std::mutex> lock(callback_mutex);
                        if (command_callback) {
                            command_callback(message);
                        }
                    }
                }
            }

            // Remove client from list
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                auto it = std::find(client_sockets.begin(), client_sockets.end(), client_socket);
                if (it != client_sockets.end()) {
                    client_sockets.erase(it);
                    std::cout << "WebSocket client disconnected (" << client_sockets.size() << " remaining)" << std::endl;
                }
            }
            close(client_socket);
            return;
        }
    }

    // Not a WebSocket connection, close it
    close(client_socket);
}

std::string WebSocketServer::create_handshake_response(const std::string& key) {
    const std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string accept_key = key + magic;

    // SHA1 hash
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(accept_key.c_str()), accept_key.length(), hash);

    // Base64 encode
    std::string accept = base64_encode(hash, SHA_DIGEST_LENGTH);

    // Create response
    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n";
    response << "Upgrade: websocket\r\n";
    response << "Connection: Upgrade\r\n";
    response << "Sec-WebSocket-Accept: " << accept << "\r\n";
    response << "\r\n";

    return response.str();
}

std::vector<uint8_t> WebSocketServer::create_websocket_frame(const std::string& message) {
    std::vector<uint8_t> frame;

    // Opcode: 1 (text frame), FIN: 1
    frame.push_back(0x81);

    // Payload length
    size_t len = message.length();
    if (len <= 125) {
        frame.push_back(static_cast<uint8_t>(len));
    } else if (len <= 65535) {
        frame.push_back(126);
        frame.push_back((len >> 8) & 0xFF);
        frame.push_back(len & 0xFF);
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--) {
            frame.push_back((len >> (i * 8)) & 0xFF);
        }
    }

    // Payload data
    frame.insert(frame.end(), message.begin(), message.end());

    return frame;
}

void WebSocketServer::set_command_callback(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(callback_mutex);
    command_callback = callback;
}

bool WebSocketServer::is_paused() const {
    return paused;
}

std::string WebSocketServer::parse_websocket_frame(const std::vector<uint8_t>& frame) {
    if (frame.size() < 2) return "";

    // Check if message is masked (from client)
    bool masked = (frame[1] & 0x80) != 0;
    uint64_t payload_len = frame[1] & 0x7F;
    size_t pos = 2;

    // Extended payload length
    if (payload_len == 126) {
        if (frame.size() < 4) return "";
        payload_len = (frame[2] << 8) | frame[3];
        pos = 4;
    } else if (payload_len == 127) {
        if (frame.size() < 10) return "";
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | frame[2 + i];
        }
        pos = 10;
    }

    // Masking key (if masked)
    uint8_t mask[4] = {0};
    if (masked) {
        if (frame.size() < pos + 4) return "";
        for (int i = 0; i < 4; i++) {
            mask[i] = frame[pos + i];
        }
        pos += 4;
    }

    // Decode payload
    std::string message;
    for (size_t i = 0; i < payload_len && (pos + i) < frame.size(); i++) {
        if (masked) {
            message += static_cast<char>(frame[pos + i] ^ mask[i % 4]);
        } else {
            message += static_cast<char>(frame[pos + i]);
        }
    }

    return message;
}
