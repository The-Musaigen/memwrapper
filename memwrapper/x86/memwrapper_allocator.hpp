#ifndef MEMWRAPPER_ALLOCATOR_HPP_
#define MEMWRAPPER_ALLOCATOR_HPP_

namespace memwrapper
{
	class basic_allocator
	{
	protected:
		uint8_t* m_code;
		uint32_t m_size;
		uint32_t m_offset;
	public:
		basic_allocator(uint32_t size = 4096) :
			m_code(nullptr),
			m_size(0),
			m_offset(0)
		{
			SYSTEM_INFO sysinfo{ 0 };
			GetSystemInfo(&sysinfo);

			m_size = detail::align_value(size, sysinfo.dwPageSize);
			m_code = reinterpret_cast<uint8_t*>(
				VirtualAlloc(NULL, m_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)
			);
		}

		// writing manipulations
		void db(uint8_t opcode)
		{
			if ((m_offset + sizeof(uint8_t)) > m_size)
				return;

			m_code[m_offset++] = opcode;
		}

		template<typename T>
		void db(T* object, uint32_t size)
		{
			uint8_t* oplist{ reinterpret_cast<uint8_t*>(object) };

			for (uint32_t i = 0; i < size; i++)
				db(oplist[i]);
		}

		template<typename T>
		void dbvalue(T value)
		{
			if constexpr (sizeof(T) > 1)
			{
				detail::byteof<T> byteof{ value };

				for (size_t i = 0; i < sizeof(T); i++)
					db(byteof.bytes[i]);
			}
			else
				db(static_cast<uint8_t>(value));
		}

		template<typename T, typename... Args>
		void dbvalues(T value, Args... args)
		{
			dbvalue(value);

			if constexpr (sizeof...(Args) > 1)
				dbvalues(args...);
			else
				dbvalue(args...);
		}

		// code pointer manipulations
		uint8_t* begin()				{ return m_code; }
		uint8_t* now()					{ return &m_code[m_offset]; }
		uint8_t* end()					{ return &m_code[m_size]; }
		uint8_t* get(uint32_t offset)	{ return &m_code[offset]; }

		void free() { VirtualFree(m_code, NULL, MEM_RELEASE); }
		void ready() { flush_memory(m_code, m_size); }

		// offset manipulations
		uint32_t get_offset()
		{ return m_offset; }

		void set_offset(uint32_t offset)
		{
			if ((offset < 0) || (offset >= m_size))
				return;

			m_offset = offset;
		}
	}; // !class basic_allocator

	class asm_allocator : public basic_allocator
	{
	public:
		asm_allocator(uint32_t size = 4096) :
			basic_allocator(size)
		{}

		// yeah, another class for one function
		void jmp(const memory_pointer& to)
		{
			auto rel32 = detail::get_relative_address(to, now());

			db(0xE9);
			dbvalue(rel32);
		}
	}; // !class asm_allocator : public basic_allocator
}

#endif // !MEMWRAPPER_ALLOCATOR_HPP_
