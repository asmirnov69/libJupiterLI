#include <iostream>
#include <string>
#include <cstring>
#include <vector>

// POSIX Networking Headers
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

// Paho Embedded MQTTPacket Headers
#include "MQTTPacket.h"

// Define a structured context to manage our threadless state engine
struct MqttSyncClient {
    int socket_fd = -1;
    unsigned char buffer[2048]; // Stack buffer for encoding packages
    int buffer_len = sizeof(buffer);

    bool connect_to_broker(const std::string& ip, int port, const std::string& client_id) {
        socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd < 0) return false;

        struct sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
            close(socket_fd);
            return false;
        }

        if (connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(socket_fd);
            return false;
        }

        // Serialize and send CONNECT frame directly down the wire
        MQTTPacket_connectData options = MQTTPacket_connectData_initializer;
        options.MQTTVersion = 4;
        options.clientID.cstring = const_cast<char*>(client_id.c_str());
        options.keepAliveInterval = 60;
        options.cleansession = 1;

        int len = MQTTSerialize_connect(buffer, buffer_len, &options);
        if (len <= 0 || write(socket_fd, buffer, len) < 0) {
            close(socket_fd);
            return false;
        }

        std::cout << "[MQTT] Connected cleanly to broker." << std::endl;
        return true;
    }

    bool publish_string(const std::string& topic, const std::string& message) {
        MQTTString topic_str = MQTTString_initializer;
        topic_str.cstring = const_cast<char*>(topic.c_str());

        // Serialize a pure QoS 0 payload into our reusable stack memory block
        int len = MQTTSerialize_publish(
            buffer, buffer_len, 0, 0, 0, 0, topic_str,
            reinterpret_cast<unsigned char*>(const_cast<char*>(message.data())), 
            message.size()
        );

        if (len <= 0) return false;

        // Directly flush the raw byte stream out to the TCP socket descriptor
        ssize_t bytes_sent = write(socket_fd, buffer, len);
        return (bytes_sent == len);
    }

    void disconnect_and_close() {
        if (socket_fd >= 0) {
            int len = MQTTSerialize_disconnect(buffer, buffer_len);
            if (len > 0) {
                write(socket_fd, buffer, len); // Send polite disconnect signal
            }
            close(socket_fd);
            socket_fd = -1;
            std::cout << "[MQTT] Disconnected cleanly and closed resource descriptors." << std::endl;
        }
    }
};

int main() {
    MqttSyncClient client;

    // 1. Connect ONCE at initialization sequence boundaries
    if (!client.connect_to_broker("127.0.0.1", 1883, "repetitive_sync_pub")) {
        std::cerr << "Initialization setup error connecting to destination target." << std::endl;
        return 1;
    }

    // Example Dataset matching your processing workflow logic
    std::vector<std::pair<std::string, std::string>> telemetry_queue = {
        {"device/engine/temp", "85.4C"},
        {"device/engine/rpm", "3200"},
        {"device/status/code", "OK"},
        {"device/gps/coords", "42.27,-71.46"}
    };

    // 2. Perform sequential, rapid fire repetitions across distinct targets
    // Each transaction finishes execution scope linearly without blocking thread loops
    for (const auto& data_point : telemetry_queue) {
        if (client.publish_string(data_point.first, data_point.second)) {
            std::cout << "Successfully pushed data directly to " << data_point.first << std::endl;
        } else {
            std::cerr << "Socket write failed for " << data_point.first << std::endl;
            break; // Network layer drop detected, exit context cleanly
        }
    }

    // 3. Tear down context container safely upon final application wrap up
    client.disconnect_and_close();
    return 0;
}

