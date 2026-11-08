#ifndef RETR_H_
#define RETR_H_

#include <WiFiClient.h>

#include "../FTPCommand.h"
#include "../FTPResponseCodes.h"
#include "../common.h"

class RETR : public FTPCommandTransfer {
public:
  explicit RETR(WiFiClient *const Client, FTPFilesystem *const Filesystem, IPAddress *DataAddress, int *DataPort, WiFiServer **PassiveServer = 0, bool *PassiveMode = 0) : FTPCommandTransfer("RETR", 1, Client, Filesystem, DataAddress, DataPort, PassiveServer, PassiveMode) {
  }

  void run(FTPPath &WorkDirectory, const std::vector<String> &Line) override {
    if (trasferInProgress()) {
      return;
    }
    if (!ConnectDataConnection()) {
      return;
    }
    String path = WorkDirectory.getFilePath(Line[1]);
    _file       = _Filesystem->open(path);
    if (!_file || _file.isDirectory()) {
      _file.close();
      CloseDataConnection();
      SendResponse(FtpCodes::FILE_NOT_FOUND, "Can't open " + path);
      return;
    }
    if (!startTransfer(1)) {
      finishTransfer();
      CloseDataConnection();
      SendResponse(FtpCodes::FILE_ACTION_ABORTED_LOCAL_ERROR, "Out of memory");
      return;
    }
    workOnData();
  }

  void workOnData() override {
    const unsigned long start = millis();
    do {
      size_t nb = _file.read(_buffers[0], _bufferSize);
      if (nb == 0) {
        CloseDataConnection();
        SendResponse(FtpCodes::TRANSFER_COMPLETE, "File successfully transferred");
        finishTransfer();
        return;
      }
      size_t sent = data_send(_buffers[0], nb);
      if (sent > 0) {
        _lastProgress = millis();
      }
      if (sent < nb) {
        // the client stopped reading or closed the data connection: don't read the rest of the file into nowhere
        if (sent == 0 && (!dataConnected() || idleTimedOut())) {
          CloseDataConnection();
          SendResponse(FtpCodes::CONNECTION_CLOSED, "Transfer aborted, data connection lost");
          finishTransfer();
          return;
        }
        // socket buffer full: send the rest next time
        _file.seek(_file.position() - (nb - sent));
        return;
      }
    } while (millis() - start < FTP_TRANSFER_SLICE_MS);
  }
};

#endif
