/*
   Asynchronous redirected HTTPS network connection to e.g. Google Apps

   Platform: ESP8266 using Arduino IDE
   Documentation: http://www.coertvonk.com/technology/embedded/esp8266-clock-import-events-from-google-calendar-15809
   Tested with: Arduino IDE 1.6.11, board package esp8266 2.3.0, Adafruit huzzah feather esp8266
   Inspired by: Sujay Phadke's HTTPSRedirect

   MIT license, check the file LICENSE for more information
   (c) Copyright 2016, Coert Vonk
   All text above must be included in any redistribution
*/

#include "WiFiClientSecureRedirect.h"

//#define DEBUG
#ifdef DEBUG
#define DPRINT(...)    Serial.print(__VA_ARGS__)
#define DPRINTLN(...)  Serial.println(__VA_ARGS__)
#else
#define DPRINT(...)
#define DPRINTLN(...)
#endif
#ifndef CHECK_FINGERPRINT
#define CHECK_FINGERPRINT (0)
#endif

WiFiClientSecureRedirect::WiFiClientSecureRedirect() {}
WiFiClientSecureRedirect::~WiFiClientSecureRedirect() {}

int  // always returns 1
WiFiClientSecureRedirect::connect(char const * const host_,
                                  uint16_t const port_)
{
  dstHost = host_;
  dstPort = port_;  
  return WiFiClientSecure::connect(host_, port_);
}

uint8_t const
_parseHeader(Stream * const stream,
             char * const host,
             size_t const hostSize,
             char * const path,
             size_t const pathSize,
             uint16_t * const port)
{
  if (stream->find("302 Moved Temporarily\r\n") &&
      stream->find("\nLocation: ") &&
      stream->find("://")) {

    size_t const hostLen = stream->readBytesUntil('/', host, hostSize - 1);
    if (!hostLen) {
      DPRINT("<1>");
      return 1;
    }
    host[hostLen] = '\0';

    size_t const pathLen = stream->readBytesUntil('\n', path + 1, pathSize - 2);
    if (!pathLen) {
      DPRINT("<2>");
      return 2;
    }
    path[0] = '/';
    path[pathLen] = '\0';
    *port = 443;  // https
    return 0;
  }
  DPRINT("<3>");
  return 3;  // if you encounter this, make sure the URL is correct
}

uint8_t const
_writeRequest(Stream * const stream,
              char const * const path,
              char const * const host)
{
  uint8_t const HEAD[] = "GET ";
  uint8_t const MIDL[] = " HTTP/1.1\r\n"
                         "Host: ";
  uint8_t const TAIL[] = "\r\n"
                         "User-Agent: ESP8266\r\n"
                         "Connection: close\r\n"
                         "\r\n";
  uint8_t const * const uPath = reinterpret_cast<uint8_t const * const>(path);
  uint8_t const * const uHost = reinterpret_cast<uint8_t const * const>(host);

  // write bits at the time, so we don't need another big buffer in user space
  stream->write(HEAD, sizeof(HEAD) - 1); stream->write(uPath, strlen(path));
  stream->write(MIDL, sizeof(MIDL) - 1); stream->write(uHost, strlen(host));
  stream->write(TAIL, sizeof(TAIL) - 1);
  stream->flush();
  return 0;
}

uint8_t const  // returns 0 on success
WiFiClientSecureRedirect::sendHostRequest()
{
  if (!WiFiClientSecure::connected()) {
    return 2;
  }

#if CHECK_FINGERPRINT
  if (dstFingerprint && !verify(dstFingerprint, dstHost)) {
    return 3;
  }
#endif
  return _writeRequest(this, dstPath, dstHost);
}

uint8_t const  // returns 0 on success
WiFiClientSecureRedirect::receiveHostReply()
{
  // we take a leap of faith .. and assume that *all* data will arrive within timeout_ms
  // from the initial data.  Would be better to only start processing if we either have all
  // the data, or we are able to process chunks at the time
  setTimeout(timeout_ms);
  if (_parseHeader(this, redirHost, sizeof(redirHost), redirPath, sizeof(redirPath), &redirPort)) {
    return 1;
  }

  while (WiFiClientSecure::connected() && WiFiClientSecure::available()) {
    (void)read();  // maybe we need to empty the Rx buffer before closing the socket
  }
  WiFiClientSecure::stop();
  return 0;
}

uint8_t const  // returns 0 on success
WiFiClientSecureRedirect::sendRedirRequest()
{
  if (!WiFiClientSecure::connected()) {
    return 2;
  }
#if CHECK_FINGERPRINT
  if (redirFingerprint && !verify(redirFingerprint, redirHost)) {
    return 3;
  }
#endif
  return _writeRequest(this, redirPath, redirHost);
}

uint8_t const // always returns 0
WiFiClientSecureRedirect::receiveRedirHeader()
{
  // skips the header
  setTimeout(timeout_ms);
  while (WiFiClientSecure::connected()) {
    String line = readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  return 0;  // connection remains open, caller should read data and call stop() method
}

uint8_t const // returns 0 on success
WiFiClientSecureRedirect::request(char const * const dstPath_,// must be static alloc'ed
                                  char const * const dstHost_, // must be static alloc'ed
                                  uint32_t const timeout_ms_,
                                  char const * const dstFingerprint_, // must be static alloc'ed
                                  char const * const redirFingerprint_)  // must be static alloc'ed
{
  dstPath = dstPath_;
  dstHost = dstHost_;
  timeout_ms = timeout_ms_;
  dstFingerprint = dstFingerprint_;
  redirFingerprint = redirFingerprint_;

  return sendHostRequest();
}


String WiFiClientSecureRedirect::getRedir()
{

  if (!WiFiClientSecure::connected()){
    Serial.println("Error! Not connected to host.");
    return "";
  }

  sendHostRequest();
  receiveHostReply();

  if(!WiFiClientSecure::connect(redirHost, redirPort))
  {
    Serial.println("Error! Not connected to redir host.");
    return "";
  }

  sendRedirRequest();

  receiveRedirHeader();

  String line;
  
  while (WiFiClientSecure::available()) {  // read and print until end of data
     line += readStringUntil('\n');
  }

  return line;
}

