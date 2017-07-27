
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/heterogeneous_queue.h>
#include <mutex>

namespace density
{
    /** Class template implementing a concurrent heterogeneous FIFO pseudo-container. 	

		concurrent_heterogeneous_queue is a concurrent version of heterogeneous_queue, with a mutex embedded within. 
		It allows different threads to put and consume elements concurrently without any external synchronization.

		@tparam COMMON_TYPE Common type of all the elements. An object of type E can be pushed on the queue only if E* is 
			implicitly convertible to COMMON_TYPE*. If COMMON_TYPE is void (the default), any type can be put in the queue. 
			Otherwise it should be an user-defined-type, and only types deriving from it can be added.
        @tparam RUNTIME_TYPE Runtime-type object used to handle the actual complete type of each element.
                This type must meet the requirements of \ref RuntimeType_concept "RuntimeType". The default is runtime_type.
        @tparam ALLOCATOR_TYPE Allocator type to be used. This type must meet the requirements of both \ref UntypedAllocator_concept
                "UntypedAllocator" and \ref PagedAllocator_concept "PagedAllocator". The default is density::void_allocator.
		
		\n <b>Thread safeness</b>: Put and consumes can be execute concurrently. Lifetime function can't.
		\n <b>Exception safeness</b>: Any function of concurrent_heterogeneous_queue is noexcept or provides the strong exception guarantee.
		

		Implementation and performance notes
		--------------------------
		concurrent_heterogeneous_queue is basically an heterogeneous_queue protected by an std::mutex to avoid data races.
		
		Non-reentrant operations keep the mutex locked during the whole operation (until the operation is 
		canceled or commited). Reentrant operations minimize the durations of the locks: the mutex is locked once when
		the operation starts, and another time to commit or cancel the operation. */
	template < typename COMMON_TYPE = void, typename RUNTIME_TYPE = runtime_type<COMMON_TYPE>, typename ALLOCATOR_TYPE = void_allocator >
        class concurrent_heterogeneous_queue
    {
		using InnerQueue = heterogeneous_queue<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE>;

		/** This type is used to make some functions of the inner classes accessible only by the queue */
		enum class PrivateType {};

	public:

		/** Minimum guaranteed alignment for every element. The actual alignment of an element may be stricter
			if the type requires it. */
		constexpr static size_t min_alignment = InnerQueue::min_alignment;

		using common_type = COMMON_TYPE;
		using runtime_type = RUNTIME_TYPE;
        using value_type = std::pair<const runtime_type &, common_type* const>;
        using allocator_type = ALLOCATOR_TYPE;
        using pointer = value_type *;
        using const_pointer = const value_type *;
        using reference = value_type;
        using const_reference = const value_type&;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
		
		class iterator;
        class const_iterator;

        /** Default constructor. The allocator is default-constructed.

            <b>Complexity</b>: constant.
            \n <b>Throws</b>: nothing.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).
            \n <i>Implementation notes</i>:
				This constructor does not allocate memory and never throws. 
				
		\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue default_construct example 1 */
		concurrent_heterogeneous_queue() noexcept = default;

		/** Constructor with allocator parameter. The allocator is copy-constructed.
			@param i_source_allocator source used to copy-construct the allocator.

            <b>Complexity</b>: constant.
            \n <b>Throws</b>: whatever the copy constructor of the allocator throws.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).
            \n <i>Implementation notes</i>:
				This constructor does not allocate memory. It throws anything the copy constructor of the allocator throws. 
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue construct_copy_alloc example 1 */
        concurrent_heterogeneous_queue(const ALLOCATOR_TYPE & i_source_allocator)
				noexcept (std::is_nothrow_copy_constructible<ALLOCATOR_TYPE>::value)
            : m_queue(i_source_allocator)
        {
        }

		/** Constructor with allocator parameter. The allocator is move-constructed.
			@param i_source_allocator source used to move-construct the allocator.

            <b>Complexity</b>: constant.
            \n <b>Throws</b>: whatever the move constructor of the allocator throws.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).
            \n <i>Implementation notes</i>:
				This constructor does not allocate memory. It throws anything the move constructor of the allocator throws.
				
		\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue construct_move_alloc example 1 */
        concurrent_heterogeneous_queue(ALLOCATOR_TYPE && i_source_allocator)
				noexcept (std::is_nothrow_move_constructible<ALLOCATOR_TYPE>::value)
            : m_queue(std::move(i_source_allocator))
        {
        }

        /** Move constructor. The allocator is move-constructed from the one of the source.
                @param i_source source to move the elements from. After the call the source is left in some valid but indeterminate state.

            <b>Complexity</b>: constant.
            \n <b>Throws</b>: nothing.
            \n <i>Implementation notes</i>:
                - After the call the source is left empty. 
				
		\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue move_construct example 1 */
		concurrent_heterogeneous_queue(concurrent_heterogeneous_queue && i_source) noexcept
			: m_queue(std::move(i_source.m_queue))
		{

		}

        /** Move assignment. The allocator is move-assigned from the one of the source.
                @param i_source source to move the elements from. After the call the source is left in some valid but indeterminate state.

            <b>Complexity</b>: Unspecified.
            \n <b>Effects on iterators</b>: Any iterator pointing to this queue or to the source queue is invalidated.
            \n <b>Throws</b>: Nothing.

            \n <i>Implementation notes</i>:
                - After the call the source is left empty.
                - The complexity is linear in the number of elements in this queue.

		\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue move_assign example 1 */
		concurrent_heterogeneous_queue & operator = (concurrent_heterogeneous_queue && i_source) noexcept
		{
			m_queue = std::move(i_source.m_queue);
			return *this;
		}

		/** Returns a copy of the allocator
		
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue get_allocator example 1 */
		allocator_type get_allocator() noexcept(std::is_nothrow_copy_constructible<allocator_type>::value)
		{
			return m_queue.get_allocator();
		}

		/** Returns a reference to the allocator 
		
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue get_allocator_ref example 1 */
		allocator_type & get_allocator_ref() noexcept
		{
			return m_queue.get_allocator_ref();
		}

		/** Returns a const reference to the allocator

		\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue get_allocator_ref example 2 */
		const allocator_type & get_allocator_ref() const noexcept
		{
			return m_queue.get_allocator_ref();
		}

		/** Swaps two queues. 
		
		\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue swap example 1 */
		friend inline void swap(concurrent_heterogeneous_queue<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE> & i_first,
			concurrent_heterogeneous_queue<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE> & i_second) noexcept
		{
            swap(i_first.m_queue, i_second.m_queue);
		}

        /** Destructor.

            <b>Complexity</b>: linear.
            \n <b>Effects on iterators</b>: any iterator pointing to this queue is invalidated.
            \n <b>Throws</b>: Nothing. */
		~concurrent_heterogeneous_queue() = default;

        /** Returns whether the queue contains no elements.

            <b>Complexity</b>: Unspecified.
            \n <b>Throws</b>: Nothing. 

		\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue empty example 1 */
        bool empty() const noexcept
        {
			std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.empty();
        }

        /** Deletes all the elements in the queue.

            <b>Complexity</b>: linear.
            \n <b>Effects on iterators</b>: any iterator is invalidated
            \n <b>Throws</b>: Nothing. 
		
		\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue clear example 1 */
        void clear() noexcept
        {
			std::lock_guard<std::mutex> lock(m_mutex);
			m_queue.clear();
        }

