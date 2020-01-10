/**
 * @file bson_flat.hpp
 * @brief Flat buffer based BSON library for C++!1
 */
#ifndef _BSONLIB_BSON_FLAT_HPP_
#define _BSONLIB_BSON_FLAT_HPP_

#include <cstddef>
#include <cstdint>
#include <utility>
#include <iterator>

namespace bson {

enum class type : std::uint8_t {
  fp64      = 0x01,
  string    = 0x02,
  document  = 0x03,
  array     = 0x04,
  binary    = 0x05,
  undefined = 0x06,
  boolean   = 0x08,
  null      = 0x0a,
  int32     = 0x10,
  int64     = 0x12,
};

enum class subtype : std::uint8_t {
  generic         = 0x00,
  function        = 0x01,
  binary          = 0x02,
  uuid_old        = 0x04,
  uuid            = 0x04,
  md5             = 0x05,
  encrypted_bson  = 0x06,
  user_defined    = 0x80,
};

/**
 * @brief BSON writer class
 */
class writer {
public:
  /**
   * @brief Construct a new BSON writer (auto allocation)
   */
  writer() noexcept;

  /**
   * @brief Construct a new BSON writer (fixed buffer)
   * 
   * @param buffer Pointer to buffer
   * @param length Max length in bytes
   */
  writer(void* buffer, std::size_t length) noexcept;

  /**
   * @brief Destroy the BSON writer
   */
  ~writer() noexcept;

  /**
   * @brief Construct a new BSON writer by move from others
   */
  writer(writer&&) noexcept = default;

  // Prohibit copying and moving
  writer(const writer&) = delete;
  writer& operator =(writer&) = delete;
  writer& operator =(writer&& other) = delete;

  // Prohibit construction with new
  void* operator new(std::size_t) = delete;
  void* operator new[](std::size_t) = delete;

  /**
   * @brief Determine if the BSON writer is valid
   */
  operator bool() const noexcept {
    return (buffer != nullptr) || (is_root != 0);
  }

  /**
   * @brief Add double value
   * 
   * @param e_name Element name
   * @param value Double value to store
   */
  bool add_double(const char* e_name, double value) noexcept;

  /**
   * @brief Add NUL-terminated string
   * 
   * @param e_name Element Name
   * @param string String to store (NUL terminated)
   */
  bool add_string(const char* e_name, const char* string) noexcept;

  /**
   * @brief Add string
   * 
   * @param e_name Element name
   * @param string String to store (can include NUL)
   * @param length Length of string in bytes
   */
  bool add_string(const char* e_name, const char* string, std::size_t length) noexcept;

  /**
   * @brief Add embedded document
   * 
   * @param e_name Element name
   */
  writer add_document(const char* e_name) noexcept {
    return add_subdocument(e_name, type::document);
  }

  /**
   * @brief Add array
   * 
   * @param e_name Element name
   */
  writer add_array(const char* e_name) noexcept {
    return add_subdocument(e_name, type::array);
  }

  /**
   * @brief Add pre-constructed embedded document
   * 
   * @param e_name Element name
   * @param subdocument Reference to BSON writer which holds document to add
   */
  bool add_document(const char* e_name, const writer& subdocument) noexcept {
    return add_subdocument(e_name, type::document, subdocument);
  }

  /**
   * @brief Add pre-constructed array
   * 
   * @param e_name Element name
   * @param subdocument Reference to BSON writer which holds array to add
   */
  bool add_array(const char* e_name, const writer& subdocument) noexcept {
    return add_subdocument(e_name, type::array, subdocument);
  }

  /**
   * @brief Add binary
   * 
   * @param e_name Element name
   * @param buffer Pointer to buffer
   * @param length Length in bytes
   * @param subtype Sub type
   */
  bool add_binary(const char* e_name, const void* buffer, std::size_t length,
                  subtype subtype = subtype::generic) noexcept;

