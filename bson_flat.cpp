/**
 * @file bson_flat.cpp
 * @brief Flat buffer based BSON library for C++!1
 */
#include "bson_flat.hpp"
#include <malloc.h>
#include <cmath>
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
    add_element(e_name, bson::type::fp64, 8)
  );
  if (!dest) {
    return false;
  }
  std::memcpy(dest, &value, 8);
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
  dest.pointer = add_element(e_name, bson::type::string, length + 5);
  if (!dest.pointer) {
    return false;
  }
  const int32_t length_buffer = length + 1;
  std::memcpy(dest.int32++, &length_buffer, 4);
  std::memcpy(dest.pointer, string, length);
  dest.chars[length] = 0;
  return true;
}

bool writer::add_binary(const char* e_name, const void* buffer,
                        std::size_t length, subtype subtype) noexcept
{
  auto pointer = add_binary(e_name, length, subtype);
  if (!pointer) {
    return false;
  }
  std::memcpy(pointer, buffer, length);
  return true;
}

void* writer::add_binary(const char* e_name, std::size_t length, subtype subtype) noexcept
{
  if (length > INT32_MAX) {
    return nullptr;
  }
  union {
    std::int32_t* int32;
    bson::subtype* subtype;
    void* pointer;
  } dest;
  dest.pointer = add_element(e_name, bson::type::binary, length + 5);
  if (!dest.pointer) {
    return nullptr;
  }
  const int32_t length_buffer = length;
  std::memcpy(dest.int32++, &length_buffer, 4);
  *dest.subtype++ = subtype;
  return dest.pointer;
}

bool writer::add_boolean(const char* e_name, bool value) noexcept
{
  const auto dest = static_cast<std::uint8_t*>(
    add_element(e_name, bson::type::boolean, 1)
  );
  if (!dest) {
    return false;
  }
  *dest = value ? 1 : 0;
  return true;
}

bool writer::add_int32(const char* e_name, std::int32_t value) noexcept
{
  const auto dest = static_cast<std::int32_t*>(
    add_element(e_name, bson::type::int32, 4)
  );
  if (!dest) {
    return false;
  }
  std::memcpy(dest, &value, 4);
  return true;
}

bool writer::add_int64(const char* e_name, std::int64_t value) noexcept
{
  const auto dest = static_cast<std::int64_t*>(
    add_element(e_name, bson::type::int64, 8)
  );
  if (!dest) {
    return false;
  }
  std::memcpy(dest, &value, 8);
  return true;
}

bool writer::get_bytes(const std::uint8_t*& bytes, std::size_t& length) const noexcept
{
  if (locked) {
    return false;
  }
  const auto root = static_cast<const writer*>(const_cast<writer*>(this)->get_root());
  const auto header_offset = (is_root ? 0 : (parent->offset - 5));
  bytes = static_cast<const std::uint8_t*>(root->buffer) + header_offset;
  length = (offset + 1) - header_offset;
  return true;
}

std::uint8_t* writer::release(std::size_t& length) noexcept
{
  if (locked || !is_root || !malloc) {
    return nullptr;
  }
  auto bytes = static_cast<std::uint8_t*>(buffer);
  length = (offset + 1);

  // Invalidate write
  parent = nullptr;
  offset = 0;
  locked = 1;
  this->length = 0;
  malloc = 0;
  return bytes;
}

void* writer::add_element(const char* e_name, type type, std::size_t space) noexcept
{
  if (locked) {
    return nullptr;
  }
  std::size_t depth;
  const auto root = get_root(&depth);
  const auto name_length = std::strlen(e_name) + 1;
  if (name_length <= 1) {
    return nullptr;
  }
  auto required = offset + 1 + name_length + space + 1 + depth;
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
  *dest++ = static_cast<std::uint8_t>(type);
  std::memcpy(dest, e_name, name_length);
  dest += name_length;
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
  const auto bytes = static_cast<std::uint8_t*>(buffer);
  const auto header_offset = is_root ? 0 : (parent->offset - 5);
  int total_buffer = new_offset + 1 - header_offset;
  std::memcpy(bytes + header_offset, &total_buffer, 4);
  bytes[new_offset] = 0x00;
  offset = new_offset;
}