        /** Move-only class template that can be bound to a put transaction, otherwise it's empty.

			@tparam ELEMENT_COMPLETE_TYPE Complete type of elements that can be handled by a transaction, or void.
				ELEMENT_COMPLETE_TYPE must decay to itself (it can't be cv-qualified).
			
			Transactional put functions on concurrent_heterogeneous_queue return a non-empty put_transaction that can be 
			used to allocate raw memory in the queue, inspect or alter the element while it is still not observable
			in the queue, and commit or cancel the push. 
			
			A put_transaction is empty when:
				- it is default constructed
				- it is used as source for a move construction or move assignment
				- after a cancel or a commit
			
			Calling \ref put_transaction::raw_allocate "raw_allocate", \ref put_transaction::raw_allocate_copy "raw_allocate_copy",
			\ref put_transaction::commit "commit", \ref put_transaction::cancel "cancel", \ref put_transaction::element_ptr "element_ptr",
			\ref put_transaction::element "element" or \ref put_transaction::complete_type "complete_type" on an empty put_transaction
			triggers undefined behavior.

			A void put_transaction can be move constructed/assigned from any put_transaction. A typed put_transaction
			can be move constructed/assigned only from a put_transaction with the same ELEMENT_COMPLETE_TYPE. */
        template <typename ELEMENT_COMPLETE_TYPE = void>
			class put_transaction
        {
			static_assert(std::is_same<ELEMENT_COMPLETE_TYPE, typename std::decay<ELEMENT_COMPLETE_TYPE>::type>::value, "");
        
		public:

			/** Constructs an empty put transaction */
			put_transaction() noexcept = default;

            /** Copy construction is not allowed.
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue put_transaction copy_construct example 1 */
            put_transaction(const put_transaction &) = delete;

            /** Copy assignment is not allowed.
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue put_transaction copy_assign example 1 */
            put_transaction & operator = (const put_transaction &) = delete;

            /** Move constructs a put_transaction, transferring the state from the source.
                    @param i_source source to move from. It becomes empty after the call. 

			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue put_transaction move_construct example 1
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue put_transaction move_construct example 2 */
			template <typename OTHERTYPE, typename = typename std::enable_if<
				std::is_same<OTHERTYPE, ELEMENT_COMPLETE_TYPE>::value || std::is_void<ELEMENT_COMPLETE_TYPE>::value >::type >
				put_transaction(put_transaction<OTHERTYPE> && i_source) noexcept
					: m_lock(std::move(i_source.m_lock)), m_put_transaction(std::move(i_source.m_put_transaction))
			{
			}

            /** Move assigns a put_transaction, transferring the state from the source.
                @param i_source source to move from. It becomes empty after the call. 
				
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue put_transaction move_assign example 1
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue put_transaction move_assign example 2 */
			template <typename OTHERTYPE, typename = typename std::enable_if<
					std::is_same<OTHERTYPE, ELEMENT_COMPLETE_TYPE>::value || std::is_void<ELEMENT_COMPLETE_TYPE>::value >::type >
				put_transaction & operator = (put_transaction<OTHERTYPE> && i_source) noexcept
            {
				if (this != static_cast<void*>(&i_source)) // cast to void to allow comparing pointers to unrelated types
				{
					if(!empty())
						cancel();

					put_transaction source(std::move(i_source));

					using namespace density;
					swap(m_put_transaction, source.m_put_transaction);
					swap(m_lock, source.m_lock);
				}
				return *this;
            }

			/** Swaps two instances of put_transaction.

				\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue put_transaction swap example 1 */
			friend void swap(put_transaction & i_first, put_transaction & i_second) noexcept
			{
				using namespace std;
				swap(i_first.m_put_transaction, i_second.m_put_transaction);
				swap(i_first.m_lock, i_second.m_lock);
			}

            /** Allocates a memory block associated to the element being added in the queue. The block may be allocated contiguously with
                the elements in the memory pages. If the block does not fit in one page, the block is allocated using non-paged memory services
                of the allocator.

                \n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial
                    content of the block is undefined.

                @param i_size size in bytes of the block to allocate.
                @param i_alignment alignment of the block to allocate. It must be a non-zero power of 2, and less than or
						equal to i_size.

                \pre The behavior is undefined if either:
					- this transaction is empty
					- the alignment is not valid
					- the size is not a multiple of the alignment

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: unspecified.
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). 
				
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue put_transaction raw_allocate example 1*/
            void * raw_allocate(size_t i_size, size_t i_alignment)
            {
                return m_put_transaction.raw_allocate(i_size, i_alignment);
            }

            /** Allocates a memory block associated to the element being added in the queue, and copies the content from a range 
				of iterators. The block may be allocated contiguously with the elements in the memory pages. If the block does not
				fit in one page, the block is allocated using non-paged memory services of the allocator.
				
                \n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial
                    content of the block is undefined.

                @param i_begin first element to be copied
                @param i_end first element not to be copied

				\n <b>Requires</b>:
					- INPUT_ITERATOR must meet the requirements of <a href="http://en.cppreference.com/w/cpp/concept/InputIterator">InputIterator</a>
					- the value type must be trivially destructible

                \pre The behavior is undefined if either:
					- this transaction is empty
					- i_end is not reachable from i_begin

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: unspecified.
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects)
				
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue put_transaction raw_allocate_copy example 1*/
            template <typename INPUT_ITERATOR>
                typename std::iterator_traits<INPUT_ITERATOR>::value_type *
                    raw_allocate_copy(INPUT_ITERATOR i_begin, INPUT_ITERATOR i_end)
            {
                return m_put_transaction.raw_allocate_copy(i_begin, i_end);
            }

			/** Allocates a memory block associated to the element being added in the queue, and copies the content from a range.
				The block may be allocated contiguously with the elements in the memory pages. If the block does not
				fit in one page, the block is allocated using non-paged memory services of the allocator.
				
                \n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial
                    content of the block is undefined.

                @param i_source_range to be iterated

				\n <b>Requires</b>:
					- The iterators of the range must meet the requirements of <a href="http://en.cppreference.com/w/cpp/concept/InputIterator">InputIterator</a>
					- the value type must be trivially destructible

                \pre The behavior is undefined if either:
					- this transaction is empty

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: unspecified.
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).
				
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue raw_allocate_copy example 2 */
            template <typename INPUT_RANGE>
                auto raw_allocate_copy(const INPUT_RANGE & i_source_range)
                    -> decltype(raw_allocate_copy(std::begin(i_source_range), std::end(i_source_range)))
            {
                return m_put_transaction.raw_allocate_copy(std::begin(i_source_range), std::end(i_source_range));
            }

            /** Makes the effects of the transaction observable. This object becomes empty.

				\pre The behavior is undefined if either:
					- this transaction is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. */
            void commit() noexcept
            {
				DENSITY_ASSERT(m_put_transaction && m_lock.owns_lock());
				m_put_transaction.commit();
				m_lock.unlock();
            }

			/** Cancel the transaction. This object becomes empty.

				\pre The behavior is undefined if either:
					- this transaction is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. 
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue put_transaction cancel example 1 */
            void cancel() noexcept
            {
				DENSITY_ASSERT(m_put_transaction && m_lock.owns_lock());
				m_put_transaction.cancel();
				m_lock.unlock();
            }

            /** Returns true whether this object is not currently bound to a transaction. 

			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue put_transaction empty example 1 */
            bool empty() const noexcept { return m_put_transaction.empty(); }

			/** Returns true whether this object is bound to a transaction. Same to !consume_operation::empty.
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue put_transaction operator_bool example 1 */
            explicit operator bool() const noexcept
            {
                return m_put_transaction.operator bool();
            }


            /** Returns a pointer to the object being added.
                \n <i>Notes</i>: 
				- The object is constructed when the transaction is started, so this function always returns a 
					pointer to a valid object.
				- This function returns a pointer to the common_type subobject of the element. Non-dynamic 
					transactional put function (start_push, start_emplace, start_reentrant_push, start_reentrant_emplace)
					return a typed_put_transaction or a reentrant_put_transaction, that provide the function element(),
					which is a better alternative to this function

				\pre The behavior is undefined if either:
					- this transaction is empty 
					
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue put_transaction element_ptr example 1
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue put_transaction element_ptr example 2 */
            common_type * element_ptr() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return m_put_transaction.element_ptr();
            }

