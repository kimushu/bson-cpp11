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
  ASSERT_TRUE(w.valid());
  w.~writer();
  ASSERT_FALSE(w);
  ASSERT_FALSE(w.valid());
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
    "03 61 00 "
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
    "01 61 62 63 00 "
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
    "02 61 00 02 00 00 00 41 00 "
    "02 62 00 04 00 00 00 42 00 40 00 "
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
    "06 58 00 "
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
    "08 61 00 01 "
    "08 62 00 00 "
    "08 63 00 01 "
    "08 64 00 00 "
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
    "0a 59 00 "
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
    "10 41 00 78 56 34 12 "
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
    "12 41 00 ef cd ab 90 78 56 34 12 "
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
    "03 64 65 66 00 "
      "0b 00 00 00 "
      "08 31 32 33 00 01 "
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
    "03 42 00 "
      "11 00 00 00 "
      "08 61 00 01 "
      "03 62 00 "
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
    "04 61 62 63 00 "
      "0c 00 00 00 "
      "08 30 00 01 "
      "0a 31 00 "
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
    "03 42 00 "
      "11 00 00 00 "
      "08 33 00 01 "
      "03 35 00 "
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
    "05 61 00 03 00 00 00 80 41 00 40 "
    "00 aa",
    buffer
  );
}

namespace bson {

std::ostream& operator<<(std::ostream& ostream, const reader::element& element)
{
  return ostream <<
    "{e_name:" << static_cast<const void*>(element.e_name) <<
    ",data:" << element.data.pointer << "}";
}

std::ostream& operator<<(std::ostream& ostream, const reader::const_iterator& iterator)
{
  return ostream <<
    "{current:" << iterator.current << ",next:" << iterator.next_position.pointer <<
    ",end:" << iterator.end_position.pointer << "}";
}

void PrintTo(const reader::const_iterator& iterator, std::ostream* ostream)
{
  *ostream << iterator;
}

} /* namespace bson */

TEST(reader, construction_too_small)
{
  std::uint8_t buffer[] = {0x00,0x00,0x00,0x00};
  bson::reader r(buffer, sizeof(buffer));
  ASSERT_TRUE(r.begin().fail());
}

TEST(reader, construction_incorrect_size)
{
  std::uint8_t buffer[] = {0x00,0x00,0x00,0x00,0x00};
  bson::reader r(buffer, sizeof(buffer));
  ASSERT_TRUE(r.begin().fail());
}

TEST(reader, construction_incorrect_termination)
{
  std::uint8_t buffer[] = {0x05,0x00,0x00,0x00,0xaa};
  bson::reader r(buffer, sizeof(buffer));
  ASSERT_TRUE(r.begin().fail());
}

TEST(reader, construction_overflow)
{
  std::uint8_t buffer[] = {0x06,0x00,0x00,0x00,0x00,0x00};
  bson::reader r(buffer, sizeof(buffer) - 1);
  ASSERT_TRUE(r.begin().fail());
}

TEST(reader, construction_underflow)
{
  std::uint8_t buffer[] = {0x05,0x00,0x00,0x00,0x00,0xaa};
  bson::reader r(buffer, sizeof(buffer));
  ASSERT_FALSE(r.begin().fail());
}

TEST(reader, iterator_begin)
{
  std::uint8_t buffer1[] = {0x08,0x00,0x00,0x00,0x06,0x41,0x00,0x00};
  std::uint8_t buffer2[] = {0x08,0x00,0x00,0x00,0x06,0x42,0x00,0x00};
  bson::reader r1(buffer1, sizeof(buffer1));
  bson::reader r2(buffer2, sizeof(buffer2));
  ASSERT_NE(r1.begin(), r2.begin());
  ASSERT_EQ(r1.begin(), r1.begin());
}

TEST(reader, iterator_begin_empty)
{
  std::uint8_t buffer[] = {0x05,0x00,0x00,0x00,0x00};
  bson::reader r(buffer, sizeof(buffer));
  ASSERT_EQ(r.begin(), r.end());
}