  /**
   * @brief Add undefined
   * 
   * @param e_name Element name
   */
  bool add_undefined(const char* e_name) noexcept {
    return add_element(e_name, type::undefined, 0) != nullptr;
  };

  /**
   * @brief Add boolean
   * 
   * @param e_name Element name
   * @param value Boolean value to store
   */
  bool add_boolean(const char* e_name, bool value) noexcept;

  /**
   * @brief Add boolean true
   * 
   * @param e_name Element name
   */
  bool add_true(const char* e_name) noexcept {
    return add_boolean(e_name, true);
  }

  /**
   * @brief Add boolean false
   * 
   * @param e_name Element name
   */
  bool add_false(const char* e_name) noexcept {
    return add_boolean(e_name, false);
  }

  /**
   * @brief Add null
   * 
   * @param e_name Element name
   */
  bool add_null(const char* e_name) noexcept {
    return add_element(e_name, type::null, 0) != nullptr;
  }

  /**
   * @brief Add 32-bit signed integer
   * 
   * @param e_name Element name
   * @param value Integer value to store
   */
  bool add_int32(const char* e_name, std::int32_t value) noexcept;

  /**
   * @brief Add 64-bit signed integer
   * 
   * @param e_name Element name
   * @param value Integer value to store
   */
  bool add_int64(const char* e_name, std::int64_t value) noexcept;

  /**
   * @brief Get the BSON bytes
   * 
   * @note When the writer is locked, this function fails.
   * @param bytes Reference to retrieve pointer
   * @param length Reference to retrieve length in bytes
   */
  bool get_bytes(const std::uint8_t*& bytes, std::size_t& length) const noexcept;

private:
  /**
   * @brief Construct an invalid BSON writer
   */
  writer(std::nullptr_t) noexcept
  : parent(nullptr), offset(0), locked(1), length(0), malloc(0) {}

  /**
   * @brief Construct a new BSON writer for subdocument
   * 
   * @param parent Parent writer
   * @param offset Start offset of subdocument
   */
  writer(writer* parent, std::uint32_t offset) noexcept
  : parent(parent), offset(offset), locked(0), length(0), malloc(0) {}

  /**
   * @brief Allocate space and add element 
   * 
   * @param e_name Element name
   * @param type Element type
   * @param space Length to allocate additionally
   */
  void* add_element(const char* e_name, type type, std::size_t space) noexcept;

  /**
   * @brief Add subdocument (embedded document or array)
   * 
   * @param e_name Element name
   * @param type Element type
   */
  writer add_subdocument(const char* e_name, type type) noexcept;

  /**
   * @brief Add pre-constructed subdocument (embedded document or array)
   * 
   * @param e_name Element name
   * @param type Element type
   * @param subdocument Reference to BSON writer which holds subdocument to add
   */
  bool add_subdocument(const char* e_name, type type, const writer& subdocument) noexcept;

  /**
   * @brief Get the root writer
   * 
   * @param depth_store If not nullptr, depth (zero for root) will be stored
   */
  writer* get_root(std::size_t* depth_store = nullptr) noexcept;

  /**
   * @brief Write offset and update total size and termination byte
   * 
   * @param buffer Pointer to root's buffer
   * @param new_offset New offset value
   */
  void update_offset(void* buffer, std::uint32_t new_offset) noexcept;

private:
  union {
    void* buffer;     ///< Data buffer (when is_root != 0)
    writer* parent;   ///< Parent document (when is_root == 0)
  };
  union {
    struct {
      std::uint32_t offset : 31;  ///< Next byte offset
      std::uint32_t locked : 1;   ///< Set when sub document is editing
    };
    std::uint32_t state;
  };
  union {
    struct {
      std::uint32_t length : 31;  ///< Buffer length (zero for subdocuments)
      std::uint32_t malloc : 1;   ///< Set if dynamic allocation enabled
    };
    std::uint32_t is_root;
  };
};

} /* namespace bson */

#endif  /* _BSONLIB_BSON_FLAT_HPP_ */