			/** Returns a reference to the element being added. This function can be used to modify the element 
					before calling the commit.
                \n <i>Note</i>: An element is observable in the queue only after commit has been called on the
					put_transaction. The element is constructed at the begin of the transaction, so the
					returned object is always valid.

				\n <b>Requires</b>:
					- ELEMENT_COMPLETE_TYPE must not be void

				\pre The behavior is undefined if:
					- this transaction is empty 
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue typed_put_transaction element example 1 */
			#ifndef DOXYGEN_DOC_GENERATION
            template <typename EL = ELEMENT_COMPLETE_TYPE>
				typename std::enable_if<!std::is_void<EL>::value && std::is_same<EL, ELEMENT_COMPLETE_TYPE>::value, EL>::type &
			#else
				ELEMENT_COMPLETE_TYPE &
			#endif
					element() const noexcept
                { return *static_cast<ELEMENT_COMPLETE_TYPE *>(m_put_transaction.element_ptr()); }

            /** Returns the type of the object being added.

				\pre The behavior is undefined if either:
					- this transaction is empty 
		
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue put_transaction complete_type example 1 */
            const RUNTIME_TYPE & complete_type() const noexcept
            {
                return m_put_transaction.complete_type();
            }

            /** If this transaction is empty the destructor has no side effects. Otherwise it cancels it. 
			
				\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue put_transaction destroy example 1 */
			~put_transaction()
			{
				if (m_lock.owns_lock())
				{
					m_put_transaction.cancel();
				}
			}

            // internal only - can't be called from outside density
            put_transaction(PrivateType, std::unique_lock<std::mutex> && i_lock, typename InnerQueue::template put_transaction<ELEMENT_COMPLETE_TYPE> && i_put_transaction) noexcept
                : m_lock(std::move(i_lock)), m_put_transaction(std::move(i_put_transaction))
            {
				if (!m_put_transaction)
					m_lock.unlock();
			}

        private: // data members
			std::unique_lock<std::mutex> m_lock;
			typename InnerQueue::template put_transaction<ELEMENT_COMPLETE_TYPE> m_put_transaction;
			template <typename OTHERTYPE> friend class put_transaction;
        };

        /** Move-only class that can be bound to a consume operation, otherwise it's empty. 

			
		Consume functions on concurrent_heterogeneous_queue return a non-empty consume_operation that can be 
			used to inspect or alter the element while it is not observable in the queue, and 
			commit or cancel the consume. 
			
			A consume_operation is empty when:
				- it is default constructed
				- it is used as source for a move construction or move assignment
				- after a cancel or a commit
			
			Calling \ref consume_operation::commit "commit", \ref consume_operation::commit_nodestroy "commit_nodestroy",
			\ref consume_operation::cancel "cancel", \ref consume_operation::element_ptr "element_ptr",
			\ref consume_operation::unaligned_element_ptr "unaligned_element_ptr",
			\ref consume_operation::element "element" or \ref consume_operation::complete_type "complete_type" on
			an empty consume_operation triggers undefined behavior. */
        class consume_operation
        {
        public:

			/** Constructs an empty consume_operation 
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue consume_operation default_construct example 1 */
			consume_operation() noexcept = default;

            /** Copy construction is not allowed 
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue consume_operation copy_construct example 1 */
            consume_operation(const consume_operation &) = delete;

            /** Copy assignment is not allowed
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue consume_operation copy_assign example 1 */
            consume_operation & operator = (const consume_operation &) = delete;

            /** Move constructor. The source is left empty. 			
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue consume_operation move_construct example 1 */
			consume_operation(consume_operation && i_source) noexcept = default;

            /** Move assignment. The source is left empty. 
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue consume_operation move_assign example 1 */
			consume_operation & operator = (consume_operation && i_source) noexcept
			{
				m_lock = std::move(i_source.m_lock);
				m_consume_operation = std::move(i_source.m_consume_operation);
				return *this;
			}

            /** Destructor: cancel the operation (if any).
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue consume_operation destroy example 1 */
            ~consume_operation()
            {
				if (m_lock.owns_lock())
					cancel();
            }

			/** Swaps two instances of consume_operation.

				\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue consume_operation swap example 1 */
			friend void swap(consume_operation & i_first, consume_operation & i_second) noexcept
			{
				std::swap(i_first.m_lock, i_second.m_lock);
				std::swap(i_first.m_consume_operation, i_second.m_consume_operation);
			}

            /** Returns true whether this object does not hold the state of an operation.
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue consume_operation empty example 1 */
            bool empty() const noexcept { return m_consume_operation.empty(); }

            /** Returns true whether this object does not hold the state of an operation. Same to !consume_operation::empty. 
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue consume_operation operator_bool example 1 */
            explicit operator bool() const noexcept
            {
                return m_consume_operation.operator bool();
            }

            /** Destroys the element, making the consume irreversible. This comnsume_operation becomes empty.

				\pre The behavior is undefined if either:
					- this object is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. */
            void commit() noexcept
            {
                DENSITY_ASSERT(!empty());
				m_consume_operation.commit();
				m_lock.unlock();
            }

            /** Destroys the element, making the consume irreversible. This comnsume_operation becomes empty.
				The caller should destroy the element before calling this function.
				This object becomes empty.

				\pre The behavior is undefined if either:
					- this object is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. 

			Note: this function may be used to combine a feature of the runtime type on the element with the 
				destruction of the element. For example a function queue may improve the performances using a feature 
				like invoke_destroy to do both the function call and the destruction of the capture in a single call,
				making a single pseudo v-call instead of two. 
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue consume_operation commit_nodestroy example 1 */
            void commit_nodestroy() noexcept
            {
                DENSITY_ASSERT(!empty());
				m_consume_operation.commit_nodestroy();
				m_lock.unlock();
            }

			 /** Cancel the operation. This consume_operation becomes empty.

				\pre The behavior is undefined if either:
					- this object is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. 
				
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue consume_operation cancel example 1 */
            void cancel() noexcept
            {
                DENSITY_ASSERT(!empty());
				m_consume_operation.cancel();
				m_lock.unlock();
            }

            /** Returns the type of the element being consumed.

                \pre The behavior is undefined if this consume_operation is empty. 
				
				\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue consume_operation complete_type example 1 */
            const RUNTIME_TYPE & complete_type() const noexcept
            {
                return m_consume_operation.complete_type();
            }

            /** Returns a pointer that, if properly aligned to the alignment of the element type, points to the element.
				The returned address is guaranteed to be aligned to min_alignment

                \pre The behavior is undefined if this consume_operation is empty, that is it has been used as source for a move operation.
                \pos The returned address is aligned at least on concurrent_heterogeneous_queue::min_alignment. 
				
				\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue consume_operation unaligned_element_ptr example 1 */
            void * unaligned_element_ptr() const noexcept
            {
				return m_consume_operation.unaligned_element_ptr();
            }

            /** Returns a pointer to the element being consumed.
				
				This call is equivalent to: <code>address_upper_align(unaligned_element_ptr(), complete_type().alignment());</code>

                \pre The behavior is undefined if this consume_operation is empty, that is it has been committed or used as source for a move operation. 
				
				\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue consume_operation element_ptr example 1 */
            COMMON_TYPE * element_ptr() const noexcept
            {
                return m_consume_operation.element_ptr();
            }

