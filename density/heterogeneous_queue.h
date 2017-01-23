
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <density/runtime_type.h>
#include <density/void_allocator.h>

namespace density
{
	namespace detail
	{
		template<typename COMMON_TYPE> struct QueueControl
		{
			uintptr_t m_next;
			COMMON_TYPE * m_element;
		};

		template<> struct QueueControl<void>
		{
			uintptr_t m_next;
		};
	}

	/** \brief Class template that implements an heterogeneous FIFO container with dynamic size.
		@tparam COMMON_TYPE Common type. An element of type E can be pushed on the queue only if E* is implicitly convertible 
			to COMMON_TYPE*. COMMON_TYPE cannot be cv-qualified, a reference or an array. In general std::decay (http://en.cppreference.com/w/cpp/types/decay) 
			must transform COMMON_TYPE into itself, or a compile time error occurs.
			If COMMON_TYPE is void, elements of any complete type can be added to the container. In this
			case, the methods of heterogeneous_queue that return a pointer to an element
			will return a void* to a complete object, while the methods that returns a reference to
			an element will return void. Use iterators and pointer semantic to write generic code
			that works with any heterogeneous_queue.
		@tparam VOID_ALLOCATOR Allocator to be used to allocate the memory buffer. This type must 
			meet the requirements of PageAllocator.
		@tparam RUNTIME_TYPE Type to be used to represent the actual complete type of each element.
			This type must meet the requirements of RuntimeType. 

		\n <b>Thread safeness</b>: None. The user is responsible to avoid race conditions.
        \n <b>Exception safeness</b>: Any function of heterogeneous_queue is noexcept or provides the strong exception guarantee.

		Text.
		Text.
		Text.


	*/
	template < typename COMMON_TYPE = void, typename RUNTIME_TYPE = runtime_type<COMMON_TYPE>, typename ALLOCATOR_TYPE = void_allocator >
		class heterogeneous_queue final : private ALLOCATOR_TYPE
	{
	private:

		using ControlBlock = typename detail::QueueControl<COMMON_TYPE>;

		struct PutData
		{
			ControlBlock * m_control_block;
			void * m_element;
		};
		
	public:

		static_assert(std::is_same<COMMON_TYPE, typename RUNTIME_TYPE::common_type>::value,
			"COMMON_TYPE and RUNTIME_TYPE::common_type must be the same type (did you try to use a type like heter_cont<A,runtime_type<B>>?)");

		static_assert(std::is_same<COMMON_TYPE, typename std::decay<COMMON_TYPE>::type>::value,
			"COMMON_TYPE can't be cv-qualified, an array or a reference");

		static_assert(ALLOCATOR_TYPE::page_size > sizeof(void*) * 8 && ALLOCATOR_TYPE::page_alignment == ALLOCATOR_TYPE::page_size,
			"The size and alignment of the pages must be the same (and not too small)");

		using allocator_type = ALLOCATOR_TYPE;
		using runtime_type = RUNTIME_TYPE;
		using common_type = COMMON_TYPE;
		using reference = typename std::add_lvalue_reference< COMMON_TYPE >::type;
		using const_reference = typename std::add_lvalue_reference< const COMMON_TYPE>::type;
		class put_transaction;
		class iterator;
		class const_iterator;

        /** Default constructor. The allocator is default-constructed.
			
			<b>Complexity</b>: constant.
            \n <b>Throws</b>: unspecified. 
			\n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects).
			\n Implementation note: Currently this constructor does not allocate memory and never throws. */
		heterogeneous_queue()
			: m_head(reinterpret_cast<ControlBlock*>(s_invalid_control_block)), m_tail(reinterpret_cast<ControlBlock*>(s_invalid_control_block))
		{
		}

        /** Move constructor. The allocator is move-constructed from the one of the source.
				@param i_source source to move the elements from. After the call the source is left in some valid but indeterminate state.

			<b>Complexity</b>: constant.
            \n <b>Throws</b>: Nothing. 			
			\n <i>Implementation notes</i>:
				- After the call the source is left empty. */
		heterogeneous_queue(heterogeneous_queue && i_source) noexcept
			: heterogeneous_queue()
		{
			swap(i_source);
		}

		/** Copy constructor. The allocator is copy-constructed from the one of the source.
			
			\n <b>Requires</b>:
                - the runtime type must support the feature type_features::copy_construct

			<b>Complexity</b>: linear in the number of elements of the source.
			\n <b>Throws</b>: unspecified. 
			\n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects). */
		heterogeneous_queue(const heterogeneous_queue & i_source)
			: allocator_type(static_cast<const allocator_type&>(i_source)),
			  m_head(reinterpret_cast<ControlBlock*>(s_invalid_control_block)), m_tail(reinterpret_cast<ControlBlock*>(s_invalid_control_block))
		{
			for (auto source_it = i_source.cbegin(); source_it != i_source.cend(); source_it++)
			{
				push_by_copy(source_it.complete_type(), source_it.element());
			}
		}

