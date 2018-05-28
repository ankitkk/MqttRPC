# MqttRPC ![MIT](https://img.shields.io/badge/license-MIT-blue.svg)

`MqttRPC` is a RPC library for C++ and uses [MQTT](https://en.wikipedia.org/wiki/MQTT) as its underlying transport. 

While there are many rpc libraries available, they are all typically based on a traditional client-server implemention. 
This forces one-to-one relationships between entities ultimately leading to scalabiltiy issues for larger distributed systems with both one-to-one and one-to-many relationships between entities.

`MqttRPC` tries to introduce a concept of a `peer` and unlike tradtional client-sever relationship it is bidrectional and supports one-to-many and many-to-many rpc.

`MqttRPC` doesn't use any IDL and is _extremely_ lightweight. And most importantly, unlike its peers, it doesn't use boost and is threading model agnostic. It is straight forward to embed this in an existing event loop. 

`MqttRPC` is cross platform and been tested with VS 2017 and gcc 6.3. 

## Example

Setup a mqtt broker e.g [mosquitto](https://mosquitto.org/download/) or [emqtt](http://emqtt.io/)
```cpp
#include <iostream>
#include "Rpc.h"

void main()
{
    auto& mqtt_instance = mqtt::MQTT::Instance();
    // connect to mqtt with a unique client id. 
    mqtt_instance.Connect("TestID", "127.0.0.1", 1883);

    bool shut_down = false; 

    // peer connection objects define one side of a connection and can exist on different mqtt clients. 
    rpc::PeerConnection peer_one, peer_two; 

    // if a connection can be defined as A-B
    // one side of the connection will be initialized as 
    peer_one.Init("A", "B");
    // and other as 
    peer_two.Init("B", "A"); 
    // peer_one & peer_two are now connected. 
    // and rpc on each other.

    peer_one.Bind("message", [&](int number) {
        std::cout << "message: number: " << number << std::endl;
        shut_down = true;
    });

    peer_two.Bind("update", [&](const std::string& message,const int number) {
        std::cout << "update:  message: " << message << " number: " << number << std::endl;
    });

    peer_one.Call("update", " Hello ! ", 10);
    peer_two.Call("message",  10);

    while (!shut_down)
    {
        // call this in your event tick, whatever thread. 
        mqtt_instance.Loop();
    }
}
```

`MqttRPC` uses two really awesome projects.

  * [cereal](https://github.com/USCiLab/cereal) A C++11 library for serialization
  * [mosquitto](https://github.com/eclipse/mosquitto) Eclipse Foundation. 

# Current Uses
  * In a cross platform production system with almost 700 entities rpc'ng each other, passing around 500k messages a day. 
# Build 

```
git clone --recursive git@github.com:ankitkk/MqttRPC.git
mkdir build 
cd build 
cmake ..\MqttRPC 
cd ..\MqttRPC ; make SimpleExample; 
./SimpleExample 
```


