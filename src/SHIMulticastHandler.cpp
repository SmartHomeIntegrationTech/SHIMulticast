/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */
#include "SHIMulticastHandler.h"

#include <Arduino.h>
#include <AsyncUDP.h>
#include <HTTPClient.h>
#include <Update.h>
#include <rom/rtc.h>

#include "SHIHardware.h"

#define C_NODENAME SHI::hw->getNodeName().c_str()

namespace {

PROGMEM const String RESET_SOURCE[] = {
    "NO_MEAN",          "POWERON_RESET",    "SW_RESET",
    "OWDT_RESET",       "DEEPSLEEP_RESET",  "SDIO_RESET",
    "TG0WDT_SYS_RESET", "TG1WDT_SYS_RESET", "RTCWDT_SYS_RESET",
    "INTRUSION_RESET",  "TGWDT_CPU_RESET",  "SW_CPU_RESET",
    "RTCWDT_CPU_RESET", "EXT_CPU_RESET",    "RTCWDT_BROWN_OUT_RESET",
    "RTCWDT_RTC_RESET"};

class StatsVisitor : public SHI::Visitor {
 public:
  void enterVisit(SHI::Sensor *sensor) override {
    for (auto &&stat : sensor->getStatistics()) {
      statString += std::string(sensor->getName()) + "." + stat.first + ":" +
                    stat.second + "\n";
    }
  }
  void enterVisit(SHI::Hardware *hardware) override {
    for (auto &&stat : hardware->getStatistics()) {
      statString += std::string(hardware->getName()) + "." + stat.first + ":" +
                    stat.second + "\n";
    }
  }
  void visit(SHI::Communicator *communicator) override {
    for (auto &&stat : communicator->getStatistics()) {
      statString += std::string(communicator->getName()) + "." + stat.first +
                    ":" + stat.second + "\n";
    }
  }
  std::string statString = "";
};
}  // namespace

void SHI::MulticastHandler::updateProgress(size_t a, size_t b) {
  udpMulticast.printf("OK UPDATE:%s %u/%u", C_NODENAME, a, b);
  SHI::hw->feedWatchdog();
}

bool SHI::MulticastHandler::isUpdateAvailable() {
  HTTPClient http;
  http.begin(config.firmwareURL.c_str() + String(C_NODENAME) + ".version");
  http.setConnectTimeout(config.CONNECT_TIMEOUT);
  http.setTimeout(config.DATA_TIMEOUT);
  int httpCode = http.GET();
  if (httpCode >= 200 && httpCode < 300) {
    String remoteVersion = http.getString();
    SHI_LOGINFO(("Remote version is:" + remoteVersion).c_str());
    return String(SHI::VERSION).compareTo(remoteVersion) < 0;
  }
  return false;
}

void SHI::MulticastHandler::startUpdate() {
  HTTPClient http;
  http.begin(config.firmwareURL.c_str() + String(C_NODENAME) + ".bin");
  http.setConnectTimeout(config.CONNECT_TIMEOUT);
  http.setTimeout(config.DATA_TIMEOUT);
  int httpCode = http.GET();
  if (httpCode >= 200 && httpCode < 300) {
    sendMulticast(String("OK UPDATE:") + C_NODENAME + " Starting");
    int size = http.getSize();
    if (size < 0) {
      sendMulticast(String("ERR UPDATE:") + C_NODENAME + " Abort, no size");
      return;
    }
    if (!Update.begin(size)) {
      sendMulticast(String("ERR UPDATE:") + C_NODENAME +
                    " Abort, not enough space");
      return;
    }
    Update.onProgress([this](size_t a, size_t b) { updateProgress(a, b); });
    size_t written = Update.writeStream(http.getStream());
    if (written == size) {
      sendMulticast(String("OK UPDATE:") + C_NODENAME + " Finishing");
      if (!Update.end()) {
        sendMulticast(String("ERR UPDATE:") + C_NODENAME +
                      " Abort finish failed: " + String(Update.getError()));
      } else {
        sendMulticast(String("OK UPDATE:") + C_NODENAME + " Finished");
      }
    } else {
      sendMulticast(String("ERR UPDATE:") + C_NODENAME + "Abort, written:" +
                    String(written) + " size:" + String(size));
    }
    SHI::hw->resetConfig();
    SHI::hw->resetWithReason("Firmware updated", true);
  }
  http.end();
}