		/** Move assignment. The allocator is move-assigned from the one of the source.
				@param i_source source to move the elements from. After the call the source is left in some valid but indeterminate state.
			
			<b>Complexity</b>: Unspecified.
			\n <b>Effects on iterators</b>: Any iterator pointing to this queue or the source queue is invalidated.
			\n <b>Throws</b>: Nothing. 

			\n <i>Implementation notes</i>:
				- After the call the source is left empty.
				- The complexity is linear in the number of elements in this queue. */
		heterogeneous_queue & operator = (heterogeneous_queue && i_source) noexcept
		{
			swap(i_source);
			i_source.destroy();
			i_source.m_tail = i_source.m_head = reinterpret_cast<ControlBlock*>(s_invalid_control_block);
			return *this;
		}

		/** Copy assignment. The allocator is move-assigned from the one of the source.
				@param i_source source to move the elements from. After the call the source is left in some valid but indeterminate state.
			
			<b>Complexity</b>: linear.
			\n <b>Effects on iterators</b>: Any iterator pointing to this queue is invalidated.			
			\n <b>Throws</b>: unspecified. 
			\n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects). */
		heterogeneous_queue & operator = (const heterogeneous_queue & i_source)
		{
			auto copy(i_source);
			swap(copy);
			return *this;
		}

		/** Swaps the content of this queue and another one. The allocator is swapped too.
				@param i_source source to move the elements from. After the call the source is left in some valid but indeterminate state.
			
			<b>Complexity</b>: constant.
			\n <b>Effects on iterators</b>: Any iterator pointing to this queue or the other queue is invalidated.			
			\n <b>Throws</b>: Nothing. */
		void swap(heterogeneous_queue & i_other) noexcept
		{
			using std::swap;
			swap(static_cast<ALLOCATOR_TYPE&>(*this), static_cast<ALLOCATOR_TYPE&>(i_other));
			swap(m_head, i_other.m_head);
			swap(m_tail, i_other.m_tail);
		}

		/** Destructor.
					
			<b>Complexity</b>: linear.
			\n <b>Effects on iterators</b>: Any iterator pointing to this queue is invalidated.
			\n <b>Throws</b>: Nothing. */
		~heterogeneous_queue()
		{
			destroy();
		}

        /** Returns whether the queue contains no elements.
			
			<b>Complexity</b>: Unspecified.
			\n <b>Throws</b>: Nothing. */
		bool empty() const noexcept
		{
			for (auto curr = m_head; curr != m_tail; )
			{
				auto const control_bits = curr->m_next & 3;
				if (control_bits == 0)
				{
					return false;
				}
				curr = reinterpret_cast<ControlBlock*>(curr->m_next - control_bits);
			}
			return true;
		}

		/** Deletes all the elements in the queue.
			
			<b>Complexity</b>: linear.
			\n <b>Effects on iterators</b>: any iterator is invalidated
			\n <b>Throws</b>: Nothing. */
		void clear() noexcept
		{
			for(;;)
			{
				auto transaction = begin_manual_consume();
				if (!transaction)
				{
					break;
				}
				auto const element = transaction.element();
				auto const & type = transaction.complete_type();
				type.destroy(element);
			}

			DENSITY_ASSERT_INTERNAL(empty());
		}

		/** Adds at the end of queue an element of a type known at compile time (ELEMENT_TYPE), copy-constructing or move-constructing
				it from the source.
		
			@param i_source object to be used as source to construct of new element.
				- If this argument is an l-value, the new element copy-constructed (and the source object is left unchanged).
				- If this argument is an r-value, the new element move-constructed (and the source object is left in an undefined but valid state).
				
            \n <b>Requires</b>:
                - ELEMENT_TYPE must be copy constructible (in case of l-value) or move constructible (in case of r-value)
                - ELEMENT_TYPE * must be implicitly convertible COMMON_TYPE *
                - COMMON_TYPE * must be convertible to ELEMENT_TYPE * with a static_cast or a dynamic_cast. \n This requirement is 
					not met for example if COMMON_TYPE is a non-polymorphic direct or indirect virtual base of ELEMENT_TYPE.

			<b>Complexity</b>: constant.
			\n <b>Effects on iterators</b>: no iterator is invalidated
			\n <b>Throws</b>: unspecified. 
			\n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects). 
			
			<b>Examples</b>
			\snippet heter_queue_samples.cpp heter_queue push example 1 */
		template <typename ELEMENT_TYPE>
			void push(ELEMENT_TYPE && i_source)
		{
			return emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
		}

