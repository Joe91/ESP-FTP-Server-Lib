#ifndef DELE_H_
#define DELE_H_

#include <WiFiClient.h>

#include "../FTPCommand.h"
#include "../FTPResponseCodes.h"

class DELE : public FTPCommand {
public:
  explicit DELE(WiFiClient *const Client, FTPFilesystem *const Filesystem) : FTPCommand("DELE", 1, Client, Filesystem) {
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
    if (!_Filesystem->exists(filepath)) {
      SendResponse(FTPResponse::FILE_ACTION_NOT_TAKEN, "File " + filepath + " not found");
      return;
    }
    if (_Filesystem->remove(filepath)) {
      SendResponse(FTPResponse::FILE_ACTION_OK, " Deleted \"" + filepath + "\"");
    } else {
      SendResponse(FTPResponse::FILE_ACTION_ABORTED, "Can't delete \"" + filepath + "\"");
    }
  }
};

#endif
