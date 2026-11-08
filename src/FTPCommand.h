#ifndef FTP_COMMAND_H_
#define FTP_COMMAND_H_

#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <memory>
#include <vector>
#if defined(ESP32)
#include <lwip/sockets.h>
#endif

#include "FTPFilesystem.h"
#include "FTPPath.h"
#include "FTPResponseCodes.h"

class FTPCommand {
public:
  FTPCommand(String Name, int ExpectedArgumentCnt, WiFiClient *const Client, FTPFilesystem *const Filesystem = 0, IPAddress *DataAddress = 0, int *DataPort = 0, WiFiServer **PassiveServer = 0, bool *PassiveMode = 0) : _Name(Name), _ExpectedArgumentCnt(ExpectedArgumentCnt), _Filesystem(Filesystem), _DataAddress(DataAddress), _DataPort(DataPort), _PassiveServer(PassiveServer), _PassiveMode(PassiveMode), _Client(Client), _DataConnection(0) {
  }
  virtual ~FTPCommand() {
    if (_DataConnection != 0) {
      _DataConnection->stop();
      delete _DataConnection;
      _DataConnection = 0;
    }
  }

  String getName() const {
    return _Name;
  }

  virtual void run(FTPPath &WorkDirectory, const std::vector<String> &Line) = 0;

  void SendResponse(int Code, String Text) {
    _Client->print(Code);
    _Client->print(" ");
    _Client->println(Text);
  }

  bool ConnectDataConnection() {
    if (_DataConnection == 0) {
      _DataConnection = new WiFiClient();
    }
    if (_DataConnection->connected()) {
      _DataConnection->stop();
    }
    if (_PassiveMode != 0 && *_PassiveMode) {
      if (_PassiveServer == 0 || *_PassiveServer == 0) {
        SendResponse(FtpCodes::NO_DATA_CONNECTION, "No passive server");
        return false;
      }
      WiFiServer *server = *_PassiveServer;
      // Clients open the data connection right after the 227 reply, but on a busy radio (e.g. WiFi
      // sharing the antenna with Bluetooth) it can take longer than 100 ms to arrive.
      const unsigned long passiveAcceptTimeoutMs = 1000;
      const unsigned long passivePollDelayMs     = 5;
      unsigned long       start                  = millis();
      while (!server->hasClient() && millis() - start < passiveAcceptTimeoutMs) {
        yield();
        delay(passivePollDelayMs);
      }
      if (!server->hasClient()) {
        StopPassiveServer();
        SendResponse(FtpCodes::NO_DATA_CONNECTION, "No data connection");
        return false;
      }
      WiFiClient client = server->accept();
      if (!client) {
        StopPassiveServer();
        SendResponse(FtpCodes::NO_DATA_CONNECTION, "No data connection");
        return false;
      }
      *_DataConnection = client;
      StopPassiveServer();
      *_PassiveMode = false;
      SendResponse(FtpCodes::DATA_CONNECTION_OPEN, "Accepted data connection");
      return true;
    }
    _DataConnection->connect(*_DataAddress, *_DataPort);
    if (!_DataConnection->connected()) {
      _DataConnection->stop();
      SendResponse(FtpCodes::NO_DATA_CONNECTION, "No data connection");
      return false;
    }
    SendResponse(FtpCodes::DATA_CONNECTION_OPEN, "Accepted data connection");
    return true;
  }

  void data_print(String str) {
    if (_DataConnection == 0 || !_DataConnection->connected()) {
      return;
    }
    _DataConnection->print(str);
  }

  void data_println(String str) {
    if (_DataConnection == 0 || !_DataConnection->connected()) {
      return;
    }
    _DataConnection->println(str);
  }

  bool dataConnected() {
    return _DataConnection != 0 && _DataConnection->connected();
  }

  // Returns the number of bytes actually sent (may be less than l, 0 if the connection is gone)
  size_t data_send(uint8_t *c, size_t l) {
    if (_DataConnection == 0 || !_DataConnection->connected()) {
      return 0;
    }
    return _DataConnection->write(c, l);
  }

