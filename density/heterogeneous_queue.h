
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <iterator>
#include <density/density_common.h>
#include <density/runtime_type.h>
#include <density/void_allocator.h>

namespace density
{
    namespace detail
    {
        template<typename COMMON_TYPE> struct QueueControl // used by heterogeneous_queue<T,...>
        {
            uintptr_t m_next; /**< This is a pointer to the next value of the queue, mixed with the flags
									defined in Queue_Flags. */
            COMMON_TYPE * m_element; /**< If COMMON_TYPE is not void we have to store the pointer to
										 this subobject. Only the compiler knows how to obtain it from
										 a pointer to the complete object. 
										 For elements whose type is fixed at compile time the value of 
										 m_element is obtained by implicit conversion of the pointer to 
										 the complete type to COMMON_TYPE. For elements constructed
										 from a runtime type object, it is returned by the construct* function. */
        };

        template<> struct QueueControl<void> // used by heterogeneous_queue<void,...>
        {
            uintptr_t m_next; /**< see the other QueueControl::m_next */
        };
		
		/** /internal Defines flags that can be set on QueueControl::m_next */
		enum Queue_Flags : uintptr_t
		{
			Queue_Busy = 1, /**< if set someone is producing or consuming this element **/
			Queue_Dead = 2,  /**< if set this element is not consumable (and the flag Queue_Busy is meaningless).
									Dead elements are:
									- elements already consumed, or elements whose constructor threw
									- raw allocations
									- page jump elements */
			Queue_External = 4, /**< if set the element is a ExternalBlock that points to an externally allocated element */
			Queue_AllFlags = Queue_Busy | Queue_Dead | Queue_External
		};
    }

    /** \brief Heterogeneous FIFO pseudo-container class template. 	

		A value of heterogeneous_queue is a pair of a runtime type object bound to a type E, and an object of type E (called element).
		heterogeneous_queue is an heterogeneous pseudo-container: elements of the same queue can have different types.
		Elements can be added only at the end (<em>put operation</em>), and can be removed only at the beginning 
		(<em>consume operation</em>).
		When doing a put, the user may associate one or more <em>raw memory blocks</em> to the element. Raw blocks are
		deallocated automatically when the value is consumed.
		heterogeneous_queue supports iterators, but they are just <a href="http://en.cppreference.com/w/cpp/concept/InputIterator">Input Iterators</a>
		so heterogeneous_queue is not a container.

		@tparam COMMON_TYPE Common type of all the elements. An object of type E can be pushed on the queue only if E* is 
			implicitly convertible to COMMON_TYPE*. If COMMON_TYPE is void (the default), any type can be put in the queue. 
			Otherwise it should be an user-defined-type, and only types deriving from it can be added.
        @tparam RUNTIME_TYPE Type to be used to handle at runtime the actual complete type of each element.
                This type must meet the requirements of \ref RuntimeType_concept "RuntimeType". The default is runtime_type.
        @tparam ALLOCATOR_TYPE Allocator type to be used. This type must meet the requirements of both \ref UntypedAllocator_concept
                "UntypedAllocator" and \ref PagedAllocator_concept "PagedAllocator". The default is density::void_allocator.
		
		\n <b>Thread safeness</b>: None. The user is responsible to avoid data races.
		\n <b>Exception safeness</b>: Any function of heterogeneous_queue is noexcept or provides the strong exception guarantee.

		Basic usage
		--------------------------
		Elements can be added with \ref heterogeneous_queue::push "push" or \ref heterogeneous_queue::emplace "emplace":
		
		\snippet heterogeneous_queue_examples.cpp heterogeneous_queue put example 1

		In this case the type of the element is fixed at compile time. In the case of emplace, it is not 
		dependent on the type of the arguments, so it must be explicitly specified.

		Put operations can be transactional, in which case the name of the function contains <code>start_</code>:

		\snippet heterogeneous_queue_examples.cpp heterogeneous_queue put example 2

		Transactional puts returns an object of type put_transaction that should be used to commit or cancel
		the transaction. If a transaction is destroyed before being committed, it is canceled automatically.
		Before being committed a transaction hasn't observable side effects (it does have non-observable side
		effects anyway, like the reservation of space in a page).
		The functions \ref put_transaction::raw_allocate "raw_allocate" and 
		\ref put_transaction::raw_allocate_copy "raw_allocate_copy" allows to associate one or
		more raw memory blocks to the element. In this case the element should keep the pointer to the blocks 
		(otherwise consumers are not able to access the blocks).

		The function \ref heterogeneous_queue::try_start_consume "try_start_consume" can be used to consume an
		element. The returned object has type consume_operation, which is similar to put_transaction (it can be 
		canceled or committed), with the difference that it has observable side effects before commit 
		or cancel is called: the element disappears from the queue when try_start_consume is called, and re-appears 
		whenever cancel is called (or the consume_operation is destroyed without being committed).

		\snippet heterogeneous_queue_examples.cpp heterogeneous_queue consume example 1

		The example above searches for an exact match of the type being consumed (using runtime_type::is). Anyway
		runtime_type (the default <code>RUNTIME_TYPE</code>) allows to add custom functions that can be called regardless 
		of the type. The following example uses the built-in type_features::ostream:
		
		\snippet heterogeneous_queue_examples.cpp heterogeneous_queue example 3
		
		The type of the element may be known only at runtime time, in which case \ref dyn_push can be used:

		\snippet heterogeneous_queue_examples.cpp heterogeneous_queue example 4
						
		Member function containing <code>reentrant_</code> in their names support reentrancy: while they are in progress, other puts, consumes, 
		iterations and any non-modifying operation are allowed, but only in the same thread (reentrancy has nothing to do with 
		multithreading). \n
		In contrast, while a non-reentrant operation is in progress, the queue is not in a consistent state: if during a put the 
		construction of the new element directly or indirectly calls other member functions on the same queue, the behavior is undefined.

		A value in the queue has the type <code>std::pair<const RUNTIME_TYPE &, COMMON_TYPE* const></code>. Iterators are 
		conceptually pointers to such pairs.
		They don't provide the multipass guarantee, so they are not
		<a href="http://en.cppreference.com/w/cpp/concept/ForwardIterator">Forward Iterators</a>, but just
		<a href="http://en.cppreference.com/w/cpp/concept/InputIterator">Input Iterators</a>.
		For this reason heterogeneous_queue is not a container.
		Insertions invalidate no iterators. Removals invalidate only the iterators pointing to the element 
		being removed.
		Past-the-end iterators are never invalidated, and they compare equal each other and with a default constructed iterator:

		\snippet heterogeneous_queue_examples.cpp heterogeneous_queue iterators example 1

		Put functions summary
		--------------------------
		Here is a summary of the put functions. Functions containing <code>dyn_</code> in their name allow to put an element whose type
		is not known at compile type (they take as argument an object of type <code>RUNTIME_TYPE</code>).
        <table>
        <caption id="multi_row">Put functions</caption>
        <tr>
            <th style="width:400px">Group</th>
            <th>Functions</th>
            <th style="width:130px">Type binding</th>
            <th style="width:130px">Constructor</th>
        </tr>
        <tr>
            <td>[start_][reentrant_]push</td>
            <td>@code push, reentrant_push, start_push, start_reentrant_push @endcode</td>
            <td>Compile time</td>
            <td>Copy/Move</td>
        </tr>
        <tr>
            <td>[start_][reentrant_]emplace</td>
            <td>@code emplace, reentrant_emplace, start_emplace, start_reentrant_emplace @endcode</td>
            <td>Compile time</td>
            <td>Any</td>
        </tr>
        <tr>
            <td>[start_][reentrant_]dyn_push</td>
            <td>@code dyn_push, reentrant_dyn_push, start_dyn_push, start_reentrant_dyn_push @endcode</td>
            <td>Runtime</td>
            <td>Default</td>
        </tr>
        <tr>
            <td>[start_][reentrant_]dyn_push_copy</td>
            <td>@code dyn_push_copy, reentrant_dyn_push_copy, start_dyn_push_copy, start_reentrant_dyn_push_copy @endcode</td>
            <td>Runtime</td>
            <td>Copy</td>
        </tr>
        <tr>
            <td>[start_][reentrant_]dyn_push_move</td>
            <td>@code dyn_push_move, reentrant_dyn_push_move, start_dyn_push_move, start_reentrant_dyn_push_move @endcode</td>
            <td>Runtime</td>
            <td>Move</td>
        </tr>
        </table>

		Implementation and performance notes
		--------------------------
		An heterogeneous_queue is basically composed by an ordered set of pages (whose size is determined
		by the allocator), an <em>head pointer</em> and <em>tail pointer</em>. The first page is the 
		<em>head page</em>, that is the one that contains the head pointer. The last page is the 
		<em>tail page</em>, that is the one that contains the head pointer.
		
		A new value is allocated in the queue by adding its size to the tail pointer. When a page overflow
		occurs, a new page is requested to the allocator. Whenever a value is too large to fit in a page,
		it is allocated outside the pages, with a legacy allocation. Raw memory blocks are allocated in the
		same way of values, with the difference that they don't have a runtime type associated.

		When a value is consumed, its size is added to the head pointer. When the last value of a page is
		consumed, the page is deallocated.

		Puts and consumes never needs copy or move elements.

		Elements and runtime-types are allocated linearly and tightly in memory pages allocated by the provided allocator.
		Elements are never reallocated or moved, so insertions and removals have always strict constant complexity.
		Pages are not recycled: when the last element in a page is consumed, the page is freed.		

		- If <code>COMMON_TYPE</code> is not void for every element the queue stores an additional pointer.
		- non-reentrant operations may be faster than reentrant
		- Transactional operations are not slower than non-transactional ones
		- Typed put operations (like \ref heterogeneous_queue::push "push") are faster than dynamic puts (like 
			\ref heterogeneous_queue::dyn_push "dyn_push"),  because they can do some computations at compile time,
			and because they don't use the <code>RUNTIME_TYPE</code> to construct the element.
    */
    template < typename COMMON_TYPE = void, typename RUNTIME_TYPE = runtime_type<COMMON_TYPE>, typename ALLOCATOR_TYPE = void_allocator >
        class heterogeneous_queue : private ALLOCATOR_TYPE
    {
		using ControlBlock = typename detail::QueueControl<COMMON_TYPE>;

