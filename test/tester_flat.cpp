#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include "../bson_flat.hpp"

static bool compare_binary(const std::string& expected, const void *actual, std::string& message)
{
  static const char* const hex_char = "0123456789abcdefABCDEF";
  std::string::size_type pos = 0;
  bool result = true;
  std::string line1 = "(expected)";
  std::string line2 = "( actual )";
  auto source = static_cast<const std::uint8_t*>(actual);
  int length;
  for (length = 0; pos != std::string::npos; ++length, ++source) {
    auto end = expected.find_first_not_of(hex_char, pos);
    auto hex_str = expected.substr(pos, end);
    auto hex_exp = std::strtol(hex_str.c_str(), nullptr, 16);
    line1 += ' ';
    line1 += hex_char[hex_exp >> 4];
    line1 += hex_char[hex_exp & 15];
    auto hex_act = *source;
    line2 += ' ';
    line2 += hex_char[hex_act >> 4];
    line2 += hex_char[hex_act & 15];
    pos = expected.find_first_of(hex_char, end);
    if (hex_exp != hex_act) {
      result = false;
    }
  }
  message = line1 + "\n" + line2;
  return result;
}

#define ASSERT_BINEQ(expected, actual) \
  do { \
    std::string message; \
    ASSERT_TRUE(compare_binary(expected, actual, message)) << message; \
  } while (0)

TEST(writer, auto_allocation)
{
  bson::writer w;
  ASSERT_TRUE(w);
  w.~writer();
  ASSERT_FALSE(w);
}

TEST(writer, fixed_buffer)
{
  std::uint8_t buffer[5];
  bson::writer w(buffer, sizeof(buffer));
  ASSERT_TRUE(w);
  w.~writer();
  ASSERT_FALSE(w);
}

TEST(writer, fixed_buffer_too_small)
{
  std::uint8_t buffer[4];
  bson::writer w(buffer, sizeof(buffer));
  ASSERT_FALSE(w);
}

TEST(writer, empty_document_auto)
{
  bson::writer w;
  auto buffer = *reinterpret_cast<const std::uint8_t**>(&w);
  ASSERT_BINEQ("05 00 00 00 00", buffer);
}

TEST(writer, empty_document_fixed)
{
  std::uint8_t buffer[16];
  std::memset(buffer, 0xaa, sizeof(buffer));
  bson::writer w(buffer, 0x05);
  ASSERT_BINEQ("05 00 00 00 00 aa", buffer);
}

TEST(writer, empty_document_sub)
{
  std::uint8_t buffer[16];
  std::memset(buffer, 0xaa, sizeof(buffer));
  bson::writer w(buffer, 0x0d);
  ASSERT_TRUE(w.add_document("a"));
  ASSERT_BINEQ(
    "0d 00 00 00 "
    "61 00 03 "
      "05 00 00 00 "
      "00 "
    "00 aa",
    buffer
  );
}

TEST(writer, add_double)
{
  std::uint8_t buffer[32];
  std::memset(buffer, 0xaa, sizeof(buffer));
  bson::writer w(buffer, 0x12);
  w.add_double("abc", 1.5);
  ASSERT_BINEQ(
    "12 00 00 00 "
    "61 62 63 00 01 "
    "00 00 00 00 00 00 f8 3f "
    "00 aa",
    buffer
  );
}

TEST(writer, add_string)
{
  std::uint8_t buffer[32];
  std::memset(buffer, 0xaa, sizeof(buffer));
  bson::writer w(buffer, 0x19);
  w.add_string("a", "A\0@");
  w.add_string("b", "B\0@", 3);
  ASSERT_BINEQ(
    "19 00 00 00 "
    "61 00 02 01 00 00 00 41 00 "
    "62 00 02 03 00 00 00 42 00 40 00 "
    "00 aa",
    buffer
  );
}

TEST(writer, add_undefined)
{
  std::uint8_t buffer[32];
  std::memset(buffer, 0xaa, sizeof(buffer));
  bson::writer w(buffer, 0x08);
  w.add_undefined("X");
  ASSERT_BINEQ(
    "08 00 00 00 "
    "58 00 06 "
    "00 aa",
    buffer
  );
}

TEST(writer, add_boolean)
{
  std::uint8_t buffer[32];
  std::memset(buffer, 0xaa, sizeof(buffer));
  bson::writer w(buffer, 0x15);
  w.add_boolean("a", true);
  w.add_boolean("b", false);
  w.add_true("c");
  w.add_false("d");
  ASSERT_BINEQ(
    "15 00 00 00 "
    "61 00 08 01 "
    "62 00 08 00 "
    "63 00 08 01 "
    "64 00 08 00 "
    "00 aa",
    buffer
  );
}