		/** Adds at the end of queue an element of a type known at compile time (ELEMENT_TYPE), inplace-constructring it from
				a perfect forwarded a parameter pack.
			\n <i>Note</i>: the template argument ELEMENT_TYPE can't be deduced from the parameters so it must explicitly specified.
		
			@param i_construction_params construction parameters for the new element.
				
            \n <b>Requires</b>:
                - ELEMENT_TYPE must be constructible from the specified parameters
                - ELEMENT_TYPE * must be implicitly convertible COMMON_TYPE *
                - COMMON_TYPE * must be convertible to ELEMENT_TYPE * with a static_cast or a dynamic_cast. \n This requirement is 
					not met for example if COMMON_TYPE is a non-polymorphic direct or indirect virtual base of ELEMENT_TYPE.

			<b>Complexity</b>: constant.
			\n <b>Effects on iterators</b>: no iterator is invalidated
			\n <b>Throws</b>: unspecified. 
			\n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects). 
			
			<b>Examples</b>
			\snippet heter_queue_samples.cpp heter_queue emplace example 1 */
		template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
			void emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
		{
			begin_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...).commit();
		}

		/** Adds at the end of queue an element of a type known at runtime, copy-constructing it from the source.
		
			@param i_type type of the new element.
			@param i_source pointer to the subobject of type COMMON_TYPE of an object or subobject of type ELEMENT_TYPE.
				<i>Note</i>: be careful using void pointers: casts from\to a base to\from a derived class can be done only by
				by the type system of the language.
				
			<b>Complexity</b>: constant.
			\n <b>Effects on iterators</b>: no iterator is invalidated
			\n <b>Throws</b>: unspecified. 
			\n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects). 
			
			<b>Examples</b>
			\snippet heter_queue_samples.cpp heter_queue push_by_copy example 1 */
		void push_by_copy(const runtime_type & i_type, const COMMON_TYPE * i_source)
		{
			begin_push_by_copy(i_type, i_source).commit();
		}

		/** Adds at the end of queue an element of a type known at runtime, move-constructing it from the source.
		
			@param i_type type of the new element
			@param i_source pointer to the subobject of type COMMON_TYPE of an object or subobject of type ELEMENT_TYPE
				<i>Note</i>: be careful using void pointers: casts from\to a base to\from a derived class can be done only by
				by the type system of the language.

			<b>Complexity</b>: constant.
			\n <b>Effects on iterators</b>: no iterator is invalidated
			\n <b>Throws</b>: unspecified. 
			\n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects). 
			
			<b>Examples</b>
			\snippet heter_queue_samples.cpp heter_queue push_by_move example 1 */
		void push_by_move(const runtime_type & i_type, COMMON_TYPE * i_source)
		{
			begin_push_by_move(i_type, i_source).commit();
		}

		/** Begins a transaction to add at the end of queue an element of a type known at compile time (ELEMENT_TYPE), copy-constructing
				or move-constructing it from the source.
			\n This function allocates space for and constructs the new element, and returns a transaction object that may be used to 
			allocate raw space associated to the element being inserted, or to alter the element is some way.
			\n When the state of the transaction object is destroyed, if commit has been called the new element becomes visible
			to iterators and consumers. Otherwise the element is destroyed and the call has no visible effects (other than some
			space wasted in the memory pages).

			@param i_source object to be used as source to construct of new element.
				- If this argument is an l-value, the new element copy-constructed (and the source object is left unchanged).
				- If this argument is an r-value, the new element move-constructed (and the source object is left in an undefined but valid state).
			@return A transaction object which can be used to allocate raw space and commit the transaction.
				
            \n <b>Requires</b>:
                - ELEMENT_TYPE must be copy constructible (in case of l-value) or move constructible (in case of r-value)
                - ELEMENT_TYPE * must be implicitly convertible COMMON_TYPE *
                - COMMON_TYPE * must be convertible to ELEMENT_TYPE * with a static_cast or a dynamic_cast. \n This requirement is 
					not met for example if COMMON_TYPE is a non-polymorphic direct or indirect virtual base of ELEMENT_TYPE.

			<b>Complexity</b>: constant.
			\n <b>Effects on iterators</b>: no iterator is invalidated
			\n <b>Throws</b>: unspecified. 
			\n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects). 
			
			<b>Examples</b>
			\snippet heter_queue_samples.cpp heter_queue begin_push example 1 */
		template <typename ELEMENT_TYPE>
			put_transaction begin_push(ELEMENT_TYPE && i_source)
		{
			return begin_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
		}

