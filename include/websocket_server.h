#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <functional>

class WebSocketServer {
public:
    WebSocketServer(int port);
    ~WebSocketServer();

    // Start the server in a background thread
    void start();

    // Stop the server
    void stop();

    // Broadcast a message to all connected clients
    void broadcast(const std::string& message);

    // Check if any clients are connected
    bool has_clients() const;

    // Get number of connected clients
    int get_client_count() const;

    // Set callback for incoming commands
    void set_command_callback(std::function<void(const std::string&)> callback);

    // Check if paused (thread-safe)
    bool is_paused() const;

private:
    void server_thread();
    void handle_client(int client_socket);
    std::string create_handshake_response(const std::string& key);
    std::vector<uint8_t> create_websocket_frame(const std::string& message);
    std::string parse_websocket_frame(const std::vector<uint8_t>& frame);

    int port;
    int server_socket;
    std::atomic<bool> running;
    std::atomic<bool> paused;
    std::thread server_thread_handle;
    std::vector<int> client_sockets;
    std::mutex clients_mutex;
    std::function<void(const std::string&)> command_callback;
    std::mutex callback_mutex;
};

#endif // WEBSOCKET_SERVER_H