		/** Pointer to the head. It is equal to s_invalid_control_block, or it is aligned to min_alignment. */
		ControlBlock * m_head;
		
		/** Pointer to the tail. It is equal to s_invalid_control_block, or it is aligned to min_alignment. */
		ControlBlock * m_tail;

	public:

		/** Minimum guaranteed alignment for every element. The actual alignment of an element may be stricter
			if the type requires it. */
        constexpr static size_t min_alignment = detail::size_max(detail::Queue_AllFlags + 1,
			alignof(ControlBlock), alignof(RUNTIME_TYPE));

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

        class put_transaction;
        template <typename TYPE> class typed_put_transaction;
        class reentrant_put_transaction;
        template <typename TYPE> class reentrant_typed_put_transaction;
        class iterator;
        class const_iterator;

    private:

		/** Value used to initialize m_head and m_tail. When the first put is done, this value
			causes a page overflow, and both m_head and m_tail are set to the a newly allocated page.
			This mechanism allow the default constructor to be small, fast, and noexcept. */
		constexpr static auto s_invalid_control_block = ALLOCATOR_TYPE::page_size - 1;

		/** Actual space allocated for a ControlBlock, which is forced to be aligned to min_alignment. This is
			to avoid too many address upper-align operations. */
		constexpr static size_t s_sizeof_ControlBlock = uint_upper_align(sizeof(ControlBlock), min_alignment);

		/** Actual space allocated for a RUNTIME_TYPE, which is forced to be aligned to min_alignment. This is
			to avoid too many address upper-align operations. */
		constexpr static size_t s_sizeof_RuntimeType = uint_upper_align(sizeof(RUNTIME_TYPE), min_alignment);

		/** Maximum size for an element to be allocated in a page. */
        constexpr static auto s_max_size_inpage = ALLOCATOR_TYPE::page_size - s_sizeof_ControlBlock - s_sizeof_RuntimeType - s_sizeof_ControlBlock;

		/** This struct is the return type of allocation functions. */
        struct Block
        {
            ControlBlock * m_control_block;
            void * m_user_storage;
        };

    public:

        static_assert(std::is_same<COMMON_TYPE, typename RUNTIME_TYPE::common_type>::value,
            "COMMON_TYPE and RUNTIME_TYPE::common_type must be the same type (did you try to use a type like heter_cont<A,runtime_type<B>>?)");

        static_assert(std::is_same<COMMON_TYPE, typename std::decay<COMMON_TYPE>::type>::value,
            "COMMON_TYPE can't be cv-qualified, an array or a reference");

		static_assert(is_power_of_2(ALLOCATOR_TYPE::page_alignment) &&
			ALLOCATOR_TYPE::page_alignment >= ALLOCATOR_TYPE::page_size &&
			(ALLOCATOR_TYPE::page_alignment % min_alignment) == 0,
			"The alignment of the pages must be a power of 2, greater or equal to the size of the pages, and a multiple of min_alignment");

        static_assert(ALLOCATOR_TYPE::page_size > (min_alignment + alignof(ControlBlock)) * 4, "Invalid page size");
		
        /** Default constructor. The allocator is default-constructed.

            <b>Complexity</b>: constant.
            \n <b>Throws</b>: nothing.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).
            \n <i>Implementation notes</i>:
				This constructor does not allocate memory and never throws. */
        heterogeneous_queue() noexcept
            : m_head(reinterpret_cast<ControlBlock*>(s_invalid_control_block)),
              m_tail(reinterpret_cast<ControlBlock*>(s_invalid_control_block))
        {
        }

		/** Constructor with allocator parameter. The allocator is copy-constructed.
			@param i_source_allocator source used to copy-construct the allocator.

            <b>Complexity</b>: constant.
            \n <b>Throws</b>: nothing.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).
            \n <i>Implementation notes</i>:
				This constructor does not allocate memory. It throws anything the copy constructor of the allocator throws. */
        heterogeneous_queue(const ALLOCATOR_TYPE & i_source_allocator)
				noexcept (std::is_nothrow_copy_constructible<ALLOCATOR_TYPE>::value)
            : ALLOCATOR_TYPE(i_source_allocator),
			  m_head(reinterpret_cast<ControlBlock*>(s_invalid_control_block)),
              m_tail(reinterpret_cast<ControlBlock*>(s_invalid_control_block))
        {
        }

		/** Constructor with allocator parameter. The allocator is move-constructed.
			@param i_source_allocator source used to move-construct the allocator.

            <b>Complexity</b>: constant.
            \n <b>Throws</b>: nothing.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).
            \n <i>Implementation notes</i>:
				This constructor does not allocate memory. It throws anything the move constructor of the allocator throws. */
        heterogeneous_queue(ALLOCATOR_TYPE && i_source_allocator)
				noexcept (std::is_nothrow_move_constructible<ALLOCATOR_TYPE>::value)
            : ALLOCATOR_TYPE(std::move(i_source_allocator)),
			  m_head(reinterpret_cast<ControlBlock*>(s_invalid_control_block)),
              m_tail(reinterpret_cast<ControlBlock*>(s_invalid_control_block))
        {
        }

        /** Move constructor. The allocator is move-constructed from the one of the source.
                @param i_source source to move the elements from. After the call the source is left in some valid but indeterminate state.

            <b>Complexity</b>: constant.
            \n <b>Throws</b>: nothing.
            \n <i>Implementation notes</i>:
                - After the call the source is left empty. */
        heterogeneous_queue(heterogeneous_queue && i_source) noexcept
            : ALLOCATOR_TYPE(std::move(static_cast<ALLOCATOR_TYPE&&>(i_source))),
              m_head(i_source.m_head), m_tail(i_source.m_tail)
        {
            i_source.m_tail = i_source.m_head = reinterpret_cast<ControlBlock*>(s_invalid_control_block);
        }

        /** Copy constructor. The allocator is copy-constructed from the one of the source.

            \n <b>Requires</b>:
                - the runtime type must support the feature type_features::copy_construct

            <b>Complexity</b>: linear in the number of elements of the source.
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
        heterogeneous_queue(const heterogeneous_queue & i_source)
            : allocator_type(static_cast<const allocator_type&>(i_source)),
              m_head(reinterpret_cast<ControlBlock*>(s_invalid_control_block)),
			  m_tail(reinterpret_cast<ControlBlock*>(s_invalid_control_block))
        {
            for (auto source_it = i_source.cbegin(); source_it != i_source.cend(); source_it++)
            {
                dyn_push_copy(source_it.complete_type(), source_it.element_ptr());
            }
        }

        /** Move assignment. The allocator is move-assigned from the one of the source.
                @param i_source source to move the elements from. After the call the source is left in some valid but indeterminate state.

            <b>Complexity</b>: Unspecified.
            \n <b>Effects on iterators</b>: Any iterator pointing to this queue or to the source queue is invalidated.
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

        /** Copy assignment. The allocator is copy-assigned from the one of the source.
                @param i_source source to move the elements from. After the call the source is left in some valid but indeterminate state.
            
			<b>Complexity</b>: linear.
            \n <b>Effects on iterators</b>: Any iterator pointing to this queue is invalidated.
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
        heterogeneous_queue & operator = (const heterogeneous_queue & i_source)
        {
            auto copy(i_source);
            swap(copy);
            return *this;
        }

		/** Returns a copy of the allocator */
		allocator_type get_allocator() noexcept(std::is_nothrow_copy_constructible<allocator_type>::value)
		{
			return *this;
		}

		/** Returns a reference to the allocator */
		allocator_type & get_allocator_ref() noexcept
		{
			return *this;
		}

		/** Returns a const reference to the allocator */
		const allocator_type & get_allocator_ref() const noexcept
		{
			return *this;
		}

        /** Swaps the content of this queue and another one. The allocators are swapped too.
                @param i_other queue to swap with.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: the domain of validity of iterators is swapped
            \n <b>Throws</b>: Nothing. */
        void swap(heterogeneous_queue & i_other) noexcept
        {
            using std::swap;
            swap(static_cast<ALLOCATOR_TYPE&>(*this), static_cast<ALLOCATOR_TYPE&>(i_other));
            swap(m_head, i_other.m_head);
            swap(m_tail, i_other.m_tail);
        }

