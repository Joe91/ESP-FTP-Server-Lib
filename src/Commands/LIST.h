#ifndef LIST_H_
#define LIST_H_

#include <WiFiClient.h>

#include "../FTPCommand.h"
#include "../FTPResponseCodes.h"
#include "../common.h"

class LIST : public FTPCommand {
public:
  explicit LIST(WiFiClient *const Client, FTPFilesystem *const Filesystem, IPAddress *DataAddress, int *DataPort, WiFiServer **PassiveServer = 0, bool *PassiveMode = 0) : FTPCommand("LIST", 1, Client, Filesystem, DataAddress, DataPort, PassiveServer, PassiveMode) {
  }

  void run(FTPPath &WorkDirectory, const std::vector<String> &Line) override {
    // Validate optional path parameter
    if (Line.size() > 1 && !FTPPath::isValidFilename(Line[1])) {
      SendResponse(FTPResponse::FILE_NAME_NOT_ALLOWED, "Illegal filename or path");
      return;
    }

    if (!ConnectDataConnection()) {
      return;
    }

    String listPath = (Line.size() > 1) ? WorkDirectory.getFilePath(Line[1]) : WorkDirectory.getPath();
    File   dir      = _Filesystem->open(listPath); //
    if (!dir || !dir.isDirectory()) {
      CloseDataConnection();
      SendResponse(FTPResponse::FILE_ACTION_NOT_TAKEN, "Can't open directory " + WorkDirectory.getPath());
      return;
    }
    int  cnt = 0;
    File f   = dir.openNextFile();
    while (f) {
      String filename = f.name();
      filename.remove(0, filename.lastIndexOf('/') + 1);
      if (f.isDirectory()) {
        data_print("drwxr-xr-x");
      } else {
        data_print("-rw-r--r--");
      }
      String filesize = String(f.size());
      data_print(" 1 owner group ");
      int fill_cnt = 13 - filesize.length();
      for (int i = 0; i < fill_cnt; i++) {
        data_print(" ");
      }
      data_println(filesize + " Jan 01  1970 " + filename);
      cnt++;
      f.close();
      f = dir.openNextFile();
    }
    CloseDataConnection();
    SendResponse(FTPResponse::TRANSFER_COMPLETE, String(cnt) + " matches total");
  }
};

#endif