		/** Begins a transaction to add at the end of queue an element of a type known at compile time (ELEMENT_TYPE), 
			inplace-constructring it from a perfect forwarded a parameter pack.
			\n This function allocates space for and constructs the new element, and returns a transaction object that may be used to 
			allocate raw space associated to the element being inserted, or to alter the element is some way.
			\n When the state of the transaction object is destroyed, if commit has been called the new element becomes visible
			to iterators and consumers. Otherwise the element is destroyed and the call has no visible effects (other than some
			space wasted in the memory pages).

			@param i_construction_params construction parameters for the new element.
			@return A transaction object which can be used to allocate raw space and commit the transaction.
				
            \n <b>Requires</b>:
                - ELEMENT_TYPE must be constructible from the specified parameters
                - ELEMENT_TYPE * must be implicitly convertible COMMON_TYPE *
                - COMMON_TYPE * must be convertible to ELEMENT_TYPE * with a static_cast or a dynamic_cast. \n This requirement is 
					not met for example if COMMON_TYPE is a non-polymorphic direct or indirect virtual base of ELEMENT_TYPE.

			<b>Complexity</b>: constant.
			\n <b>Effects on iterators</b>: no iterator is invalidated
			\n <b>Throws</b>: unspecified. 
			\n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects). 
			
			<b>Examples</b>
			\snippet heter_queue_samples.cpp heter_queue begin_push example 1 */
		template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
			put_transaction begin_emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
		{
			static_assert(std::is_convertible<ELEMENT_TYPE*, COMMON_TYPE*>::value,
				"ELEMENT_TYPE must derive from COMMON_TYPE, or COMMON_TYPE must be void");

			size_t element_size = sizeof(ELEMENT_TYPE);
			size_t element_alignment = alignof(ELEMENT_TYPE);
			if (element_alignment < s_internal_alignment)
			{
				element_alignment = s_internal_alignment;
				element_size = uint_upper_align(element_size, s_internal_alignment);
			}

			auto push_data = element_size <= s_max_size_inpage ?
				implace_allocate(element_size, element_alignment) :
				external_allocate(element_size, element_alignment);

			DENSITY_ASSERT_INTERNAL(push_data.m_control_block != nullptr && push_data.m_element != nullptr);
			new (type_after_control(push_data.m_control_block)) runtime_type(runtime_type::template make<ELEMENT_TYPE>());
			auto const element = new (push_data.m_element) ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
			
			return put_transaction(this, push_data, std::is_same<COMMON_TYPE, void>(), element);
		}

		put_transaction begin_push_by_copy(const runtime_type & i_type, const COMMON_TYPE * i_source)
		{
			size_t element_size = i_type.size();
			size_t element_alignment = i_type.alignment();
			if (element_alignment < s_internal_alignment)
			{
				element_alignment = s_internal_alignment;
				element_size = uint_upper_align(element_size, s_internal_alignment);
			}

			auto push_data = element_size <= s_max_size_inpage ?
				implace_allocate(element_size, element_alignment) :
				external_allocate(element_size, element_alignment);

			DENSITY_ASSERT_INTERNAL(push_data.m_control_block != nullptr && push_data.m_element != nullptr);
			new (type_after_control(push_data.m_control_block)) runtime_type(i_type);
			auto const element = i_type.copy_construct(push_data.m_element, i_source);

			return put_transaction(this, push_data, std::is_same<COMMON_TYPE, void>(), element);
		}

		put_transaction begin_push_by_move(const runtime_type & i_type, COMMON_TYPE * i_source)
		{
			size_t element_size = i_type.size();
			size_t element_alignment = i_type.alignment();
			if (element_alignment < s_internal_alignment)
			{
				element_alignment = s_internal_alignment;
				element_size = uint_upper_align(element_size, s_internal_alignment);
			}

			auto push_data = element_size <= s_max_size_inpage ?
				implace_allocate(element_size, element_alignment) :
				external_allocate(element_size, element_alignment);

			DENSITY_ASSERT_INTERNAL(push_data.m_control_block != nullptr && push_data.m_element != nullptr);
			new (type_after_control(push_data.m_control_block)) runtime_type(i_type);
			auto const element = i_type.move_construct(push_data.m_element, i_source);

			return put_transaction(this, push_data, std::is_same<COMMON_TYPE, void>(), element);
		}

		/** Move-only class that holds the state of a push\emplace transaction, or is empty.
			Push\emplace functions whose name start with 'begin' return a put_transaction that can be used to allocate raw memory,
			in the queue, inspect or alter the element, and commit the push. */
		class put_transaction
		{
		public:

			/** Copy construction is not allowed */
			put_transaction(const put_transaction &) = delete;

			/** Move construction is not allowed */
			put_transaction & operator = (const put_transaction &) = delete;

			/** Move constructs a put_transaction, transferring the state from the source.
					@i_source source to move from. It becomes empty after the call. */
			put_transaction(put_transaction && i_source) noexcept
				: m_queue(i_source.m_queue), m_push_data(i_source.m_push_data), m_committed(i_source.m_committed)
			{
				i_source.m_queue = nullptr;
			}

			/** Move assigns a put_transaction, transferring the state from the source.
				@i_source source to move from. It becomes empty after the call. */
			put_transaction & operator = (put_transaction && i_source) noexcept
			{
				if (this != &i_source)
				{
					m_queue = i_source.m_queue;
					m_push_data = i_source.m_push_data;
					m_committed = i_source.m_committed;
					i_source.m_queue = nullptr;
				}
				return *this;
			}

