#ifndef MSLD_H_
#define MSLD_H_
// Copyright (c) 2020 Arkady Korotkevich
// Based on LIST.h by Peter Buchegger
#include <WiFiClient.h>

#include "../FTPCommand.h"
#include "../common.h"
#include <time.h>

class MLSD : public FTPCommand {
public:
  explicit MLSD(WiFiClient *const Client, FTPFilesystem *const Filesystem, IPAddress *DataAddress, int *DataPort, WiFiServer **PassiveServer = 0, bool *PassiveMode = 0) : FTPCommand("MLSD", 1, Client, Filesystem, DataAddress, DataPort, PassiveServer, PassiveMode) {
  }

  void run(FTPPath &WorkDirectory, const std::vector<String> &Line) override {
    // Handle arguments: MLSD can take a path or flags
    FTPPath listPath = WorkDirectory;
    if (Line.size() > 1 && !Line[1].isEmpty() && Line[1] != Line[0] && !Line[1].startsWith("-")) {
      // If argument exists, is not empty, is not the command itself, and doesn't start with -, treat it as a path
      listPath.changePath(Line[1]);
    }
    // Flags are ignored for compatibility

    if (!ConnectDataConnection()) {
      return;
    }

    File root = _Filesystem->open(listPath.getPath(), "r");

    if (!root || !root.isDirectory()) {
      root.close();
      CloseDataConnection();
      SendResponse(550, "Can't open directory " + listPath.getPath());
      return;
    }

    int  cnt = 0;
    File f   = root.openNextFile();
    while (f) {
      // type=
      data_print("type=");
      if (f.isDirectory())
        data_print("dir;");
      else
        data_print("file;");

      // size=
      if (f.isDirectory())
        data_print("sizd=");
      else
        data_print("size=");
      data_print(String(f.size()));
      data_print(";");

      // modify=YYYYMMDDHHMMSS; // GMT (!!!!)
      char             buf[128];
      time_t           ft = f.getLastWrite();
      const struct tm *t  = localtime(&ft);
      sprintf(buf, "modify=%4d%02d%02d%02d%02d%02d;", (t->tm_year) + 1900, (t->tm_mon) + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
      data_print(String(buf));

      // UNIX.mode=
      data_print("UNIX.mode=0700;");

      // <filename>
      data_print(" ");
      String filename = f.name();
      filename.remove(0, filename.lastIndexOf('/') + 1);
      data_println(filename);

      cnt++;
      f = root.openNextFile();
    }

    root.close();
    CloseDataConnection();
    SendResponse(226, String(cnt) + " matches total");
  }
};

#endif
