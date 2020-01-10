/**
 * @file bson_flat.cpp
 * @brief Flat buffer based BSON library for C++!1
 */
#include "bson_flat.hpp"
#include <malloc.h>
#include <cstring>

namespace bson {

writer::writer() noexcept
: buffer(nullptr), offset(0), locked(0), length(128), malloc(1)
{
  buffer = ::malloc(length);
  if (!buffer) {
    locked = 1;
    is_root = 0;
  } else {
    update_offset(buffer, 4);
  }
}

writer::writer(void* buffer, std::size_t length) noexcept
{
  if ((length < 5) || (length > INT32_MAX)) {
    // Construct invalid writer
    this->buffer = nullptr;
    this->offset = 0;
    this->locked = 1;
    this->is_root = 0;
  } else {
    // Construct fixed buffer writer
    this->buffer = buffer;
    this->locked = 0;
    this->is_root = length;
    update_offset(buffer, 4);
  }
}

writer::~writer() noexcept
{
  if (locked) {
    return;
  }
  locked = 1;
  if (is_root) {
    if (malloc) {
      ::free(buffer);
    }
    is_root = 0;
  } else {
    // Update parent's size

    //    <--total-->          <--total-->
    // .. yy 00 00 00 .. .. .. xx 00 00 00 .. .. .. .. .. 00
    //                  |                    |           |
    // parent->parent->offset     parent->offset       this->offset

    parent->locked = 0;
    auto root = get_root();
    parent->update_offset(root->buffer, offset + 1);
  }
  buffer = nullptr;
}

bool writer::add_double(const char* e_name, double value) noexcept
{
  auto dest = static_cast<double*>(
    add_element(e_name, type::fp64, 8)
  );
  if (!dest) {
    return false;
  }
  *dest = value;
  return true;
}

bool writer::add_string(const char* e_name, const char* string) noexcept
{
  return add_string(e_name, string, std::strlen(string));
}

bool writer::add_string(const char* e_name, const char* string,
                        std::size_t length) noexcept
{
  if (length >= INT32_MAX) {
    return false;
  }
  union {
    std::int32_t* int32;
    char* chars;
    void* pointer;
  } dest;
  dest.pointer = add_element(e_name, type::string, length + 5);
  if (!dest.pointer) {
    return false;
  }
  *dest.int32++ = static_cast<std::int32_t>(length);
  std::memcpy(dest.pointer, string, length);
  dest.chars[length] = 0;
  return true;
}

bool writer::add_binary(const char* e_name, const void* buffer,
                        std::size_t length, subtype subtype) noexcept
{
  if (length > INT32_MAX) {
    return false;
  }
  union {
    std::int32_t* int32;
    bson::subtype* subtype;
    void* pointer;
  } dest;
  dest.pointer = add_element(e_name, type::binary, length + 5);
  if (!dest.pointer) {
    return false;
  }
  *dest.int32++ = static_cast<std::int32_t>(length);
  *dest.subtype++ = subtype;
  std::memcpy(dest.pointer, buffer, length);
  return true;
}

bool writer::add_boolean(const char* e_name, bool value) noexcept
{
  auto dest = static_cast<std::uint8_t*>(
    add_element(e_name, type::boolean, 1)
  );
  if (!dest) {
    return false;
  }
  *dest = value ? 1 : 0;
  return true;
}

bool writer::add_int32(const char* e_name, std::int32_t value) noexcept
{
  auto dest = static_cast<std::int32_t*>(
    add_element(e_name, type::int32, 4)
  );
  if (!dest) {
    return false;
  }
  *dest = value;
  return true;
}

bool writer::add_int64(const char* e_name, std::int64_t value) noexcept
{
  auto dest = static_cast<std::int64_t*>(
    add_element(e_name, type::int64, 8)
  );
  if (!dest) {
    return false;
  }
  *dest = value;
  return true;
}

bool writer::get_bytes(const std::uint8_t*& bytes, std::size_t& length) const noexcept
{
  if (locked) {
    return false;
  }
  auto root = static_cast<const writer*>(const_cast<writer*>(this)->get_root());
  auto header_offset = (is_root ? 0 : (parent->offset - 5));
  bytes = static_cast<const std::uint8_t*>(root->buffer) + header_offset;
  length = (offset + 1) - header_offset;
  return true;
}

void* writer::add_element(const char* e_name, type type, std::size_t space) noexcept
{
  if (locked) {
    return nullptr;
  }
  std::size_t depth;
  auto root = get_root(&depth);
  auto name_length = std::strlen(e_name) + 1;
  if (name_length <= 1) {
    return nullptr;
  }
  auto required = offset + name_length + 1 + space + 1 + depth;
  if (root->length < required) {
    // Insufficient space
    if (!root->malloc) {
      // Fixed buffer full
      return nullptr;
    }
    // Growth buffer
    auto new_length = root->length;
    do { new_length *= 2; } while (new_length < required);
    auto new_buffer = ::realloc(root->buffer, new_length);
    if (!new_buffer) {
      return nullptr;
    }
    root->buffer = new_buffer;
    root->length = new_length;
  }

  //    <--size--->
  // .. xx 00 00 00 .. .. .. .. nn nn nn nn nn 00 tt ss ss ss ss 00
  //                  |        |<--name_length-->   |<--space-->|
  //       parent->offset    this->offset         dest        this->offset
  //                             (old)                            (new)

  auto dest = static_cast<std::uint8_t*>(root->buffer) + offset;
  std::memcpy(dest, e_name, name_length);
  dest += name_length;
  *dest++ = static_cast<std::uint8_t>(type);
  update_offset(root->buffer, offset + name_length + 1 + space);
  return dest;
}

writer writer::add_subdocument(const char* e_name, type type) noexcept
{
  auto dest = static_cast<std::int32_t*>(
    add_element(e_name, type, 5)
  );
  if (!dest) {
    return writer(nullptr);
  }
  *dest++ = 5;
  *reinterpret_cast<std::uint8_t*>(dest) = 0x00;
  locked = 1;
  return writer(this, offset - 1);
}

bool writer::add_subdocument(const char* e_name, type type, const writer& subdocument) noexcept
{
  const std::uint8_t* bytes;
  std::size_t space;

  if (!subdocument.get_bytes(bytes, space)) {
    return false;
  }
  auto dest = add_element(e_name, type, space);
  if (!dest) {
    return false;
  }
  std::memcpy(dest, bytes, space);
  return true;
}

writer* writer::get_root(std::size_t* depth_store) noexcept
{
  auto target = this;
  std::size_t depth = 0;
  while (target->is_root == 0) {
    target = target->parent;
    ++depth;
  }
  if (depth_store) {
    *depth_store = depth;
  }
  return target;
}

void writer::update_offset(void* buffer, std::uint32_t new_offset) noexcept
{
  auto header_offset = is_root ? 0 : (parent->offset - 5);
  *reinterpret_cast<std::int32_t*>(
    static_cast<std::uint8_t*>(buffer) + header_offset
  ) = new_offset + 1 - header_offset;
  static_cast<std::uint8_t*>(buffer)[new_offset] = 0x00;
  offset = new_offset;
}

} /* namespace bson */
