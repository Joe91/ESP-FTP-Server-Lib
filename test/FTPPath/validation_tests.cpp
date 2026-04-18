#include "FTPPath.h"
#include <unity.h>

void test_valid_filename() {
  TEST_ASSERT_TRUE(FTPPath::isValidFilename("file.txt"));
  TEST_ASSERT_TRUE(FTPPath::isValidFilename("document.pdf"));
  TEST_ASSERT_TRUE(FTPPath::isValidFilename("12345"));
  TEST_ASSERT_TRUE(FTPPath::isValidFilename("file_with_underscores.doc"));
  TEST_ASSERT_TRUE(FTPPath::isValidFilename("file-with-dashes.txt"));
}

void test_invalid_filename() {
  TEST_ASSERT_FALSE(FTPPath::isValidFilename(""));
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("file?.txt"));
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("file*.txt"));
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("file:name.txt"));
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("file\"name.txt"));
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("file<name>.txt"));
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("file|name.txt"));
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("file\\name.txt"));
}

void test_path_with_invalid_filename() {
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("/path/file?.txt"));
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("folder/file*.txt"));
  TEST_ASSERT_TRUE(FTPPath::isValidFilename("/folder/file.txt"));
}

void test_path_traversal_protection() {
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("../file.txt"));
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("../../../etc/passwd"));
  TEST_ASSERT_FALSE(FTPPath::isValidFilename(".."));
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("/path/../file.txt"));
  TEST_ASSERT_TRUE(FTPPath::isValidFilename("file..txt"));
}

void test_reserved_names() {
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("CON"));
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("PRN"));
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("AUX"));
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("NUL"));
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("COM1"));
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("LPT1"));
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("con"));    // case insensitive
  TEST_ASSERT_FALSE(FTPPath::isValidFilename("Com1"));   // case insensitive
  TEST_ASSERT_TRUE(FTPPath::isValidFilename("console")); // not exact match
  TEST_ASSERT_TRUE(FTPPath::isValidFilename("file.txt"));
}

void test_length_limits() {
  String longFilename = String(256, 'a'); // 256 characters
  TEST_ASSERT_FALSE(FTPPath::isValidFilename(longFilename));

  String maxFilename = String(255, 'a'); // 255 characters
  TEST_ASSERT_TRUE(FTPPath::isValidFilename(maxFilename));
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_valid_filename);
  RUN_TEST(test_invalid_filename);
  RUN_TEST(test_path_with_invalid_filename);
  RUN_TEST(test_path_traversal_protection);
  RUN_TEST(test_reserved_names);
  RUN_TEST(test_length_limits);
  UNITY_END();
}

void loop() {
  // Empty
}