TEST(writer, add_null)
{
  std::uint8_t buffer[32];
  std::memset(buffer, 0xaa, sizeof(buffer));
  bson::writer w(buffer, 0x08);
  w.add_null("Y");
  ASSERT_BINEQ(
    "08 00 00 00 "
    "59 00 0a "
    "00 aa",
    buffer
  );
}

TEST(writer, add_int32)
{
  std::uint8_t buffer[32];
  std::memset(buffer, 0xaa, sizeof(buffer));
  bson::writer w(buffer, 0x0c);
  w.add_int32("A", 0x12345678);
  ASSERT_BINEQ(
    "0c 00 00 00 "
    "41 00 10 78 56 34 12 "
    "00 aa",
    buffer
  );
}

TEST(writer, add_int64)
{
  std::uint8_t buffer[32];
  std::memset(buffer, 0xaa, sizeof(buffer));
  bson::writer w(buffer, 0x10);
  w.add_int64("A", 0x1234567890abcdef);
  ASSERT_BINEQ(
    "10 00 00 00 "
    "41 00 12 ef cd ab 90 78 56 34 12 "
    "00 aa",
    buffer
  );
}

TEST(writer, add_document)
{
  const std::uint8_t* bytes;
  std::size_t length;
  std::uint8_t buffer[32];
  std::memset(buffer, 0xaa, sizeof(buffer));
  bson::writer w(buffer, 0x15);
  {
    auto s = w.add_document("def");
    s.add_true("123");
    ASSERT_TRUE(s.get_bytes(bytes, length));
    ASSERT_EQ(buffer + 9, bytes);
    ASSERT_EQ(11, length);
    ASSERT_FALSE(w.get_bytes(bytes, length));
  }
  ASSERT_TRUE(w.get_bytes(bytes, length));
  ASSERT_EQ(buffer + 0, bytes);
  ASSERT_EQ(21, length);
  ASSERT_BINEQ(
    "15 00 00 00 "
    "64 65 66 00 03 "
      "0b 00 00 00 "
      "31 32 33 00 08 01 "
      "00 "
    "00 aa",
    buffer
  );
}

TEST(writer, add_document_with_writer)
{
  std::uint8_t buffer[32];
  std::memset(buffer, 0xaa, sizeof(buffer));
  bson::writer w(buffer, 0x19);
  bson::writer sub;
  sub.add_true("a");
  {
    auto subsub = sub.add_document("b");
    ASSERT_TRUE(subsub);
    ASSERT_FALSE(w.add_document("A", sub));
  }
  ASSERT_TRUE(w.add_document("B", sub));
  ASSERT_BINEQ(
    "19 00 00 00 "
    "42 00 03 "
      "11 00 00 00 "
      "61 00 08 01 "
      "62 00 03 "
        "05 00 00 00 "
        "00 "
      "00 "
    "00 aa",
    buffer
  );
}

TEST(writer, add_array)
{
  const std::uint8_t* bytes;
  std::size_t length;
  std::uint8_t buffer[32];
  std::memset(buffer, 0xaa, sizeof(buffer));
  bson::writer w(buffer, 0x16);
  {
    auto s = w.add_array("abc");
    s.add_true("0");
    s.add_null("1");
    ASSERT_TRUE(s.get_bytes(bytes, length));
    ASSERT_FALSE(w.get_bytes(bytes, length));
  }
  ASSERT_TRUE(w.get_bytes(bytes, length));
  ASSERT_BINEQ(
    "16 00 00 00 "
    "61 62 63 00 04 "
      "0c 00 00 00 "
      "30 00 08 01 "
      "31 00 0a "
      "00 "
    "00 aa",
    buffer
  );
}

TEST(writer, add_array_with_writer)
{
  std::uint8_t buffer[32];
  std::memset(buffer, 0xaa, sizeof(buffer));
  bson::writer w(buffer, 0x19);
  bson::writer sub;
  sub.add_true("3");
  {
    auto subsub = sub.add_document("5");
    ASSERT_TRUE(subsub);
    ASSERT_FALSE(w.add_document("A", sub));
  }
  ASSERT_TRUE(w.add_document("B", sub));
  ASSERT_BINEQ(
    "19 00 00 00 "
    "42 00 03 "
      "11 00 00 00 "
      "33 00 08 01 "
      "35 00 03 "
        "05 00 00 00 "
        "00 "
      "00 "
    "00 aa",
    buffer
  );
}

TEST(writer, add_binary)
{
  std::uint8_t buffer[32];
  std::memset(buffer, 0xaa, sizeof(buffer));
  bson::writer w(buffer, 0x10);
  w.add_binary("a", "A\0@", 3, bson::subtype::user_defined);
  ASSERT_BINEQ(
    "10 00 00 00 "
    "61 00 05 03 00 00 00 80 41 00 40 "
    "00 aa",
    buffer
  );
}
