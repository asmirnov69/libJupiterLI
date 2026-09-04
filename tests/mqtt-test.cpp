#include <iostream>
#include <string>
#include <cstring>
#include <vector>

#include <libjupiterli/mqtt-client.h>

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