bool reader::element::truthy() const noexcept
{
  union {
    double fp64;
    int32_t int32;
    int64_t int64;
  } buffer;
  switch (type()) {
  case bson::type::fp64:
    std::memcpy(&buffer.fp64, data.fp64, 8);
    return (!std::isnan(buffer.fp64)) && (buffer.fp64 != 0.0);
  case bson::type::string:
    std::memcpy(&buffer.int32, data.int32, 4);
    return (buffer.int32 > 1);
  case bson::type::document:
  case bson::type::array:
  case bson::type::binary:
    return true;
  case bson::type::boolean:
    return (*data.byte != 0x00);
  case bson::type::int32:
    std::memcpy(&buffer.int32, data.int32, 4);
    return (buffer.int32 != 0);
  case bson::type::int64:
    std::memcpy(&buffer.int64, data.int64, 8);
    return (buffer.int64 != 0);
  case bson::type::undefined:
  case bson::type::null:
  default:
    return false;
  }
}

bool reader::element::get_double(double& value) const noexcept
{
  if (!is_double()) {
    return false;
  }
  std::memcpy(&value, data.fp64, sizeof(double));
  return true;
}

bool reader::element::get_string(const char*& string) const noexcept
{
  if (!is_string()) {
    return false;
  }
  string = data.offset<char>(4);
  return true;
}

bool reader::element::get_string(const char*& string, std::size_t& length) const noexcept
{
  if (!is_string()) {
    return false;
  }
  string = data.offset<char>(4);
  int32_t length_buffer;
  std::memcpy(&length_buffer, data.int32, 4);
  length = length_buffer - 1;
  return true;
}

bool reader::element::get_binary(const void*& buffer, std::size_t& length) const noexcept
{
  if (!is_binary()) {
    return false;
  }
  buffer = data.offset<void>(5);
  int32_t length_buffer;
  std::memcpy(&length_buffer, data.int32, 4);
  length = length_buffer;
  return true;
}

bool reader::element::get_binary(const void*& buffer, std::size_t& length, bson::subtype& subtype) const noexcept
{
  if (!is_binary()) {
    return false;
  }
  buffer = data.offset<void>(5);
  int32_t length_buffer;
  std::memcpy(&length_buffer, data.int32, 4);
  length = length_buffer;
  subtype = *data.offset<bson::subtype>(4);
  return true;
}

bool reader::element::get_boolean(bool& value) const noexcept
{
  if (!is_boolean()) {
    return false;
  }
  value = (*data.byte) != 0;
  return true;
}

bool reader::element::get_int32(int32_t& value) const noexcept
{
  if (!is_int32()) {
    return false;
  }
  std::memcpy(&value, data.int32, 4);
  return true;
}

bool reader::element::get_int64(int64_t& value) const noexcept
{
  if (!is_int64()) {
    return false;
  }
  std::memcpy(&value, data.int64, 8);
  return true;
}

bool reader::element::get_integer(int64_t& value) const noexcept
{
  switch (type()) {
  case bson::type::int32:
    {
      int32_t value_buffer;
      std::memcpy(&value_buffer, data.int32, 4);
      value = value_buffer;
    }
    return true;
  case bson::type::int64:
    std::memcpy(&value, data.int64, 8);
    return true;
  default:
    return false;
  }
}

bool reader::element::get_number(double& value) const noexcept
{
  switch (type()) {
  case bson::type::fp64:
    std::memcpy(&value, data.fp64, 8);
    return true;
  case bson::type::int32:
    {
      int32_t value_buffer;
      std::memcpy(&value_buffer, data.int32, 4);
      value = value_buffer;
    }
    return true;
  case bson::type::int64:
    {
      int64_t value_buffer;
      std::memcpy(&value_buffer, data.int64, 8);
      value = value_buffer;
    }
    return true;
  default:
    return false;
  }
}

