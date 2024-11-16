#ifndef MESSAGEBUS
#define MESSAGEBUS

#include <map>
#include <string>
#include <unordered_map>
#include <functional>
#include "common.h"

// Message Bus class for broadcasting messages
class MessageBus
{
public:
    // Function to register a listener
    void subscribe(const std::string &listenerName, std::function<void(const std::string &)> callback);

    // Function to broadcast a message to all listeners
    void broadcast(const std::string &senderName, std::string message);

private:
    std::unordered_map<std::string, std::function<void(const std::string &)>> listeners;
};

#endif