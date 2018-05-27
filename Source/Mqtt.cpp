#include "Mqtt.h"
#include <mosquitto.h>
#include <algorithm>
#include <cassert>

namespace mqtt
{
    void MQTT::on_connect(int rc)
    {
        // @todo. 
    }

    void MQTT::on_disconnect(int rc)
    {
        // @todo. 
    }

    MQTT& MQTT::Instance()
    {
        static MQTT Broker;
        return Broker;
    }

    void MQTT::Connect(const std::string& clientid, const std::string& ip, const int port)
    {
        mosqpp::lib_init();
        reinitialise(clientid.data(), true);
        auto res = connect(ip.data(), port, 60);
        assert(res == MOSQ_ERR_SUCCESS);
    }

    void MQTT::Subscribe(const std::string& topic, std::function<void(const shared::PayLoadSharedPtr, const std::string& topic)> message_handler)
    {
        MessageHandlers.insert(std::make_pair(topic, message_handler));
        int RetVal = subscribe(NULL, topic.data(), 0);
    }

    void MQTT::PublishAsync(const std::string& topic, shared::PayLoadPtr payload)
    {
        auto Ptr = new  AsyncData();
        Ptr->payload = std::move(payload);
        Ptr->topic = topic; 
        ToPublishQueue.enqueue(std::move(Ptr));
    }

    void MQTT::Loop()
    {
        {
            // publish whatever is queued, one at a time. 
            AsyncData* data = nullptr;
            if (ToPublishQueue.try_dequeue(data))
            {
                int res = publish(nullptr, data->topic.c_str(), (int)data->payload->size(), data->payload->data(), 0, false);
                assert(res == MOSQ_ERR_SUCCESS);
                delete data;
            }
        }
        {
            // tick mqtt. 
            // add reconnect logic here. @todo.
            int res = loop(0);
            assert(res == MOSQ_ERR_SUCCESS);
            // @todo - reconnect logic. 
        }
    }
    void MQTT::on_message(const mosquitto_message *message)
    {
        std::shared_ptr<shared::PayLoadType> payload(new shared::PayLoadType);
        std::copy((uint8_t*)message->payload, (uint8_t*)message->payload + message->payloadlen, std::back_inserter(*payload));
        std::string Key(message->topic);

        if (MessageHandlers.find(Key) != MessageHandlers.end())
        {
            for (auto KVP : MessageHandlers)
            {
                bool match = false;
                mosquitto_topic_matches_sub(KVP.first.data(), Key.data(), &match);
                if (match) {
                    Key = KVP.first;
                    break;
                }
            }
        }

        auto range = MessageHandlers.equal_range(Key);

        for (auto it = range.first; it != range.second; ++it)
        {
            it->second(payload, message->topic);
        }
    }
}