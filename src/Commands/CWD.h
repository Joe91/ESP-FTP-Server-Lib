#ifndef CWD_H_
#define CWD_H_

#include <WiFiClient.h>

#include "../FTPCommand.h"
#include "../FTPResponseCodes.h"

class CWD : public FTPCommand {
public:
  explicit CWD(WiFiClient *const Client, FTPFilesystem *const Filesystem) : FTPCommand("CWD", 1, Client, Filesystem) {
  }

  void run(FTPPath &WorkDirectory, const std::vector<String> &Line) override {
    if (Line.size() < 2) {
      SendResponse(FTPResponse::SYNTAX_ERROR_PARAMS, "Syntax error in parameters");
      return;
    }

    FTPPath path = WorkDirectory;
    if (Line[1] == "..") {
      path.goPathUp();
    } else {
      // Validate path for illegal characters
      if (!FTPPath::isValidFilename(Line[1])) {
        SendResponse(FTPResponse::FILE_NAME_NOT_ALLOWED, "Illegal filename or path");
        return;
      }
      path.changePath(Line[1]);
    }
    File dir = _Filesystem->open(path.getPath());
    if (dir.isDirectory()) {
      WorkDirectory = path;
      SendResponse(FTPResponse::FILE_ACTION_OK, "Ok. Current directory is " + WorkDirectory.getPath());
    } else {
      SendResponse(FTPResponse::FILE_ACTION_NOT_TAKEN, "Directory does not exist");
    }
  }
};

#endif
