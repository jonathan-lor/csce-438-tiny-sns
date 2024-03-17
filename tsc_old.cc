#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <string>
#include <utility>
#include <cctype>
#include <unistd.h>
#include <csignal>
#include <grpc++/grpc++.h>
#include "client.h"

#include "sns.grpc.pb.h"
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;

void sig_ignore(int sig) {
  std::cout << "Signal caught " + sig;
}

Message MakeMessage(const std::string& username, const std::string& msg) {
    Message m;
    m.set_username(username);
    m.set_msg(msg);
    google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
    timestamp->set_seconds(time(NULL));
    timestamp->set_nanos(0);
    m.set_allocated_timestamp(timestamp);
    return m;
}


class Client : public IClient
{
public:
  Client(const std::string& hname,
	 const std::string& uname,
	 const std::string& p)
    :hostname(hname), username(uname), port(p) {}

  
protected:
  virtual int connectTo();
  virtual IReply processCommand(std::string& input);
  virtual void processTimeline();

private:
  std::string hostname;
  std::string username;
  std::string port;
  
  // You can have an instance of the client stub
  // as a member variable.
  std::unique_ptr<SNSService::Stub> stub_;
  
  IReply Login();
  IReply List();
  IReply Follow(const std::string &username);
  IReply UnFollow(const std::string &username);
  void   Timeline(const std::string &username);

  // helper functions
  std::pair<std::string, std::string> split(std::string& input, char delim);
};


int Client::connectTo()
{


  std::string login_info = hostname + ":" + port;
  // Create a channel using the server address and insecure credentials
  auto channel = grpc::CreateChannel(login_info, grpc::InsecureChannelCredentials());
  // Create a stub using the channel
  stub_ = csce438::SNSService::NewStub(channel);

  auto reply = Login();
  if(!reply.grpc_status.ok() || reply.comm_status != SUCCESS) return -1;
  //////////////////////////////////////////////////////////
  return 1;
}

IReply Client::processCommand(std::string& input)
{

  IReply ire;
  // read commands from input for processing (pretty sure getCommand validates input):
  auto [cmd, arg] = split(input, ' ');

  // std::cout << "Command: " << cmd << std::endl;
  // std::cout << "Arg: " << arg << std::endl;
  if(cmd == "FOLLOW") {
    return Follow(arg);
  } else if(cmd == "UNFOLLOW") {
    return UnFollow(arg);
  } else if(cmd == "LIST") {
    return List();
  } else if (cmd == "TIMELINE") {
    ire.comm_status = SUCCESS;
    return ire;
  }
    return ire;
}


void Client::processTimeline()
{
    Timeline(username);
}

// List Command
IReply Client::List() {
    IReply ire;
    Request request; // Assuming the Request may carry some user info if needed
    request.set_username(username);
    ListReply listReply;
    grpc::ClientContext context;

    grpc::Status status = stub_->List(&context, request, &listReply);

    ire.grpc_status = status;
    if (status.ok()) {
        for(auto& u : listReply.all_users()) ire.all_users.push_back(u);
        for(auto& f : listReply.followers()) ire.followers.push_back(f);
        ire.comm_status = SUCCESS;
    } else {
        ire.comm_status = FAILURE_UNKNOWN;
    }

    return ire;
}

// Follow Command        
IReply Client::Follow(const std::string& username2) {

    IReply ire;
    Request request;
    request.set_username(username);
    request.add_arguments(username2);
    Reply reply;
    grpc::ClientContext context;

    grpc::Status status = stub_->Follow(&context, request, &reply);

    ire.grpc_status = status;
    if (status.ok()) {
        if(reply.msg() == "requested user does not exist.") {
            ire.comm_status = FAILURE_INVALID_USERNAME;
        } else if (reply.msg() == "can't follow yourself.") {
            ire.comm_status = FAILURE_ALREADY_EXISTS;
        } else {
          ire.comm_status = SUCCESS;
        }
    } else {
        ire.comm_status = FAILURE_UNKNOWN;
    }

    return ire;
}

