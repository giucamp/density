
#pragma once
#include <memory>

namespace testity
{
    class SharedBlockRegistry
    {
    public:

		SharedBlockRegistry();

		SharedBlockRegistry(const SharedBlockRegistry &);

		SharedBlockRegistry(SharedBlockRegistry &&);

		~SharedBlockRegistry();

		SharedBlockRegistry & operator = (const SharedBlockRegistry &);

		SharedBlockRegistry & operator = (SharedBlockRegistry &&);
		
        void add_block(void * i_block, size_t i_size, size_t i_alignment, size_t i_alignment_offset);

        void remove_block(void * i_block, size_t i_size, size_t i_alignment, size_t i_alignment_offset);

		bool operator == (const SharedBlockRegistry & i_other) const
			{ return m_data == i_other.m_data; }

		bool operator != (const SharedBlockRegistry & i_other) const
			{ return m_data != i_other.m_data; }

    private:
		struct AllocationEntry;
		struct Data;

    private:
		std::shared_ptr<Data> m_data;
    };

    template <class TYPE>
        class TestAllocator
    {
    public:
        typedef TYPE value_type;

        TestAllocator() {}

        template <class OTHER_TYPE> TestAllocator(const TestAllocator<OTHER_TYPE>& i_other) 
			: m_block_registry(i_other.m_block_registry)
				{ }

        TYPE * allocate(std::size_t i_count)
        {
            exception_check_point();
            void * block = operator new (i_count * sizeof(TYPE));
			m_block_registry.add_block(block, i_count * sizeof(TYPE), alignof(std::max_align_t));
            return static_cast<TYPE*>(block);
        }

        void deallocate(TYPE * i_block, std::size_t i_count)
        {
			m_block_registry.remove_block(i_block, sizeof(TYPE) * i_count, alignof(std::max_align_t));
            operator delete( i_block );
        }

        template <typename OTHER_TYPE>
            bool operator == (const TestAllocator<OTHER_TYPE> & i_other ) const
        {
            return m_block_registry == i_other.m_block_registry;
        }

        template <typename OTHER_TYPE>
            bool operator != (const TestAllocator<OTHER_TYPE> &) const
        {
			return m_block_registry != i_other.m_block_registry;
        }

        #if defined(_MSC_VER) && _MSC_VER < 1900 // Visual Studio 2013 and below

            template<class Other> struct rebind { typedef TestAllocator<Other> other; };

            void construct(TYPE * i_pointer)
            {
                new (i_pointer) TYPE();
            }

            void construct(TYPE * i_pointer, const TYPE & i_source)
            {
                new (i_pointer) TYPE(i_source);
            }

            template<class OTHER_TYPE, class... ARG_TYPES>
                void construct(OTHER_TYPE * i_pointer, ARG_TYPES &&... i_args)
            {
                new (i_pointer)OTHER_TYPE(std::forward<ARG_TYPES>(i_args)...);
            }

            void destroy(TYPE * i_pointer)
            {
                i_pointer->~TYPE();
                (void)i_pointer; // avoid warning C4100: 'i_pointer' : unreferenced formal parameter
            }

        #endif

	private:
		SharedBlockRegistry m_block_registry;
    };

} // namespace testity