#ifndef MEMWRAPPER_BASIC_HPP_
#define MEMWRAPPER_BASIC_HPP_

namespace memwrapper {
/**
 * @brief Union for simple interaction with objects in memory.
 * @code{.cpp}
 * void main() {
 *  // Defining a {text} constant.
 *  const char* const text = "Hello world!";
 *
 *  // Sending this constant to memory_pointer constructor
 *  // Will be called constructor from const pointers.
 *  memwrapper::memory_pointer memptr {text};
 *
 *  // Sending to the console the address of the constant.
 *  std::cout << std::hex << std::uppercase << memptr.addressof();
 *
 *  // Returning.
 *  return 0;
 * }
 * @endcode
 */
union memory_pointer {
  protected:
    /**
     * Stores pointer of a object in the memory.
     */
    void* m_ptr;
    /**
     * Stores address of a object in the memory.
     */
    uintptr_t m_address;

  public:
    /**
     * Default constructor.
     *
     */
    memory_pointer() = default;
    /**
     * Copy-constructor.
     *
     * \param lval Another \c memory_pointer \c object to copy.
     */
    memory_pointer(const memory_pointer& lval) = default;
    /**
     * Move-constructor.
     *
     * \param rval Temporary \c memory_pointer \c object to move.
     */
    memory_pointer(memory_pointer&& rval) = default;

    /**
     * Constructor for an address.
     *
     * \param address Address of a object in the memory.
     */
    memory_pointer(const uintptr_t address)
        : m_address(address) {}

    /**
     * Constructor for a pointer.
     *
     * \param pointer Pointer of a object in the memory.
     */
    memory_pointer(void* pointer)
        : m_ptr(pointer) {}

    /**
     * Constructor for any other pointers.
     *
     * \param object Pointer of a object in the memory.
     */
    template<typename T>
    memory_pointer(T* object)
        : m_ptr(static_cast<void*>(object)) {}

    /**
     * Constructor for any constant pointer.
     * The pointer will be read-only.
     *
     * \param object Any constant object in the memory.
     */
    template<typename T>
    memory_pointer(const T* object)
        : m_ptr(static_cast<void*>(const_cast<T*>(object))) {}

    /**
     * Assigns the address.
     *
     * \param address New address.
     * \return New \c memory_pointer \c
     */
    memory_pointer operator=(uintptr_t address) {
        return m_address = address, *this;
    }

    /**
     * Assigns the pointer.
     *
     * \param pointer New pointer
     * \return New \c memory_pointer \c
     */
    memory_pointer operator=(void* pointer) { return m_ptr = pointer, *this; }

    /**
     * Assigns a object to the pointer.
     * The pointer will be read-only.
     *
     * \param object Any constant object
     * \return New \c memory_pointer \c
     */
    template<typename T>
    memory_pointer operator=(const T* object) {
        return m_ptr = static_cast<void*>(const_cast<T*>(object)), *this;
    }

    /**
     * Assigns a memory pointer to our pointer.
     *
     * \param mp Any memory pointer
     * \return New \c memory_pointer \c
     */
    memory_pointer operator=(const memory_pointer& mp) {
        return m_ptr = mp.pointerof(), *this;
    }

    /**
     * Assigns a temporary memory_pointer to our pointer.
     *
     * \param mp Any temporary memory pointer
     * \return New \c memory_pointer \c
     */
    memory_pointer operator=(memory_pointer&& mp) {
        return m_ptr = mp.pointerof(), mp.m_ptr = nullptr, *this;
    }

    /**
     * Returns the pointer in the memory.
     *
     * \return Pointer of the object in the memory.
     */
    void* pointerof() const { return m_ptr; }

    /**
     * Returns the address of the object in the memory.
     *
     * \return Address of the object in the memory.
     */
    uintptr_t addressof() const { return m_address; }

    /**
     * Casts the pointer into other integral type and returns it.
     *
     * @code{.cpp}
     * int main() {
     *  // Declaring value
     *  int val = 1;
     *
     *  // Declaring memory pointer on this value
     *  memory_pointer ptr {val};
     *
     *  // reinterpret_cast used.
     *  int val1 = ptr.cast<int>();
     *
     *  // Logging the value;
     *  std::cout << val; // output: 1
     *
     *  return 0;
     * }
     * @endcode
     *
     * \return Casted pointer.
     */
    template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    T cast() const {
        return reinterpret_cast<T>(m_ptr);
    }

    /**
     * Casts the pointer into other pointered type and returns it.
     *
     * @code{.cpp}
     * int main() {
     *  // Declaring array
     *  int[2] arr = {1, 2};
     *
     *  // Declaring memory pointer on this array
     *  memory_pointer ptr {arr};
     *
     *  // int[2] -> int* because std::decay_t
     *  // otherwise, error
     *  int* arr1 = ptr.cast<int[2]>();
     *
     *  // Logging the first element of the array.
     *  std::cout << arr1[0]; // output: 1
     *
     *  return 0;
     * }
     * @endcode
     *
     * \return Casted pointer.
     */
    template<typename T, typename Y = std::decay_t<T>,
             typename = std::enable_if_t<std::is_pointer_v<Y>>>
    Y cast() const {
        return static_cast<Y>(m_ptr);
    }

    /**
     * Shifts the address forward for a specific step and returns it.
     *
     * \param step Step to move forward.
     * \return Shifted \c memory_pointer \c
     */
    memory_pointer front(const uintptr_t step) const {
        return { m_address + step };
    }

    /**
     * Shifts the address back for a specific step and returns it.
     *
     * \param step Step to move back.
     * \return Shifted \c memory_pointer \c
     */
    memory_pointer back(const uintptr_t step) const {
        return { m_address - step };
    }

    /**
     * Shifts the address forward for a specific step and returns it.
     *
     * \param mp Step to move forward
     * \return Shifted \c memory_pointer \c
     */
    memory_pointer operator+(const uintptr_t step) const { return front(step); }

    /**
     * Shifts the address back for a specific step and returns it.
     *
     * \param mp Step to move back.
     * \return Shifted \c memory_pointer \c
     */
    memory_pointer operator-(const uintptr_t step) const { return back(step); }

    /**
     * Casts the pointer in the specific type and returns it.
     *
     * \return Casted pointer.
     */
    template<typename T>
    operator T*() const {
        return cast<T*>();
    }

    /**
     * \return Pointer on the stored object.
     */
    operator void*() const { return m_ptr; }

    /**
     * \return Address of the stored object.
     */
    operator unsigned int() const { return m_address; }

    /**
     * Checks if the pointer is nullptr.
     *
     * \return Pointer is nullptr or not.
     */
    operator bool() const { return (m_ptr != nullptr); }
};
}   // namespace memwrapper

#endif   // !MEMWRAPPER_BASIC_HPP_
