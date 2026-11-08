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
    _fill = 0;
    if (!startTransfer()) {
      failTransfer(FtpCodes::FILE_ACTION_ABORTED_LOCAL_ERROR, "Out of memory");
      return;
    }
    workOnData();
  }

  void workOnData() override {
    const unsigned long start = millis();
    do {
      int nb = data_read(_buffer.get() + _fill, FTP_BUF_SIZE - _fill);
      if (nb > 0) {
        _lastProgress = millis();
        _fill += nb;
        // write whole blocks only: many small, unaligned writes (one per TCP segment) are slow on SD cards
        if (_fill == FTP_BUF_SIZE && !writeBuffer()) {
          return;
        }
        continue;
      }
      // nothing to read right now: either the client is done or the data is still on its way
      if (data_finished()) {
        if (!writeBuffer()) {
          return;
        }
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
  // writes the collected data to the file; on error the transfer is failed and false returned
  bool writeBuffer() {
    if (_fill == 0) {
      return true;
    }
    const auto wb = _file.write(_buffer.get(), _fill);
    if (wb != static_cast<std::remove_cv<decltype(wb)>::type>(_fill)) {
      failTransfer(FtpCodes::EXCEEDED_STORAGE, "Error occured while STORing");
      return false;
    }
    _fill = 0;
    return true;
  }

  // closes and removes the incomplete file
  void failTransfer(int code, const String &text) {
    finishTransfer();
    this->_Filesystem->remove(_ftpFsFilePath);
    SendResponse(code, text);
    CloseDataConnection();
  }

  String _ftpFsFilePath;
  size_t _fill = 0; // bytes collected in _buffer, not yet written
};

#endif