TEST(reader, iterator_end)
{
  std::uint8_t buffer[] = {0x05,0x00,0x00,0x00,0x00};
  bson::reader r(buffer, sizeof(buffer));
  ASSERT_EQ(r.end(), r.end());
}

TEST(reader, iterator_next)
{
  std::uint8_t buffer[] = {
    0x0b,0x00,0x00,0x00,0x06,0x41,0x00,0x0a,0x42,0x00,0x00,0xaa
  };
  bson::reader r(buffer, sizeof(buffer));
  auto i = r.begin();
  ASSERT_EQ(r.begin(), i++);
  ASSERT_NE(r.begin(), i);
  auto i2 = i;
  ASSERT_NE(i2, ++i);
  ASSERT_NE(r.end(), i2);
  ASSERT_EQ(r.end(), i);
}

TEST(reader, iterator_fail_first)
{
  std::uint8_t buffer[] = {
    0x05,0x00,0x00,0x00,0xaa
  };
  bson::reader r(buffer, sizeof(buffer));
  ASSERT_TRUE(r.begin().fail());
}

TEST(reader, iterator_fail_second)
{
  std::uint8_t buffer[] = {
    0x09,0x00,0x00,0x00,0x06,0x41,0x00,0xaa,0x00
  };
  bson::reader r(buffer, sizeof(buffer));
  auto i = r.begin();
  ASSERT_FALSE(i.fail());
  ASSERT_TRUE((++i).fail());
  ASSERT_TRUE((++i).fail());
}

TEST(reader, query_size)
{
  std::uint8_t buffer[] = {0x05, 0x00, 0x00, 0x00, 0x00, 0xaa};
  ASSERT_LT(bson::reader::query_size(buffer, 3), 0);
  ASSERT_EQ(5, bson::reader::query_size(buffer, 4));
  ASSERT_EQ(5, bson::reader::query_size(buffer, 5));
  ASSERT_EQ(5, bson::reader::query_size(buffer, 6));
}

TEST(reader, element_double)
{
  std::uint8_t buffer[] = {
    0x13,0x00,0x00,0x00,
      0x01,0x41,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0xf8,0x3f,
      0x06,0x42,0x00,
    0x00,
  };
  bson::reader r(buffer, sizeof(buffer));
  auto i = r.begin();
  ASSERT_FALSE(i.fail());
  ASSERT_NE(i, r.end());
  auto e = *i;
  ASSERT_STREQ("A", e.name());
  ASSERT_TRUE(e.is_double());
  ASSERT_FALSE(e.is_string());
  ASSERT_FALSE(e.is_document());
  ASSERT_FALSE(e.is_array());
  ASSERT_FALSE(e.is_binary());
  ASSERT_FALSE(e.is_undefined());
  ASSERT_FALSE(e.is_boolean());
  ASSERT_FALSE(e.is_null());
  ASSERT_FALSE(e.is_int32());
  ASSERT_FALSE(e.is_int64());
  ASSERT_FALSE(e.is_null_or_undefined());
  ASSERT_FALSE(e.is_integer());
  ASSERT_TRUE(e.is_number());

  double v = 0.0;
  ASSERT_TRUE(e.get_double(v));
  ASSERT_DOUBLE_EQ(1.5, v);

  v = 0.0;
  ASSERT_TRUE(e.get_number(v));
  ASSERT_DOUBLE_EQ(1.5, v);

  ASSERT_DOUBLE_EQ(1.5, e.as_double(2.0));
  ASSERT_DOUBLE_EQ(1.5, e.as_number(2.0));

  e = *(++i);
  ASSERT_DOUBLE_EQ(2.0, e.as_double(2.0));
  ASSERT_DOUBLE_EQ(3.0, e.as_number(3.0));
}

