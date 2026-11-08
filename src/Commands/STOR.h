#ifndef STOR_H_
#define STOR_H_

#include <WiFiClient.h>

#include "../FTPCommand.h"
#include "../common.h"

#if defined(ESP32)
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// The file is written by a task of its own, so receiving the next block and writing the previous one to
// the SD card overlap. Done one after the other on the caller's loop, the TCP window filled up while a
// block was being written and the sender had to stop.
#ifndef FTP_WRITER_TASK_PRIORITY
#define FTP_WRITER_TASK_PRIORITY 2
#endif
#ifndef FTP_WRITER_TASK_CORE
#define FTP_WRITER_TASK_CORE 1
#endif
#ifndef FTP_WRITER_TASK_STACK
#define FTP_WRITER_TASK_STACK 4096
#endif
// how long the end of an upload may wait for the last blocks to be written
#ifndef FTP_WRITER_FINISH_TIMEOUT_MS
#define FTP_WRITER_FINISH_TIMEOUT_MS 10000
#endif
#endif

class STOR : public FTPCommandTransfer {
public:
  explicit STOR(WiFiClient *const Client, FTPFilesystem *const Filesystem, IPAddress *DataAddress, int *DataPort, WiFiServer **PassiveServer = 0, bool *PassiveMode = 0) : FTPCommandTransfer("STOR", 1, Client, Filesystem, DataAddress, DataPort, PassiveServer, PassiveMode) {
  }

