# ESP-FTP-Server-Lib

This library will provide a simple and modern FTP server for your ESP32 or ESP8266 device.
You can setup multiple users and mutliple filesystems (SD-Card, MMC-Card or/and SPIFFS).

## Security Features

This library includes comprehensive input validation and security protections:
- **Filename validation**: Prevents illegal characters in filenames (?, *, :, ", <, >, |, \)
- **Parameter validation**: Validates command syntax and required parameters
- **Path traversal protection**: Helps prevent directory traversal attacks
- **Error handling**: Proper FTP response codes for security-related errors

## Examples

In the example folder you can find a very simple usage of the FTP server. You just need to setup the users, add the filesystems which you want to use, and call the handle function in the loop.

## Known Commands to the server

Currently all kind of simple commands are known to the server:
* CDUP
* CWD
* DELE
* LISST
* MKD
* PASV
* PORT
* PWD
* RETR
* RMD
* RNFR
* RNTO
* STOR
* TYPE
* SYST
* QUIT
* ABOR
* NLST
* STAT

## What is still missing / TODO

Some commands are still missing, if you need them create a ticket :)

**Security Improvements Completed:**
- Input validation for all critical commands (DELE, MKD, RETR, RMD, RNFR, RNTO, STOR)
- Filename validation with illegal character detection
- Proper error responses with standard FTP codes

**Still missing:**
- Additional FTP commands: FEAT, MDTM, SIZE, REST
- Rate limiting and connection throttling
- Enhanced logging capabilities