TEST(reader, element_string)
{
  std::uint8_t buffer[] = {
    0x13,0x00,0x00,0x00,
      0x02,0x43,0x00,
        0x04,0x00,0x00,0x00,0x61,0x00,0x62,0x00,
      0x06,0x44,0x00,
    0x00,
  };
  bson::reader r(buffer, sizeof(buffer));
  auto i = r.begin();
  ASSERT_FALSE(i.fail());
  ASSERT_NE(i, r.end());
  const auto e = *i;
  ASSERT_STREQ("C", e.name());
  ASSERT_FALSE(e.is_double());
  ASSERT_TRUE(e.is_string());
  ASSERT_FALSE(e.is_document());
  ASSERT_FALSE(e.is_array());
  ASSERT_FALSE(e.is_binary());
  ASSERT_FALSE(e.is_undefined());
  ASSERT_FALSE(e.is_boolean());
  ASSERT_FALSE(e.is_null());
  ASSERT_FALSE(e.is_int32());
  ASSERT_FALSE(e.is_int64());
  ASSERT_FALSE(e.is_null_or_undefined());
  ASSERT_FALSE(e.is_integer());
  ASSERT_FALSE(e.is_number());

  const char* v = nullptr;
  std::size_t l = 0;
  ASSERT_TRUE(e.get_string(v));
  ASSERT_BINEQ("61 00", v);
  v = nullptr;
  ASSERT_TRUE(e.get_string(v, l));
  ASSERT_EQ(3, l);
  ASSERT_BINEQ("61 00 62 00", v);

  l = 0;
  ASSERT_STREQ("a", e.as_string("x"));
  ASSERT_STREQ("a", e.as_string(l, "x\0yz", 4));
  ASSERT_EQ(3, l);
  const auto e2 = *++i;
  l = 0;
  ASSERT_STREQ("x", e2.as_string("x"));
  ASSERT_BINEQ("78 00 79 7a 00", e2.as_string(l, "x\0yz", 4));
  ASSERT_EQ(4, l);
}

TEST(reader, element_document)
{
  std::uint8_t buffer[] = {
    0x10,0x00,0x00,0x00,
      0x03,0x45,0x00,
        0x08,0x00,0x00,0x00,0x06,0x61,0x00,0x00,
    0x00,
  };
  bson::reader r(buffer, sizeof(buffer));
  auto i = r.begin();
  ASSERT_FALSE(i.fail());
  ASSERT_NE(i, r.end());
  const auto e = *i;
  ASSERT_STREQ("E", e.name());
  ASSERT_FALSE(e.is_double());
  ASSERT_FALSE(e.is_string());
  ASSERT_TRUE(e.is_document());
  ASSERT_FALSE(e.is_array());
  ASSERT_FALSE(e.is_binary());
  ASSERT_FALSE(e.is_undefined());
  ASSERT_FALSE(e.is_boolean());
  ASSERT_FALSE(e.is_null());
  ASSERT_FALSE(e.is_int32());
  ASSERT_FALSE(e.is_int64());
  ASSERT_FALSE(e.is_null_or_undefined());
  ASSERT_FALSE(e.is_integer());
  ASSERT_FALSE(e.is_number());

  auto r2 = e.as_document();
  ASSERT_TRUE(r2);
  ASSERT_TRUE(r2.valid());
  auto i2 = r2.begin();
  auto e2 = *i2;
  ASSERT_STREQ("a", e2.name());
  ASSERT_TRUE(e2.is_undefined());
  ASSERT_EQ(r2.end(), ++i2);

  ++i2;
  auto r3 = i2->as_document();
  ASSERT_FALSE(r3);
  ASSERT_FALSE(r3.valid());

  auto r4 = i2->as_document(r);
  ASSERT_TRUE(r4);
  ASSERT_TRUE(r4.valid());
  ASSERT_STREQ("E", r4.begin()->name());
}

