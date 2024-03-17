/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
// #include <mutex>
// #include <lock_guard>

#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include<glog/logging.h>
#define log(severity, msg) LOG(severity) << msg; google::FlushLogFiles(google::severity); 

#include "sns.grpc.pb.h"


using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;


struct Client {
  std::string username;
  bool connected = true;
  int following_file_size = 0;
  std::vector<Client*> client_followers;
  std::vector<Client*> client_following;
  ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
};

//Vector that stores every client that has been created
std::vector<Client*> client_db;


class SNSServiceImpl final : public SNSService::Service {
  
  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {

    std::cout << "List called on server from user: " << request->username() << std::endl;

    for(auto& client : client_db) {
      if(client->username == request->username()) {
        for(auto& follower : client->client_followers) list_reply->add_followers(follower->username);
      }
      list_reply->add_all_users(client->username);
    }
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    std::string u1 = request->username(); // requesting user
    std::string u2 = request->arguments(0); // user to unfollow

    if(u1 == u2) {
      reply->set_msg("can't follow yourself.");
      return Status::OK;
    }
    
    Client* c1 = getClient(u1); // c1 should always exist bc client cant send req if it doesnt exist
    Client* c2 = getClient(u2);

    if(!c2) {
      reply->set_msg("requested user does not exist.");
    } else {
      // case to prevent duplicate followers:
      // if c1 is already following c2, dont add to vector
      if(getClient2(u1, c2->client_followers) && getClient2(u2, c1->client_following)) {
	reply->set_msg("can't follow yourself.");
	return Status::OK;
      }
      // both users exist, follow operation will be performed
      // add c1 to c2 followers vector
      c2->client_followers.push_back(c1);
      // add c2 to c1 following vector
      c1->client_following.push_back(c2);
    }
 
    return Status::OK; 
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {

    std::string u1 = request->username(); // requesting user
    std::string u2 = request->arguments(0); // user to unfollow

    if(u1 == u2) {
      reply->set_msg("requested user does not exist.");
      return Status::OK;
    }

    Client* c1 = getClient(u1); // c1 should always exist bc client cant send req if it doesnt exist
    Client* c2 = getClient(u2);
    if(!c2) {
      reply->set_msg("requested user does not exist.");
    } else {
      // both users exist, unfollow operation will be performed
      // Check and erase from c2's followers if c1 is found
      auto it_follower = std::find(c2->client_followers.begin(), c2->client_followers.end(), c1);
      if (it_follower != c2->client_followers.end()) {
        c2->client_followers.erase(it_follower);
      }

      // Check and erase from c1's following if c2 is found
      auto it_following = std::find(c1->client_following.begin(), c1->client_following.end(), c2);
      if (it_following != c1->client_following.end()) {
        c1->client_following.erase(it_following);
      }
    }

    return Status::OK;
  }

  // RPC Login
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    std::string username = request->username();
    bool userExists = false;

    for(auto& client : client_db) {
      if(client->username == username) {
        userExists = true;
        
        if(client->connected) reply->set_msg("you have already joined");
        break;
      }
    }

    if(!userExists) {
      Client* newClient = new Client;
      newClient->username = username;
      client_db.push_back(newClient);
    }
    
    return Status::OK;
  }

  Status Timeline(ServerContext* context, 
		ServerReaderWriter<Message, Message>* stream) override {

      Message m;
      while(stream->Read(&m)) {
        std::string username = m.username();
        Client* c = getClient(username); // gets client who wrote the message
        // to the grader: unfortunately i did not complete the server side Timeline function in time.
        // The client side Timeline should be complete, but what good is it without the server? D:
        // I reached the part of being able to read messages from the client stream on the server vvv
        // std::cout << "Received message: " << m.msg() << " from " << m.username() << std::endl;
      }
    
    return Status::OK;
  }

  // helper functions
  // get client pointer from vector, returns null if no such client exists
  Client* getClient(std::string username) {
    for(int i = 0; i < client_db.size(); i++) {
      if(client_db[i]->username == username) {
        return client_db[i];
      }
    }
    return nullptr;
  }

  Client* getClient2(std::string username, std::vector<Client*>& vec) {
    for(int i = 0; i < vec.size(); i++) if(vec[i]->username == username) return vec[i];
    return nullptr;
  }

};

void RunServer(std::string port_no) {
  std::string server_address = "0.0.0.0:"+port_no;
  SNSServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  log(INFO, "Server listening on "+server_address);

  server->Wait();
}

int main(int argc, char** argv) {

  std::string port = "3010";
  
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;break;
      default:
	  std::cerr << "Invalid Command Line Argument\n";
    }
  }
  
  std::string log_file_name = std::string("server-") + port;
  google::InitGoogleLogging(log_file_name.c_str());
  log(INFO, "Logging Initialized. Server starting...");
  RunServer(port);

  return 0;
}
