#include <functional>
#include <iostream>
#include <sstream>
#include "Rpc.h"
#include "Shared.h"
#include "Mqtt.h"

namespace rpc
{

    std::string PeerConnection::source_topic_in_progress;

    void PeerConnection::Init(const std::string InYourTopic, const std::string InPeerTopic)
    {
        my_topic        = InYourTopic;
        peer_topic      = InPeerTopic;

        // listen for messages from the peer directed towards me. 
        mqtt::MQTT::Instance().Subscribe(my_topic + "/" + peer_topic,
            [&](const shared::PayLoadSharedPtr payload, const std::string& topic) {

            auto NVPs = payload->data();

            std::stack<std::vector<uint8_t>>  queue;

            std::stringstream ssout(std::stringstream::in | std::stringstream::out | std::stringstream::binary);
            {
                ssout.write((const char*)payload->data(), payload->size());
                InputArchive ar(ssout); // stream to cout
                ar(cereal::make_nvp("mqttrpc", queue));
            }

            std::string ret;

            auto func_name = queue.top();

            std::string name((char*)func_name.data(), func_name.size());

            queue.pop();

            if (function_registry.find(name) != function_registry.end())
            {
                source_topic_in_progress = topic;
                auto Fptr = function_registry[name];
                Fptr(queue, &ret);
                source_topic_in_progress = "";
            }
        }

        ); // topic: from/to
    }
}