TEST(reader, element_array)
{
  std::uint8_t buffer[] = {
    0x10,0x00,0x00,0x00,
      0x04,0x46,0x00,
        0x08,0x00,0x00,0x00,0x06,0x31,0x00,0x00,
    0x00,
  };
  bson::reader r(buffer, sizeof(buffer));
  auto i = r.begin();
  ASSERT_FALSE(i.fail());
  ASSERT_NE(i, r.end());
  const auto e = *i;
  ASSERT_STREQ("F", e.name());
  ASSERT_FALSE(e.is_double());
  ASSERT_FALSE(e.is_string());
  ASSERT_FALSE(e.is_document());
  ASSERT_TRUE(e.is_array());
  ASSERT_FALSE(e.is_binary());
  ASSERT_FALSE(e.is_undefined());
  ASSERT_FALSE(e.is_boolean());
  ASSERT_FALSE(e.is_null());
  ASSERT_FALSE(e.is_int32());
  ASSERT_FALSE(e.is_int64());
  ASSERT_FALSE(e.is_null_or_undefined());
  ASSERT_FALSE(e.is_integer());
  ASSERT_FALSE(e.is_number());

  auto r2 = e.as_array();
  auto i2 = r2.begin();
  auto e2 = *i2;
  ASSERT_STREQ("1", e2.name());
  ASSERT_TRUE(e2.is_undefined());
  ASSERT_EQ(r2.end(), ++i2);

  ++i2;
  auto r3 = i2->as_array();
  ASSERT_FALSE(r3);
  ASSERT_FALSE(r3.valid());

  auto r4 = i2->as_array(r);
  ASSERT_TRUE(r4);
  ASSERT_TRUE(r4.valid());
  ASSERT_STREQ("F", r4.begin()->name());
}

TEST(reader, element_binary)
{
  std::uint8_t buffer[] = {
    0x13,0x00,0x00,0x00,
      0x05,0x47,0x00,
        0x03,0x00,0x00,0x00,0x04,0xca,0xfe,0xda,
      0x06,0x48,0x00,
    0x00,
  };
  bson::reader r(buffer, sizeof(buffer));
  auto i = r.begin();
  auto e = *i;
  ASSERT_STREQ("G", e.name());
  ASSERT_FALSE(e.is_double());
  ASSERT_FALSE(e.is_string());
  ASSERT_FALSE(e.is_document());
  ASSERT_FALSE(e.is_array());
  ASSERT_TRUE(e.is_binary());
  ASSERT_FALSE(e.is_undefined());
  ASSERT_FALSE(e.is_boolean());
  ASSERT_FALSE(e.is_null());
  ASSERT_FALSE(e.is_int32());
  ASSERT_FALSE(e.is_int64());
  ASSERT_FALSE(e.is_null_or_undefined());
  ASSERT_FALSE(e.is_integer());
  ASSERT_FALSE(e.is_number());

  const char* const x = "xyz";
  const void* v = nullptr;
  std::size_t l = 0;
  bson::subtype s = bson::subtype::generic;
  ASSERT_TRUE(e.get_binary(v, l));
  ASSERT_EQ(buffer + 12, v);
  ASSERT_EQ(3, l);

  v = nullptr;
  l = 0;
  ASSERT_TRUE(e.get_binary(v, l, s));
  ASSERT_EQ(buffer + 12, v);
  ASSERT_EQ(3, l);
  ASSERT_EQ(bson::subtype::uuid, s);

  l = 0;
  ASSERT_EQ(buffer + 12, e.as_binary(l, s));
  ASSERT_EQ(3, l);

  l = 0;
  ASSERT_EQ(buffer + 12, e.as_binary(l, s, x, 1));
  ASSERT_EQ(3, l);

  l = 0;
  s = bson::subtype::generic;
  ASSERT_EQ(buffer + 12, e.as_binary(l, s));
  ASSERT_EQ(3, l);
  ASSERT_EQ(bson::subtype::uuid, s);

  l = 0;
  s = bson::subtype::generic;
  ASSERT_EQ(buffer + 12, e.as_binary(l, s, x, 1));
  ASSERT_EQ(3, l);
  ASSERT_EQ(bson::subtype::uuid, s);

  auto e2 = *(++i);
  l = 999;
  ASSERT_EQ(nullptr, e2.as_binary(l));
  ASSERT_EQ(0, l);

  l = 0;
  ASSERT_EQ(x, e2.as_binary(l, x, 1));
  ASSERT_EQ(1, l);

  l = 0;
  s = bson::subtype::generic;
  ASSERT_EQ(x, e2.as_binary(l, s, x, 2, bson::subtype::md5));
  ASSERT_EQ(2, l);
  ASSERT_EQ(bson::subtype::md5, s);
}

