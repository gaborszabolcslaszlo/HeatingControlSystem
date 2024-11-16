#include "MessageBus.h"
// Function to register a listener
void MessageBus::subscribe(const std::string &listenerName, std::function<void(const std::string &)> callback)
{
    listeners[listenerName.c_str()] = callback;
}

// Function to broadcast a message to all listeners
void MessageBus::broadcast(const std::string &senderName, std::string message)
{
    for (const auto &listener : listeners)
    {
        if (listener.first != senderName)
        { // Avoid sending the message back to the sender
            listener.second(message);
        }
    }
}