		/** Global function that swaps two queue. Equivalent to this->swap(i_second). */
		friend inline void swap(heterogeneous_queue<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE> & i_first,
			heterogeneous_queue<COMMON_TYPE, RUNTIME_TYPE, ALLOCATOR_TYPE> & i_second) noexcept
		{
			i_first.swap(i_second);
		}

        /** Destructor.

            <b>Complexity</b>: linear.
            \n <b>Effects on iterators</b>: any iterator pointing to this queue is invalidated.
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
			// the queue may contain busy or dead elements, that must be ignored
            for (auto curr = m_head; curr != m_tail; )
            {
                auto const control_bits = curr->m_next & (detail::Queue_Busy | detail::Queue_Dead);
                if (control_bits != detail::Queue_Dead)
                {
                    return false;
                }
                curr = reinterpret_cast<ControlBlock*>(curr->m_next & ~detail::Queue_AllFlags);
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
				auto transaction = try_start_consume();
                if (!transaction)
                    break;
                transaction.commit();
            }

            DENSITY_ASSERT_INTERNAL(empty());
        }

        /** Adds at the end of queue an element of a type known at compile time (ELEMENT_TYPE), copy-constructing or move-constructing
                it from the source.

            @param i_source object to be used as source to construct of new element.
                - If this argument is an l-value, the new element copy-constructed
                - If this argument is an r-value, the new element move-constructed

            \n <b>Requires</b>:
                - ELEMENT_TYPE must be copy constructible (in case of l-value) or move constructible (in case of r-value)
                - ELEMENT_TYPE * must be implicitly convertible COMMON_TYPE *
                - COMMON_TYPE * must be convertible to ELEMENT_TYPE * with a static_cast or a dynamic_cast. \n This requirement is
                    not met for example if COMMON_TYPE is a non-polymorphic direct or indirect virtual base of ELEMENT_TYPE.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue push example 1 */
        template <typename ELEMENT_TYPE>
            void push(ELEMENT_TYPE && i_source)
        {
            return emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Adds at the end of queue an element of a type known at compile time (ELEMENT_TYPE), inplace-constructing it from
                a perfect forwarded parameter pack.
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
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            void emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
            start_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...).commit();
        }

        /** Adds at the end of queue an element of a type known at runtime, default-constructing it.

            @param i_type type of the new element.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue dyn_push example 1 */
        void dyn_push(const runtime_type & i_type)
        {
            start_dyn_push(i_type).commit();
        }

        /** Adds at the end of queue an element of a type known at runtime, copy-constructing it from the source.

            @param i_type type of the new element.
            @param i_source pointer to the subobject of type COMMON_TYPE of an object or subobject of type ELEMENT_TYPE.
                <i>Note</i>: be careful using void pointers: casts from\to a base to\from a derived class can be done only
                by the type system of the language.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue dyn_push_copy example 1 */
        void dyn_push_copy(const runtime_type & i_type, const COMMON_TYPE * i_source)
        {
            start_dyn_push_copy(i_type, i_source).commit();
        }

        /** Adds at the end of queue an element of a type known at runtime, move-constructing it from the source.

            @param i_type type of the new element
            @param i_source pointer to the subobject of type COMMON_TYPE of an object or subobject of type ELEMENT_TYPE
                <i>Note</i>: be careful using void pointers: casts from\to a base to\from a derived class can be done only
                by the type system of the language.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue dyn_push_move example 1 */
        void dyn_push_move(const runtime_type & i_type, COMMON_TYPE * i_source)
        {
            start_dyn_push_move(i_type, i_source).commit();
        }

        /** Begins a transaction to add at the end of queue an element of a type known at compile time (ELEMENT_TYPE), copy-constructing
                or move-constructing it from the source.
            \n This function allocates space for and constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
            \n Call the member function commit on the returned transaction in order to make the effects observable.
			If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.

            @param i_source object to be used as source to construct of new element.
                - If this argument is an l-value, the new element copy-constructed.
                - If this argument is an r-value, the new element move-constructed.
            @return A transaction object which can be used to allocate raw space and commit the transaction.

            \n <b>Requires</b>:
                - ELEMENT_TYPE must be copy constructible (in case of l-value) or move constructible (in case of r-value)
                - ELEMENT_TYPE * must be implicitly convertible COMMON_TYPE *
                - COMMON_TYPE * must be convertible to ELEMENT_TYPE * with a static_cast or a dynamic_cast. \n This requirement is
                    not met for example if COMMON_TYPE is a non-polymorphic direct or indirect virtual base of ELEMENT_TYPE.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue start_push example 1 */
        template <typename ELEMENT_TYPE>
            typed_put_transaction<ELEMENT_TYPE> start_push(ELEMENT_TYPE && i_source)
        {
            return start_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Begins a transaction to add at the end of queue an element of a type known at compile time (ELEMENT_TYPE),
            inplace-constructing it from a perfect forwarded parameter pack.
            \n This function allocates space for and constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
			\n Call the member function commit on the returned transaction in order to make the effects observable.
			If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.

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
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue start_push example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            typed_put_transaction<ELEMENT_TYPE> start_emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
            static_assert(std::is_convertible<ELEMENT_TYPE*, COMMON_TYPE*>::value,
                "ELEMENT_TYPE must derive from COMMON_TYPE, or COMMON_TYPE must be void");
			
			auto push_data = implace_allocate<0, true, sizeof(ELEMENT_TYPE), alignof(ELEMENT_TYPE)>();

			COMMON_TYPE * element = nullptr;
			runtime_type * type = nullptr; 
			try
			{
				auto const type_storage = type_after_control(push_data.m_control_block);
				DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
				type = new (type_storage) runtime_type(runtime_type::template make<ELEMENT_TYPE>());

				DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
				element = new (push_data.m_user_storage) ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
			}
			catch (...)
			{
				if (type != nullptr)
					type->RUNTIME_TYPE::~RUNTIME_TYPE();
				DENSITY_ASSERT_INTERNAL((push_data.m_control_block->m_next & (detail::Queue_Busy | detail::Queue_Dead)) == 0);
				push_data.m_control_block->m_next += detail::Queue_Dead;
				throw;
			}

            return typed_put_transaction<ELEMENT_TYPE>(this, push_data, std::is_void<COMMON_TYPE>(), element);
        }

        /** Begins a transaction to add at the end of queue an element of a type known at runtime, default-constructing it.
            \n This function allocates space for and constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
			\n Call the member function commit on the returned transaction in order to make the effects observable.
			If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.

            @param i_type type of the new element.
            @return A transaction object which can be used to allocate raw space and commit the transaction.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue start_dyn_push example 1 */
        put_transaction start_dyn_push(const runtime_type & i_type)
        {
			auto push_data = implace_allocate<0, true>(i_type.size(), i_type.alignment());

			COMMON_TYPE * element = nullptr;
			runtime_type * type = nullptr;
			try
			{
				auto const type_storage = type_after_control(push_data.m_control_block);
				DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
				type = new (type_storage) runtime_type(i_type);
				
				DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
				element = i_type.default_construct(push_data.m_user_storage);
			}
			catch (...)
			{
				if (type != nullptr)
					type->RUNTIME_TYPE::~RUNTIME_TYPE();
				DENSITY_ASSERT_INTERNAL((push_data.m_control_block->m_next & (detail::Queue_Busy | detail::Queue_Dead)) == 0);
				push_data.m_control_block->m_next += detail::Queue_Dead;
				throw;
			}

            return put_transaction(this, push_data, std::is_void<COMMON_TYPE>(), element);
        }


        /** Begins a transaction to add at the end of queue an element of a type known at runtime, copy-constructing it from the source..
            \n This function allocates space for and constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
			\n Call the member function commit on the returned transaction in order to make the effects observable.
			If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.

            @param i_type type of the new element.
            @param i_source pointer to the subobject of type COMMON_TYPE of an object or subobject of type ELEMENT_TYPE.
                <i>Note</i>: be careful using void pointers: casts from\to a base to\from a derived class can be done only
                by the type system of the language.
            @return A transaction object which can be used to allocate raw space and commit the transaction.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue start_dyn_push_copy example 1 */
        put_transaction start_dyn_push_copy(const runtime_type & i_type, const COMMON_TYPE * i_source)
        {
			auto push_data = implace_allocate<0, true>(i_type.size(), i_type.alignment());
			
			COMMON_TYPE * element = nullptr;
			runtime_type * type = nullptr;			
			try
			{
				auto const type_storage = type_after_control(push_data.m_control_block);
				DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
				type = new (type_storage) runtime_type(i_type);
				
				DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
				element = i_type.copy_construct(push_data.m_user_storage, i_source);
			}
			catch (...)
			{
				if (type != nullptr)
					type->RUNTIME_TYPE::~RUNTIME_TYPE();
				DENSITY_ASSERT_INTERNAL((push_data.m_control_block->m_next & (detail::Queue_Busy | detail::Queue_Dead)) == 0);
				push_data.m_control_block->m_next += detail::Queue_Dead;
				throw;
			}

            return put_transaction(this, push_data, std::is_same<COMMON_TYPE, void>(), element);
        }

