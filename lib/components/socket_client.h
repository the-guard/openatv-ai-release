#ifndef UNIX_SOCKET_CLIENT_H
#define UNIX_SOCKET_CLIENT_H

#include <string>
#include <mutex>
#include <lib/base/ebase.h>
#include <lib/base/smartptr.h>
class UnixSocketClient {
public:
    // Public static method to get the singleton instance
    static UnixSocketClient& getInstance(const std::string& socketPath);

    void sendJsonData(const std::string& jsonData);
    std::string getLatestResult();
    void closeConnection();

private:
    // Private constructor to prevent instantiation
    UnixSocketClient(const std::string& socketPath);
    ~UnixSocketClient();

    // Delete copy constructor and assignment operator
    UnixSocketClient(const UnixSocketClient&) = delete;
    UnixSocketClient& operator=(const UnixSocketClient&) = delete;

    int clientSocket;
    std::string socketPath;
    bool isConnected;
    std::mutex connectionMutex;

    void connectToServer();
    void sendAll(const std::string& message);
    std::string receiveAll();
    std::string latest_translation;
    void TimerCheck();
    ePtr<eTimer> m_polling_timer;
    bool isBusy;
};

#endif // UNIX_SOCKET_CLIENT_H
