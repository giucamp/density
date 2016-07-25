
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <cstddef>
#include <limits>
#include <memory>

namespace testity
{
	#ifdef _WIN32 // currently supported only on windows
    
	class SanitizerAllocator
    {
    public:

		SanitizerAllocator();

		SanitizerAllocator(const SanitizerAllocator &);

		SanitizerAllocator(SanitizerAllocator &&) noexcept;

		SanitizerAllocator & operator = (const SanitizerAllocator &);

		SanitizerAllocator & operator = (SanitizerAllocator &&) noexcept;

		~SanitizerAllocator();

		void * allocate(size_t i_size, size_t i_alignment, size_t i_alignment_offset);
        
		void deallocate(void * i_block);

		void deallocate(void * i_block, size_t i_size );

		void deallocate(void * i_block, size_t i_size, size_t i_alignment, size_t i_alignment_offset);

	private:
		class Impl;
		std::unique_ptr<Impl> m_impl;
    };

	#endif // #ifdef _WIN32

} // namespace testity
