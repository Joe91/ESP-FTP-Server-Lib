#ifndef MKD_H_
#define MKD_H_

#include <WiFiClient.h>

#include "../FTPCommand.h"
#include "../FTPResponseCodes.h"

class MKD : public FTPCommand {
public:
  explicit MKD(WiFiClient *const Client, FTPFilesystem *const Filesystem) : FTPCommand("MKD", 1, Client, Filesystem) {
  }

  void run(FTPPath &WorkDirectory, const std::vector<String> &Line) override {
    if (Line.size() < 2) {
      SendResponse(FTPResponse::SYNTAX_ERROR_PARAMS, "Syntax error in parameters");
      return;
    }
    if (!FTPPath::isValidFilename(Line[1])) {
      SendResponse(FTPResponse::FILE_NAME_NOT_ALLOWED, "Illegal filename");
      return;
    }
    String filepath = WorkDirectory.getFilePath(Line[1]);
    if (_Filesystem->exists(filepath)) {
      SendResponse(FTPResponse::FILE_ACTION_NOT_TAKEN, "Can't create \"" + filepath + "\", Directory exists");
      return;
    }
    if (_Filesystem->mkdir(filepath)) {
      SendResponse(FTPResponse::PATHNAME_CREATED, "\"" + filepath + "\" created");
    } else {
      SendResponse(FTPResponse::FILE_ACTION_NOT_TAKEN, "Can't create \"" + filepath + "\"");
    }
  }
};

#endif