        /** Begins a transaction to add at the end of queue an element of a type known at runtime, move-constructing it from the source..
            \n This function allocates space for and constructs the new element, and returns a transaction object that may be used to
            allocate raw space associated to the element being inserted, or to alter the element in some way.
			\n Call the member function commit on the returned transaction in order to make the effects observable.
			If the transaction is destroyed before commit has been called, the transaction is canceled and it has no observable effects.

            @param i_type type of the new element.
            @param i_source pointer to the subobject of type COMMON_TYPE of an object or subobject of type ELEMENT_TYPE.
                <i>Note</i>: be careful using void pointers: casts from\to a base to\from a derived class can be done only
                by the type system of the language.
            @return A transaction object which can be used to allocate raw space and commit the transaction.

            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: no iterator is invalidated
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue start_dyn_push_copy example 1 */
        put_transaction start_dyn_push_move(const runtime_type & i_type, COMMON_TYPE * i_source)
        {
			auto push_data = implace_allocate<0, true>(i_type.size(), i_type.alignment());

			COMMON_TYPE * element = nullptr;
			runtime_type * type = nullptr;			
			try
			{
				auto const type_storage = type_after_control(push_data.m_control_block);
				DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
				type = new (type_storage) runtime_type(i_type);
				
				DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
				element = i_type.move_construct(push_data.m_user_storage, i_source);
			}
			catch (...)
			{
				if (type != nullptr)
					type->RUNTIME_TYPE::~RUNTIME_TYPE();
				DENSITY_ASSERT_INTERNAL((push_data.m_control_block->m_next & (detail::Queue_Busy | detail::Queue_Dead)) == 0);
				push_data.m_control_block->m_next += detail::Queue_Dead;
				throw;
			}

            return put_transaction(this, push_data, std::is_same<COMMON_TYPE, void>(), element);
        }

        /** Move-only class that holds the state of a put transaction, or is empty.

            Transactional put functions on heterogeneous_queue return a put_transaction that can be used to 
			allocate raw memory in the queue, inspect or alter the element, and commit the push. Move operations transfer 
			the transaction state to the destination, leaving the source in the empty state. A committed transaction is in 
			the empty state.
			If an operative function (like raw_allocate or commit) is called on an empty transaction, the behavior is undefined. */
        class put_transaction
        {
        public:

            /** Copy construction is not allowed */
            put_transaction(const put_transaction &) = delete;

            /** Copy assignment is not allowed */
            put_transaction & operator = (const put_transaction &) = delete;

            /** Move constructs a put_transaction, transferring the state from the source.
                    @i_source source to move from. It becomes empty after the call. */
            put_transaction(put_transaction && i_source) noexcept
                : m_queue(i_source.m_queue), m_put_data(i_source.m_put_data)
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
                    m_put_data = i_source.m_put_data;
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
                @param i_alignment alignment of the block to allocate. It must be a non-zero power of 2, and less than or
						equal to i_size.

                \pre The behavior is undefined if either:
					- this transaction is empty
					- the alignment is not valid
					- the size is not a multiple of the alignment

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: unspecified.
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
            void * raw_allocate(size_t i_size, size_t i_alignment)
            {
                DENSITY_ASSERT(!empty());

				auto push_data = m_queue->implace_allocate<detail::Queue_Dead, false>(i_size, i_alignment);
                return push_data.m_user_storage;
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
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
            template <typename INPUT_ITERATOR>
                typename std::iterator_traits<INPUT_ITERATOR>::value_type *
                    raw_allocate_copy(INPUT_ITERATOR i_begin, INPUT_ITERATOR i_end)
            {
				using DiffType = typename std::iterator_traits<INPUT_ITERATOR>::difference_type;
                using ValueType = typename std::iterator_traits<INPUT_ITERATOR>::value_type;
                static_assert(std::is_trivially_destructible<ValueType>::value,
                    "put_transaction provides a raw memory inplace allocation that does not invoke destructors when deallocating");

                auto const count_s = std::distance(i_begin, i_end);
                auto const count = static_cast<size_t>(count_s);
                DENSITY_ASSERT(static_cast<DiffType>(count) == count_s);

                auto const elements = static_cast<ValueType*>(raw_allocate(sizeof(ValueType), alignof(ValueType)));
				for (auto curr = elements; i_begin != i_end; ++i_begin, ++curr)
                    new(curr) ValueType(*i_begin);
                return elements;
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
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
            template <typename INPUT_RANGE>
                auto raw_allocate_copy(const INPUT_RANGE & i_source_range)
                    -> decltype(raw_allocate_copy(std::begin(i_source_range), std::end(i_source_range)))
            {
                return raw_allocate_copy(std::begin(i_source_range), std::end(i_source_range));
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
				m_queue = nullptr;
            }

			/** Cancel the transaction. This object becomes empty.

				\pre The behavior is undefined if either:
					- this transaction is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. */
            void cancel() noexcept
            {
                DENSITY_ASSERT(!empty());
				cancel_put_impl(m_put_data.m_control_block);
				m_queue = nullptr;
            }

            /** Returns true whether this object does not hold the state of a transaction. */
            bool empty() const noexcept { return m_queue == nullptr; }

            /** Returns a pointer to the object being added.
                \n <i>Note</i>: the object is constructed at the begin of the transaction, so this
                    function always returns a pointer to a valid object.

				\pre The behavior is undefined if either:
					- this transaction is empty */
            COMMON_TYPE * element_ptr() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return m_put_data.m_user_storage;
            }

            /** Returns the type of the object being added.

				\pre The behavior is undefined if either:
					- this transaction is empty */
            const RUNTIME_TYPE & type() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return *type_after_control(m_put_data.m_control);
            }

            /** If this transaction is empty the destructor has no side effects. Otherwise it cancels it. */
            ~put_transaction()
            {
                if (m_queue != nullptr)
                {
					cancel_put_impl(m_put_data.m_control_block);
                }
            }

            #ifndef DOXYGEN_DOC_GENERATION

                // internal only - do not use
                put_transaction(heterogeneous_queue * i_queue, Block i_push_data, std::true_type, void *) noexcept
                    : m_queue(i_queue), m_put_data(i_push_data)
                        { }

                // internal only - do not use
                put_transaction(heterogeneous_queue * i_queue, Block i_push_data, std::false_type, COMMON_TYPE * i_element_storage) noexcept
                    : m_queue(i_queue), m_put_data(i_push_data)
                {
                    m_put_data.m_user_storage = i_element_storage;
                    m_put_data.m_control_block->m_element = i_element_storage;
                }

            #endif // #ifndef DOXYGEN_DOC_GENERATION

        private:
            heterogeneous_queue * m_queue;
            Block m_put_data;
        };


		/** This class is used as return type from put functions with the element type known at compile time.
			
			typed_put_transaction derives from put_transaction adding just an element_ptr() that returns a
			pointer of correct the type. */
        template <typename TYPE>
            class typed_put_transaction : public put_transaction
        {
        public:

            using put_transaction::put_transaction;

            /** Returns a reference to the element being added. This function can be used to modify the element 
					before calling the commit.
                \n <i>Note</i>: An element is observable in the queue only after commit has been called on the
					put_transaction. The element is constructed at the begin of the transaction, so the
					returned object is always valid.

				\pre The behavior is undefined if either:
					- this transaction is empty */
            TYPE & element() const noexcept
                { return *static_cast<TYPE *>(put_transaction::element_ptr()); }
        };

        /** Move-only class that holds the state of a consume operation, or is empty. */
        class consume_operation
        {
        public:

            /** Copy construction is not allowed */
            consume_operation(const consume_operation &) = delete;

            /** Copy assignment is not allowed */
            consume_operation & operator = (const consume_operation &) = delete;

            /** Move constructor. The source is left empty. */
            consume_operation(consume_operation && i_source) noexcept
                : m_queue(i_source.m_queue), m_control(i_source.m_control)
            {
                i_source.m_control = nullptr;
            }

            /** Move assignment. The source is left empty. */
            consume_operation & operator = (consume_operation && i_source) noexcept
            {
                if (this != &i_source)
                {
                    m_queue = i_source.m_queue;
                    m_control = i_source.m_control;
                    i_source.m_control = nullptr;
                }
                return *this;
            }

            /** Destructor: cancel the operation (if any) */
            ~consume_operation()
            {
                if(m_control != nullptr)
                {
					m_queue->cancel_consume_impl(m_control);
                }
            }

            /** Returns true whether this object does not hold the state of an operation. */
            bool empty() const noexcept { return m_control == nullptr; }

            /** Returns true whether this object does not hold the state of an operation. Same to !consume_operation::empty. */
            explicit operator bool() const noexcept
            {
                return m_control != nullptr;
            }

            /** Destroys the element, and makes the effects of the operation observable. This object becomes empty.

				\pre The behavior is undefined if either:
					- this object is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. */
            void commit() noexcept
            {
				DENSITY_ASSERT(!empty());

				auto const & type = complete_type();
				auto const element = element_ptr();
				type.destroy(element);

				type.RUNTIME_TYPE::~RUNTIME_TYPE();
                
				m_queue->commit_consume_impl(m_control);
				m_control = nullptr;
            }

