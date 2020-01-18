/**
 * @file bson_flat.hpp
 * @brief Flat buffer based BSON library for C++11
 */
#ifndef _BSON_CPP11_BSON_FLAT_HPP_
#define _BSON_CPP11_BSON_FLAT_HPP_

#ifdef __BYTE_ORDER__
# if (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__)
#  error "This library only supports little-endian environment!"
# endif
#endif

#include <cstddef>
#include <cstdint>
#include <utility>
#include <limits>
#include <iterator>
#include <ostream>

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
   * @brief Check whether the BSON writer is valid
   */
  bool valid() const noexcept
  {
    return (buffer != nullptr) || (is_root != 0);
  }

  /**
   * @brief Check if the BSON writer is valid
   */
  operator bool() const noexcept
  {
    return valid();
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
  writer add_document(const char* e_name) noexcept
  {
    return add_subdocument(e_name, bson::type::document);
  }

  /**
   * @brief Add array
   * 
   * @param e_name Element name
   */
  writer add_array(const char* e_name) noexcept
  {
    return add_subdocument(e_name, bson::type::array);
  }

  /**
   * @brief Add pre-constructed embedded document
   * 
   * @param e_name Element name
   * @param subdocument Reference to BSON writer which holds document to add
   */
  bool add_document(const char* e_name, const writer& subdocument) noexcept
  {
    return add_subdocument(e_name, bson::type::document, subdocument);
  }

  /**
   * @brief Add pre-constructed array
   * 
   * @param e_name Element name
   * @param subdocument Reference to BSON writer which holds array to add
   */
  bool add_array(const char* e_name, const writer& subdocument) noexcept
  {
    return add_subdocument(e_name, bson::type::array, subdocument);
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
   * @brief Add binary (without copy)
   * 
   * @param e_name Element name
   * @param length Length in bytes
   * @param subtype Sub type
   */
  void* add_binary(const char* e_name, std::size_t length,
                   subtype subtype = subtype::generic) noexcept;

  /**
   * @brief Add undefined
   * 
   * @param e_name Element name
   */
  bool add_undefined(const char* e_name) noexcept
  {
    return add_element(e_name, bson::type::undefined, 0) != nullptr;
  }

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
  bool add_true(const char* e_name) noexcept
  {
    return add_boolean(e_name, true);
  }

  /**
   * @brief Add boolean false
   * 
   * @param e_name Element name
   */
  bool add_false(const char* e_name) noexcept
  {
    return add_boolean(e_name, false);
  }

  /**
   * @brief Add null
   * 
   * @param e_name Element name
   */
  bool add_null(const char* e_name) noexcept
  {
    return add_element(e_name, bson::type::null, 0) != nullptr;
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

  /**
   * @brief Release BSON bytes
   * 
   * @param length Reference to retrieve length in bytes
   */
  std::uint8_t* release(std::size_t& length) noexcept;

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

/**
 * @brief BSON reader class
 */
class reader {
private:
  union accessor {
    const char* name;
    const bson::type* type;
    const bson::subtype* subtype;
    const char* string;
    const std::uint8_t* byte;
    const double* fp64;
    const std::int32_t* int32;
    const std::int64_t* int64;
    const void* pointer;

    accessor(const void* pointer) noexcept : pointer(pointer) {}

    operator bool() const noexcept { return (pointer != nullptr); }

    template <class T>
    const T* offset(int byte_offset) const noexcept
    {
      return reinterpret_cast<const T*>(byte + byte_offset);
    }
  };

public:
  // Forward declaration
  class const_iterator;

  /**
   * @brief Element reader class
   */
  class element {
  public:
    /**
     * @brief Construct a new element (copy)
     * 
     * @param other Another element
     */
    element(const element& other) noexcept
    : e_name(other.e_name), data(other.data.pointer) {}

    /**
     * @brief Construct a new element (move)
     * 
     * @param other Another element
     */
    element(element&& other) noexcept : element(other) {}

    /**
     * @brief Destroy the element
     * 
     */
    ~element() noexcept = default;

    /**
     * @brief Copy from another element
     * 
     * @param other Another element
     */
    element& operator=(const element& other) noexcept
    {
      e_name = other.e_name;
      data = other.data.pointer;
      return *this;
    }

    /**
     * @brief Move from another element
     * 
     * @param other Another element
     */
    element& operator=(element&& other) noexcept
    {
      return *this = other;
    }

    /**
     * @brief Check if the element is valid
     */
    operator bool() const noexcept { return valid(); }

    /**
     * @brief Check if the element is valid
     */
    bool valid() const noexcept { return data; }

    /**
     * @brief Check if the element is valid and has truthy value
     */
    bool truthy() const noexcept;

    /**
     * @brief Check if the element is invalid or has falsy value
     */
    bool falsy() const noexcept { return !truthy(); }

    /**
     * @brief Check if the element type is double
     */
    bool is_double() const noexcept { return type() == bson::type::fp64; }

    /**
     * @brief Check if the element type is string
     */
    bool is_string() const noexcept { return type() == bson::type::string; }

    /**
     * @brief Check if the element type is document
     */
    bool is_document() const noexcept { return type() == bson::type::document; }

    /**
     * @brief Check if the element type is array
     */
    bool is_array() const noexcept { return type() == bson::type::array; }

    /**
     * @brief Check if the element type is binary
     */
    bool is_binary() const noexcept { return type() == bson::type::binary; }

    /**
     * @brief Check if the element type is undefined
     */
    bool is_undefined() const noexcept { return type() == bson::type::undefined; }

    /**
     * @brief Check if the element type is boolean
     */
    bool is_boolean() const noexcept { return type() == bson::type::boolean; }

    /**
     * @brief Check if the element type is null
     */
    bool is_null() const noexcept { return type() == bson::type::null; }

    /**
     * @brief Check if the element type is int32
     */
    bool is_int32() const noexcept { return type() == bson::type::int32; }

    /**
     * @brief Check if the element type is int64
     */
    bool is_int64() const noexcept { return type() == bson::type::int64; }

    /**
     * @brief Check if the element type is null or undefined
     */
    bool is_null_or_undefined() const noexcept
    {
      switch (type()) {
      case bson::type::undefined:
      case bson::type::null:
        return true;
      default:
        return false;
      }
    }

    /**
     * @brief Check if the element type is int32 or int64
     */
    bool is_integer() const noexcept
    {
      switch (type()) {
      case bson::type::int32:
      case bson::type::int64:
        return true;
      default:
        return false;
      }
    }

    /**
     * @brief Check if the element type is double, int32 or int64
     */
    bool is_number() const noexcept
    {
      switch (type()) {
      case bson::type::fp64:
      case bson::type::int32:
      case bson::type::int64:
        return true;
      default:
        return false;
      }
    }

    /**
     * @brief Get double value
     * 
     * @param value Reference to store value
     */
    bool get_double(double& value) const noexcept;

    /**
     * @brief Get string value
     * 
     * @param string Reference to store pointer
     */
    bool get_string(const char*& string) const noexcept;

    /**
     * @brief Get string value
     * 
     * @param string Reference to store pointer
     * @param length Reference to store length (not includes NUL termination)
     */
    bool get_string(const char*& string, std::size_t& length) const noexcept;

    /**
     * @brief Get binary value
     * 
     * @param buffer Rerefence to store pointer
     * @param length Reference to store length
     */
    bool get_binary(const void*& buffer, std::size_t& length) const noexcept;

    /**
     * @brief Get binary value
     * 
     * @param buffer Rerefence to store pointer
     * @param length Reference to store length
     * @param subtype Reference to store subtype
     */
    bool get_binary(const void*& buffer, std::size_t& length, bson::subtype& subtype) const noexcept;

    /**
     * @brief Get boolean value
     * 
     * @param value Reference to store value
     */
    bool get_boolean(bool& value) const noexcept;

    /**
     * @brief Get int32 value
     * 
     * @param value Reference to store value
     */
    bool get_int32(int32_t& value) const noexcept;

    /**
     * @brief Get int64 value
     * 
     * @param value Reference to store value
     */
    bool get_int64(int64_t& value) const noexcept;

    /**
     * @brief Get integer value
     * 
     * @param value Reference to store value
     */
    bool get_integer(int64_t& value) const noexcept;

    /**
     * @brief Get number value
     * 
     * @param value Reference to store value
     */
    bool get_number(double& value) const noexcept;

    /**
     * @brief Get double as return value
     * 
     * @param default_value Default value if the type is not double
     */
    double as_double(double default_value = std::numeric_limits<double>::quiet_NaN()) const noexcept
    {
      (void)get_double(default_value);
      return default_value;
    }

    /**
     * @brief Get string as return value
     * 
     * @param default_value Default string if the type is not string
     */
    const char* as_string(const char* default_value = "") const noexcept
    {
      (void)get_string(default_value);
      return default_value;
    }

    /**
     * @brief Get string as return value
     * 
     * @param length Reference to store length
     */
    const char* as_string(std::size_t& length) const noexcept
    {
      const char* result;
      return get_string(result, length) ? result : (length = 0, "");
    }

    /**
     * @brief Get string as return value
     * 
     * @param length Reference to store length
     * @param default_value Default string if the type is not string
     * @param default_length Default length if the type is not string
     */
    const char* as_string(std::size_t& length, const char* default_value, std::size_t default_length) const noexcept
    {
      const char* result;
      return get_string(result, length) ? result : (length = default_length, default_value);
    }

    /**
     * @brief Get document reader as return value
     */
    reader as_document() const noexcept
    {
      return as_document(reader(nullptr, 0));
    }

    /**
     * @brief Get document reader as return value
     * 
     * @param default_value Reader for default value if the type is not document
     */
    reader as_document(const reader& default_value) const noexcept
    {
      return as_subdocument(default_value, bson::type::document);
    }

    /**
     * @brief Get array reader as return value
     */
    reader as_array() const noexcept
    {
      return as_array(reader(nullptr, 0));
    }

    /**
     * @brief Get array reader as return value
     * 
     * @param default_value Reader for default value if the type is not array
     */
    reader as_array(const reader& default_value) const noexcept
    {
      return as_subdocument(default_value, bson::type::array);
    }

    /**
     * @brief Get binary as return value
     * 
     * @param length Reference to store length
     */
    const void* as_binary(std::size_t& length) const noexcept
    {
      const void* result;
      return get_binary(result, length) ? result : (length = 0, nullptr);
    }

    /**
     * @brief Get binary as return value
     * 
     * @param length Reference to store length
     * @param subtype Reference to store subtype
     */
    const void* as_binary(std::size_t& length, subtype& subtype) const noexcept
    {
      const void* result;
      return get_binary(result, length, subtype) ? result : (length = 0, nullptr);
    }

    /**
     * @brief Get binary as return value
     * 
     * @param length Reference to store length
     * @param default_value Default pointer if the type is not binary
     * @param default_length Default length if the type is not binary
     */
    const void* as_binary(std::size_t& length, const void* default_value, std::size_t default_length) const noexcept
    {
      const void* result;
      return get_binary(result, length) ? result : (length = default_length, default_value);
    }

    /**
     * @brief Get binary as return value
     * 
     * @param length Reference to store length
     * @param subtype Reference to store subtype
     * @param default_value Default pointer if the type is not binary
     * @param default_length Default length if the type is not binary
     * @param default_subtype Default subtype if the type is not binary
     */
    const void* as_binary(std::size_t& length, subtype& subtype,
      const void* default_value, std::size_t default_length, 
      bson::subtype default_subtype = bson::subtype::generic) const noexcept
    {
      const void* result;
      return get_binary(result, length, subtype) ? result :
        (length = default_length, subtype = default_subtype, default_value);
    }

    /**
     * @brief Get boolean as return value
     * 
     * @param default_value Default value if the type is not boolean
     */
    bool as_boolean(bool default_value = false) const noexcept
    {
      (void)get_boolean(default_value);
      return default_value;
    }

    /**
     * @brief Get int32 as return value
     * 
     * @param default_value Default value if the type is not int32
     */
    int32_t as_int32(int32_t default_value = 0) const noexcept
    {
      (void)get_int32(default_value);
      return default_value;
    }

    /**
     * @brief Get int64 as return value
     * 
     * @param default_value Default value if the type is not int64
     */
    int64_t as_int64(int64_t default_value = 0) const noexcept
    {
      (void)get_int64(default_value);
      return default_value;
    }

    /**
     * @brief Get integer as return value
     * 
     * @param default_value Default value if the type is not int32 nor int64
     */
    int64_t as_integer(int64_t default_value = 0) const noexcept
    {
      (void)get_integer(default_value);
      return default_value;
    }

    /**
     * @brief Get number as return value
     * 
     * @param default_value Default value if the type is not double, int32 nor int64
     */
    double as_number(double default_value = std::numeric_limits<double>::quiet_NaN()) const noexcept
    {
      (void)get_number(default_value);
      return default_value;
    }

    /**
     * @brief Get element name
     */
    const char* name() const noexcept
    {
      return e_name;
    }

    /**
     * @brief Get element type
     */
    bson::type type() const noexcept
    {
      return data ? static_cast<bson::type>(e_name[-1]) : static_cast<bson::type>(0);
    }

  private:
    reader as_subdocument(const reader& default_value, bson::type type) const noexcept;

    element(const char* e_name, const void* data)
    : e_name(e_name), data(data) {}

    friend class const_iterator;
    friend class reader;
    friend std::ostream& operator<<(std::ostream&, const element&);

  private:
    const char* e_name = nullptr;
    accessor data { nullptr };
  };

  /**
   * @brief Iterator class for BSON reader
   */
  class const_iterator : public std::iterator<std::forward_iterator_tag, const element> {
  public:
    /**
     * @brief Construct a new const_iterator
     */
    const_iterator() noexcept {}

    /**
     * @brief Construct a new const_iterator (copy)
     * 
     * @param other Another iterator
     */
    const_iterator(const const_iterator& other) noexcept
    : current(other.current),
      next_position(other.next_position), end_position(other.end_position) {}

    /**
     * @brief Destroy the const_iterator
     */
    ~const_iterator() noexcept = default;

    /**
     * @brief Access element as reference
     */
    reference operator*() const noexcept
    {
      return current;
    }

    /**
     * @brief Access element as pointer
     */
    pointer operator->() const noexcept
    {
      return &this->operator*();
    }

    /**
     * @brief Advance to next element (Pre-increment)
     */
    const_iterator& operator++() noexcept;

    /**
     * @brief Advance to next element (Post-increment)
     */
    const_iterator operator++(int) noexcept
    {
      const_iterator old = *this;
      this->operator++();
      return old;
    }

    /**
     * @brief Compare (!=) two iterators
     * 
     * @param other Another iterator
     */
    bool operator!=(const const_iterator& other) const noexcept
    {
      if (end_position) {
        if (other.end_position) {
          return (current.e_name != other.current.e_name);
        }
        return true;
      }
      return other.end_position;
    }

    /**
     * @brief Compare (==) two iterators
     * 
     * @param other Another iterator
     */
    bool operator==(const const_iterator& other) const noexcept
    {
      return !this->operator!=(other);
    }

    /**
     * @brief Check if the iterator is failed state
     */
    bool fail() const noexcept
    {
      return ((!end_position) && (next_position));
    }

  private:
    const_iterator(const reader& owner) noexcept;

    void parse(accessor start) noexcept;

    void set_next(const void* position) noexcept;

    friend class reader;
    friend std::ostream& operator<<(std::ostream&, const const_iterator&);

  private:
    element current { nullptr, nullptr };
    accessor next_position { nullptr };
    accessor end_position { nullptr };
  };

  using iterator = const_iterator;

public:
  /**
   * @brief Query document length
   * 
   * @param buffer Pointer to buffer
   * @param length Length of buffer
   * @return <0 Cannot detect (more data required)
   * @return >=0 Length of document
   */
  static int query_size(const void* buffer, std::size_t length) noexcept;

  /**
   * @brief Construct a new BSON reader
   * 
   * @param buffer Pointer to buffer
   * @param length Length of buffer
   */
  reader(const void* buffer, std::size_t length) noexcept
  : buffer(static_cast<const std::uint8_t*>(buffer)), length(length) {}

  /**
   * @brief Destroy the BSON reader
   */
  ~reader() noexcept = default;

  /**
   * @brief Check if the reader has a valid buffer
   */
  bool valid() const noexcept { return (buffer.pointer != nullptr); }

  /**
   * @brief Check if the reader has a valid buffer
   */
  operator bool() const noexcept { return valid(); }

  /**
   * @brief Return an iterator to the beginning
   */
  iterator begin() const noexcept { return cbegin(); }

  /**
   * @brief Return an iterator to the end
   */
  iterator end() const noexcept { return cend(); }

  /**
   * @brief Return an iterator to the beginning (const)
   */
  const_iterator cbegin() const noexcept { return const_iterator(*this); }

  /**
   * @brief Return an iterator to the end (const)
   */
  const_iterator cend() const noexcept { return const_iterator(); }

  /**
   * @brief Find a field
   * 
   * @param e_name Element name to find
   */
  element find(const char* e_name) const noexcept;

private:
  accessor buffer;
  std::size_t length;
  friend class element;
};

} /* namespace bson */

#endif  /* _BSON_CPP11_BSON_FLAT_HPP_ */
