#include <iostream>
#include "Rpc.h"

struct Messsage
{
    int x; 
    int y; 
    int z; 

    std::string info;

    // cereal interface. 
    template<class Archive>
    void serialize(Archive & archive)
    {
        archive(x, y, z, info); 
    }
};

std::ostream &operator<<(std::ostream& stream, const Messsage& object) {
    
    stream << "[" << object.x << " " << object.y << " " << object.z <<  " " << object.info << "]"; 
    return stream;
}

int main()
{
    auto& mqtt_instance = mqtt::MQTT::Instance();
    // connect to mqtt with a unique client id. 
    mqtt_instance.Connect("TestID", "127.0.0.1", 1883);

    bool shut_down = false; 
    // 
    // Peer connection objects define one side of a connection
    rpc::PeerConnection peer_one, peer_two, peer_three; 

    // if a connection can be defined as A-B
    // one side of the connection will be initialized as 
    peer_one.Init("A", "B");
    // and other as 
    peer_two.Init("B", "A"); 
    // now you can call "Call" and "Bind" on the other to setup a bidirectional channel for rpc. 

    // a peer can also masquerade as another peer. 
    peer_three.Init("B", "A");

    // peer_one & peer_two are now connected. 
    // and call rpc on each other. 
    peer_two.Bind("Test", [&](const float f,const std::string& str, const Messsage& recieved) {

        std::cout << "Arg 1 " << f << std::endl;
        std::cout << "Arg 2 " << str << std::endl;
        std::cout << "Arg 3 " << recieved << std::endl;

        peer_two.Call("Test_ret", "Done");
    });


    // broadcast from one to two & three. 
    // 
    peer_two.Bind("update", [&](const std::string& message) {
        std::cout << "message to two : " << message << std::endl; 
    });
    // 
    peer_three.Bind("update", [&](const std::string message) {
        std::cout << "message to three : " << message << std::endl;
    });


    // called when peer_two calls "Test_ret"
    peer_one.Bind("Test_ret", [&](const std::string x) {
        std::cout << "Test_ret: " << x << std::endl; 
        shut_down = true; 
    });

    // broadcast to three and two. 
    peer_one.Call("update", " Hello ! ");

    Messsage bottle;

    bottle.x = 1;
    bottle.y = 2;
    bottle.z = 3;
    bottle.info = "This is a test Message";

    peer_one.Call("Test", 0.4f, "string arg", bottle);

    while (!shut_down)
    {
        // call this in your event tick, whatever thread. 
        mqtt_instance.Loop();
    }

    return 0;
}
