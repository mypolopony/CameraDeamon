//  main.cpp
//  pubsub
//
//  Created by abraham on 9/8/16.
//  Copyright © 2016 agridata. All rights reserved.
//

#include <iostream>
#include "zmq.hpp"

using namespace std;

int main() {
    zmq::context_t context(1);
    zmq::socket_t subscriber(context, ZMQ_SUB);
    subscriber.connect("tcp://localhost:4999");

    zmq::socket_t server(context, ZMQ_PUB);
    server.bind("tcp://*:4998");
    subscriber.setsockopt(ZMQ_SUBSCRIBE, "", 0);
    zmq_sleep(.5);


    zmq::message_t update;

    std::string id_hash;


    while (1) {
        subscriber.recv(&update, ZMQ_NOBLOCK);
        // convert non null terminating cstring into a string
        string received = string(static_cast<const char*>(update.data()), update.size());

        if (!received.empty()) {
                    cout << received << endl;

                    id_hash = received.substr(0, received.find('_')); // note the index of the delimiter. this is only for the first


                    string reply = id_hash + "_" + "This is a message from the c++ code";

                    cout << reply << endl;
                    zmq::message_t messageS(reply.size());
                    memcpy(messageS.data(), reply.data(), reply.size());
                    server.send(messageS);
        }
    }

return 0;
}
