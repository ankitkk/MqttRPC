#pragma once
#include <map>
#include <functional>
#include <mosquittopp.h>
#include "Shared.h"

namespace mqtt
{
    struct AsyncData
    {
        AsyncData() :
            payload(nullptr) {}

        ~AsyncData()
        {}

        std::unique_ptr<shared::PayLoadType> payload;
        std::string topic;

        MemoryPoolTrait(AsyncData, 1024)
    };

    class MQTT : public mosqpp::mosquittopp
    {
    public:
        MQTT(){};

        void Connect(const std::string& clientid, const std::string& ip, const int port);
        void Subscribe(const std::string& topic, std::function<void(const shared::PayLoadSharedPtr,const std::string& topic)> message_handler);
        void PublishAsync(const std::string& topic, shared::PayLoadPtr);
        void Loop();

        static MQTT& Instance();

    private:

        virtual void on_connect(int rc) override;
        virtual void on_disconnect(int rc) override;
        virtual void on_message(const struct mosquitto_message *message) override;

        // Message Handlers. 
        std::multimap<std::string, std::function<void(std::shared_ptr<shared::PayLoadType>, const std::string& topic)>> MessageHandlers;
        // Async Publish queue. 
        shared::bounded_queue<AsyncData*> ToPublishQueue;
    };

}