// UNFollow Command  
IReply Client::UnFollow(const std::string& username2) {
    IReply ire;
    Request request;
    request.set_username(username);
    request.add_arguments(username2);
    Reply reply;
    grpc::ClientContext context;

    grpc::Status status = stub_->UnFollow(&context, request, &reply);

    ire.grpc_status = status;
    if (status.ok()) {
        if(reply.msg() == "requested user does not exist.") {
            ire.comm_status = FAILURE_INVALID_USERNAME;
        } else if (reply.msg() == "can't follow yourself.") {
            ire.comm_status = FAILURE_ALREADY_EXISTS;
        } else {
          ire.comm_status = SUCCESS;
        }
    } else {
        ire.comm_status = FAILURE_UNKNOWN;
    }

    return ire;
}

// Login Command  
IReply Client::Login() {

    IReply ire;
    // we'll call login with our stub to see if we can successfully connect to the server

    Request request;
    request.set_username(username);
    Reply reply;
    ClientContext context;

    grpc::Status status = stub_->Login(&context, request, &reply);

    ire.grpc_status = status;

    if(status.ok()) {
      if(reply.msg() == "you have already joined") {
        ire.comm_status = FAILURE_ALREADY_EXISTS;
      } else {
        std::cout << "Connection succeeded: " << reply.msg() << std::endl;
        ire.comm_status = SUCCESS;
      }
    } else {
      std::cerr << "RPC failed: " << status.error_message() << std::endl;
      ire.comm_status = FAILURE_UNKNOWN;
    }

    return ire;
}

// Timeline Command
void Client::Timeline(const std::string& username) {

    // ------------------------------------------------------------
    // In this function, you are supposed to get into timeline mode.
    // You may need to call a service method to communicate with
    // the server. Use getPostMessage/displayPostMessage functions 
    // in client.cc file for both getting and displaying messages 
    // in timeline mode.
    // ------------------------------------------------------------

    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    //
    // Once a user enter to timeline mode , there is no way
    // to command mode. You don't have to worry about this situation,
    // and you can terminate the client program by pressing
    // CTRL-C (SIGINT)
    // ------------------------------------------------------------
  
    grpc::ClientContext context;

    std::shared_ptr<ClientReaderWriter<Message, Message>> stream(
        stub_->Timeline(&context));

    std::thread reader_thread([stream]() {
      Message message;
      while (stream->Read(&message)) {
        const google::protobuf::Timestamp& timestamp = message.timestamp();
        std::time_t time = timestamp.seconds();
        // std::cout << "dispalying from reader thread" << std::endl;
        displayPostMessage(message.username(), message.msg(), time);
      }
    });

    std::thread writer_thread([stream, username]() {
      while(true) {
        std::string input = getPostMessage();
        Message messageToWrite = MakeMessage(username, input);
        stream->Write(messageToWrite);
      }
      stream->WritesDone();

    });

    reader_thread.join();
    writer_thread.join();

    grpc::Status status = stream->Finish();
    if(!status.ok()) std::cerr << "Timeline ran into an error: " << status.error_message() << std::endl;
}

// splitting input; used in processCommand()
std::pair<std::string, std::string> Client::split(std::string& input, char delim) {
  std::pair<std::string, std::string> res;
  size_t pos = input.find(delim);

  if(pos != std::string::npos) {
    res.first = input.substr(0, pos);
    res.second = input.substr(pos + 1);
  } else {
    res.first = input;
  }
  return res;
}



//////////////////////////////////////////////
// Main Function
/////////////////////////////////////////////
int main(int argc, char** argv) {

  std::string hostname = "localhost";
  std::string username = "default";
  std::string port = "3010";
    
  int opt = 0;
  while ((opt = getopt(argc, argv, "h:u:p:")) != -1){
    switch(opt) {
    case 'h':
      hostname = optarg;break;
    case 'u':
      username = optarg;break;
    case 'p':
      port = optarg;break;
    default:
      std::cout << "Invalid Command Line Argument\n";
    }
  }
      
  std::cout << "Logging Initialized. Client starting...";
  
  Client myc(hostname, username, port);
  
  myc.run();
  
  return 0;
}
