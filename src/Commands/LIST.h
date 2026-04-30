#ifndef LIST_H_
#define LIST_H_

#include <WiFiClient.h>

#include "../FTPCommand.h"
#include "../common.h"

class LIST : public FTPCommand {
public:
  explicit LIST(WiFiClient *const Client, FTPFilesystem *const Filesystem, IPAddress *DataAddress, int *DataPort, WiFiServer **PassiveServer = 0, bool *PassiveMode = 0) : FTPCommand("LIST", 1, Client, Filesystem, DataAddress, DataPort, PassiveServer, PassiveMode) {
  }

  void run(FTPPath &WorkDirectory, const std::vector<String> &Line) override {
    FTPPath listPath = WorkDirectory;

    // 1. Better Argument Parsing
    // Look for the first argument that DOES NOT start with '-'
    // This correctly skips 'LIST -la /sdcard' flags and finds the path
    for (size_t i = 1; i < Line.size(); ++i) {
      if (!Line[i].startsWith("-") && !Line[i].isEmpty()) {
        listPath.changePath(Line[i]);
        break;
      }
    }

    if (!ConnectDataConnection()) {
      return;
    }
    File dir = _Filesystem->open(listPath.getPath()); //
    if (!dir || !dir.isDirectory()) {
      CloseDataConnection();
      SendResponse(550, "Can't open directory " + listPath.getPath());
      return;
    }
    int cnt = 2;
    data_println("drwxr-xr-x 1 owner group 0 Jan 01  1970 .");
    data_println("drwxr-xr-x 1 owner group 0 Jan 01  1970 ..");
    File f = dir.openNextFile();
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
    SendResponse(226, String(cnt) + " matches total");
  }
};

#endif