void SHI::MulticastHandler::sendMulticast(const String &message) {
  AsyncUDPMessage msg;
  msg.print(message);
  udpMulticast.send(msg);
}

void SHI::MulticastHandler::loopCommunication() {
  if (doUpdate) {
    if (isUpdateAvailable()) {
      startUpdate();
    } else {
      SHI_LOGINFO("No newer version available");
      sendMulticast(String("OK UPDATE:") + C_NODENAME + " No Update available");
    }
    doUpdate = false;
  }
}

void SHI::MulticastHandler::setupCommunication() {
  IPAddress addr;
  addr.fromString(config.multicastAddr.c_str());
  if (udpMulticast.listenMulticast(addr, config.PORT)) {
    auto handleUDP = std::bind(&SHI::MulticastHandler::handleUDPPacket, this,
                               std::placeholders::_1);
    udpMulticast.onPacket(handleUDP);
  }
}

void SHI::MulticastHandler::updateHandler(AsyncUDPPacket *packet) {
  SHI_LOGINFO("UPDATE called");
  packet->printf("OK UPDATE:%s", C_NODENAME);
  packet->flush();
  doUpdate = true;
}

void SHI::MulticastHandler::resetHandler(AsyncUDPPacket *packet) {
  SHI_LOGINFO("RESET called");
  packet->printf("OK RESET:%s", C_NODENAME);
  packet->flush();
  SHI::hw->resetWithReason(std::string("UDP RESET request from: ") +
                               packet->remoteIP().toString().c_str(),
                           true);
}

void SHI::MulticastHandler::reconfHandler(AsyncUDPPacket *packet) {
  SHI_LOGINFO("RECONF called");
  SHI::hw->resetConfig();
  packet->printf("OK RECONF:%s", C_NODENAME);
  packet->flush();
  SHI::hw->resetWithReason("UDP RECONF request", true);
}

void SHI::MulticastHandler::infoHandler(AsyncUDPPacket *packet) {
  SHI_LOGINFO("INFO called");
  StatsVisitor stats;
  SHI::hw->accept(stats);
  SHI_LOGINFO(("Stats:" + stats.statString).c_str());
  packet->printf(
      "OK INFO:%s\n"
      "Version:%s\n"
      "ResetReason:%s\n"
      "RunTimeInMillis:%lu\n"
      "ResetSource:%s:%s\n"
      "LocalIP:%s\n"
      "Mac:%s\n"
      "%s",
      C_NODENAME, SHI::VERSION, SHI::hw->getResetReason().c_str(), millis(),
      RESET_SOURCE[rtc_get_reset_reason(0)].c_str(),
      RESET_SOURCE[rtc_get_reset_reason(1)].c_str(),
      WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str(),
      stats.statString.c_str());
  packet->flush();
}

void SHI::MulticastHandler::versionHandler(AsyncUDPPacket *packet) {
  SHI_LOGINFO("VERSION called");
  packet->printf("OK VERSION:%s\nVersion:%s", C_NODENAME, SHI::VERSION);
}

void SHI::MulticastHandler::addUDPPacketHandler(
    String trigger, AuPacketHandlerFunction handler) {
  registeredHandlers[trigger] = handler;
}

void SHI::MulticastHandler::handleUDPPacket(AsyncUDPPacket &packet) {
  const char *data = (const char *)(packet.data());
  if (packet.length() < 10) {
    auto handler = registeredHandlers[String(data)];
    if (handler != NULL) {
      handler(packet);
    }
  }
}