reader reader::element::as_subdocument(const reader& default_value, bson::type type) const noexcept
{
  if (this->type() != type) {
    return default_value;
  }
  int32_t length_buffer;
  std::memcpy(&length_buffer, data.int32, 4);
  return reader(data.pointer, length_buffer);
}

reader::const_iterator::const_iterator(const reader& owner) noexcept
{
  /*
    if (end_position) {
      // Current element is valid
    } else {
      // Current element is invalid
      if (next_position)
        // Failed (next_position indicates failed position)
      } else {
        // Ended
      }
    }
  */
  if (!owner.buffer) {
    // No valid buffer
    return;
  }
  next_position.pointer = owner.buffer.pointer;
  if (owner.length < 4) {
    // No total length field
    return;
  }
  int32_t total;
  std::memcpy(&total, next_position.int32, 4);
  if ((total < 4) || (static_cast<std::size_t>(total) > owner.length)) {
    // Invalid total length
    return;
  }

  // Get the first element
  end_position.pointer = next_position.offset<void>(total);
  ++next_position.int32;
  this->operator++();
}

reader::const_iterator& reader::const_iterator::operator++() noexcept
{
  if (!end_position) {
    // No more elements
    return *this;
  }

  // Get type
  if (next_position.pointer >= end_position.pointer) {
    // Buffer ended without termination
terminate:
    current.e_name = nullptr;
    current.data = nullptr;
    end_position = nullptr;
    return *this;
  }
  auto type = *next_position.type++;
  if (static_cast<std::uint8_t>(type) == 0x00) {
    // End of document
    next_position = nullptr;
    goto terminate;
  }

  // Parse e_name
  current.e_name = next_position.name;
  for (;;) {
    if (next_position.name >= end_position.name) {
      // Abnormal termination in e_name/type
      goto terminate;
    }
    if (*next_position.name++ == '\0') {
      break;
    }
  }

  // Update current
  current.data = next_position.pointer;

  switch (type) {
  case bson::type::fp64:
  case bson::type::int64:
    if (++next_position.int64 <= end_position.pointer) {
      return *this;
    }
    break;
  case bson::type::string:
    if (++next_position.int32 <= end_position.pointer) {
      std::int32_t length;
      std::memcpy(&length, &next_position.int32[-1], 4);
      if (length >= 1) {
        next_position.string += length;
        if (next_position.pointer <= end_position.pointer) {
          if (next_position.string[-1] == '\0') {
            return *this;
          }
        }
      }
    }
    break;
  case bson::type::document:
  case bson::type::array:
    if (++next_position.int32 <= end_position.pointer) {
      std::int32_t length;
      std::memcpy(&length, &next_position.int32[-1], 4);
      if (length >= 5) {
        next_position.byte += (length - 4);
        if (next_position.pointer <= end_position.pointer) {
          if (next_position.byte[-1] == 0x00) {
            return *this;
          }
        }
      }
    }
    break;
  case bson::type::binary:
    if (++next_position.int32 <= end_position.pointer) {
      std::int32_t length;
      std::memcpy(&length, &next_position.int32[-1], 4);
      if (length >= 0) {
        next_position.byte += (length + 1);
        if (next_position.pointer <= end_position.pointer) {
          return *this;
        }
      }
    }
    break;
  case bson::type::undefined:
  case bson::type::null:
    return *this;
  case bson::type::boolean:
    if (++next_position.byte <= end_position.pointer) {
      return *this;
    }
    break;
  case bson::type::int32:
    if (++next_position.int32 <= end_position.pointer) {
      return *this;
    }
    break;
  }

  // Abnormal termination in element
  goto terminate;
}

int reader::query_size(const void* buffer, std::size_t length) noexcept
{
  if (length < 4) {
    return -1;
  }
  std::int32_t total;
  std::memcpy(&total, buffer, 4);
  return total;
}

reader::element reader::find(const char* e_name) const noexcept
{
  for (const auto& field : *this) {
    if (std::strcmp(field.name(), e_name) == 0) {
      return field;
    }
  }
  return element { nullptr, nullptr };
}

} /* namespace bson */
