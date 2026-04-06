#ifndef PASV_H_
#define PASV_H_

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

#include "../FTPCommand.h"
#include "../common.h"

class PASV : public FTPCommand {
public:
  explicit PASV(WiFiClient *const Client, IPAddress *DataAddress, int *DataPort, WiFiServer **PassiveServer = 0, bool *PassiveMode = 0) : FTPCommand("PASV", 0, Client, 0, DataAddress, DataPort, PassiveServer, PassiveMode) {
  }

  void run(FTPPath &WorkDirectory, const std::vector<String> &Line) override {
    if (_PassiveServer != 0 && *_PassiveServer != 0) {
      (*_PassiveServer)->stop();
      delete *_PassiveServer;
      *_PassiveServer = 0;
    }
    if (_PassiveMode != 0) {
      *_PassiveMode = true;
    }
    int port   = 20000 + random(0, 1000);
    *_DataPort = port;
    if (_PassiveServer != 0) {
      *_PassiveServer = new WiFiServer(port);
      (*_PassiveServer)->begin();
    }
    IPAddress localIP  = WiFi.localIP();
    int       p1       = port / 256;
    int       p2       = port % 256;
    String    response = "Entering Passive Mode (" + String(localIP[0]) + "," + String(localIP[1]) + "," + String(localIP[2]) + "," + String(localIP[3]) + "," + String(p1) + "," + String(p2) + ")";
    SendResponse(227, response);
  }
};

#endif
