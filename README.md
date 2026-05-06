# ESP-FTP-Server-Lib

This library will provide a simple and modern FTP server for your ESP32 or ESP8266 device.
You can setup multiple users and mutliple filesystems (SD-Card, MMC-Card or/and SPIFFS).

## Examples

In the example folder you can find a very simple usage of the FTP server. You just need to setup the users, add the filesystems which you want to use, and call the handle function in the loop.

## Known Commands to the server

Currently all kind of simple commands are known to the server:
* CDUP
* CLNT
* CWD
* DELE
* FEAT
* LISST
* MKD
* MLSD
* NLST
* OPTS
* PASV
* PORT
* PWD
* RETR
* RMD
* RNFR
* RNTO
* STAT
* STOR
* TYPE
* USER
* PASS
* SYST
* QUIT
* ABOR

## What is still missing / TODO

Some commands are still missing, if you need them create a ticket :)
