#ifndef RNFR_H_
#define RNFR_H_

#include <WiFiClient.h>

#include "../FTPCommand.h"
#include "../FTPResponseCodes.h"

class RNFR_RNTO : public FTPCommand {
public:
  explicit RNFR_RNTO(WiFiClient *const Client, FTPFilesystem *const Filesystem) : FTPCommand("RN", 1, Client, Filesystem), _fromSet(false) {
  }

  void run(FTPPath &WorkDirectory, const std::vector<String> &Line) override {
    if (Line[0] == "RNFR") {
      from(WorkDirectory, Line);
    } else if (Line[0] == "RNTO") {
      to(WorkDirectory, Line);
    }
  }

  void from(const FTPPath &WorkDirectory, const std::vector<String> &Line) {
    if (Line.size() < 2) {
      SendResponse(FTPResponse::SYNTAX_ERROR_PARAMS, "Syntax error in parameters");
      return;
    }
    String filepath = WorkDirectory.getFilePath(Line[1]);
    if (!_Filesystem->exists(filepath)) {
      SendResponse(FTPResponse::FILE_ACTION_NOT_TAKEN, "File " + Line[1] + " not found");
      return;
    }
    _fromSet = true;
    _from    = filepath;
    SendResponse(FTPResponse::FILE_ACTION_PENDING, "RNFR accepted - file exists, ready for destination");
  }

  void to(const FTPPath &WorkDirectory, const std::vector<String> &Line) {
    if (Line.size() < 2) {
      SendResponse(FTPResponse::SYNTAX_ERROR_PARAMS, "Syntax error in parameters");
      return;
    }
    if (!_fromSet) {
      SendResponse(FTPResponse::BAD_SEQUENCE, "Need RNFR before RNTO");
      return;
    }
    if (!FTPPath::isValidFilename(Line[1])) {
      SendResponse(FTPResponse::FILE_NAME_NOT_ALLOWED, "Illegal filename");
      _fromSet = false;
      _from    = "";
      return;
    }
    String filepath = WorkDirectory.getFilePath(Line[1]);
    if (_Filesystem->exists(filepath)) {
      SendResponse(FTPResponse::FILE_NAME_NOT_ALLOWED, "File " + Line[1] + " already exists");
      return;
    }
    if (_Filesystem->rename(_from, filepath)) {
      SendResponse(FTPResponse::FILE_ACTION_OK, "File successfully renamed or moved");
    } else {
      SendResponse(FTPResponse::FILE_ACTION_ABORTED_LOCAL_ERROR, "Rename/move failure");
    }
    _fromSet = false;
    _from    = "";
  }

private:
  bool   _fromSet;
  String _from;
};

#endif
