#pragma once
#include "nvrhi/nvrhi.h"


#include <iostream>


struct MessageCallback : nvrhi::IMessageCallback
{
    void message(nvrhi::MessageSeverity severity, const char *messageText) override;
};

inline void MessageCallback::message(nvrhi::MessageSeverity severity, const char *messageText)
{
    switch (severity)
    {
        using nvrhi::MessageSeverity;

        case MessageSeverity::Info:
            std::cout << "nvrhi::INFO: " << messageText << std::endl;
            break;
        case MessageSeverity::Warning:
            std::cout << "nvrhi::WARNING: " << messageText << std::endl;
            break;
        case MessageSeverity::Error:
            std::cerr << "nvrhi::ERROR: " << messageText << std::endl;
            break;
        case MessageSeverity::Fatal:
            std::cerr << "nvrhi::FATAL: " << messageText << std::endl;
            break;
        default:
            std::cout << "nvrhi::OTHER " << messageText << std::endl;
    }
}
