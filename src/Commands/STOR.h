#ifndef STOR_H_
#define STOR_H_

#include <WiFiClient.h>

#include "../FTPCommand.h"
#include "../common.h"

class STOR : public FTPCommandTransfer {
public:
  explicit STOR(WiFiClient *const Client, FTPFilesystem *const Filesystem, IPAddress *DataAddress, int *DataPort, WiFiServer **PassiveServer = 0, bool *PassiveMode = 0) : FTPCommandTransfer("STOR", 1, Client, Filesystem, DataAddress, DataPort, PassiveServer, PassiveMode) {
  }

  void run(FTPPath &WorkDirectory, const std::vector<String> &Line) override {
    if (trasferInProgress()) {
      return;
    }
    if (!ConnectDataConnection()) {
      return;
    }
    _ftpFsFilePath = WorkDirectory.getFilePath(Line[1]);
    _file          = _Filesystem->open(_ftpFsFilePath, "w");
    if (!_file) {
      SendResponse(FtpCodes::FILE_ACTION_ABORTED_LOCAL_ERROR, "Can't open/create " + _ftpFsFilePath);
      CloseDataConnection();
      return;
    }
    if (!startTransfer()) {
      failTransfer(FtpCodes::FILE_ACTION_ABORTED_LOCAL_ERROR, "Out of memory");
      return;
    }
    workOnData();
  }

  void workOnData() override {
    const unsigned long start = millis();
    do {
      int nb = data_read(_buffer.get(), FTP_BUF_SIZE);
      if (nb > 0) {
        _lastProgress = millis();
        const auto wb = _file.write(_buffer.get(), nb);
        if (wb != static_cast<std::remove_cv<decltype(wb)>::type>(nb)) {
          failTransfer(FtpCodes::EXCEEDED_STORAGE, "Error occured while STORing");
          return;
        }
        continue;
      }
      // nothing to read right now: either the client is done or the data is still on its way
      if (data_finished()) {
        _file.close();
        SendResponse(FtpCodes::TRANSFER_COMPLETE, "File successfully transferred");
        CloseDataConnection();
        finishTransfer();
        return;
      }
      if (idleTimedOut()) {
        failTransfer(FtpCodes::CONNECTION_CLOSED, "Transfer aborted, no data received");
      }
      return;
    } while (millis() - start < FTP_TRANSFER_SLICE_MS);
  }

private:
  // closes and removes the incomplete file
  void failTransfer(int code, const String &text) {
    finishTransfer();
    this->_Filesystem->remove(_ftpFsFilePath);
    SendResponse(code, text);
    CloseDataConnection();
  }

  String _ftpFsFilePath;
};

#endif