  // Non-blocking: returns the bytes that are already there (0 if none yet), never waits for more
  int data_read(uint8_t *c, size_t l) {
    if (_DataConnection == 0) {
      return 0;
    }
    int avail = _DataConnection->available();
    if (avail <= 0) {
      return 0;
    }
    int res = _DataConnection->read(c, (static_cast<size_t>(avail) < l) ? static_cast<size_t>(avail) : l);
    return (res > 0) ? res : 0;
  }

  // True once the peer has closed the data connection and everything it sent has been read. A pause in
  // the data is not an end: the upload used to be considered complete after 500 ms without data, so a
  // stall on a busy radio silently truncated the file.
  bool data_finished() {
    if (_DataConnection == 0) {
      return true;
    }
    if (_DataConnection->available() > 0) {
      return false;
    }
#if defined(ESP32)
    // WiFiClient::connected() stays true after an orderly shutdown (FIN), so ask the socket directly
    int fd = _DataConnection->fd();
    if (fd < 0) {
      return true;
    }
    uint8_t dummy;
    int     res = recv(fd, &dummy, 1, MSG_DONTWAIT | MSG_PEEK);
    if (res == 0) {
      return true; // FIN received, no data left
    }
    if (res < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
      return true; // reset / error
    }
    return false;
#else
    return !_DataConnection->connected();
#endif
  }

  void CloseDataConnection() {
    if (_DataConnection != 0) {
      _DataConnection->stop();
    }
    if (_PassiveMode != 0 && *_PassiveMode) {
      StopPassiveServer();
      *_PassiveMode = false;
    }
  }

private:
  void StopPassiveServer() {
    if (_PassiveServer != 0 && *_PassiveServer != 0) {
      (*_PassiveServer)->stop();
      delete *_PassiveServer;
      *_PassiveServer = 0;
    }
  }

protected:
  String               _Name;
  int                  _ExpectedArgumentCnt;
  FTPFilesystem *const _Filesystem;
  IPAddress *const     _DataAddress;
  int *const           _DataPort;
  WiFiServer **const   _PassiveServer;
  bool *const          _PassiveMode;

private:
  WiFiClient *const _Client;
  WiFiClient       *_DataConnection;
};

#ifndef FTP_BUF_SIZE
#define FTP_BUF_SIZE 4096
#endif
// handle() returns after this much time spent on one transfer, so the caller's loop keeps running
#ifndef FTP_TRANSFER_SLICE_MS
#define FTP_TRANSFER_SLICE_MS 20
#endif
// a transfer without any progress for this long is aborted
#ifndef FTP_TRANSFER_IDLE_TIMEOUT_MS
#define FTP_TRANSFER_IDLE_TIMEOUT_MS 30000
#endif

class FTPCommandTransfer : public FTPCommand {
public:
  FTPCommandTransfer(String Name, int ExpectedArgumentCnt, WiFiClient *const Client, FTPFilesystem *const Filesystem = 0, IPAddress *DataAddress = 0, int *DataPort = 0, WiFiServer **PassiveServer = 0, bool *PassiveMode = 0) : FTPCommand(Name, ExpectedArgumentCnt, Client, Filesystem, DataAddress, DataPort, PassiveServer, PassiveMode) {
  }

  virtual void workOnData() = 0;

  bool trasferInProgress() {
    return _file;
  }

  void abort() {
    if (_file) {
      CloseDataConnection();
      SendResponse(FtpCodes::CONNECTION_CLOSED, "Transfer aborted");
      finishTransfer();
    }
  }

protected:
  // The transfer buffer lives on the heap for the duration of a transfer only: 4 KB on the stack of the
  // caller's loop task (8 KB on ESP32 Arduino) left too little for everything else running there.
  bool startTransfer() {
    if (!_buffer) {
      _buffer.reset(new (std::nothrow) uint8_t[FTP_BUF_SIZE]);
    }
    _lastProgress = millis();
    return static_cast<bool>(_buffer);
  }

  void finishTransfer() {
    if (_file) {
      _file.close();
    }
    _buffer.reset();
  }

  bool idleTimedOut() const {
    return millis() - _lastProgress > FTP_TRANSFER_IDLE_TIMEOUT_MS;
  }

  File                       _file;
  std::unique_ptr<uint8_t[]> _buffer;
  unsigned long              _lastProgress = 0;
};

#endif