TEST(reader, element_undefined)
{
  std::uint8_t buffer[] = {
    0x08,0x00,0x00,0x00,
      0x06,0x49,0x00,
    0x00,
  };
  bson::reader r(buffer, sizeof(buffer));
  auto i = r.begin();
  auto e = *i;
  ASSERT_STREQ("I", e.name());
  ASSERT_FALSE(e.is_double());
  ASSERT_FALSE(e.is_string());
  ASSERT_FALSE(e.is_document());
  ASSERT_FALSE(e.is_array());
  ASSERT_FALSE(e.is_binary());
  ASSERT_TRUE(e.is_undefined());
  ASSERT_FALSE(e.is_boolean());
  ASSERT_FALSE(e.is_null());
  ASSERT_FALSE(e.is_int32());
  ASSERT_FALSE(e.is_int64());
  ASSERT_TRUE(e.is_null_or_undefined());
  ASSERT_FALSE(e.is_integer());
  ASSERT_FALSE(e.is_number());
}

TEST(reader, element_boolean)
{
  std::uint8_t buffer[] = {
    0x14,0x00,0x00,0x00,
      0x08,0x4a,0x00,0x00,
      0x08,0x4b,0x00,0x01,
      0x08,0x4c,0x00,0x02,
      0x06,0x4d,0x00,
    0x00,
  };
  bson::reader r(buffer, sizeof(buffer));
  auto i = r.begin();
  auto e = *i;
  ASSERT_STREQ("J", e.name());
  ASSERT_FALSE(e.is_double());
  ASSERT_FALSE(e.is_string());
  ASSERT_FALSE(e.is_document());
  ASSERT_FALSE(e.is_array());
  ASSERT_FALSE(e.is_binary());
  ASSERT_FALSE(e.is_undefined());
  ASSERT_TRUE(e.is_boolean());
  ASSERT_FALSE(e.is_null());
  ASSERT_FALSE(e.is_int32());
  ASSERT_FALSE(e.is_int64());
  ASSERT_FALSE(e.is_null_or_undefined());
  ASSERT_FALSE(e.is_integer());
  ASSERT_FALSE(e.is_number());

  bool v = true;
  ASSERT_TRUE(e.get_boolean(v));
  ASSERT_FALSE(v);
  ASSERT_FALSE(e.as_boolean(true));

  e = *(++i);
  v = false;
  ASSERT_STREQ("K", e.name());
  ASSERT_TRUE(e.get_boolean(v));
  ASSERT_TRUE(v);
  ASSERT_TRUE(e.as_boolean(false));

  e = *(++i);
  v = false;
  ASSERT_STREQ("L", e.name());
  ASSERT_TRUE(e.get_boolean(v));
  ASSERT_TRUE(v);
  ASSERT_TRUE(e.as_boolean(false));

  e = *(++i);
  v = false;
  ASSERT_STREQ("M", e.name());
  ASSERT_FALSE(e.get_boolean(v));
  ASSERT_FALSE(v);
  ASSERT_TRUE(e.as_boolean(true));
  ASSERT_FALSE(e.as_boolean(false));
}