            /** Makes the effects of the operation observable without destroying the element. 
				The caller should destroy the element before calling this function.
				This object becomes empty.

				\pre The behavior is undefined if either:
					- this object is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. */
            void commit_nodestroy() noexcept
            {
				DENSITY_ASSERT(!empty());

				auto const & type = complete_type();
				type.RUNTIME_TYPE::~RUNTIME_TYPE();

				m_queue->commit_consume_impl(m_control);
				m_control = nullptr;
            }

			 /** Cancel the operation. This object becomes empty.

				\pre The behavior is undefined if either:
					- this object is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. */
            void cancel() noexcept
            {
                DENSITY_ASSERT(!empty());

				m_queue->cancel_consume_impl(m_control);
				m_control = nullptr;
            }

            /** Returns the type of the element being consumed.

                \pre The behavior is undefined if this consume_operation is empty, that is it has been used as source for a move operation. */
            const RUNTIME_TYPE & complete_type() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return *type_after_control(m_control);
            }

            /** Returns a pointer that, if properly aligned to the alignment of the element type, points to the element.
                \n This call is equivalent to: address_upper_align(unaligned_element_ptr(), complete_type().alignment());

                \pre The behavior is undefined if this consume_operation is empty, that is it has been used as source for a move operation.
                \pos The returned address is aligned at least on heterogeneous_queue::min_alignment. */
            void * unaligned_element_ptr() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return get_unaligned_element(m_control);
            }

            /** Returns a pointer to the element being consumed.

                \pre The behavior is undefined if this consume_operation is empty, that is it has been committed or used as source for a move operation. */
            COMMON_TYPE * element_ptr() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return get_element(m_control);
            }

			/** Returns a reference to the element being consumed.

                \pre The behavior is undefined if this consume_operation is empty, that is it has been committed or used as source for a move operation.
				\pre The behavior is undefined if COMPLETE_ELEMENT_TYPE is not exactly the complete type of the element. */
            template <typename COMPLETE_ELEMENT_TYPE>
				COMPLETE_ELEMENT_TYPE & element() const noexcept
            {
                DENSITY_ASSERT(!empty() && complete_type() == runtime_type::template make<COMPLETE_ELEMENT_TYPE>());
                return *static_cast<COMPLETE_ELEMENT_TYPE*>(get_element(m_control));
            }

            #ifndef DOXYGEN_DOC_GENERATION

                // internal only - do not use
                consume_operation(heterogeneous_queue * i_queue, ControlBlock * i_control) noexcept
                    : m_queue(i_queue), m_control(i_control)
                {
                }

				bool start_consume(heterogeneous_queue * i_queue)
				{
					if (m_control != nullptr)
					{
						m_queue->cancel_consume_impl(m_control);
					}
					m_queue = i_queue;
					m_control = i_queue->start_consume_impl();
					return m_control != nullptr;
				}

            #endif // #ifndef DOXYGEN_DOC_GENERATION

        private:
            heterogeneous_queue * m_queue;
            ControlBlock * m_control;
        };

        /** Removes and destroy the first element of the queue.

            \pre The behavior is undefined if the queue is empty.

            <b>Complexity</b>: constant
            \n <b>Effects on iterators</b>: any iterator pointing to the first element is invalidated
            \n <b>Throws</b>: nothing */
        void pop() noexcept
        {
			try_start_consume().commit();
        }

        /** Removes and destroy the first element of the queue, if the queue is not empty. Otherwise it has no effect.

            @return whether an element was actually removed.

            \pre The behavior is undefined if the queue is empty.
            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: any iterator pointing to the first element is invalidated
            \n <b>Throws</b>: nothing */
        bool pop_if_any() noexcept
        {
            auto operation = try_start_consume();
            if (operation)
            {
                operation.commit();
                return true;
            }
            return false;
        }

        /** Calls a user provided function object passing as parameters the first element of the queue and its type, then removes and
            destroys the element.

            @param i_func funcion object to be invoked with  const RUNTIME_TYPE & i_type, COMMON_TYPE * i_element
            @return return value of i_func(std::declval<RUNTIME_TYPE>(), std::declval<COMMON_TYPE *>())

            \pre The behavior is undefined if the queue is empty.

            <b>Complexity</b>: Constant
            \n <b>Effects on iterators</b>: any iterator pointing to the first element is invalidated
            \n <b>Throws</b>: anything thrown by i_func.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue consume example 1
        */
        template <typename FUNC>
            auto consume(FUNC && i_func)
                noexcept(noexcept(i_func(std::declval<const RUNTIME_TYPE &>(), std::declval<COMMON_TYPE *>())))
                    -> unvoid_t<decltype(i_func(std::declval<const RUNTIME_TYPE &>(), std::declval<COMMON_TYPE *>()))>
        {
            using ReturnType = decltype(i_func(std::declval<const RUNTIME_TYPE &>(), std::declval<COMMON_TYPE *>()));
            return consume_impl(std::forward<FUNC>(i_func), std::is_void<ReturnType>());
        }

        /**
            const RUNTIME_TYPE & i_type, COMMON_TYPE * i_element

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue consume_if_any example 1

        */
        template <typename FUNC>
            auto consume_if_any(FUNC && i_func)
                noexcept(noexcept((i_func(std::declval<const RUNTIME_TYPE>(), std::declval<COMMON_TYPE *>()))))
                    -> optional<unvoid_t<decltype(i_func(std::declval<const RUNTIME_TYPE &>(), std::declval<COMMON_TYPE *>()))>>
        {
            using ReturnType = decltype(i_func(std::declval<const RUNTIME_TYPE &>(), std::declval<COMMON_TYPE *>()));
            return consume_if_any_impl(std::forward<FUNC>(i_func), std::is_void<ReturnType>());
        }


		bool try_start_consume(consume_operation & i_consume) noexcept
        {
            return i_consume.start_consume(this);
        }

        consume_operation try_start_consume() noexcept
        {
            return consume_operation(this, start_consume_impl());
        }


                    // reentrant

        /** Same to heterogeneous_queue::push, but allow reentrancy: during the construction of the element the queue is in a
            valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue reentrant_push example 1 */
        template <typename ELEMENT_TYPE>
            void reentrant_push(ELEMENT_TYPE && i_source)
        {
            return reentrant_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Same to heterogeneous_queue::emplace, but allow reentrancy: during the construction of the element the queue is in a
            valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue reentrant_emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            void reentrant_emplace(CONSTRUCTION_PARAMS &&... i_construction_params)
        {
            start_reentrant_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...).commit();
        }

        /** Same to heterogeneous_queue::dyn_push, but allow reentrancy: during the construction of the element the queue is in a
            valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue reentrant_dyn_push example 1 */
        void reentrant_dyn_push(const runtime_type & i_type)
        {
            start_reentrant_dyn_push(i_type).commit();
        }

        /** Same to heterogeneous_queue::dyn_push_copy, but allow reentrancy: during the construction of the element the queue is in a
            valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue reentrant_dyn_push_copy example 1 */
        void reentrant_dyn_push_copy(const runtime_type & i_type, const COMMON_TYPE * i_source)
        {
            start_reentrant_dyn_push_copy(i_type, i_source).commit();
        }

        /** Same to heterogeneous_queue::dyn_push_move, but allow reentrancy: during the construction of the element the queue is in a
            valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue reentrant_dyn_push_move example 1 */
        void reentrant_dyn_push_move(const runtime_type & i_type, COMMON_TYPE * i_source)
        {
            start_reentrant_dyn_push_move(i_type, i_source).commit();
        }

        /** Same to heterogeneous_queue::start_push, but allow reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue start_reentrant_push example 1 */
        template <typename ELEMENT_TYPE>
            reentrant_typed_put_transaction<ELEMENT_TYPE> start_reentrant_push(ELEMENT_TYPE && i_source)
        {
            return start_reentrant_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Same to heterogeneous_queue::start_emplace, but allow reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue start_reentrant_emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            reentrant_typed_put_transaction<ELEMENT_TYPE> start_reentrant_emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
            static_assert(std::is_convertible<ELEMENT_TYPE*, COMMON_TYPE*>::value,
                "ELEMENT_TYPE must derive from COMMON_TYPE, or COMMON_TYPE must be void");
			
			auto push_data = implace_allocate<detail::Queue_Busy, true, sizeof(ELEMENT_TYPE), alignof(ELEMENT_TYPE)>();

			COMMON_TYPE * element = nullptr;
			runtime_type * type = nullptr; 
			try
			{
				auto const type_storage = type_after_control(push_data.m_control_block);
				DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
				type = new (type_storage) runtime_type(runtime_type::template make<ELEMENT_TYPE>());

				DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
				element = new (push_data.m_user_storage) ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
			}
			catch (...)
			{
				if (type != nullptr)
					type->RUNTIME_TYPE::~RUNTIME_TYPE();
				DENSITY_ASSERT_INTERNAL((push_data.m_control_block->m_next & (detail::Queue_Busy | detail::Queue_Dead)) == detail::Queue_Busy);
				push_data.m_control_block->m_next += detail::Queue_Dead - detail::Queue_Busy;
				throw;
			}

            return reentrant_typed_put_transaction<ELEMENT_TYPE>(this, push_data, std::is_void<COMMON_TYPE>(), element);
        }

        /** Same to heterogeneous_queue::start_dyn_push, but allow reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue start_reentrant_dyn_push example 1 */
        reentrant_put_transaction start_reentrant_dyn_push(const runtime_type & i_type)
        {
			auto push_data = implace_allocate<detail::Queue_Busy, true>(i_type.size(), i_type.alignment());

			COMMON_TYPE * element = nullptr;
			runtime_type * type = nullptr;
			try
			{
				auto const type_storage = type_after_control(push_data.m_control_block);
				DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
				type = new (type_storage) runtime_type(i_type);
				
				DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
				element = i_type.default_construct(push_data.m_user_storage);
			}
			catch (...)
			{
				if (type != nullptr)
					type->RUNTIME_TYPE::~RUNTIME_TYPE();
				DENSITY_ASSERT_INTERNAL((push_data.m_control_block->m_next & (detail::Queue_Busy | detail::Queue_Dead)) == 0);
				push_data.m_control_block->m_next += detail::Queue_Dead - detail::Queue_Busy;
				throw;
			}

            return reentrant_put_transaction(this, push_data, std::is_void<COMMON_TYPE>(), element);
        }


        /** Same to heterogeneous_queue::start_dyn_push_copy, but allow reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue start_reentrant_dyn_push_copy example 1 */
        reentrant_put_transaction start_reentrant_dyn_push_copy(const runtime_type & i_type, const COMMON_TYPE * i_source)
        {
			auto push_data = implace_allocate<detail::Queue_Busy, true>(i_type.size(), i_type.alignment());

			COMMON_TYPE * element = nullptr;
			runtime_type * type = nullptr;
			try
			{
				auto const type_storage = type_after_control(push_data.m_control_block);
				DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
				type = new (type_storage) runtime_type(i_type);
				
				DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
				element = i_type.copy_construct(push_data.m_user_storage, i_source);
			}
			catch (...)
			{
				if (type != nullptr)
					type->RUNTIME_TYPE::~RUNTIME_TYPE();
				DENSITY_ASSERT_INTERNAL((push_data.m_control_block->m_next & (detail::Queue_Busy | detail::Queue_Dead)) == 0);
				push_data.m_control_block->m_next += detail::Queue_Dead - detail::Queue_Busy;
				throw;
			}

            return reentrant_put_transaction(this, push_data, std::is_void<COMMON_TYPE>(), element);
        }

        /** Same to heterogeneous_queue::start_dyn_push_move, but allow reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet old_heterogeneous_queue_samples.cpp heter_queue start_reentrant_dyn_push_move example 1 */
        reentrant_put_transaction start_reentrant_dyn_push_move(const runtime_type & i_type, COMMON_TYPE * i_source)
        {
			auto push_data = implace_allocate<detail::Queue_Busy, true>(i_type.size(), i_type.alignment());

			COMMON_TYPE * element = nullptr;
			runtime_type * type = nullptr;
			try
			{
				auto const type_storage = type_after_control(push_data.m_control_block);
				DENSITY_ASSERT_INTERNAL(type_storage != nullptr);
				type = new (type_storage) runtime_type(i_type);
				
				DENSITY_ASSERT_INTERNAL(push_data.m_user_storage != nullptr);
				element = i_type.move_construct(push_data.m_user_storage, i_source);
			}
			catch (...)
			{
				if (type != nullptr)
					type->RUNTIME_TYPE::~RUNTIME_TYPE();
				DENSITY_ASSERT_INTERNAL((push_data.m_control_block->m_next & (detail::Queue_Busy | detail::Queue_Dead)) == 0);
				push_data.m_control_block->m_next += detail::Queue_Dead - detail::Queue_Busy;
				throw;
			}

            return reentrant_put_transaction(this, push_data, std::is_void<COMMON_TYPE>(), element);
        }

        /** Move-only class that holds the state of a put transaction, or is empty.

            Transactional put functions on heterogeneous_queue return a reentrant_put_transaction that can be used to allocate raw memory in the queue,
            inspect or alter the element, and commit the push. Move operations transfer the transaction state to the destination, leaving the source
            in the empty state. If an operative function (like raw_allocate or commit) is called on an empty transaction, the
            behavior is undefined. */
        class reentrant_put_transaction
        {
        public:

            /** Copy construction is not allowed */
            reentrant_put_transaction(const reentrant_put_transaction &) = delete;

            /** Copy assignment is not allowed */
            reentrant_put_transaction & operator = (const reentrant_put_transaction &) = delete;

            /** Move constructs a reentrant_put_transaction, transferring the state from the source.
                    @i_source source to move from. It becomes empty after the call. */
            reentrant_put_transaction(reentrant_put_transaction && i_source) noexcept
                : m_queue(i_source.m_queue), m_put_data(i_source.m_put_data)
            {
                i_source.m_queue = nullptr;
            }

            /** Move assigns a reentrant_put_transaction, transferring the state from the source.
                @i_source source to move from. It becomes empty after the call. */
            reentrant_put_transaction & operator = (reentrant_put_transaction && i_source) noexcept
            {
                if (this != &i_source)
                {
                    m_queue = i_source.m_queue;
                    m_put_data = i_source.m_put_data;
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

                \pre The behavior is undefined if either:
					- this reentrant_put_transaction is empty
					- the alignment is not valid
					- the size is not a multiple of the alignment

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: unspecified.
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
            void * raw_allocate(size_t i_size, size_t i_alignment)
            {
                DENSITY_ASSERT(!empty());

				auto push_data = m_queue->implace_allocate<detail::Queue_Dead, false>(i_size, i_alignment);
                return push_data.m_user_storage;
            }

            /** Marks the state of the transaction so that when it is destroyed the element becomes visible to iterators and consumers.
                If the state of the transaction is not committed, it will never become visible.

				\pre The behavior is undefined if this transaction is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. */
            void commit() noexcept
            {
                DENSITY_ASSERT(!empty());
				commit_reentrant_put_impl(m_put_data.m_control_block);
				m_queue = nullptr;
            }

			/** Cancel the transaction. This object becomes empty.

				\pre The behavior is undefined if either:
					- this transaction is empty

                <b>Complexity</b>: Constant.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: Nothing. */
            void cancel() noexcept
            {
                DENSITY_ASSERT(!empty());
				cancel_reentrant_put_impl(m_put_data.m_control_block);
				m_queue = nullptr;
            }

            /** Returns true whether this object does not hold the state of a transaction. */
            bool empty() const noexcept { return m_queue == nullptr; }

			/** Returns the queue that created this transaction. 
			
			\pre The behavior is undefined if either:
				- this transaction is empty */
			heterogeneous_queue & queue() const noexcept 
			{
				DENSITY_ASSERT(!empty());
				return *m_queue; 
			}

            /** Returns a pointer to the object being added.
                \n <i>Note</i>: the object is constructed at the begin of the transaction, so this
                function always returns a pointer to a valid object.

                \pre The behavior is undefined if this reentrant_put_transaction is empty, that is it has been used as source for a move operation. */
            COMMON_TYPE * element_ptr() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return m_put_data.m_user_storage;
            }

            /** Returns the type of the object being added.

                \pre The behavior is undefined if this reentrant_put_transaction is empty, that is it has been used as source for a move operation. */
            const RUNTIME_TYPE & type() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return *type_after_control(m_put_data.m_control);
            }

            /** If this reentrant_put_transaction the destructor has no side effects. Otherwise it commits or cancels the transaction, depending
                on whether commit() has been called on not. */
            ~reentrant_put_transaction()
            {
                if (m_queue != nullptr)
                {
					cancel_reentrant_put_impl(m_put_data.m_control_block);
                }
            }

            #ifndef DOXYGEN_DOC_GENERATION

                // internal only - do not use
                reentrant_put_transaction(heterogeneous_queue * i_queue, Block i_push_data, std::true_type, void *) noexcept
                    : m_queue(i_queue), m_put_data(i_push_data)
                        { }

                // internal only - do not use
                reentrant_put_transaction(heterogeneous_queue * i_queue, Block i_push_data, std::false_type, COMMON_TYPE * i_element) noexcept
                    : m_queue(i_queue), m_put_data(i_push_data)
                {
                    m_put_data.m_user_storage = i_element;
                    m_put_data.m_control_block->m_element = i_element;
                }

            #endif // #ifndef DOXYGEN_DOC_GENERATION

        private:
            heterogeneous_queue * m_queue;
            Block m_put_data;
        };

        template <typename TYPE>
            class reentrant_typed_put_transaction : public reentrant_put_transaction
        {
        public:

            using reentrant_put_transaction::reentrant_put_transaction;

            TYPE * element_ptr() const noexcept
                { return static_cast<TYPE *>(reentrant_put_transaction::element_ptr()); }
        };


                    // iterator

        class iterator
        {
        public:

			using iterator_category = std::input_iterator_tag;
            using runtime_type = RUNTIME_TYPE;
            using common_type = COMMON_TYPE;
            using value_type = std::pair<const runtime_type &, common_type * const>;
            using pointer = value_type *;
            using const_pointer = const value_type *;
            using reference = value_type &;
            using const_reference = const value_type&;
            using size_type = std::size_t;
            using difference_type = std::ptrdiff_t;

            iterator() noexcept = default;
            iterator(const iterator & i_source) noexcept = default;
            iterator & operator = (const iterator & i_source) noexcept = default;

            iterator(heterogeneous_queue * i_queue, ControlBlock * i_control) noexcept
                : m_control(i_control), m_queue(i_queue)
                    { }

            /** Returns a reference to the subobject of type COMMON_TYPE of current element. If COMMON_TYPE is void this function is useless, because the return type is void. */
            value_type operator * () const noexcept { return value_type(*type_after_control(m_control), get_element(m_control)); }

			const value_type * operator -> () const noexcept
			{
				return std::pointer_traits<const value_type *>::pointer_to(**this);
			}

            /** Returns a pointer to the subobject of type COMMON_TYPE of current element */
            common_type * element_ptr() const noexcept { return static_cast<value_type *>(get_element(m_control)); }

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
            ControlBlock * m_control = nullptr;
            heterogeneous_queue * m_queue = nullptr;
        };

        iterator begin() noexcept { return iterator(this, first_valid(m_head)); }
        iterator end() noexcept { return iterator(); }

        class const_iterator
        {
        public:

            using iterator_category = std::input_iterator_tag;
            using runtime_type = RUNTIME_TYPE;
            using common_type = COMMON_TYPE;
            using value_type = const std::pair<const runtime_type &, const common_type* const>;
            using pointer = const value_type *;
            using const_pointer = const value_type *;
            using reference = const value_type &;
            using const_reference = const value_type&;
            using size_type = std::size_t;
            using difference_type = std::ptrdiff_t;

            const_iterator() noexcept = default;
            const_iterator(const const_iterator & i_source) noexcept = default;
            const_iterator & operator = (const const_iterator & i_source) noexcept = default;

            const_iterator(const heterogeneous_queue * i_queue, ControlBlock * i_control) noexcept
                : m_control(i_control), m_queue(i_queue)
                    { }

            /** Returns a reference to the subobject of type COMMON_TYPE of current element. If COMMON_TYPE is void this function is useless, because the return type is void. */
            value_type operator * () const noexcept { return value_type(*type_after_control(m_control), get_element(m_control)); }

			const value_type * operator->() const
			{
				return std::pointer_traits<pointer>::pointer_to(**this);
			}

            /** Returns a pointer to the subobject of type COMMON_TYPE of current element. If COMMON_TYPE is void, then the return type is void *. */
            const common_type * element_ptr() const noexcept { return get_element(m_control); }

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
            ControlBlock * m_control = nullptr;
            const heterogeneous_queue * m_queue = nullptr;
        };

        const_iterator begin() const noexcept { return const_iterator(this, first_valid(m_head)); }
        const_iterator end() const noexcept { return const_iterator(); }

        const_iterator cbegin() const noexcept { return const_iterator(this, first_valid(m_head)); }
        const_iterator cend() const noexcept { return const_iterator(); }

        bool operator == (const heterogeneous_queue & i_source) const
        {
            const auto end_1 = cend();
            for (auto it_1 = cbegin(), it_2 = i_source.cbegin(); it_1 != end_1; ++it_1, ++it_2)
            {
                if (it_1.complete_type() != it_2.complete_type() ||
                    !it_1.complete_type().are_equal(it_1.element_ptr(), it_2.element_ptr()))
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

        template <typename FUNC>
            auto consume_impl(FUNC && i_func, std::false_type)
                noexcept(noexcept(i_func(std::declval<const RUNTIME_TYPE &>(), std::declval<COMMON_TYPE *>())))
                    -> decltype(i_func(std::declval<const RUNTIME_TYPE &>(), std::declval<COMMON_TYPE *>()))
        {
            auto transaction = try_start_consume();
            DENSITY_ASSERT(static_cast<bool>(transaction));
            auto const & type = transaction.complete_type();
            auto const element = transaction.element_ptr();

            auto res = i_func(type, element);

            transaction.commit();
            return res;
        }

        template <typename FUNC>
            unvoid_t<void> consume_impl(FUNC && i_func, std::true_type)
                noexcept(noexcept(i_func(std::declval<const RUNTIME_TYPE &>(), std::declval<COMMON_TYPE *>())))
        {
            auto transaction = try_start_consume();
            DENSITY_ASSERT(static_cast<bool>(transaction));
            auto const & type = transaction.complete_type();
            auto const element = transaction.element_ptr();

            i_func(type, element);

            transaction.commit();

            return unvoid_t<void>();
        }

        template <typename FUNC>
            auto consume_if_any_impl(FUNC && i_func, std::false_type)
                noexcept(noexcept((i_func(std::declval<const RUNTIME_TYPE>(), std::declval<COMMON_TYPE *>()))))
                    -> optional<decltype(i_func(std::declval<const RUNTIME_TYPE &>(), std::declval<COMMON_TYPE *>()))>
        {
            using ReturnType = decltype(i_func(std::declval<const RUNTIME_TYPE &>(), std::declval<COMMON_TYPE *>()));
            auto transaction = try_start_consume();
            if (transaction)
            {
                auto const & type = transaction.complete_type();
                auto const element = transaction.element_ptr();
                auto result = make_optional<ReturnType>(i_func(type, element));
                transaction.commit();
                return result;
            }
            return optional<ReturnType>();
        }

        template <typename FUNC>
            optional<unvoid_t<void>> consume_if_any_impl(FUNC && i_func, std::true_type)
                noexcept(noexcept((i_func(std::declval<const RUNTIME_TYPE>(), std::declval<COMMON_TYPE *>()))))
        {
            auto transaction = try_start_consume();
            if (transaction)
            {
                auto const & type = transaction.complete_type();
                auto const element = transaction.element_ptr();
                i_func(type, element);
                transaction.commit();
                return make_optional<unvoid_t<void>>(unvoid_t<void>());
            }
            return optional<unvoid_t<void>>();
        }


        ControlBlock * first_valid(ControlBlock * i_from) const
        {
            for (auto curr = i_from; curr != m_tail; )
            {
                if ((curr->m_next & (detail::Queue_Busy | detail::Queue_Dead)) == 0)
                {
                    return curr;
                }
                curr = reinterpret_cast<ControlBlock*>(curr->m_next & ~detail::Queue_AllFlags);
            }
            return nullptr;
        }

        ControlBlock * next_valid(ControlBlock * i_from) const
        {
            DENSITY_ASSERT_INTERNAL(i_from != m_tail);
            for (auto curr = reinterpret_cast<ControlBlock*>(i_from->m_next & ~static_cast<uintptr_t>(3)); curr != m_tail; )
            {
                if ((curr->m_next & (detail::Queue_Busy | detail::Queue_Dead)) == 0)
                {
                    return curr;
                }
                curr = reinterpret_cast<ControlBlock*>(curr->m_next & ~detail::Queue_AllFlags);
            }
            return nullptr;
        }

        static runtime_type * type_after_control(ControlBlock * i_control) noexcept
        {
            return reinterpret_cast<runtime_type*>(address_add(i_control, s_sizeof_ControlBlock));
        }

        static void * get_unaligned_element(detail::QueueControl<void> * i_control) noexcept
        {
            auto result = address_add(i_control, s_sizeof_ControlBlock + s_sizeof_RuntimeType);
            if (i_control->m_next & detail::Queue_External)
            {
                result = static_cast<ExternalBlock*>(result)->m_element;
            }
            return result;
        }

        static void * get_element(detail::QueueControl<void> * i_control) noexcept
        {
            auto result = address_add(i_control, s_sizeof_ControlBlock + s_sizeof_RuntimeType);
            if (i_control->m_next & detail::Queue_External)
            {
                result = static_cast<ExternalBlock*>(result)->m_element;
            }
            else
            {
                result = address_upper_align(result, type_after_control(i_control)->alignment());
            }
            return result;
        }

        template <typename TYPE>
            static void * get_unaligned_element(detail::QueueControl<TYPE> * i_control) noexcept
        {
            return i_control->m_element;
        }

        template <typename TYPE>
            static TYPE * get_element(detail::QueueControl<TYPE> * i_control) noexcept
        {
            return i_control->m_element;
        }

		/** Returns whether the input addresses belongs to the same page or they are both nullptr */
		static bool same_page(const void * i_first, const void * i_second) noexcept
		{
			auto const page_mask = ALLOCATOR_TYPE::page_alignment - 1;
			return ((reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) & ~page_mask) == 0;
		}

        struct ExternalBlock
        {
            void * m_element;
            size_t m_size, m_alignment;
        }; 

		static void * get_end_of_page(void * i_address) noexcept
		{
			return address_add(address_lower_align(i_address, allocator_type::page_alignment), 
				allocator_type::page_size - s_sizeof_ControlBlock);
		}


		/** Allocates an element and its control block. 
			This function may throw anything the allocator may throw. */
        template <uintptr_t CONTROL_BITS, bool INCLUDE_TYPE>
            Block implace_allocate(size_t i_size, size_t i_alignment)
        {
			DENSITY_ASSERT_INTERNAL(is_power_of_2(i_alignment) && (i_size % i_alignment) == 0);
			DENSITY_ASSERT_INTERNAL(address_is_aligned(m_tail, min_alignment) || 
				m_tail == reinterpret_cast<ControlBlock*>(s_invalid_control_block));

            if (i_alignment < min_alignment)
            {
                i_alignment = min_alignment;
                i_size = uint_upper_align(i_size, min_alignment);
            }

            for(;;)
            {
				// allocate space for the control block and the runtime type
                auto const control_block = m_tail;
                void * new_tail = address_add(control_block, 
					INCLUDE_TYPE ? (s_sizeof_ControlBlock + s_sizeof_RuntimeType) : s_sizeof_ControlBlock);

				// allocate space for the element
                new_tail = address_upper_align(new_tail, i_alignment);
                void * new_element = new_tail;
                new_tail = address_add(new_tail, i_size);

				// check if a page overflow would occur
				void * end_of_page = get_end_of_page(control_block);
                if (DENSITY_LIKELY(new_tail <= end_of_page))
                {
                    DENSITY_ASSERT_INTERNAL(control_block != nullptr);
                    new(control_block) ControlBlock();

                    control_block->m_next = reinterpret_cast<uintptr_t>(new_tail) + CONTROL_BITS;
                    m_tail = static_cast<ControlBlock *>(new_tail);
                    return Block{ control_block, new_element };
                }
				else if (i_size + (i_alignment - min_alignment) <= s_max_size_inpage)
				{
					// allocate a new page and redo
					allocate_new_page();
				}
				else
				{
					// this allocation would never fit in a page, allocate an external block
					return external_allocate<CONTROL_BITS>(i_size, i_alignment);
				}
            }
        }

		/** Overload of implace_allocate with fixed size and alignment */
        template <uintptr_t CONTROL_BITS, bool INCLUDE_TYPE, size_t SIZE, size_t ALIGNMENT>
            Block implace_allocate()
        {
			static_assert(is_power_of_2(ALIGNMENT) && (SIZE % ALIGNMENT) == 0, "");

			DENSITY_ASSERT_INTERNAL(address_is_aligned(m_tail, min_alignment) || 
				m_tail == reinterpret_cast<ControlBlock*>(s_invalid_control_block));

			constexpr auto alignment = detail::size_max(ALIGNMENT, min_alignment);
			constexpr auto size = uint_upper_align(SIZE, alignment);
			constexpr auto can_fit_in_a_page = size + (alignment - min_alignment) <= s_max_size_inpage;
			constexpr auto over_aligned = alignment > min_alignment;

            for(;;)
            {
				// allocate space for the control block and the runtime type
                auto const control_block = m_tail;
                void * new_tail = address_add(control_block, 
					INCLUDE_TYPE ? (s_sizeof_ControlBlock + s_sizeof_RuntimeType) : s_sizeof_ControlBlock);

				// allocate space for the element
				if (over_aligned)
				{
					new_tail = address_upper_align(new_tail, alignment);
				}
				DENSITY_ASSERT_INTERNAL(address_is_aligned(new_tail, min_alignment) || 
					m_tail == reinterpret_cast<ControlBlock*>(s_invalid_control_block));
				void * new_element = new_tail;
                new_tail = address_add(new_tail, size);

				// check if a page overflow would occur
				void * end_of_page = get_end_of_page(control_block);
                if (DENSITY_LIKELY(new_tail <= end_of_page))
                {
                    DENSITY_ASSERT_INTERNAL(control_block != nullptr);
                    new(control_block) ControlBlock();

                    control_block->m_next = reinterpret_cast<uintptr_t>(new_tail) + CONTROL_BITS;
                    m_tail = static_cast<ControlBlock *>(new_tail);
                    return Block{ control_block, new_element };
                }
				else if (can_fit_in_a_page)
				{
					// allocate a new page and redo
					allocate_new_page();
				}
				else
				{
					// this allocation would never fit in a page, allocate an external block
					return external_allocate<CONTROL_BITS>(size, alignment);
				}
            }
        }

        template <uintptr_t CONTROL_BITS>
            Block external_allocate(size_t i_size, size_t i_alignment)
        {
            auto const external_block = ALLOCATOR_TYPE::allocate(i_size, i_alignment);

			try
			{
				/* external blocks always allocate space for the type, because it would be complicated 
					for the consumers to handle both cases*/
				auto const inplace_put = implace_allocate<CONTROL_BITS, true>(sizeof(ExternalBlock), alignof(ExternalBlock));

				new(inplace_put.m_user_storage) ExternalBlock{external_block, i_size, i_alignment};

				DENSITY_ASSERT((inplace_put.m_control_block->m_next & detail::Queue_External) == 0);
				inplace_put.m_control_block->m_next |= detail::Queue_External;
				return Block{ inplace_put.m_control_block, external_block };
			}
			catch (...)
			{
				/* if implace_allocate fails, that means that we were able to allocate the external block,
					but we were not able to put the struct ExternalBlock in the page (because a new page was
					necessary, but we could not allocate it). */
				ALLOCATOR_TYPE::deallocate(external_block, i_size, i_alignment);
				throw;
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
                control_block->m_next = reinterpret_cast<uintptr_t>(new_page) + detail::Queue_Dead;
                m_tail = static_cast<ControlBlock *>(new_page);
            }
            else
            {
                // this happens only on a virgin queue
                m_tail = m_head = static_cast<ControlBlock*>(allocator_type::allocate_page());
            }
        }

        DENSITY_NO_INLINE static void cancel_put_impl(ControlBlock * i_control_block)
        {
            const auto type_ptr = type_after_control(i_control_block);
            type_ptr->destroy(get_element(i_control_block));

			type_ptr->RUNTIME_TYPE::~RUNTIME_TYPE();

            DENSITY_ASSERT_INTERNAL((i_control_block->m_next & (detail::Queue_Busy | detail::Queue_Dead)) == 0);
            i_control_block->m_next += detail::Queue_Dead;
        }

        static void commit_reentrant_put_impl(ControlBlock * i_control_block)
        {
            DENSITY_ASSERT_INTERNAL((i_control_block->m_next & (detail::Queue_Busy | detail::Queue_Dead)) == detail::Queue_Busy);
            i_control_block->m_next -= detail::Queue_Busy;
        }

        DENSITY_NO_INLINE static void cancel_reentrant_put_impl(ControlBlock * i_control_block)
        {
            const auto type_ptr = type_after_control(i_control_block);
            type_ptr->destroy(get_element(i_control_block));

            DENSITY_ASSERT_INTERNAL((i_control_block->m_next & (detail::Queue_Busy | detail::Queue_Dead)) == detail::Queue_Busy);
            i_control_block->m_next += (detail::Queue_Dead - detail::Queue_Busy);
        }

        ControlBlock * start_consume_impl() noexcept
        {
            auto curr = m_head;
            while (curr != m_tail)
            {
                if ((curr->m_next & (detail::Queue_Busy | detail::Queue_Dead)) == 0)
                {
                    curr->m_next += detail::Queue_Busy;
                    return curr;
                }

                curr = reinterpret_cast<ControlBlock*>(curr->m_next & ~detail::Queue_AllFlags);
            }

            return nullptr;
        }

        void commit_consume_impl(ControlBlock * i_control_block) noexcept
        {
			DENSITY_ASSERT_INTERNAL((i_control_block->m_next & (detail::Queue_Busy | detail::Queue_Dead)) == detail::Queue_Busy);
            i_control_block->m_next += (detail::Queue_Dead - detail::Queue_Busy);

            auto curr = m_head;
			while (curr != m_tail)
			{
				// break if the current block is busy or is not dead
                if ((curr->m_next & (detail::Queue_Busy | detail::Queue_Dead)) != detail::Queue_Dead)
                {
                    break;
                }

                auto next = reinterpret_cast<ControlBlock*>(curr->m_next & ~detail::Queue_AllFlags);
                if (curr->m_next & detail::Queue_External)
                {
                    auto result = address_add(curr, s_sizeof_ControlBlock + s_sizeof_RuntimeType);
                    const auto & block = *static_cast<ExternalBlock*>(result);
                    ALLOCATOR_TYPE::deallocate(block.m_element, block.m_size, block.m_alignment);
                }

                if (!same_page(next, curr))
                {
                    allocator_type::deallocate_page(curr);
                }

                curr = next;
            }

			DENSITY_ASSERT_INTERNAL(curr == m_tail || 
				(curr->m_next & (detail::Queue_Busy | detail::Queue_Dead)) != detail::Queue_Dead);
            m_head = curr;
        }

        void cancel_consume_impl(ControlBlock * i_control_block) noexcept
        {
			DENSITY_ASSERT_INTERNAL((i_control_block->m_next & (detail::Queue_AllFlags - detail::Queue_External)) == detail::Queue_Busy);
			i_control_block->m_next -= detail::Queue_Busy;
        }

        void destroy() noexcept
        {
            clear();
            DENSITY_ASSERT_INTERNAL(m_tail == m_head);
			if (m_head != reinterpret_cast<ControlBlock*>(s_invalid_control_block))
			{
				allocator_type::deallocate_page(m_head);
			}
        }
    };

} // namespace density
