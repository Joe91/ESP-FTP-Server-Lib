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
    _path       = path;
    _offset     = 0;
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
      if (nb == 0 && _offset < _file.size()) {
        nb = retryRead();
        if (nb == 0) {
          // don't report a truncated file as complete
          CloseDataConnection();
          SendResponse(FtpCodes::FILE_ACTION_ABORTED_LOCAL_ERROR, "Read error");
          finishTransfer();
          return;
        }
      }
      if (nb == 0) {
        CloseDataConnection();
        SendResponse(FtpCodes::TRANSFER_COMPLETE, "File successfully transferred");
        finishTransfer();
        return;
      }
      size_t sent = data_send(_buffers[0], nb);
      _offset += sent;
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
        _file.seek(_offset);
        return;
      }
    } while (millis() - start < FTP_TRANSFER_SLICE_MS);
  }

private:
  // A read can fail for a moment (e.g. the SD driver gets no DMA memory while WiFi and Bluetooth are busy).
  // FatFs keeps a file in its error state after a disk error, so reopen it and continue at the same place.
  // The position is counted here: after a read error the file's own position isn't reliable.
  size_t retryRead() {
    for (int attempt = 1; attempt <= 3; attempt++) {
#ifndef NO_GLOBAL_INSTANCES
      Serial.printf("RETR: read error at %u of %s, retry %d\n", (unsigned)_offset, _path.c_str(), attempt);
#else
      logPrintlnW("RETR: read error, retry");
#endif
      delay(20 * attempt);
      _file.close();
      _file = _Filesystem->open(_path);
      if (_file && _file.seek(_offset)) {
        size_t nb = _file.read(_buffers[0], _bufferSize);
        if (nb > 0) {
          return nb;
        }
      }
    }
    return 0;
  }

  String _path;
  size_t _offset = 0; // bytes delivered to the client = where the next read has to start
};

#endif