			/** Allocates a memory block associated to the element being added in the queue. The block may be allocated contiguously with
				the elements in the memory pages. If the block does not fit in one page, the block is allocated using non-paged memory services
				of the allocator.

				\n The block doesn't need to be deallocated, and is guaranteed to be valid until the associated element is destroyed. The initial 
					content of the block is undefined.

				@param i_size size in bytes of the block to allocate.
				@param i_alignment alignment of the block to allocate. It must be a non-zero power of 2.

				<b>Complexity</b>: Unspecified.
				\n <b>Effects on iterators</b>: no iterator is invalidated
				\n <b>Throws</b>: unspecified. 
				\n <b>Exception guarantee</b>: strong (in case of exception the function has no visible side effects). */
			void * raw_allocate(size_t i_size, size_t i_alignment = alignof(std::max_align_t))
			{
				DENSITY_ASSERT(!empty());
				auto push_data = i_size <= s_max_size_inpage ?
					m_queue->implace_allocate(element_size, element_alignment) :
					m_queue->external_allocate(element_size, element_alignment);
				DENSITY_ASSERT_INTERNAL((push_data.m_control->m_next & 3) == 1);
				push_data.m_control->m_next++;
				return push_data.m_element;
			}
			
			/** Marks the state of the transaction so that when it is destroyed the element becomes visible to iterators and consumers. 
				If the state of the transaction is not committed, it will never become visible. 
				
				<b>Complexity</b>: Constant.
				\n <b>Effects on iterators</b>: no iterator is invalidated
				\n <b>Throws</b>: Nothing. */
			void commit() noexcept
			{
				DENSITY_ASSERT(!empty());
				m_committed = true;
			}

			/** Returns true whether this object does not hold the state of a transaction. */
			bool empty() const noexcept { return m_queue == nullptr; }

			/** Returns a pointer to the object being added. 
				\n <i>Note</i>Note: the object is constructed at the begin of the transaction, so this 
				function always returns a pointer to a valid object. */
			COMMON_TYPE * element_ptr() const noexcept
			{
				DENSITY_ASSERT(!empty());
				return get_element(m_push_data.m_control);
			}

			/** Returns the type of the object being added. */
			const RUNTIME_TYPE & type() const noexcept
			{
				DENSITY_ASSERT(!empty());
				return *type_after_control(m_push_data.m_control);
			}

			/** If this put_transaction the destructor has no side effects. Otherwise it commits or cancels the transaction, depending 
				on whether commit() has been called on not. */
			~put_transaction()
			{
				if (m_queue != nullptr)
				{
					if (DENSITY_LIKELY(m_committed))
						commit_put_impl(m_push_data.m_control_block);
					else
						cancel_put_impl(m_push_data.m_control_block);
				}
			}

			#ifndef DOXYGEN_DOC_GENERATION

				// not part of the public interface
				put_transaction(heterogeneous_queue * i_queue, PutData i_push_data, std::true_type, void *) noexcept
					: m_queue(i_queue), m_push_data(i_push_data), m_committed(false)
						{ }

				// not part of the public interface
				put_transaction(heterogeneous_queue * i_queue, PutData i_push_data, std::false_type, COMMON_TYPE * i_element) noexcept
					: m_queue(i_queue), m_push_data(i_push_data), m_committed(false)
				{
					m_push_data.m_element = i_element;
					m_push_data.m_control_block->m_element = i_element;
				}

			#endif // #ifndef DOXYGEN_DOC_GENERATION

		private:
			heterogeneous_queue * m_queue;
			PutData m_push_data;
			bool m_committed;
		};

		class consume_transaction
		{
		public:

			consume_transaction(heterogeneous_queue * i_queue, ControlBlock * i_control) noexcept
				: m_queue(i_queue), m_control(i_control)
			{

			}

			consume_transaction(consume_transaction && i_source) noexcept
				: m_queue(i_source.m_queue), m_control(i_source.m_control)
			{
				i_source.m_control = nullptr;
			}

			consume_transaction & operator = (consume_transaction && i_source) noexcept
			{
				if (this != &i_source)
				{
					m_queue = i_source.m_queue;
					m_control = i_source.m_control;
					i_source.m_control = nullptr;
				}
				return *this;
			}

			consume_transaction(const consume_transaction &) = delete;
			consume_transaction & operator = (const consume_transaction &) = delete;

			~consume_transaction()
			{
				if(m_control != nullptr)
				{
					m_queue->end_consume_impl(m_control);
				}
			}

			const RUNTIME_TYPE & complete_type() const noexcept
			{
				return *type_after_control(m_control);
			}

			void * unaligned_element_ptr() const noexcept
			{
				return get_unaligned_element(m_control);
			}

			COMMON_TYPE * element() const noexcept
			{
				return get_element(m_control);
			}

			explicit operator bool() const noexcept
			{
				return m_control != nullptr;
			}

		private:
			heterogeneous_queue * m_queue;
			ControlBlock * m_control;
		};

		void pop() noexcept
		{
			auto transaction = begin_manual_consume();
			DENSITY_ASSERT((bool)transaction);
			auto const & type = transaction.complete_type();
			auto const element = transaction.element();
			type.destroy(element);
		}

		bool try_pop() noexcept
		{
			auto transaction = begin_manual_consume();
			if (transaction)
			{
				auto const & type = transaction.complete_type();
				auto const element = transaction.element();
				type.destroy(element);
				return true;
			}
			return false;
		}

