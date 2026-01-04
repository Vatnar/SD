#pragma once
#include "nvrhi/nvrhi.h"

struct MessageCallback : nvrhi::IMessageCallback
{
    void message(nvrhi::MessageSeverity severity, const char *messageText) override;
};


inline void MessageCallback::message(const nvrhi::MessageSeverity severity, const char *messageText)
{
    const auto logger = spdlog::get("nvrhi");
    switch (severity)
    {
        using nvrhi::MessageSeverity;

        case MessageSeverity::Info:
            logger->info(messageText);
            break;
        case MessageSeverity::Warning:
            logger->warn(messageText);
            break;
        case MessageSeverity::Error:
            logger->error(messageText);
            break;
        case MessageSeverity::Fatal:
            Engine::Abort(std::format("NVRHI: {}", messageText));
            break;
        default:
            logger->info(messageText);
    }
}
