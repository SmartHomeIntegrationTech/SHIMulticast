/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

// WARNING, this is an automatically generated file!
// Don't change anything in here.
// Last update 2020-03-25

# include <iostream>
# include <string>


# include "SHIMulticastHandler.h"
// Configuration implementation for class SHI::MulticastHandlerConfig

namespace {
    
}  // namespace

SHI::MulticastHandlerConfig::MulticastHandlerConfig(const JsonObject &obj):
      CONNECT_TIMEOUT(obj["CONNECT_TIMEOUT"] | 500),
      DATA_TIMEOUT(obj["DATA_TIMEOUT"] | 1000),
      PORT(obj["PORT"] | 2323),
      multicastAddr(obj["multicastAddr"] | "239.1.23.42"),
      firmwareURL(obj["firmwareURL"] | "http://192.168.188.250/esp/firmware/")
  {}

void SHI::MulticastHandlerConfig::fillData(JsonObject &doc) const {
    doc["CONNECT_TIMEOUT"] = CONNECT_TIMEOUT;
  doc["DATA_TIMEOUT"] = DATA_TIMEOUT;
  doc["PORT"] = PORT;
  doc["multicastAddr"] = multicastAddr;
  doc["firmwareURL"] = firmwareURL;
}

int SHI::MulticastHandlerConfig::getExpectedCapacity() const {
  return JSON_OBJECT_SIZE(5);
}