TEST(reader, element_null)
{
  std::uint8_t buffer[] = {
    0x08,0x00,0x00,0x00,
      0x0a,0x4e,0x00,
    0x00,
  };
  bson::reader r(buffer, sizeof(buffer));
  auto i = r.begin();
  auto e = *i;
  ASSERT_STREQ("N", e.name());
  ASSERT_FALSE(e.is_double());
  ASSERT_FALSE(e.is_string());
  ASSERT_FALSE(e.is_document());
  ASSERT_FALSE(e.is_array());
  ASSERT_FALSE(e.is_binary());
  ASSERT_FALSE(e.is_undefined());
  ASSERT_FALSE(e.is_boolean());
  ASSERT_TRUE(e.is_null());
  ASSERT_FALSE(e.is_int32());
  ASSERT_FALSE(e.is_int64());
  ASSERT_TRUE(e.is_null_or_undefined());
  ASSERT_FALSE(e.is_integer());
  ASSERT_FALSE(e.is_number());
}

TEST(reader, element_int32)
{
  std::uint8_t buffer[] = {
    0x0f,0x00,0x00,0x00,
      0x10,0x4f,0x00,0xef,0xbe,0xad,0xde,
      0x06,0x50,0x00,
    0x00,
  };
  bson::reader r(buffer, sizeof(buffer));
  auto i = r.begin();
  auto e = *i;
  ASSERT_STREQ("O", e.name());
  ASSERT_FALSE(e.is_double());
  ASSERT_FALSE(e.is_string());
  ASSERT_FALSE(e.is_document());
  ASSERT_FALSE(e.is_array());
  ASSERT_FALSE(e.is_binary());
  ASSERT_FALSE(e.is_undefined());
  ASSERT_FALSE(e.is_boolean());
  ASSERT_FALSE(e.is_null());
  ASSERT_TRUE(e.is_int32());
  ASSERT_FALSE(e.is_int64());
  ASSERT_FALSE(e.is_null_or_undefined());
  ASSERT_TRUE(e.is_integer());
  ASSERT_TRUE(e.is_number());

  int32_t v = 0;
  ASSERT_TRUE(e.get_int32(v));
  ASSERT_EQ(-559038737, v);
  ASSERT_EQ(-559038737, e.as_int32(12345));
  int64_t vi = 0;
  ASSERT_TRUE(e.get_integer(vi));
  ASSERT_EQ(-559038737, vi);
  ASSERT_EQ(-559038737, e.as_integer(12345));
  double vn = 0.0;
  ASSERT_TRUE(e.get_number(vn));
  ASSERT_DOUBLE_EQ(-559038737, vn);
  ASSERT_DOUBLE_EQ(-559038737, e.as_number(12345));

  e = *(++i);
  v = 0;
  ASSERT_FALSE(e.get_int32(v));
  ASSERT_EQ(0, v);
  ASSERT_EQ(12345, e.as_int32(12345));
  vi = 0;
  ASSERT_FALSE(e.get_integer(vi));
  ASSERT_EQ(0, vi);
  ASSERT_EQ(12345, e.as_integer(12345));
  vn = 0.0;
  ASSERT_FALSE(e.get_number(vn));
  ASSERT_DOUBLE_EQ(0, vn);
  ASSERT_DOUBLE_EQ(12345, e.as_number(12345));
}

