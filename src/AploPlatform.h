// AploEmbed platform compatibility helpers
//
// Provides a small Arduino-networking abstraction for ESP32/ESP32-C3 and ESP8266.

#ifndef APLOEMBED_PLATFORM_H
#define APLOEMBED_PLATFORM_H

#include <Arduino.h>

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <WiFiClient.h>
  #include <WiFiClientSecureBearSSL.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <WiFiClient.h>
  #include <WiFiClientSecure.h>
#else
  #include <WiFi.h>
  #include <WiFiClient.h>
  #include <WiFiClientSecure.h>
#endif

#endif // APLOEMBED_PLATFORM_H