			/** Returns a reference to the element being consumed.

                \pre The behavior is undefined if this consume_operation is empty, that is it has been committed or used as source for a move operation.
				\pre The behavior is undefined if COMPLETE_ELEMENT_TYPE is not exactly the complete type of the element.
				
				\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue consume_operation element example 1 */
            template <typename COMPLETE_ELEMENT_TYPE>
				COMPLETE_ELEMENT_TYPE & element() const noexcept
            {
                return m_consume_operation.template element<COMPLETE_ELEMENT_TYPE>();
            }

            // internal only - can't be called from outside density
            consume_operation(PrivateType, std::unique_lock<std::mutex> && i_lock, typename InnerQueue::consume_operation && i_consume_operation) noexcept
                : m_lock(std::move(i_lock)), m_consume_operation(std::move(i_consume_operation))
            {
            }

			// internal only - can't be called from outside density
			bool start_consume_impl(PrivateType, concurrent_heterogeneous_queue * i_queue)
			{
				if (m_lock.owns_lock())
					cancel();

				m_lock = std::unique_lock<std::mutex>(i_queue->m_mutex);
				
				bool const result = i_queue->m_queue.try_start_consume(m_consume_operation);
				if (!result)
					m_lock.unlock();
				return result;
			}

        private:
			std::unique_lock<std::mutex> m_lock;
			typename InnerQueue::consume_operation m_consume_operation;
        };