		/**
			const RUNTIME_TYPE & i_type, COMMON_TYPE * i_element
		*/
		template <typename FUNC>
			void consume(FUNC && i_func) 
				noexcept(noexcept(i_func(std::declval<const RUNTIME_TYPE &>(), std::declval<COMMON_TYPE *>())))
		{
			auto transaction = begin_manual_consume();
			DENSITY_ASSERT(transaction);
			auto const & type = transaction.complete_type();
			auto const element = transaction.element();
			i_func(type, element);
			type.destroy(element);
		}

		/**
			const RUNTIME_TYPE & i_type, COMMON_TYPE * i_element
		
		*/
		template <typename FUNC>
			bool try_consume(FUNC && i_func) 
				noexcept(noexcept(i_func(std::declval<const RUNTIME_TYPE &>(), std::declval<COMMON_TYPE *>())))
		{
			auto transaction = begin_manual_consume();
			if (transaction)
			{
				auto const & type = transaction.complete_type();
				auto const element = transaction.element();
				i_func(type, element);
				type.destroy(element);
				return true;
			}
			return false;
		}

			
		consume_transaction begin_manual_consume() noexcept
		{
			return consume_transaction(this, begin_consume_impl());
		}


					// iterator
			
		class iterator
		{
		public:

			using iterator_category = std::forward_iterator_tag;
			using difference_type = ptrdiff_t;
			using size_type = size_t;
			using value_type = COMMON_TYPE;
			using pointer = COMMON_TYPE *;
			using reference = typename heterogeneous_queue::reference;
			using const_reference = typename heterogeneous_queue::const_reference;

			iterator() noexcept = default;
			iterator(const iterator & i_source) noexcept = default;
			iterator & operator = (const iterator & i_source) noexcept = default;

			iterator(heterogeneous_queue * i_queue, ControlBlock * i_control) noexcept
				: m_control(i_control), m_queue(i_queue)
					{ }

			/** Returns a reference to the subobject of type COMMON_TYPE of current element. If COMMON_TYPE is void this function is useless, because the return type is void. */
			reference operator * () const noexcept { return detail::DeferenceVoidPtr<value_type>::apply(get_element(m_control)); }

			/** Returns a pointer to the subobject of type COMMON_TYPE of current element. If COMMON_TYPE is void this function is useless, because the return type is void *. */
			pointer operator -> () const noexcept { return static_cast<value_type *>(get_element(m_control)); }

			/** Returns a pointer to the subobject of type COMMON_TYPE of current element. If COMMON_TYPE is void, then the return type is void *. */
			pointer element() const noexcept { return static_cast<value_type *>(get_element(m_control)); }

			/** Returns the RUNTIME_TYPE associated to this element. The user may use the function type_info of RUNTIME_TYPE
			(whenever supported) to obtain a const-reference to a std::type_info. */
			const RUNTIME_TYPE & complete_type() const noexcept
			{
				return *type_after_control(m_control);
			}

			iterator & operator ++ () noexcept
			{
				DENSITY_ASSERT(m_queue != nullptr);
				m_control = m_queue->next_valid(m_control);
				return *this;
			}

			iterator operator ++ (int) noexcept
			{
				DENSITY_ASSERT(m_queue != nullptr);
				auto const prev = m_control;
				++*this;
				return iterator(m_queue, prev);
			}

			bool operator == (const iterator & i_other) const noexcept
			{
				return m_control == i_other.m_control;
			}

			bool operator != (const iterator & i_other) const noexcept
			{
				return m_control != i_other.m_control;
			}

			bool operator == (const const_iterator & i_other) const noexcept
			{
				return m_control == i_other.m_control;
			}

			bool operator != (const const_iterator & i_other) const noexcept
			{
				return m_control != i_other.m_control;
			}
					
		private:
			ControlBlock * m_control;
			heterogeneous_queue * m_queue;
		};

		iterator begin() noexcept { return iterator(this, first_valid(m_head)); }
		iterator end() noexcept { return iterator(this, m_tail); }

		class const_iterator
		{
		public:

			using iterator_category = std::forward_iterator_tag;
			using difference_type = ptrdiff_t;
			using size_type = size_t;
			using value_type = const COMMON_TYPE;
			using pointer = const COMMON_TYPE *;
			using reference = typename heterogeneous_queue::const_reference;
			using const_reference = typename heterogeneous_queue::const_reference;

			const_iterator() noexcept = default;
			const_iterator(const const_iterator & i_source) noexcept = default;
			const_iterator & operator = (const const_iterator & i_source) noexcept = default;

			const_iterator(const heterogeneous_queue * i_queue, ControlBlock * i_control) noexcept
				: m_control(i_control), m_queue(i_queue)
					{ }

			/** Returns a reference to the subobject of type COMMON_TYPE of current element. If COMMON_TYPE is void this function is useless, because the return type is void. */
			reference operator * () const noexcept { return detail::DeferenceVoidPtr<value_type>::apply(get_element(m_control)); }

