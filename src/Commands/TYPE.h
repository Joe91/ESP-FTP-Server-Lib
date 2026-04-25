#ifndef TYPE_H_
#define TYPE_H_

#include <WiFiClient.h>

#include "../FTPCommand.h"
#include "../FTPResponseCodes.h"

class TYPE : public FTPCommand {
public:
  explicit TYPE(WiFiClient *const Client) : FTPCommand("TYPE", 1, Client) {
  }

  void run(FTPPath &WorkDirectory, const std::vector<String> &Line) override {
    if (Line[1] == "A") {
      SendResponse(FTPResponse::COMMAND_OK, "TYPE is now ASCII");
      return;
    } else if (Line[1] == "I") {
      SendResponse(FTPResponse::COMMAND_OK, "TYPE is now 8-bit binary");
      return;
    }
    SendResponse(FTPResponse::COMMAND_NOT_IMPLEMENTED_FOR_PARAMETER, "Unknown TYPE");
  }
};

#endif