TEST(reader, element_int64)
{
  std::uint8_t buffer[] = {
    0x0f,0x00,0x00,0x00,
      0x12,0x51,0x00,0xef,0xbe,0xad,0xde,0xfe,0xca,0xad,0xba,
      0x06,0x52,0x00,
    0x00,
  };
  bson::reader r(buffer, sizeof(buffer));
  auto i = r.begin();
  auto e = *i;
  ASSERT_STREQ("Q", e.name());
  ASSERT_FALSE(e.is_double());
  ASSERT_FALSE(e.is_string());
  ASSERT_FALSE(e.is_document());
  ASSERT_FALSE(e.is_array());
  ASSERT_FALSE(e.is_binary());
  ASSERT_FALSE(e.is_undefined());
  ASSERT_FALSE(e.is_boolean());
  ASSERT_FALSE(e.is_null());
  ASSERT_FALSE(e.is_int32());
  ASSERT_TRUE(e.is_int64());
  ASSERT_FALSE(e.is_null_or_undefined());
  ASSERT_TRUE(e.is_integer());
  ASSERT_TRUE(e.is_number());

  int64_t v = 0;
  ASSERT_TRUE(e.get_int64(v));
  ASSERT_EQ(-4995113215677579537ll, v);
  ASSERT_EQ(-4995113215677579537ll, e.as_int64(12345));
  int64_t vi = 0;
  ASSERT_TRUE(e.get_integer(vi));
  ASSERT_EQ(-4995113215677579537ll, vi);
  ASSERT_EQ(-4995113215677579537ll, e.as_integer(12345));
  double vn = 0.0;
  ASSERT_TRUE(e.get_number(vn));
  ASSERT_DOUBLE_EQ(-4995113215677579537ll, vn);
  ASSERT_DOUBLE_EQ(-4995113215677579537ll, e.as_number(12345));

  e = *(++i);
  v = 0;
  ASSERT_FALSE(e.get_int64(v));
  ASSERT_EQ(0, v);
  ASSERT_EQ(12345, e.as_int32(12345));
  vi = 0;
  ASSERT_FALSE(e.get_integer(vi));
  ASSERT_EQ(0, vi);
  ASSERT_EQ(12345, e.as_integer(12345));
  vn = 0.0;
  ASSERT_FALSE(e.get_number(vn));
  ASSERT_DOUBLE_EQ(0, vn);
  ASSERT_DOUBLE_EQ(12345, e.as_number(12345));
}

TEST(reader, element_truthy)
{
  std::uint8_t buffer[] = {
    0x47,0x00,0x00,0x00,
      0x01,0x41,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf0,0x3f, // Non-zero double
      0x02,0x42,0x00,0x02,0x00,0x00,0x00,0x00,0x00,           // Non-empty string
      0x03,0x43,0x00,0x05,0x00,0x00,0x00,0x00,                // Empty document
      0x04,0x44,0x00,0x05,0x00,0x00,0x00,0x00,                // Empty array
      0x05,0x45,0x00,0x00,0x00,0x00,0x00,0x00,                // Empty binary
      0x08,0x46,0x00,0x01,                                    // true
      0x10,0x47,0x00,0x01,0x00,0x00,0x00,                     // Non-zero int32
      0x12,0x48,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // Non-zero int64
    0x00,
  };
  bson::reader r(buffer, sizeof(buffer));
  for (auto e : r) {
    ASSERT_TRUE(e.truthy()) << "key=\"" << e.name() << "\",type=" << static_cast<int>(e.type());
  }
}

TEST(reader, element_falsy)
{
  std::uint8_t buffer[] = {
    0x4a,0x00,0x00,0x00,
      0x01,0x41,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // Zero double
      0x01,0x42,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf8,0x7f, // qNaN double
      0x01,0x43,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0xf0,0x7f, // sNaN double
      0x02,0x44,0x00,0x01,0x00,0x00,0x00,0x00,                // Empty string
      0x06,0x45,0x00,                                         // undefined
      0x08,0x46,0x00,0x00,                                    // false
      0x06,0x47,0x00,                                         // null
      0x10,0x48,0x00,0x00,0x00,0x00,0x00,                     // Zero int32
      0x12,0x49,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // Zero int64
    0x00,
  };
  bson::reader r(buffer, sizeof(buffer));
  for (auto e : r) {
    ASSERT_TRUE(e.falsy()) << "key=\"" << e.name() << "\",type=" << static_cast<int>(e.type());
  }
}
