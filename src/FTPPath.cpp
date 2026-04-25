#include "FTPPath.h"
#include "common.h"

FTPPath::FTPPath() {
}

FTPPath::FTPPath(String path) {
  changePath(path);
}

FTPPath::~FTPPath() {
}

void FTPPath::changePath(String path) {
  std::list<String> p = splitPath(path);
  if (!path.isEmpty() && path[0] == '/') {
    _Path.assign(p.begin(), p.end());
  } else {
    std::copy(p.begin(), p.end(), std::back_inserter(_Path));
  }
}

void FTPPath::goPathUp() {
  if (_Path.size() != 0) // Added Akoro 2021-02-27
    _Path.pop_back();
}

String FTPPath::getPath() const {
  return createPath(_Path);
}

String FTPPath::getFilePath(String filename) const {
  if (*filename.begin() == '/') {
    return filename;
  }
  if (_Path.size() == 0) {
    return "/" + filename;
  }
  return getPath() + "/" + filename;
}

std::list<String> FTPPath::splitPath(String path) {
  std::list<String> p = Split<std::list<String>>(path, '/');
  p.erase(std::remove_if(p.begin(), p.end(),
                         [](const String &s) {
                           if (s.isEmpty()) {
                             return true;
                           }
                           return false;
                         }),
          p.end());
  return p;
}

String FTPPath::createPath(const std::list<String> &path) {
  if (path.size() == 0) {
    return "/";
  }
  String new_path;
  for (const String &p : path) {
    new_path += "/";
    new_path += p;
  }
  return new_path;
}

bool FTPPath::isValidFilename(const String &filepath) {
  if (filepath.isEmpty()) {
    return false;
  }

  // Check for path traversal attempts
  if (filepath.indexOf("../") != -1 || filepath.startsWith("..")) {
    return false;
  }

  int    lastSlash = filepath.lastIndexOf('/');
  String filename  = (lastSlash >= 0) ? filepath.substring(lastSlash + 1) : filepath;

  if (filename.isEmpty()) {
    return false;
  }

  // Check for invalid characters
  const char invalid_chars[] = {'?', '*', ':', '"', '<', '>', '|', '\\'};
  for (char c : invalid_chars) {
    if (filename.indexOf(c) != -1) {
      return false;
    }
  }

  // Check for Windows reserved names (case-insensitive)
  String upperFilename = filename;
  upperFilename.toUpperCase();
  const char *reserved_names[] = {"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
  for (const char *reserved : reserved_names) {
    if (upperFilename == reserved) {
      return false;
    }
  }

  // Check for reasonable length limits
  if (filename.length() > 255) {
    return false;
  }

  return true;
}
