/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#pragma once
#include <AsyncUDP.h>

#include <map>
#include <string>

#include "SHICommunicator.h"

namespace SHI {

class MulticastHandlerConfig : public Configuration {
 public:
  MulticastHandlerConfig() {}
  explicit MulticastHandlerConfig(const JsonObject &obj);
  void fillData(JsonObject &doc) const override;
  int CONNECT_TIMEOUT = 500;
  int DATA_TIMEOUT = 1000;
  int PORT = 2323;
  std::string multicastAddr = "239.1.23.42";
  std::string firmwareURL = "http://192.168.188.250/esp/firmware/";

 protected:
  int getExpectedCapacity() const override;
};

class MulticastHandler : public Communicator {
 public:
  explicit MulticastHandler(MulticastHandlerConfig &config)
      : Communicator("Multicast"), config(config) {
    registeredHandlers.insert(
        {"UPDATE", [this](AsyncUDPPacket &packet) { updateHandler(&packet); }});
    registeredHandlers.insert(
        {"RESET", [this](AsyncUDPPacket &packet) { resetHandler(&packet); }});
    registeredHandlers.insert(
        {"RECONF", [this](AsyncUDPPacket &packet) { reconfHandler(&packet); }});
    registeredHandlers.insert(
        {"INFO", [this](AsyncUDPPacket &packet) { infoHandler(&packet); }});
    registeredHandlers.insert({"VERSION", [this](AsyncUDPPacket &packet) {
                                 versionHandler(&packet);
                               }});
  }
  void setupCommunication() override;
  void loopCommunication() override;
  void newReading(const MeasurementBundle &reading) override {}
  void newStatus(const Measurement &status, SHIObject *src) override {}
  void addUDPPacketHandler(String trigger, AuPacketHandlerFunction handler);
  const Configuration *getConfig() const override { return &config; }
  bool reconfigure(SHI::Configuration *newConfig) {
    config = castConfig<MulticastHandlerConfig>(newConfig);
    return true;
  }

 private:
  void updateHandler(AsyncUDPPacket *packet);
  void resetHandler(AsyncUDPPacket *packet);
  void reconfHandler(AsyncUDPPacket *spacket);
  void infoHandler(AsyncUDPPacket *packet);
  void versionHandler(AsyncUDPPacket *packet);
  void handleUDPPacket(
      AsyncUDPPacket &packet);  // NOLINT as defined from callback handler
  void sendMulticast(const String &message);
  void updateProgress(size_t a, size_t b);
  bool isUpdateAvailable();
  void startUpdate();
  bool doUpdate = false;
  AsyncUDP udpMulticast;
  MulticastHandlerConfig config;
  std::map<String, AuPacketHandlerFunction> registeredHandlers;
};

}  // namespace SHI