			/** Returns a pointer to the subobject of type COMMON_TYPE of current element. If COMMON_TYPE is void this function is useless, because the return type is void *. */
			pointer operator -> () const noexcept { return static_cast<value_type *>(get_element(m_control)); }

			/** Returns a pointer to the subobject of type COMMON_TYPE of current element. If COMMON_TYPE is void, then the return type is void *. */
			pointer element() const noexcept { return static_cast<value_type *>(get_element(m_control)); }

			/** Returns the RUNTIME_TYPE associated to this element. The user may use the function type_info of RUNTIME_TYPE
				(whenever supported) to obtain a const-reference to a std::type_info. */
			const RUNTIME_TYPE & complete_type() const noexcept
			{
				return *type_after_control(m_control);
			}

			const_iterator & operator ++ () noexcept
			{
				DENSITY_ASSERT(m_queue != nullptr);
				m_control = m_queue->next_valid(m_control);
				return *this;
			}

			const_iterator operator ++ (int) noexcept
			{
				DENSITY_ASSERT(m_queue != nullptr);
				auto const prev = m_control;
				++*this;
				return const_iterator(m_queue, prev);
			}

			bool operator == (const const_iterator & i_other) const noexcept
			{
				return m_control == i_other.m_control;
			}

			bool operator != (const const_iterator & i_other) const noexcept
			{
				return m_control != i_other.m_control;
			}
					
			bool operator == (const iterator & i_other) const noexcept
			{
				return m_control == i_other.m_control;
			}

			bool operator != (const iterator & i_other) const noexcept
			{
				return m_control != i_other.m_control;
			}

		private:
			ControlBlock * m_control;
			const heterogeneous_queue * m_queue;
		};

		const_iterator begin() const noexcept { return const_iterator(this, first_valid(m_head)); }
		const_iterator end() const noexcept { return const_iterator(this, m_tail); }

		const_iterator cbegin() const noexcept { return const_iterator(this, first_valid(m_head)); }
		const_iterator cend() const noexcept { return const_iterator(this, m_tail); }

		bool operator == (const heterogeneous_queue & i_source) const
		{
			const auto end_1 = cend();
			for (auto it_1 = cbegin(), it_2 = i_source.cbegin(); it_1 != end_1; ++it_1, ++it_2)
			{
				auto const equal_comparer = it_1.complete_type().template get_feature<type_features::equals>();
				if (it_1.complete_type() != it_2.complete_type() ||
					!equal_comparer(it_1.element(), it_2.element()))
				{
					return false;
				}
			}
			return true;
		}

		bool operator != (const heterogeneous_queue & i_source) const
		{
			return !operator == (i_source);
		}

	private:	
		
		constexpr static auto s_invalid_control_block = ALLOCATOR_TYPE::page_size - 1;
		constexpr static size_t s_internal_alignment = detail::size_max(4, detail::size_max(alignof(ControlBlock), alignof(runtime_type)));
		constexpr static size_t s_sizeof_ControlBlock = (sizeof(ControlBlock) + (s_internal_alignment - 1)) & ~(s_internal_alignment - 1);
		constexpr static size_t s_sizeof_RuntimeType = (sizeof(runtime_type) + (s_internal_alignment - 1)) & ~(s_internal_alignment - 1);
		constexpr static auto s_max_size_inpage = ALLOCATOR_TYPE::page_size - s_sizeof_ControlBlock - s_sizeof_RuntimeType;

		ControlBlock * first_valid(ControlBlock * i_from) const
		{
			for (auto curr = i_from; curr != m_tail; )
			{
				auto const control_bits = curr->m_next & 3;
				if (control_bits == 0)
				{
					return curr;
				}
				curr = reinterpret_cast<ControlBlock*>(curr->m_next - control_bits);
			}
			return m_tail;
		}

		ControlBlock * next_valid(ControlBlock * i_from) const
		{
			DENSITY_ASSERT_INTERNAL(i_from != m_tail);
			for (auto curr = reinterpret_cast<ControlBlock*>(i_from->m_next & ~static_cast<uintptr_t>(3)); curr != m_tail; )
			{
				auto const control_bits = curr->m_next & 3;
				if (control_bits == 0)
				{
					return curr;
				}
				curr = reinterpret_cast<ControlBlock*>(curr->m_next - control_bits);
			}
			return m_tail;
		}

		static runtime_type * type_after_control(ControlBlock * i_control)
		{
			return reinterpret_cast<runtime_type*>(address_add(i_control, s_sizeof_ControlBlock));
		}

		static void * get_unaligned_element(detail::QueueControl<void> * i_control)
		{
			return address_add(i_control, s_sizeof_ControlBlock + s_sizeof_RuntimeType );
		}

		static void * get_element(detail::QueueControl<void> * i_control)
		{
			return address_upper_align(get_unaligned_element(i_control), type_after_control(i_control)->alignment());
		}

		template <typename TYPE>
			static void * get_unaligned_element(detail::QueueControl<TYPE> * i_control)
		{
			return i_control->m_element;
		}

		template <typename TYPE>
			static TYPE * get_element(detail::QueueControl<TYPE> * i_control)
		{
			return i_control->m_element;
		}

