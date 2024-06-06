#include "lib/components/socket_client.h"
#include <lib/base/ebase.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>      // Include fcntl.h for fcntl() and related constants
#include <iostream>
#include <cstring>
#include "lib/components/stbzone.h"
// Static method to get the singleton instance
UnixSocketClient& UnixSocketClient::getInstance(const std::string& socketPath) {
    static UnixSocketClient instance(socketPath);
    return instance;
}

// Private constructor
UnixSocketClient::UnixSocketClient(const std::string& socketPath)
    : socketPath(socketPath), clientSocket(-1), isConnected(false), isBusy(false),
    m_polling_timer(eTimer::create(eApp)), latest_translation("")
{
    connectToServer();
    CONNECT(m_polling_timer->timeout, UnixSocketClient::TimerCheck);
    m_polling_timer->start(700, true); // Poll every 200 ms
    //eDebug("[SocketClient] Polling timer started..");
}

// Destructor
UnixSocketClient::~UnixSocketClient() {
    closeConnection();
}

// Connect to the server
void UnixSocketClient::connectToServer() {
    std::lock_guard<std::mutex> lock(connectionMutex);
    if (isConnected) return;

    clientSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        std::cout << "Failed to create socket" << std::endl;

    }

    struct sockaddr_un serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sun_family = AF_UNIX;
    strncpy(serverAddress.sun_path, socketPath.c_str(), sizeof(serverAddress.sun_path) - 1);

    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        close(clientSocket);
        std::cout << "Failed to connect to server: " << strerror(errno) << std::endl;
    }

    isConnected = true;
}

// Send JSON data to the server
void UnixSocketClient::sendJsonData(const std::string& jsonData) {
    isBusy = true;
    std::lock_guard<std::mutex> lock(connectionMutex);
    if (!isConnected) connectToServer();
    sendAll(jsonData + "\n");
    isBusy = false;

}

void UnixSocketClient::TimerCheck() {
    if (!isBusy)
    {
        m_polling_timer->stop();
        std::lock_guard<std::mutex> lock(connectionMutex);
        if (!isConnected) connectToServer();
        sendAll("RESULT\n");
        std::string response = receiveAll();
        if (response != latest_translation && STBZone::GetInstance().subtitle_type =="1")
        {
            latest_translation = response;
            STBZone::GetInstance().translation_result = response;
            STBZone::GetInstance().translation_received = true;
        }
    }
    m_polling_timer->start(700, true); // Restart the timer
}
// Get the latest result from the server
std::string UnixSocketClient::getLatestResult() {
    std::lock_guard<std::mutex> lock(connectionMutex);
    if (!isConnected) connectToServer();
    sendAll("RESULT\n");
    return receiveAll();
}

// Close the connection
void UnixSocketClient::closeConnection() {
    std::lock_guard<std::mutex> lock(connectionMutex);
    if (isConnected) {
        close(clientSocket);
        isConnected = false;
    }
}

// Helper method to send all data
void UnixSocketClient::sendAll(const std::string& message) {
    size_t totalBytesSent = 0;
    size_t messageLength = message.size();
    while (totalBytesSent < messageLength) {
        ssize_t bytesSent = send(clientSocket, message.c_str() + totalBytesSent, messageLength - totalBytesSent, 0);
        if (bytesSent < 0) {
            std::cout << "Failed to send data: " << strerror(errno) << std::endl;
        }
        totalBytesSent += bytesSent;
    }
}

// Helper method to receive all data
std::string UnixSocketClient::receiveAll() {
    char buffer[1024];
    std::string response;
    ssize_t bytesRead;

    // Set the socket to non-blocking mode
    int flags = fcntl(clientSocket, F_GETFL, 0);
    if (flags == -1) {
        std::cout << "Failed to get socket flags: " << strerror(errno) << std::endl;

    }
    if (fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cout << "Failed to set socket to non-blocking: " << strerror(errno) << std::endl;

    }

    while (true) {
        bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // No more data to read
            }
            else {
                std::cout << "Failed to receive data: " << strerror(errno) << std::endl;

            }
        }
        else if (bytesRead == 0) {
            break; // Connection closed
        }
        buffer[bytesRead] = '\0';
        response.append(buffer, bytesRead);
        if (response.find('\n') != std::string::npos) {
            break;
        }
    }

    // Remove trailing newline character
    if (!response.empty() && response.back() == '\n') {
        response.pop_back();
    }

    // Restore socket flags
    if (fcntl(clientSocket, F_SETFL, flags) == -1) {
        std::cout << "Failed to restore socket flags: " << strerror(errno) << std::endl;

    }

    return response;
}