        /** Appends at the end of queue an element of type <code>ELEMENT_TYPE</code>, copy-constructing or move-constructing
                it from the source.

            @param i_source object to be used as source to construct of new element.
                - If this argument is an l-value, the new element copy-constructed
                - If this argument is an r-value, the new element move-constructed

            \n <b>Requires</b>:
                - <code>ELEMENT_TYPE</code> must be copy constructible (in case of l-value) or move constructible (in case of r-value)
                - <code>ELEMENT_TYPE *</code> must be implicitly convertible <code>COMMON_TYPE *</code>
                - <code>COMMON_TYPE *</code> must be convertible to <code>ELEMENT_TYPE *</code> with a static_cast or a dynamic_cast. \n This requirement is
                    not met for example if <code>COMMON_TYPE</code> is a non-polymorphic direct or indirect virtual base of <code>ELEMENT_TYPE</code>.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue push example 1 */
        template <typename ELEMENT_TYPE>
            void push(ELEMENT_TYPE && i_source)
        {
            return emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Appends at the end of queue an element of type <code>ELEMENT_TYPE</code>, inplace-constructing it from
                a perfect forwarded parameter pack.
            \n <i>Note</i>: the template argument ELEMENT_TYPE can't be deduced from the parameters so it must explicitly specified.

            @param i_construction_params construction parameters for the new element.

            \n <b>Requires</b>:
                - <code>ELEMENT_TYPE</code> must be constructible with <code>std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...</code>
                - <code>ELEMENT_TYPE *</code> must be implicitly convertible <code>COMMON_TYPE *</code>
                - <code>COMMON_TYPE *</code> must be convertible to <code>ELEMENT_TYPE *</code> with a static_cast or a dynamic_cast. \n This requirement is
                    not met for example if <code>COMMON_TYPE</code> is a non-polymorphic direct or indirect virtual base of <code>ELEMENT_TYPE</code>.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            void emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
            start_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...).commit();
        }

        /** Adds at the end of queue an element of a type known at runtime, default-constructing it.

            @param i_type type of the new element.

			\n <b>Requires</b>:
				- The function <code>RUNTIME_TYPE::default_construct</code> must be invokable. If 
				  <code>RUNTIMETYPE</code> is runtime_type this means that <code>default_construct</code> must be
				  included in the feature list.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue dyn_push example 1 */
        void dyn_push(const runtime_type & i_type)
        {
            start_dyn_push(i_type).commit();
        }

        /** Appends at the end of queue an element of a type known at runtime, copy-constructing it from the source.

            @param i_type type of the new element.
            @param i_source pointer to the subobject of type <code>COMMON_TYPE</code> of an object or subobject of type <code>ELEMENT_TYPE</code>.
                <i>Note</i>: be careful using void pointers: only the compiler knows how to casts from/to a base to/from a derived class.

			\n <b>Requires</b>:
				- The function <code>RUNTIME_TYPE::copy_construct</code> must be invokable. If 
				  <code>RUNTIMETYPE</code> is runtime_type this means that <code>copy_construct</code> must be
				  included in the feature list.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue dyn_push_copy example 1 */
        void dyn_push_copy(const runtime_type & i_type, const COMMON_TYPE * i_source)
        {
            start_dyn_push_copy(i_type, i_source).commit();
        }

        /** Adds at the end of queue an element of a type known at runtime, move-constructing it from the source.

            @param i_type type of the new element
            @param i_source pointer to the subobject of type <code>COMMON_TYPE</code> of an object or subobject of type <code>ELEMENT_TYPE</code>
                <i>Note</i>: be careful using void pointers: casts from\to a base to\from a derived class can be done only
                by the type system of the language.

			\n <b>Requires</b>:
				- The function <code>RUNTIME_TYPE::move_construct</code> must be invokable. If 
				  <code>RUNTIMETYPE</code> is runtime_type this means that <code>move_construct</code> must be
				  included in the feature list.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue dyn_push_move example 1 */
        void dyn_push_move(const runtime_type & i_type, COMMON_TYPE * i_source)
        {
            start_dyn_push_move(i_type, i_source).commit();
        }

        /** Begins a transaction that appends an element of type <code>ELEMENT_TYPE</code>, copy-constructing
                or move-constructing it from the source.
            \n This function allocates the required space, constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
            \n Call the member function commit on the returned transaction in order to make the effects observable.
			If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.
			Until the returned transaction is committed or canceled, the queue is not in a consistent state. If any
			function is called in this timespan, the behavior is undefined.

            @param i_source object to be used as source to construct of new element.
                - If this argument is an l-value, the new element copy-constructed.
                - If this argument is an r-value, the new element move-constructed.
            @return The associated transaction object.

            \n <b>Requires</b>:
                - <code>ELEMENT_TYPE</code> must be copy constructible (in case of l-value) or move constructible (in case of r-value)
                - <code>ELEMENT_TYPE *</code> must be implicitly convertible <code>COMMON_TYPE *</code>
                - <code>COMMON_TYPE *</code> must be convertible to <code>ELEMENT_TYPE *</code> with a static_cast or a dynamic_cast. \n This requirement is
                    not met for example if <code>COMMON_TYPE</code> is a non-polymorphic direct or indirect virtual base of <code>ELEMENT_TYPE</code>.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue start_push example 1 */
        template <typename ELEMENT_TYPE>
            put_transaction<typename std::decay<ELEMENT_TYPE>::type> start_push(ELEMENT_TYPE && i_source)
        {
            return start_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Begins a transaction that appends an element of a type <code>ELEMENT_TYPE</code>,
            inplace-constructing it from a perfect forwarded parameter pack.
            \n This function allocates the required space, constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
			\n Call the member function commit on the returned transaction in order to make the effects observable.
			If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.
			Until the returned transaction is committed or canceled, the queue is not in a consistent state. If any
			function is called in this timespan, the behavior is undefined.

            @param i_construction_params construction parameters for the new element.
            @return The associated transaction object.

            \n <b>Requires</b>:
                - <code>ELEMENT_TYPE</code> must be constructible with <code>std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...</code>
                - <code>ELEMENT_TYPE *</code> must be implicitly convertible <code>COMMON_TYPE *</code>
                - <code>COMMON_TYPE *</code> must be convertible to <code>ELEMENT_TYPE *</code> with a static_cast or a dynamic_cast. \n This requirement is
                    not met for example if <code>COMMON_TYPE</code> is a non-polymorphic direct or indirect virtual base of <code>ELEMENT_TYPE</code>.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue start_emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            put_transaction<typename std::decay<ELEMENT_TYPE>::type> start_emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
			std::unique_lock<std::mutex> lock(m_mutex);
			return put_transaction<typename std::decay<ELEMENT_TYPE>::type>( PrivateType(), 
				std::move(lock), m_queue.template start_emplace<ELEMENT_TYPE>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)... ));
        }

        /** Begins a transaction that appends an element of a type known at runtime, default-constructing it.
            \n This function allocates space for and constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
			\n Call the member function commit on the returned transaction in order to make the effects observable.
			If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.
			Until the returned transaction is committed or canceled, the queue is not in a consistent state. If any
			function is called in this timespan, the behavior is undefined.

            @param i_type type of the new element.
            @return The associated transaction object.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue start_dyn_push example 1 */
        put_transaction<> start_dyn_push(const runtime_type & i_type)
        {
			std::unique_lock<std::mutex> lock(m_mutex);
			return put_transaction<>(PrivateType(), std::move(lock), m_queue.start_dyn_push(i_type));
        }


        /** Begins a transaction that appends an element of a type known at runtime, copy-constructing it from the source..
            \n This function allocates space for and constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
			\n Call the member function commit on the returned transaction in order to make the effects observable.
			If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.
			Until the returned transaction is committed or canceled, the queue is not in a consistent state. If any
			function is called in this timespan, the behavior is undefined.

            @param i_type type of the new element.
            @param i_source pointer to the subobject of type COMMON_TYPE of an object or subobject of type ELEMENT_TYPE.
                <i>Note</i>: be careful using void pointers: casts from\to a base to\from a derived class can be done only
                by the type system of the language.
            @return The associated transaction object.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue start_dyn_push_copy example 1 */
        put_transaction<> start_dyn_push_copy(const runtime_type & i_type, const COMMON_TYPE * i_source)
        {
			std::unique_lock<std::mutex> lock(m_mutex);
			return put_transaction<>(PrivateType(), std::move(lock), m_queue.start_dyn_push_copy(i_type, i_source));
        }

        /** Begins a transaction that appends an element of a type known at runtime, move-constructing it from the source..
            \n This function allocates space for and constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
			\n Call the member function commit on the returned transaction in order to make the effects observable.
			If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.
			Until the returned transaction is committed or canceled, the queue is not in a consistent state. If any
			function is called in this timespan, the behavior is undefined.

            @param i_type type of the new element.
            @param i_source pointer to the subobject of type COMMON_TYPE of an object or subobject of type ELEMENT_TYPE.
                <i>Note</i>: be careful using void pointers: casts from\to a base to\from a derived class can be done only
                by the type system of the language.
            @return The associated transaction object.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue start_dyn_push_move example 1 */
        put_transaction<> start_dyn_push_move(const runtime_type & i_type, COMMON_TYPE * i_source)
        {
			std::unique_lock<std::mutex> lock(m_mutex);
			return put_transaction<>(PrivateType(), std::move(lock), m_queue.start_dyn_push_move(i_type, i_source));
        }
		

        /** Removes and destroy the first element of the queue. This function discards the element. Use a consume function
			if you want to access the element before it gets destroyed.

			This function is equivalent to:
			
			<code>try_start_consume().commit(); </code>

            \pre The behavior is undefined if the queue is empty.

            <b>Complexity</b>: constant
            \n <b>Effects on iterators</b>: any iterator pointing to the first element is invalidated
            \n <b>Throws</b>: nothing 
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue pop example 1 */
        void pop() noexcept
        {
			try_start_consume().commit();
        }

        /** Removes and destroy the first element of the queue, if the queue is not empty. Otherwise it has no effect.
			This function discards the element. Use a consume function if you want to access the element before it 
			gets destroyed.

            @return whether an element was actually removed.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: any iterator pointing to the first element is invalidated
            \n <b>Throws</b>: nothing
		\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue try_pop example 1 */
        bool try_pop() noexcept
        {
            if (auto operation = try_start_consume())
            {
                operation.commit();
                return true;
            }
            return false;
        }

		/** Tries to start a consume operation.
			@return a consume_operation which is empty if there are no elements to consume

			A non-empty consume must be committed for the consume to have effect.

			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue try_start_consume example 1 */
        consume_operation try_start_consume() noexcept
        {
			std::unique_lock<std::mutex> lock(m_mutex);
			auto consume = m_queue.try_start_consume();
			if (!consume)
				lock.unlock();
			return consume_operation(PrivateType(), std::move(lock), std::move(consume));
        }

		/** Tries to start a consume operation using an existing consume_operation.
			@param i_consume reference to a consume_operation to be used. If it is non-empty
				it gets canceled before trying to start the new consume.
			@return whether i_consume is non-empty after the call, that is whether the queue was
				not empty.

			A non-empty consume must be committed for the consume to have effect.

			This overload is similar to the one taking no arguments and returning a consume_operation.
			For an concurrent_heterogeneous_queue there is no performance difference between the two overloads. Anyway
			for lock-free concurrent queue this overload may be faster.

			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue try_start_consume_ example 1 */
		bool try_start_consume(consume_operation & i_consume) noexcept
        {
            return i_consume.start_consume_impl(PrivateType(), this);
        }


        /** Move-only class template that can be bound to a reentrant put transaction, otherwise it's empty.

			@tparam ELEMENT_COMPLETE_TYPE Complete type of elements that can be handled by a transaction, or void.
				ELEMENT_COMPLETE_TYPE must decay to itself (it can't be cv-qualified).
			
			Reentrant transactional put functions on concurrent_heterogeneous_queue return a non-empty reentrant_put_transaction that can be 
			used to allocate raw memory in the queue, inspect or alter the element while it is still not observable
			in the queue, and commit or cancel the push. 
			
			A reentrant_put_transaction is empty when:
				- it is default constructed
				- it is used as source for a move construction or move assignment
				- after a cancel or a commit
			
			Calling \ref reentrant_put_transaction::raw_allocate "raw_allocate", \ref reentrant_put_transaction::raw_allocate_copy "raw_allocate_copy",
			\ref reentrant_put_transaction::commit "commit", \ref reentrant_put_transaction::cancel "cancel", \ref reentrant_put_transaction::element_ptr "element_ptr",
			\ref reentrant_put_transaction::element "element" or \ref reentrant_put_transaction::complete_type "complete_type" on an empty reentrant_put_transaction
			triggers undefined behavior.

			A void reentrant_put_transaction can be move constructed/assigned from any reentrant_put_transaction. A typed reentrant_put_transaction
			can be move constructed/assigned only from a reentrant_put_transaction with the same ELEMENT_COMPLETE_TYPE. */
        template <typename ELEMENT_COMPLETE_TYPE = void>
			class reentrant_put_transaction
        {
			static_assert(std::is_same<ELEMENT_COMPLETE_TYPE, typename std::decay<ELEMENT_COMPLETE_TYPE>::type>::value, "");
        
		public:

			/** Constructs an empty put transaction */
			reentrant_put_transaction() noexcept = default;

            /** Copy construction is not allowed.
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_put_transaction copy_construct example 1 */
            reentrant_put_transaction(const reentrant_put_transaction &) = delete;

            /** Copy assignment is not allowed.
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_put_transaction copy_assign example 1 */
            reentrant_put_transaction & operator = (const reentrant_put_transaction &) = delete;

            /** Move constructs a reentrant_put_transaction, transferring the state from the source.
                    @param i_source source to move from. It becomes empty after the call. 

			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_put_transaction move_construct example 1
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_put_transaction move_construct example 2 */
            template <typename OTHERTYPE, typename = typename std::enable_if<
					std::is_same<OTHERTYPE, ELEMENT_COMPLETE_TYPE>::value || std::is_void<ELEMENT_COMPLETE_TYPE>::value >::type >
				reentrant_put_transaction(reentrant_put_transaction<OTHERTYPE> && i_source) noexcept
					: m_queue(i_source.m_queue), m_put_transaction(std::move(i_source.m_put_transaction))
            {
                i_source.m_queue = nullptr;
            }

            /** Move assigns a reentrant_put_transaction, transferring the state from the source.
                @param i_source source to move from. It becomes empty after the call. 
				
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_put_transaction move_assign example 1
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_put_transaction move_assign example 2 */
			template <typename OTHERTYPE, typename = typename std::enable_if<
					std::is_same<OTHERTYPE, ELEMENT_COMPLETE_TYPE>::value || std::is_void<ELEMENT_COMPLETE_TYPE>::value >::type >
				reentrant_put_transaction & operator = (reentrant_put_transaction<OTHERTYPE> && i_source) noexcept
            {
				m_queue = i_source.m_queue;
				m_put_transaction = std::move(i_source.m_put_transaction);
				i_source.m_queue = nullptr;
				return *this;
            }

			/** Swaps two instances of reentrant_put_transaction.

				\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_put_transaction swap example 1 */
			friend void swap(reentrant_put_transaction & i_first, reentrant_put_transaction & i_second) noexcept
			{
				std::swap(i_first.m_queue, i_second.m_queue);
				std::swap(i_first.m_put_transaction, i_second.m_put_transaction);
			}

            /** Allocates a memory block associated to the element being added in the queue. The block may be allocated contiguously with
                the elements in the memory pages. If the block does not fit in one page, the block is allocated using non-paged memory services
                of the allocator.

                \n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial
                    content of the block is undefined.

                @param i_size size in bytes of the block to allocate.
                @param i_alignment alignment of the block to allocate. It must be a non-zero power of 2, and less than or
						equal to i_size.

                \pre The behavior is undefined if either:
					- this transaction is empty
					- the alignment is not valid
					- the size is not a multiple of the alignment

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: unspecified.
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). 
				
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_put_transaction raw_allocate example 1*/
            void * raw_allocate(size_t i_size, size_t i_alignment)
            {
				DENSITY_ASSERT(!empty());
				std::lock_guard<std::mutex> lock(m_queue->m_mutex);
				return m_put_transaction.raw_allocate(i_size, i_alignment);
            }

            /** Allocates a memory block associated to the element being added in the queue, and copies the content from a range 
				of iterators. The block may be allocated contiguously with the elements in the memory pages. If the block does not
				fit in one page, the block is allocated using non-paged memory services of the allocator.
				
                \n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial
                    content of the block is undefined.

                @param i_begin first element to be copied
                @param i_end first element not to be copied

				\n <b>Requires</b>:
					- INPUT_ITERATOR must meet the requirements of <a href="http://en.cppreference.com/w/cpp/concept/InputIterator">InputIterator</a>
					- the value type must be trivially destructible

                \pre The behavior is undefined if either:
					- this transaction is empty
					- i_end is not reachable from i_begin

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: unspecified.
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects)
				
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_put_transaction raw_allocate_copy example 1*/
            template <typename INPUT_ITERATOR>
                typename std::iterator_traits<INPUT_ITERATOR>::value_type *
                    raw_allocate_copy(INPUT_ITERATOR i_begin, INPUT_ITERATOR i_end)
            {
				DENSITY_ASSERT(!empty());
				std::lock_guard<std::mutex> lock(m_queue->m_mutex);
				return m_put_transaction.raw_allocate_copy(i_begin, i_end);
            }

			/** Allocates a memory block associated to the element being added in the queue, and copies the content from a range.
				The block may be allocated contiguously with the elements in the memory pages. If the block does not
				fit in one page, the block is allocated using non-paged memory services of the allocator.
				
                \n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial
                    content of the block is undefined.

                @param i_source_range to be iterated

				\n <b>Requires</b>:
					- The iterators of the range must meet the requirements of <a href="http://en.cppreference.com/w/cpp/concept/InputIterator">InputIterator</a>
					- the value type must be trivially destructible

                \pre The behavior is undefined if either:
					- this transaction is empty

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: unspecified.
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).
				
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_put_transaction raw_allocate_copy example 2 */
            template <typename INPUT_RANGE>
                auto raw_allocate_copy(const INPUT_RANGE & i_source_range)
                    -> decltype(raw_allocate_copy(std::begin(i_source_range), std::end(i_source_range)))
            {
				DENSITY_ASSERT(!empty());
				std::lock_guard<std::mutex> lock(m_queue->m_mutex);
				return m_put_transaction.raw_allocate_copy(std::begin(i_source_range), std::end(i_source_range));
            }

            /** Makes the effects of the transaction observable. This object becomes empty.

				\pre The behavior is undefined if either:
					- this transaction is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. */
            void commit() noexcept
            {
                DENSITY_ASSERT(!empty());
				std::lock_guard<std::mutex> lock(m_queue->m_mutex);
				m_put_transaction.commit();
				m_queue = nullptr;
            }


			/** Cancel the transaction. This object becomes empty.

				\pre The behavior is undefined if either:
					- this transaction is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. 
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_put_transaction cancel example 1 */
            void cancel() noexcept
            {
                DENSITY_ASSERT(!empty());
				std::lock_guard<std::mutex> lock(m_queue->m_mutex);
				m_put_transaction.cancel();
				m_queue = nullptr;
            }

            /** Returns true whether this object does not hold the state of a transaction. 
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_put_transaction empty example 1 */
            bool empty() const noexcept { return m_queue == nullptr; }

			/** Returns true whether this object is bound to a transaction. Same to !consume_operation::empty.
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_put_transaction operator_bool example 1 */
            explicit operator bool() const noexcept
            {
                return m_queue != nullptr;
            }

			/** Returns a pointer to the target queue if a transaction is bound, otherwise returns nullptr

				\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_put_transaction queue example 1 */
			concurrent_heterogeneous_queue * queue() const noexcept { return m_queue; }

            /** Returns a pointer to the object being added.
                \n <i>Notes</i>: 
				- The object is constructed when the transaction is started, so this function always returns a 
					pointer to a valid object.
				- This function returns a pointer to the common_type subobject of the element. Non-dynamic 
					transactional put function (start_push, start_emplace, start_reentrant_push, start_reentrant_emplace)
					return a typed_put_transaction or a reentrant_put_transaction, that provide the function element(),
					which is a better alternative to this function

				\pre The behavior is undefined if either:
					- this transaction is empty 
					
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_put_transaction element_ptr example 1
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_put_transaction element_ptr example 2 */
            common_type * element_ptr() const noexcept
            {
                return m_put_transaction.element_ptr();
            }

			/** Returns a reference to the element being added. This function can be used to modify the element 
					before calling the commit.
                \n <i>Note</i>: An element is observable in the queue only after commit has been called on the
					reentrant_put_transaction. The element is constructed at the begin of the transaction, so the
					returned object is always valid.

				\n <b>Requires</b>:
					- ELEMENT_COMPLETE_TYPE must not be void

				\pre The behavior is undefined if:
					- this transaction is empty 
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue typed_put_transaction element example 1 */
			#ifndef DOXYGEN_DOC_GENERATION
            template <typename EL = ELEMENT_COMPLETE_TYPE>
				typename std::enable_if<!std::is_void<EL>::value && std::is_same<EL, ELEMENT_COMPLETE_TYPE>::value, EL>::type &
			#else
				ELEMENT_COMPLETE_TYPE &
			#endif
					element() const noexcept
                { return *static_cast<ELEMENT_COMPLETE_TYPE *>(element_ptr()); }

            /** Returns the type of the object being added.

				\pre The behavior is undefined if either:
					- this transaction is empty 
		
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_put_transaction complete_type example 1 */
            const RUNTIME_TYPE & complete_type() const noexcept
            {
				return m_put_transaction.complete_type();
            }

            /** If this transaction is empty the destructor has no side effects. Otherwise it cancels it. 
			
				\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_put_transaction destroy example 1 */
            ~reentrant_put_transaction()
            {
                if (m_put_transaction)
                {
					cancel();
                }
            }

            // internal only - can't be called from outside density
            reentrant_put_transaction(PrivateType, concurrent_heterogeneous_queue * i_queue,
				typename InnerQueue::template reentrant_put_transaction<ELEMENT_COMPLETE_TYPE> && i_put_transaction) noexcept
                : m_queue(i_queue), m_put_transaction(std::move(i_put_transaction))
            {
            }

        private:
			concurrent_heterogeneous_queue * m_queue = nullptr;
			typename InnerQueue::template reentrant_put_transaction<ELEMENT_COMPLETE_TYPE> m_put_transaction;
			template <typename OTHERTYPE> friend class reentrant_put_transaction;
        };


        /** Move-only class that can be bound to a reentrant consume operation, otherwise it's empty. 

		Reentrant consume functions on concurrent_heterogeneous_queue return a non-empty reentrant_consume_operation that can be 
			used to inspect or alter the element while it is not observable in the queue, and 
			commit or cancel the consume. 
			
			A reentrant_consume_operation is empty when:
				- it is default constructed
				- it is used as source for a move construction or move assignment
				- after a cancel or a commit
			
			Calling \ref reentrant_consume_operation::commit "commit", \ref reentrant_consume_operation::commit_nodestroy "commit_nodestroy",
			\ref reentrant_consume_operation::cancel "cancel", \ref reentrant_consume_operation::element_ptr "element_ptr",
			\ref reentrant_consume_operation::unaligned_element_ptr "unaligned_element_ptr",
			\ref reentrant_consume_operation::element "element" or \ref reentrant_consume_operation::complete_type "complete_type" on
			an empty reentrant_consume_operation triggers undefined behavior. */
        class reentrant_consume_operation
        {
        public:

			/** Constructs an empty reentrant_consume_operation 
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_consume_operation default_construct example 1 */
			reentrant_consume_operation() noexcept = default;

            /** Copy construction is not allowed 
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_consume_operation copy_construct example 1 */
            reentrant_consume_operation(const reentrant_consume_operation &) = delete;

            /** Copy assignment is not allowed
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_consume_operation copy_assign example 1 */
            reentrant_consume_operation & operator = (const reentrant_consume_operation &) = delete;

            /** Move constructor. The source is left empty. 			
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_consume_operation move_construct example 1 */
			reentrant_consume_operation(reentrant_consume_operation && i_source) noexcept = default;

            /** Move assignment. The source is left empty. 
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_consume_operation move_assign example 1 */
			reentrant_consume_operation & operator = (reentrant_consume_operation && i_source) noexcept = default;

            /** Destructor: cancel the operation (if any).
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_consume_operation destroy example 1 */
            ~reentrant_consume_operation()
            {
                if(!empty())
                {
					cancel();
                }
            }

			/** Swaps two instances of reentrant_consume_operation.

				\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_consume_operation swap example 1 */
			friend void swap(reentrant_consume_operation & i_first, reentrant_consume_operation & i_second) noexcept
			{
				std::swap(i_first.m_queue, i_second.m_queue);
				std::swap(i_first.m_consume_operation, i_second.m_consume_operation);
			}

            /** Returns true whether this object does not hold the state of an operation.
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_consume_operation empty example 1 */
            bool empty() const noexcept { return m_consume_operation.empty(); }

            /** Returns true whether this object does not hold the state of an operation. Same to !reentrant_consume_operation::empty. 
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_consume_operation operator_bool example 1 */
            explicit operator bool() const noexcept
            {
                return m_consume_operation.operator bool();
            }

			/** Returns a pointer to the target queue if a transaction is bound, otherwise returns nullptr

				\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_consume_operation queue example 1 */
			concurrent_heterogeneous_queue * queue() const noexcept { return m_queue; }

            /** Destroys the element, making the consume irreversible. This comnsume_operation becomes empty.

				\pre The behavior is undefined if either:
					- this object is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. */
            void commit() noexcept
            {
				DENSITY_ASSERT(!empty());
				std::lock_guard<std::mutex> lock(m_queue->m_mutex);
                m_consume_operation.commit();
            }

            /** Destroys the element, making the consume irreversible. This comnsume_operation becomes empty.
				The caller should destroy the element before calling this function.
				This object becomes empty.

				\pre The behavior is undefined if either:
					- this object is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. 

			Note: this function may be used to combine a feature of the runtime type on the element with the 
				destruction of the element. For example a function queue may improve the performances using a feature 
				like invoke_destroy to do both the function call and the destruction of the capture in a single call,
				making a single pseudo v-call instead of two. 
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_consume_operation commit_nodestroy example 1 */
            void commit_nodestroy() noexcept
            {
				DENSITY_ASSERT(!empty());
				std::lock_guard<std::mutex> lock(m_queue->m_mutex);
                m_consume_operation.commit_nodestroy();
            }

			 /** Cancel the operation. This reentrant_consume_operation becomes empty.

				\pre The behavior is undefined if either:
					- this object is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. 
				
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_consume_operation cancel example 1 */
            void cancel() noexcept
            {
				DENSITY_ASSERT(!empty());
				std::lock_guard<std::mutex> lock(m_queue->m_mutex);
                m_consume_operation.cancel();
            }

            /** Returns the type of the element being consumed.

                \pre The behavior is undefined if this reentrant_consume_operation is empty. 
				
				\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_consume_operation complete_type example 1 */
            const RUNTIME_TYPE & complete_type() const noexcept
            {
                return m_consume_operation.complete_type();
            }

            /** Returns a pointer that, if properly aligned to the alignment of the element type, points to the element.
				The returned address is guaranteed to be aligned to min_alignment

                \pre The behavior is undefined if this reentrant_consume_operation is empty, that is it has been used as source for a move operation.
                \pos The returned address is aligned at least on concurrent_heterogeneous_queue::min_alignment. 
				
				\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_consume_operation unaligned_element_ptr example 1 */
            void * unaligned_element_ptr() const noexcept
            {
                return m_consume_operation.unaligned_element_ptr();
            }

            /** Returns a pointer to the element being consumed.
				
				This call is equivalent to: <code>address_upper_align(unaligned_element_ptr(), complete_type().alignment());</code>

                \pre The behavior is undefined if this reentrant_consume_operation is empty, that is it has been committed or used as source for a move operation. 
				
				\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_consume_operation element_ptr example 1 */
            COMMON_TYPE * element_ptr() const noexcept
            {
				return m_consume_operation.element_ptr();
            }

			/** Returns a reference to the element being consumed.

                \pre The behavior is undefined if this reentrant_consume_operation is empty, that is it has been committed or used as source for a move operation.
				\pre The behavior is undefined if COMPLETE_ELEMENT_TYPE is not exactly the complete type of the element.
				
				\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_consume_operation element example 1 */
            template <typename COMPLETE_ELEMENT_TYPE>
				COMPLETE_ELEMENT_TYPE & element() const noexcept
            {
                return m_consume_operation.template element<COMPLETE_ELEMENT_TYPE>();
            }

            // internal only - can't be called from outside density
            reentrant_consume_operation(PrivateType, concurrent_heterogeneous_queue * i_queue, 
					typename InnerQueue::reentrant_consume_operation && i_consume_operation) noexcept
                : m_queue(i_queue), m_consume_operation(std::move(i_consume_operation))
            {
            }

			// internal only - can't be called from outside density
			bool start_consume_impl(PrivateType, concurrent_heterogeneous_queue * i_queue)
			{
				// first we take the locks, because they may throw
				std::unique_lock<std::mutex> lock;
				if (m_queue != nullptr)
				{
					lock = std::unique_lock<std::mutex>(m_queue->m_mutex);
				}
				std::unique_lock<std::mutex> new_lock;
				if (m_queue != i_queue && i_queue != nullptr)
				{
					new_lock = std::unique_lock<std::mutex>(i_queue->m_mutex);
				}

				// nothing can throw from now on
				if (m_consume_operation)
					m_consume_operation.cancel();
				lock = std::move(new_lock);
				m_queue = i_queue;
				return m_queue->m_queue.try_start_reentrant_consume(m_consume_operation);
			}

        private:
            concurrent_heterogeneous_queue * m_queue = nullptr;
			typename InnerQueue::reentrant_consume_operation m_consume_operation;
        };

        /** Same to concurrent_heterogeneous_queue::push, but allow reentrancy: during the construction of the element the queue is in a
            valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_push example 1 */
        template <typename ELEMENT_TYPE>
            void reentrant_push(ELEMENT_TYPE && i_source)
        {
            return reentrant_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Same to concurrent_heterogeneous_queue::emplace, but allow reentrancy: during the construction of the element the queue is in a
            valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            void reentrant_emplace(CONSTRUCTION_PARAMS &&... i_construction_params)
        {
            start_reentrant_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...).commit();
        }

        /** Same to concurrent_heterogeneous_queue::dyn_push, but allow reentrancy: during the construction of the element the queue is in a
            valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_dyn_push example 1 */
        void reentrant_dyn_push(const runtime_type & i_type)
        {
            start_reentrant_dyn_push(i_type).commit();
        }

        /** Same to concurrent_heterogeneous_queue::dyn_push_copy, but allow reentrancy: during the construction of the element the queue is in a
            valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_dyn_push_copy example 1 */
        void reentrant_dyn_push_copy(const runtime_type & i_type, const COMMON_TYPE * i_source)
        {
            start_reentrant_dyn_push_copy(i_type, i_source).commit();
        }

        /** Same to concurrent_heterogeneous_queue::dyn_push_move, but allow reentrancy: during the construction of the element the queue is in a
            valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_dyn_push_move example 1 */
        void reentrant_dyn_push_move(const runtime_type & i_type, COMMON_TYPE * i_source)
        {
            start_reentrant_dyn_push_move(i_type, i_source).commit();
        }

        /** Same to concurrent_heterogeneous_queue::start_push, but allow reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue start_reentrant_push example 1 */
        template <typename ELEMENT_TYPE>
            reentrant_put_transaction<typename std::decay<ELEMENT_TYPE>::type> start_reentrant_push(ELEMENT_TYPE && i_source)
        {
            return start_reentrant_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Same to concurrent_heterogeneous_queue::start_emplace, but allow reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue start_reentrant_emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            reentrant_put_transaction<typename std::decay<ELEMENT_TYPE>::type> start_reentrant_emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
			std::lock_guard<std::mutex> lock(m_mutex);
			auto put_transaction = m_queue.template start_reentrant_emplace<ELEMENT_TYPE>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
			return reentrant_put_transaction<typename std::decay<ELEMENT_TYPE>::type>(PrivateType(), 
				this, std::move(put_transaction) );
        }

        /** Same to concurrent_heterogeneous_queue::start_dyn_push, but allow reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue start_reentrant_dyn_push example 1 */
        reentrant_put_transaction<> start_reentrant_dyn_push(const runtime_type & i_type)
        {
			std::lock_guard<std::mutex> lock(m_mutex);
			auto put_transaction = m_queue.start_reentrant_dyn_push(i_type);
			return reentrant_put_transaction<>(PrivateType(), 
				this, std::move(put_transaction) );
        }


        /** Same to concurrent_heterogeneous_queue::start_dyn_push_copy, but allow reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue start_reentrant_dyn_push_copy example 1 */
        reentrant_put_transaction<> start_reentrant_dyn_push_copy(const runtime_type & i_type, const COMMON_TYPE * i_source)
        {
			std::lock_guard<std::mutex> lock(m_mutex);
			auto put_transaction = m_queue.start_reentrant_dyn_push_copy(i_type, i_source);
			return reentrant_put_transaction<>(PrivateType(), 
				this, std::move(put_transaction) );
        }

        /** Same to concurrent_heterogeneous_queue::start_dyn_push_move, but allow reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue start_reentrant_dyn_push_move example 1 */
        reentrant_put_transaction<> start_reentrant_dyn_push_move(const runtime_type & i_type, COMMON_TYPE * i_source)
        {
			std::lock_guard<std::mutex> lock(m_mutex);
			auto put_transaction = m_queue.start_reentrant_dyn_push_move(i_type, i_source);
			return reentrant_put_transaction<>(PrivateType(), 
				this, std::move(put_transaction) );
        }
		
        /** Removes and destroy the first element of the queue. This is the reentrant version of pop.
			This function discards the element. Use a consume function if you want to access the 
			element before it gets destroyed.

			This function is equivalent to:
			
			<code>try_start_reentrant_consume().commit(); </code>

            \pre The behavior is undefined if the queue is empty.

            <b>Complexity</b>: constant
            \n <b>Effects on iterators</b>: any iterator pointing to the first element is invalidated
            \n <b>Throws</b>: nothing 
			
			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue reentrant_pop example 1 */
		void reentrant_pop() noexcept
        {
			try_start_reentrant_consume().commit();
        }

        /** Removes and destroy the first element of the queue, if the queue is not empty. Otherwise it has no effect.
			This is the reentrant version of try_pop.
			This function discards the element. Use a consume function if you want to access the element before it 
			gets destroyed.

            @return whether an element was actually removed.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: any iterator pointing to the first element is invalidated
            \n <b>Throws</b>: nothing
		\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue try_reentrant_pop example 1 */
        bool try_reentrant_pop() noexcept
        {
            if (auto operation = try_start_reentrant_consume())
            {
                operation.commit();
                return true;
            }
            return false;
        }


		/** Tries to start a reentrant consume operation.
			This is the reentrant version of try_start_consume.

			@return a consume_operation which is empty if there are no elements to consume

			A non-empty consume must be committed for the consume to have effect.

			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue try_start_reentrant_consume example 1 */
        reentrant_consume_operation try_start_reentrant_consume() noexcept
        {
			std::lock_guard<std::mutex> lock(m_mutex);
			return reentrant_consume_operation(PrivateType(), this, m_queue.try_start_reentrant_consume());
        }
		
		/** Tries to start a consume operation using an existing consume_operation.
			This is the reentrant version of try_start_consume.

			@param i_consume reference to a consume_operation to be used. If it is non-empty
				it gets canceled before trying to start the new consume.
			@return whether i_consume is non-empty after the call, that is whether the queue was
				not empty.

			A non-empty consume must be committed for the consume to have effect.

			This overload is similar to the one taking no arguments and returning a consume_operation.
			For an concurrent_heterogeneous_queue there is no performance difference between the two overloads. Anyway
			for lock-free concurrent queue this overload may be faster.

			\snippet concurrent_heterogeneous_queue_examples.cpp concurrent_heterogeneous_queue try_start_reentrant_consume_ example 1 */
		bool try_start_reentrant_consume(reentrant_consume_operation & i_consume) noexcept
        {
            return i_consume.start_consume_impl(PrivateType(), this);
        }

	private:
		mutable std::mutex m_mutex;
		InnerQueue m_queue;
    };


} // namespace density
