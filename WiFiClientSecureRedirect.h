#pragma once
#include <WiFiClientSecure.h>

class WiFiClientSecureRedirect : public WiFiClientSecure {

  public:
    WiFiClientSecureRedirect();
    ~WiFiClientSecureRedirect();

  // async connect over HTTPS
  int connect(char const * const host, uint16_t const port);


  // request data over HTTPS where host uses redirection
  uint8_t const request(char const * const dstPath,
                      char const * const dstHost,
                      uint32_t const timeout_ms,
                      char const * const dstFingerprint,
                      char const * const redirFingerprint);

  // poll to see if redirection host responded
  int  response();

    // poll to see if data is available from the redir host

    // close the connection
  String getRedir();

  private:
  char const * dstPath;
  char const * dstHost;
  uint16_t dstPort;
  uint32_t timeout_ms;
  char const * dstFingerprint;
  char const * redirFingerprint;
  char redirHost[30];
  char redirPath[300];
  uint16_t redirPort;
  uint32_t beginWait;

  uint8_t const sendHostRequest();
  uint8_t const receiveHostReply();
  uint8_t const sendRedirRequest();
  uint8_t const receiveRedirHeader();
};