		static bool are_in_same_page(void * i_first, void * i_second)
		{
			return (reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) < allocator_type::page_size;
		}

		struct ExternalBlock
		{
			void * m_block;
		};

		PutData external_allocate(size_t i_size, size_t i_alignment)
		{
			auto const external_block = ALLOCATOR_TYPE::allocate(i_size, i_alignment);

			PutData inplace_put = implace_allocate(sizeof(ExternalBlock), alignof(ExternalBlock));
					
			new(inplace_put.m_element) ExternalBlock{ external_block };

			return PutData{ inplace_put.m_control_block, external_block };
		}

		PutData implace_allocate(size_t i_size, size_t i_alignment)
		{
			DENSITY_ASSERT_INTERNAL(i_alignment >= s_internal_alignment && is_power_of_2(i_alignment) && (i_size % i_alignment) == 0
				&& i_size <= s_max_size_inpage);

			for(;;)
			{
				auto const control_block = m_tail;
				void * new_tail = address_add(control_block, s_sizeof_ControlBlock + s_sizeof_RuntimeType );

				new_tail = address_upper_align(new_tail, i_alignment);
				void * new_element = new_tail;
				new_tail = address_add(new_tail, i_size);

				if (DENSITY_LIKELY(are_in_same_page(address_add(new_tail, s_sizeof_ControlBlock), control_block)))
				{
					DENSITY_ASSERT_INTERNAL(control_block != nullptr);
					new(control_block) ControlBlock();

					control_block->m_next = reinterpret_cast<uintptr_t>(new_tail) + 1;
					m_tail = static_cast<ControlBlock *>(new_tail);
					return PutData{ control_block, new_element };
				}
				else
				{
					allocate_new_page();
				}
			}
		}

		DENSITY_NO_INLINE void allocate_new_page()
		{
			// page overflow
			if (DENSITY_LIKELY(m_tail != reinterpret_cast<ControlBlock*>(s_invalid_control_block)))
			{
				auto const control_block = m_tail;

				DENSITY_ASSERT_INTERNAL(control_block != nullptr);
				new(control_block) ControlBlock();

				auto new_page = allocator_type::allocate_page();
				control_block->m_next = reinterpret_cast<uintptr_t>(new_page) + 2;
				m_tail = static_cast<ControlBlock *>(new_page);
			}
			else
			{
				// this happens only on a virgin queue
				m_tail = m_head = static_cast<ControlBlock*>(allocator_type::allocate_page());
			}
		}

		static void commit_put_impl(ControlBlock * i_control_block)
		{
			DENSITY_ASSERT_INTERNAL((i_control_block->m_next & 3) == 1);
			i_control_block->m_next--;
		}

		DENSITY_NO_INLINE static void cancel_put_impl(ControlBlock * i_control_block)
		{
			const auto type_ptr = type_after_control(i_control_block);
			type_ptr->destroy(get_element(i_control_block));

			DENSITY_ASSERT_INTERNAL((i_control_block->m_next & 3) == 1);
			i_control_block->m_next++;
		}

		ControlBlock * begin_consume_impl() noexcept
		{
			auto curr = m_head;
			while (curr != m_tail)
			{
				auto const control_bits = curr->m_next & 3;
				if (control_bits == 0)
				{
					curr->m_next |= 1;
					return curr;
				}

				curr = reinterpret_cast<ControlBlock*>(curr->m_next - control_bits);
			}

			return nullptr;
		}

		void end_consume_impl(ControlBlock * i_control_block) noexcept
		{
			DENSITY_ASSERT_INTERNAL((i_control_block->m_next & 3) == 1);
			i_control_block->m_next++;

			if(i_control_block == m_head)
			{
				auto curr = i_control_block;
				DENSITY_ASSERT_INTERNAL(curr != m_tail);
				do {
					auto const control_bits = curr->m_next & 3;
					if (control_bits != 2)
					{
						break;
					}

					auto next = reinterpret_cast<ControlBlock*>(curr->m_next - control_bits);

					auto address_xor = reinterpret_cast<uintptr_t>(next) ^ reinterpret_cast<uintptr_t>(curr);
					if (address_xor >= allocator_type::page_size)
					{
						allocator_type::deallocate_page(address_lower_align(curr, allocator_type::page_size));
					}

					curr = next;

				} while (curr != m_tail);

				m_head = curr;
			}
		}

		void destroy() noexcept
		{
			clear();
			DENSITY_ASSERT_INTERNAL(m_tail == m_head);
			allocator_type::deallocate_page(address_lower_align(m_head, allocator_type::page_size));
		}

	private:
		ControlBlock * m_head, * m_tail;
	};
	
	template <typename COMMON_TYPE, typename RUNTIME_TYPE, typename ALLOCATOR_TYPE>
		inline void swap(heterogeneous_queue<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE> & i_first,
			heterogeneous_queue<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE> & i_second) noexcept
	{
		i_first.swap(i_second);
	}

} // namespace density
