// Bare Bones Emulator v2
//
// Created by Selwyn-Lloyd on 9/9/16.
// Copyright © 2016 AgriData. All rights reserved.
//

// Utilities
#include "zmq.hpp"

// Additional include files.
#include <atomic>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <exception>
#include <thread>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

using namespace std;


vector<string> split(const string &s, char delim) {
    stringstream ss(s);
    string item;
    vector <string>tokens;
    while (getline(ss, item, delim)) {
        tokens.push_back(item);
    }
    return tokens;
}


int main()
{    
    // Listen on port 4999
    zmq::context_t context(1);
    zmq::socket_t client(context, ZMQ_SUB);
    client.connect("tcp://*:4999");
    
    // Publish on 4448
    zmq::socket_t publisher(context, ZMQ_PUB);
    publisher.bind("tcp://*:4998");

    
    zmq_sleep(1.5); // Wait for sockets
    
    bool block = false;
    int ret;
    
    while(true) {
        
        zmq::message_t messageR;
        
        client.recv(&messageR);
        
        std::string recieved = std::string(static_cast<char*>(messageR.data()), messageR.size());
        
        printf("%sn", recieved.c_str());
        
        //Parse the string
        char** argv;
        int argc = 0;
        size_t pos = 0;
        string s;
        char delimiter = '_';
        string reply;

        s = recieved;
        vector <string>tokens = split(s,delimiter);
        ostringstream oss;

        if (!block) {
            string id = tokens[0];
            string parameter = tokens[1];
            string value = tokens[2];
            string response = "";

            if (parameter == "WAIT") {
                usleep(1000);
                response = "OK";
            } else if (parameter == "NOWAIT") {
                cout << "" << endl;
                response = "OK";
            } else {
                response = "FAIL: Invalid Command";
            }   
            
            oss << id << "_" << response;
            reply = oss.str();
        }


        zmq::message_t messageS(reply.size());
        memcpy(messageS.data(), reply.data(), reply.size());
        publisher.send(messageS);
        block = false;
    }
    
    return 0;
}