  ~STOR() {
    // the connection can go away in the middle of an upload: the writer must be done with buffers and file first
    finishTransfer();
#if defined(ESP32)
    delete _ctx;
#endif
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
    _current    = -1;
    _fill       = 0;
    _writeError = false;
#if defined(ESP32)
    if (!startTransfer(2) || !startWriter()) {
#else
    if (!startTransfer(1)) {
#endif
      failTransfer(FtpCodes::FILE_ACTION_ABORTED_LOCAL_ERROR, "Out of memory");
      return;
    }
    workOnData();
  }

  void workOnData() override {
    const unsigned long start = millis();
    do {
      if (writeFailed()) {
        failTransfer(FtpCodes::EXCEEDED_STORAGE, "Error occured while STORing");
        return;
      }
      if (_current < 0 && !takeFreeBuffer()) {
        return; // both buffers are still being written: the client waits (TCP flow control)
      }
      int nb = data_read(_buffers[_current] + _fill, _bufferSize - _fill);
      if (nb > 0) {
        _lastProgress = millis();
        _fill += nb;
        // write whole blocks only: many small, unaligned writes (one per TCP segment) are slow on SD cards
        if (_fill == _bufferSize) {
          submitBuffer();
        }
        continue;
      }
      // nothing to read right now: either the client is done or the data is still on its way
      if (data_finished()) {
        submitBuffer();
        if (!waitForWriter() || writeFailed()) {
          failTransfer(FtpCodes::EXCEEDED_STORAGE, "Error occured while STORing");
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

protected:
  void finishTransfer() override {
    if (!waitForWriter()) {
      // The writer hangs in a write and still uses its context (with its own copy of the file) and the
      // buffers: leave them to it instead of pulling them away under it.
      _ctx = nullptr;
      for (size_t i = 0; i < MaxBuffers; i++) {
        _buffers[i] = nullptr;
      }
      _file = File();
    }
    FTPCommandTransfer::finishTransfer();
  }

private:
  // closes and removes the incomplete file
  void failTransfer(int code, const String &text) {
    finishTransfer();
    this->_Filesystem->remove(_ftpFsFilePath);
    SendResponse(code, text);
    CloseDataConnection();
  }

#if defined(ESP32)
  struct Block {
    int    index; // buffer index, -1 = stop
    size_t length;
  };

  // Everything the writer task touches. Owned by STOR, but leaked if the writer hangs, so the task never
  // uses freed memory.
  struct WriterContext {
    QueueHandle_t     toWriter = nullptr; // filled buffers (and the stop request) for the writer
    QueueHandle_t     toReader = nullptr; // buffers the writer is done with
    SemaphoreHandle_t done     = nullptr;
    std::atomic<bool> error {false};
    File              file;
    uint8_t          *buffers[MaxBuffers] = {nullptr, nullptr};

    ~WriterContext() {
      if (toWriter != nullptr) {
        vQueueDelete(toWriter);
      }
      if (toReader != nullptr) {
        vQueueDelete(toReader);
      }
      if (done != nullptr) {
        vSemaphoreDelete(done);
      }
    }
  };

  bool startWriter() {
    if (_ctx == nullptr) {
      _ctx = new (std::nothrow) WriterContext();
      if (_ctx == nullptr) {
        return false;
      }
      _ctx->toWriter = xQueueCreate(MaxBuffers + 1, sizeof(Block));
      _ctx->toReader = xQueueCreate(MaxBuffers, sizeof(int));
      _ctx->done     = xSemaphoreCreateBinary();
    }
    if (_ctx->toWriter == nullptr || _ctx->toReader == nullptr || _ctx->done == nullptr) {
      return false;
    }
    xQueueReset(_ctx->toWriter);
    xQueueReset(_ctx->toReader);
    xSemaphoreTake(_ctx->done, 0);
    for (int i = 0; i < static_cast<int>(MaxBuffers); i++) {
      _ctx->buffers[i] = _buffers[i];
      xQueueSend(_ctx->toReader, &i, 0);
    }
    _ctx->error = false;
    _ctx->file  = _file;
    if (xTaskCreatePinnedToCore(writerTask, "ftpWriter", FTP_WRITER_TASK_STACK, _ctx, FTP_WRITER_TASK_PRIORITY, nullptr, FTP_WRITER_TASK_CORE) != pdPASS) {
      _ctx->file = File();
      return false;
    }
    _writerRunning = true;
    return true;
  }

  static void writerTask(void *parameter) {
    WriterContext *ctx = static_cast<WriterContext *>(parameter);
    Block          block;
    while (xQueueReceive(ctx->toWriter, &block, portMAX_DELAY) == pdTRUE) {
      if (block.index < 0) {
        break;
      }
      if (!ctx->error) {
        const size_t written = ctx->file.write(ctx->buffers[block.index], block.length);
        if (written != block.length) {
          ctx->error = true;
        }
      }
      xQueueSend(ctx->toReader, &block.index, portMAX_DELAY);
    }
    // last access to the context: once this is given, the transfer may be torn down
    xSemaphoreGive(ctx->done);
    vTaskDelete(nullptr);
  }

  bool takeFreeBuffer() {
    int index;
    if (xQueueReceive(_ctx->toReader, &index, 0) != pdTRUE) {
      return false;
    }
    _current = index;
    _fill    = 0;
    return true;
  }

  // hands the filled buffer to the writer
  void submitBuffer() {
    if (_current < 0 || _fill == 0) {
      return;
    }
    Block block = {_current, _fill};
    xQueueSend(_ctx->toWriter, &block, portMAX_DELAY); // never blocks: the queue has room for every buffer
    _current = -1;
    _fill    = 0;
  }

  // Lets the writer write everything submitted and end. Returns false if it doesn't finish in time.
  bool waitForWriter() {
    if (!_writerRunning) {
      return true;
    }
    _writerRunning = false;
    Block stop     = {-1, 0};
    xQueueSend(_ctx->toWriter, &stop, portMAX_DELAY);
    if (xSemaphoreTake(_ctx->done, pdMS_TO_TICKS(FTP_WRITER_FINISH_TIMEOUT_MS)) != pdTRUE) {
      return false;
    }
    _writeError = _writeError || _ctx->error;
    _ctx->file  = File(); // drop the writer's reference, the file is closed by finishTransfer()
    return true;
  }

  bool writeFailed() const {
    return _writeError || (_ctx != nullptr && _ctx->error);
  }

  WriterContext *_ctx           = nullptr;
  bool           _writerRunning = false;
  bool           _writeError    = false;
#else
  // without a writer task the file is written right here, one block at a time
  bool takeFreeBuffer() {
    _current = 0;
    _fill    = 0;
    return true;
  }

  void submitBuffer() {
    if (_current < 0 || _fill == 0 || _writeError) {
      return;
    }
    if (_file.write(_buffers[_current], _fill) != _fill) {
      _writeError = true;
    }
    _current = -1;
    _fill    = 0;
  }

  bool waitForWriter() {
    return true;
  }

  bool writeFailed() const {
    return _writeError;
  }

  bool _writeError = false;
#endif

  String _ftpFsFilePath;
  int    _current = -1; // buffer being filled from the network, -1 = none
  size_t _fill    = 0;  // bytes in that buffer
};

#endif
