// -*- c++ -*-
#pragma once

// Define a structured context to manage our threadless state engine
struct MqttSyncClient
{
  int socket_fd = -1;
  unsigned char buffer[2048]; // Stack buffer for encoding packages
  int buffer_len = sizeof(buffer);
  
  bool connect_to_broker(const std::string& ip, int port, const std::string& client_id);
  bool publish_string(const std::string& topic, const std::string& message);
  void disconnect_and_close();
};
