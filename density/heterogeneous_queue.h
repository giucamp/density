
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
        template<typename COMMON_TYPE> struct QueueControl // used by heterogeneous_queue<T,...>
        {
            uintptr_t m_next;
            COMMON_TYPE * m_element;
        };

        template<> struct QueueControl<void> // used by heterogeneous_queue<void,...>
        {
            uintptr_t m_next;
        };
    }

    /** \brief Heterogeneous FIFO container-like class template. 	
		
		Heterogeneous FIFO container-like class template. Since this pseudo-container is heterogeneous, each element can have 
		a different type. A type-eraser is associated to each element to allow type-specific operations. Insertions (put operations)
		are possible only at the end of the queue. Removals (consumptions) are possible only at the beginning of the queue.

		@tparam COMMON_TYPE Common type. An element of type E can be pushed on the queue only if E* is implicitly convertible
            to COMMON_TYPE*. COMMON_TYPE may be:
                - void (the default). In this case elements of any type can be added to the queue.
                - a class\\struct. In this case only elements of a type deriving from COMMON_TYPE can be added.
        @tparam TYPE_ERASER_TYPE Type to be used to handle at runtime the actual complete type of each element.
                This type must meet the requirements of \ref RuntimeType_concept "RuntimeType". The default is runtime_type.
        @tparam ALLOCATOR_TYPE Allocator type to be used. This type must meet the requirements of both \ref UntypedAllocator_concept
                "UntypedAllocator" and \ref PagedAllocator_concept "PagedAllocator". The default is void_allocator.

		\n <b>Thread safeness</b>: None. The user is responsible to avoid data races.
		\n <b>Exception safeness</b>: Any function of heterogeneous_queue is noexcept or provides the strong exception guarantee.
		
		Usage
		--------------------------
		An element is an object of any (non cv-qualified) type T such that T* is implicitly convertible to COMMON_TYPE*. 
		
		A value of the queue is an std::pair of a reference to a type eraser and a pointer to the element. 

		When an element of a type known at compile time is added to the queue, its type eraser is deduced:

		\snippet heter_queue_samples.cpp heter_queue example 1

		The type of the element may be unknown at compile time, in which case \ref dyn_push_copy can be used:

		\snippet heter_queue_samples.cpp heter_queue example 2
						
		Iterators don't provide the multipass guarantee, so they are not
		<a href="http://en.cppreference.com/w/cpp/concept/ForwardIterator">Forward Iterators</a>, but just
		<a href="http://en.cppreference.com/w/cpp/concept/InputIterator">Input Iterators</a>.
		For this reason heterogeneous_queue is not a container.

		Insertions invalidate no iterators. Removals invalidate iterators pointing to the element being removed. \n
		Past-the-end iterators are never invalidated. They compare equal each other and with a default constructed iterator:

		\snippet heter_queue_samples.cpp heter_queue iterators 1		

		Reentrant operations
		--------------------------
		Member function containing "reentrant_" in their names support reentrancy: while they are in progress, other puts, consumes, 
		iterations and any non-modifying operation are allowed. These operations are allowed only in the same thread (reentrancy has 
		nothing to do with multithreading). \n
		In contrast, while a non-reentrant operation is in progress, the queue is not in a consistent state: if during a put the 
		construction of the new element directly or indirectly calls other member functions on the same queue, the behavior is undefined.		

		Transactional puts and consume operations
		--------------------------
		Put member functions containing "start_" in their names are transactional: they start a transaction and return an object 
		bound to it. The effects of the transaction are not observable until the member function commit is called on
		it. If the transaction is destroyed and commit has not been called, the transaction is canceled, with no effect ever 
		observable (memory allocations are not considered an observable effect to the queue). \n
		Consume member functions containing "start_" start an operation and return an object bound to it. The member function commit
		makes the operation persistent. If the operation object is destroyed without being committed, the operation is canceled.
		The operation object is not a transaction, as the element disappears from the queue until commit is called.	
		
		\snippet heter_queue_samples.cpp heter_queue start_push example 1
		
		In the following sample transactional puts and reentrancy are used together.

		\snippet heter_queue_samples.cpp heter_queue put_transaction example 1

		If a transaction or a consume operation is in progress (not yet committed) on a queue used as destination of an assignment 
		or being destroyed, the behavior is undefined.

		Put functions summary
		--------------------------
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
            <td>Runtime time</td>
            <td>Default</td>
        </tr>
        <tr>
            <td>[start_][reentrant_]dyn_push_copy</td>
            <td>@code dyn_push_copy, reentrant_dyn_push_copy, start_dyn_push_copy, start_reentrant_dyn_push_copy @endcode</td>
            <td>Runtime time</td>
            <td>Copy</td>
        </tr>
        <tr>
            <td>[start_][reentrant_]dyn_push_move</td>
            <td>@code dyn_push_move, reentrant_dyn_push_move, start_dyn_push_move, start_reentrant_dyn_push_move @endcode</td>
            <td>Runtime time</td>
            <td>Move</td>
        </tr>
        </table>

		Implementation and performance notes
		--------------------------
		Elements and type-eraser are allocated linearly and tightly in memory pages allocated by the provided allocator.
		Whenever an element would not fit (alone) in a page, a normal memory block is requested to the allocator and used for the storage.
		Elements are never reallocated or moved, so insertions and removals have always strict constant complexity.
		Pages are not recycled: when the last element in a page is consumed, the page is freed.		

		- If COMMON_TYPE is not void for every element the queue stores an additional pointer for every element. It's 
			recommended to use void as COMMON_TYPE.
		- In general non-reentrant operations are faster than reentrant
		- Transactional operations are not slower than non-transactional ones

    */
    template < typename COMMON_TYPE = void, typename TYPE_ERASER_TYPE = runtime_type<COMMON_TYPE>, typename ALLOCATOR_TYPE = void_allocator >
        class heterogeneous_queue : private ALLOCATOR_TYPE
    {
    private:
        using ControlBlock = typename detail::QueueControl<COMMON_TYPE>;
        struct PutData
        {
            ControlBlock * m_control_block;
            void * m_element;
        };

    public:

        static_assert(std::is_same<COMMON_TYPE, typename TYPE_ERASER_TYPE::common_type>::value,
            "COMMON_TYPE and TYPE_ERASER_TYPE::common_type must be the same type (did you try to use a type like heter_cont<A,runtime_type<B>>?)");

        static_assert(std::is_same<COMMON_TYPE, typename std::decay<COMMON_TYPE>::type>::value,
            "COMMON_TYPE can't be cv-qualified, an array or a reference");

        static_assert(ALLOCATOR_TYPE::page_size > (sizeof(ControlBlock) + alignof(ControlBlock)) * 4 && ALLOCATOR_TYPE::page_alignment == ALLOCATOR_TYPE::page_size,
            "The size and alignment of the pages must be the same (and not too small)");
		        
        using common_type = COMMON_TYPE;
		using type_eraser_type = TYPE_ERASER_TYPE;
        using value_type = std::pair<const type_eraser_type &, common_type* const>;
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

        /** Minimum alignment used for the storage of the elements. The storage of elements is always aligned according to the most-derived type. */
        constexpr static size_t min_alignment = detail::size_max(8, detail::size_max(alignof(ControlBlock), alignof(type_eraser_type)));

        /** Default constructor. The allocator is default-constructed.

            <b>Complexity</b>: constant.
            \n <b>Throws</b>: unspecified.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).
            \n <i>Implementation notes</i>:
				-Currently this constructor does not allocate memory and never throws. */
        heterogeneous_queue()
            : m_head(reinterpret_cast<ControlBlock*>(s_invalid_control_block)),
              m_tail(reinterpret_cast<ControlBlock*>(s_invalid_control_block))
        {
        }

        /** Move constructor. The allocator is move-constructed from the one of the source.
                @param i_source source to move the elements from. After the call the source is left in some valid but indeterminate state.

            <b>Complexity</b>: constant.
            \n <b>Throws</b>: Nothing.
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
              m_head(reinterpret_cast<ControlBlock*>(s_invalid_control_block)), m_tail(reinterpret_cast<ControlBlock*>(s_invalid_control_block))
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

        /** Copy assignment. The allocator is move-assigned from the one of the source.
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

        /** Swaps the content of this queue and another one. The allocators are swapped too.
                @param i_source source to move the elements from. After the call the source is left in some valid but indeterminate state.

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
                auto transaction = start_consume();
                if (!transaction)
                {
                    break;
                }
                auto const element = transaction.element_ptr();
                auto const & type = transaction.complete_type();
                type.destroy(element);
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
            \snippet heter_queue_samples.cpp heter_queue push example 1 */
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
            \snippet heter_queue_samples.cpp heter_queue emplace example 1 */
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
            \snippet heter_queue_samples.cpp heter_queue dyn_push example 1 */
        void dyn_push(const type_eraser_type & i_type)
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
            \snippet heter_queue_samples.cpp heter_queue dyn_push_copy example 1 */
        void dyn_push_copy(const type_eraser_type & i_type, const COMMON_TYPE * i_source)
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
            \snippet heter_queue_samples.cpp heter_queue dyn_push_move example 1 */
        void dyn_push_move(const type_eraser_type & i_type, COMMON_TYPE * i_source)
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
            \snippet heter_queue_samples.cpp heter_queue start_push example 1 */
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
            \snippet heter_queue_samples.cpp heter_queue start_push example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            typed_put_transaction<ELEMENT_TYPE> start_emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
            static_assert(std::is_convertible<ELEMENT_TYPE*, COMMON_TYPE*>::value,
                "ELEMENT_TYPE must derive from COMMON_TYPE, or COMMON_TYPE must be void");

            auto const sa = adjust_alignment(detail::SizeAndAlignment{sizeof(ELEMENT_TYPE), alignof(ELEMENT_TYPE)}, min_alignment);
            auto push_data = sa.m_size <= s_max_size_inpage ?
                implace_allocate<0>(sa) : external_allocate<0>(sa);

            DENSITY_ASSERT_INTERNAL(push_data.m_control_block != nullptr && push_data.m_element != nullptr);
            new (type_after_control(push_data.m_control_block)) type_eraser_type(type_eraser_type::template make<ELEMENT_TYPE>());
            auto const element = new (push_data.m_element) ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);

            return typed_put_transaction<ELEMENT_TYPE>(this, push_data, std::is_same<COMMON_TYPE, void>(), element);
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
            \snippet heter_queue_samples.cpp heter_queue start_dyn_push example 1 */
        put_transaction start_dyn_push(const type_eraser_type & i_type)
        {
            const auto sa = adjust_alignment(detail::SizeAndAlignment{i_type.size(), i_type.alignment()}, min_alignment);
            auto push_data = sa.m_size <= s_max_size_inpage ?
                implace_allocate<0>(sa) : external_allocate<0>(sa);

            DENSITY_ASSERT_INTERNAL(push_data.m_control_block != nullptr && push_data.m_element != nullptr);
            new (type_after_control(push_data.m_control_block)) type_eraser_type(i_type);
            auto const element = i_type.default_construct(push_data.m_element);

            return put_transaction(this, push_data, std::is_same<COMMON_TYPE, void>(), element);
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
            \snippet heter_queue_samples.cpp heter_queue start_dyn_push_copy example 1 */
        put_transaction start_dyn_push_copy(const type_eraser_type & i_type, const COMMON_TYPE * i_source)
        {
            const auto sa = adjust_alignment(detail::SizeAndAlignment{ i_type.size(), i_type.alignment() }, min_alignment);
            auto push_data = sa.m_size <= s_max_size_inpage ?
                implace_allocate<0>(sa) : external_allocate<0>(sa);

            DENSITY_ASSERT_INTERNAL(push_data.m_control_block != nullptr && push_data.m_element != nullptr);
            new (type_after_control(push_data.m_control_block)) type_eraser_type(i_type);
            auto const element = i_type.copy_construct(push_data.m_element, i_source);

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
            \snippet heter_queue_samples.cpp heter_queue start_dyn_push_copy example 1 */
        put_transaction start_dyn_push_move(const type_eraser_type & i_type, COMMON_TYPE * i_source)
        {
            const auto sa = adjust_alignment(detail::SizeAndAlignment{ i_type.size(), i_type.alignment() }, min_alignment);
            auto push_data = sa.m_size <= s_max_size_inpage ?
                implace_allocate<0>(sa) : external_allocate<0>(sa);

            DENSITY_ASSERT_INTERNAL(push_data.m_control_block != nullptr && push_data.m_element != nullptr);
            new (type_after_control(push_data.m_control_block)) type_eraser_type(i_type);
            auto const element = i_type.move_construct(push_data.m_element, i_source);

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
                : m_queue(i_source.m_queue), m_push_data(i_source.m_push_data)
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
					- this transaction is empty
					- the alignment is not valid
					- the size is not a multiple of the alignment

                <b>Complexity</b>: Unspecified.
                \n <b>Effects on iterators</b>: no iterator is invalidated
                \n <b>Throws</b>: unspecified.
                \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). */
            void * raw_allocate(size_t i_size, size_t i_alignment = alignof(std::max_align_t))
            {
                DENSITY_ASSERT(!empty());

                const auto sa = adjust_alignment(detail::SizeAndAlignment{i_size,i_alignment}, min_alignment);
                auto push_data = sa.m_size <= s_max_size_inpage ?
                    m_queue->implace_allocate<2>(sa) : m_queue->external_allocate<2>(sa);

                return push_data.m_element;
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
                    "put_transaction provides a raw memory implace allocation that does not invoke destructors when deallocating");

                auto const count_s = std::distance(i_begin, i_end);
                auto const count = static_cast<size_t>(count_s);
                DENSITY_ASSERT(static_cast<DiffType>(count) == count_s);

                auto const elements = static_cast<ValueType*>(raw_allocate(sizeof(ValueType), alignof(ValueType)));
                //std::uninitialized_copy(i_begin, i_end, elements);
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
				cancel_put_impl(m_push_data.m_control_block);
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
                return m_push_data.m_element;
            }

            /** Returns the type of the object being added.

				\pre The behavior is undefined if either:
					- this transaction is empty */
            const TYPE_ERASER_TYPE & type() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return *type_after_control(m_push_data.m_control);
            }

            /** If this transaction is empty the destructor has no side effects. Otherwise it cancels it. */
            ~put_transaction()
            {
                if (m_queue != nullptr)
                {
					cancel_put_impl(m_push_data.m_control_block);
                }
            }

            #ifndef DOXYGEN_DOC_GENERATION

                // not part of the public interface
                put_transaction(heterogeneous_queue * i_queue, PutData i_push_data, std::true_type, void *) noexcept
                    : m_queue(i_queue), m_push_data(i_push_data)
                        { }

                // not part of the public interface
                put_transaction(heterogeneous_queue * i_queue, PutData i_push_data, std::false_type, COMMON_TYPE * i_element) noexcept
                    : m_queue(i_queue), m_push_data(i_push_data)
                {
                    m_push_data.m_element = i_element;
                    m_push_data.m_control_block->m_element = i_element;
                }

            #endif // #ifndef DOXYGEN_DOC_GENERATION

        private:
            heterogeneous_queue * m_queue;
            PutData m_push_data;
        };


		/** This class is used as return type from put functions with an element type known at compile time.
			
			typed_put_transaction derives from put_transaction adding just an element_ptr() that returns a
			pointer of correct the type. */
        template <typename TYPE>
            class typed_put_transaction : public put_transaction
        {
        public:

            using put_transaction::put_transaction;

            /** Returns a pointer to the object being added.
                \n <i>Note</i>: the object is constructed at the begin of the transaction, so this
                    function always returns a pointer to a valid object.

				\pre The behavior is undefined if either:
					- this transaction is empty */
            TYPE * element_ptr() const noexcept
                { return static_cast<TYPE *>(put_transaction::element_ptr()); }
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
				if (m_control != nullptr)
				{
					m_queue->cancel_consume_impl(m_control);
					m_control = nullptr;
				}
            }

            /** Returns the type of the element being consumed.

                \pre The behavior is undefined if this consume_operation is empty, that is it has been used as source for a move operation. */
            const TYPE_ERASER_TYPE & complete_type() const noexcept
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

                \pre The behavior is undefined if this consume_operation is empty, that is it has been used as source for a move operation. */
            COMMON_TYPE * element_ptr() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return get_element(m_control);
            }

            #ifndef DOXYGEN_DOC_GENERATION

                // not part of the public interface
                consume_operation(heterogeneous_queue * i_queue, ControlBlock * i_control) noexcept
                    : m_queue(i_queue), m_control(i_control)
                {
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
			start_consume().commit();
            auto operation = start_consume();

			operation.commit();
        }

        /** Removes and destroy the first element of the queue, if the queue is not empty. Otherwise it has no effect.

            @return whether an element was actually removed.

            \pre The behavior is undefined if the queue is empty.
            <b>Complexity</b>: constant.
            \n <b>Effects on iterators</b>: any iterator pointing to the first element is invalidated
            \n <b>Throws</b>: nothing */
        bool pop_if_any() noexcept
        {
            auto operation = start_consume();
            if (operation)
            {
                auto const & type = operation.complete_type();
                auto const element = operation.element_ptr();
                type.destroy(element);
				operation.commit();
                return true;
            }
            return false;
        }

        /** Calls a user provided function object passing as parameters the first element of the queue and its type, then removes and
            destroys the element.

            @param i_func funcion object to be invoked with  const TYPE_ERASER_TYPE & i_type, COMMON_TYPE * i_element
            @return return value of i_func(std::declval<TYPE_ERASER_TYPE>(), std::declval<COMMON_TYPE *>())

            \pre The behavior is undefined if the queue is empty.

            <b>Complexity</b>: Constant
            \n <b>Effects on iterators</b>: any iterator pointing to the first element is invalidated
            \n <b>Throws</b>: anything thrown by i_func.
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).

            <b>Examples</b>
            \snippet heter_queue_samples.cpp heter_queue consume example 1
        */
        template <typename FUNC>
            auto consume(FUNC && i_func)
                noexcept(noexcept(i_func(std::declval<const TYPE_ERASER_TYPE &>(), std::declval<COMMON_TYPE *>())))
                    -> unvoid_t<decltype(i_func(std::declval<const TYPE_ERASER_TYPE &>(), std::declval<COMMON_TYPE *>()))>
        {
            using ReturnType = decltype(i_func(std::declval<const TYPE_ERASER_TYPE &>(), std::declval<COMMON_TYPE *>()));
            return consume_impl(std::forward<FUNC>(i_func), std::is_void<ReturnType>());
        }

        /**
            const TYPE_ERASER_TYPE & i_type, COMMON_TYPE * i_element

            <b>Examples</b>
            \snippet heter_queue_samples.cpp heter_queue consume_if_any example 1

        */
        template <typename FUNC>
            auto consume_if_any(FUNC && i_func)
                noexcept(noexcept((i_func(std::declval<const TYPE_ERASER_TYPE>(), std::declval<COMMON_TYPE *>()))))
                    -> optional<unvoid_t<decltype(i_func(std::declval<const TYPE_ERASER_TYPE &>(), std::declval<COMMON_TYPE *>()))>>
        {
            using ReturnType = decltype(i_func(std::declval<const TYPE_ERASER_TYPE &>(), std::declval<COMMON_TYPE *>()));
            return consume_if_any_impl(std::forward<FUNC>(i_func), std::is_void<ReturnType>());
        }


        consume_operation start_consume() noexcept
        {
            return consume_operation(this, start_consume_impl());
        }


                    // reentrant

        /** Same to heterogeneous_queue::push, but allow reentrancy: during the construction of the element the queue is in a
            valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet heter_queue_samples.cpp heter_queue reentrant_push example 1 */
        template <typename ELEMENT_TYPE>
            void reentrant_push(ELEMENT_TYPE && i_source)
        {
            return reentrant_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Same to heterogeneous_queue::emplace, but allow reentrancy: during the construction of the element the queue is in a
            valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet heter_queue_samples.cpp heter_queue reentrant_emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            void reentrant_emplace(CONSTRUCTION_PARAMS &&... i_construction_params)
        {
            start_reentrant_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...).commit();
        }

        /** Same to heterogeneous_queue::dyn_push, but allow reentrancy: during the construction of the element the queue is in a
            valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet heter_queue_samples.cpp heter_queue reentrant_dyn_push example 1 */
        void reentrant_dyn_push(const type_eraser_type & i_type)
        {
            start_reentrant_dyn_push(i_type).commit();
        }

        /** Same to heterogeneous_queue::dyn_push_copy, but allow reentrancy: during the construction of the element the queue is in a
            valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet heter_queue_samples.cpp heter_queue reentrant_dyn_push_copy example 1 */
        void reentrant_dyn_push_copy(const type_eraser_type & i_type, const COMMON_TYPE * i_source)
        {
            start_reentrant_dyn_push_copy(i_type, i_source).commit();
        }

        /** Same to heterogeneous_queue::dyn_push_move, but allow reentrancy: during the construction of the element the queue is in a
            valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet heter_queue_samples.cpp heter_queue reentrant_dyn_push_move example 1 */
        void reentrant_dyn_push_move(const type_eraser_type & i_type, COMMON_TYPE * i_source)
        {
            start_reentrant_dyn_push_move(i_type, i_source).commit();
        }

        /** Same to heterogeneous_queue::start_push, but allow reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet heter_queue_samples.cpp heter_queue start_reentrant_push example 1 */
        template <typename ELEMENT_TYPE>
            reentrant_typed_put_transaction<ELEMENT_TYPE> start_reentrant_push(ELEMENT_TYPE && i_source)
        {
            return start_reentrant_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Same to heterogeneous_queue::start_emplace, but allow reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet heter_queue_samples.cpp heter_queue start_reentrant_emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            reentrant_typed_put_transaction<ELEMENT_TYPE> start_reentrant_emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
            static_assert(std::is_convertible<ELEMENT_TYPE*, COMMON_TYPE*>::value,
                "ELEMENT_TYPE must derive from COMMON_TYPE, or COMMON_TYPE must be void");

            auto const sa = adjust_alignment(detail::SizeAndAlignment{sizeof(ELEMENT_TYPE), alignof(ELEMENT_TYPE)}, min_alignment);
            auto push_data = sa.m_size <= s_max_size_inpage ?
                implace_allocate<1>(sa) : external_allocate<1>(sa);

            DENSITY_ASSERT_INTERNAL(push_data.m_control_block != nullptr && push_data.m_element != nullptr);
            new (type_after_control(push_data.m_control_block)) type_eraser_type(type_eraser_type::template make<ELEMENT_TYPE>());
            auto const element = new (push_data.m_element) ELEMENT_TYPE(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);

            return reentrant_typed_put_transaction<ELEMENT_TYPE>(this, push_data, std::is_same<COMMON_TYPE, void>(), element);
        }

        /** Same to heterogeneous_queue::start_dyn_push, but allow reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet heter_queue_samples.cpp heter_queue start_reentrant_dyn_push example 1 */
        reentrant_put_transaction start_reentrant_dyn_push(const type_eraser_type & i_type)
        {
            const auto sa = adjust_alignment(detail::SizeAndAlignment{i_type.size(), i_type.alignment()}, min_alignment);
            auto push_data = sa.m_size <= s_max_size_inpage ?
                implace_allocate<1>(sa) : external_allocate<1>(sa);

            DENSITY_ASSERT_INTERNAL(push_data.m_control_block != nullptr && push_data.m_element != nullptr);
            new (type_after_control(push_data.m_control_block)) type_eraser_type(i_type);
            auto const element = i_type.default_construct(push_data.m_element);

            return reentrant_put_transaction(this, push_data, std::is_same<COMMON_TYPE, void>(), element);
        }


        /** Same to heterogeneous_queue::start_dyn_push_copy, but allow reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet heter_queue_samples.cpp heter_queue start_reentrant_dyn_push_copy example 1 */
        reentrant_put_transaction start_reentrant_dyn_push_copy(const type_eraser_type & i_type, const COMMON_TYPE * i_source)
        {
            const auto sa = adjust_alignment(detail::SizeAndAlignment{ i_type.size(), i_type.alignment() }, min_alignment);
            auto push_data = sa.m_size <= s_max_size_inpage ?
                implace_allocate<1>(sa) : external_allocate<1>(sa);

            DENSITY_ASSERT_INTERNAL(push_data.m_control_block != nullptr && push_data.m_element != nullptr);
            new (type_after_control(push_data.m_control_block)) type_eraser_type(i_type);
            auto const element = i_type.copy_construct(push_data.m_element, i_source);

            return reentrant_put_transaction(this, push_data, std::is_same<COMMON_TYPE, void>(), element);
        }

        /** Same to heterogeneous_queue::start_dyn_push_move, but allow reentrancy: during the construction of the element, and until the state of
            the transaction gets destroyed, the queue is in a valid state. The effects of the call are not observable until the function returns.

            <b>Examples</b>
            \snippet heter_queue_samples.cpp heter_queue start_reentrant_dyn_push_move example 1 */
        reentrant_put_transaction start_reentrant_dyn_push_move(const type_eraser_type & i_type, COMMON_TYPE * i_source)
        {
            const auto sa = adjust_alignment(detail::SizeAndAlignment{ i_type.size(), i_type.alignment() }, min_alignment);
            auto push_data = sa.m_size <= s_max_size_inpage ?
                implace_allocate<1>(sa) : external_allocate<1>(sa);

            DENSITY_ASSERT_INTERNAL(push_data.m_control_block != nullptr && push_data.m_element != nullptr);
            new (type_after_control(push_data.m_control_block)) type_eraser_type(i_type);
            auto const element = i_type.move_construct(push_data.m_element, i_source);

            return reentrant_put_transaction(this, push_data, std::is_same<COMMON_TYPE, void>(), element);
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
                : m_queue(i_source.m_queue), m_push_data(i_source.m_push_data)
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
                    m_push_data = i_source.m_push_data;
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
            void * raw_allocate(size_t i_size, size_t i_alignment = alignof(std::max_align_t))
            {
                DENSITY_ASSERT(!empty());

                size_t size = i_size;
                size_t alignment = i_alignment;
                if (alignment < min_alignment)
                {
                    alignment = min_alignment;
                    size = uint_upper_align(size, min_alignment);
                }
                auto push_data = size <= s_max_size_inpage ?
                    m_queue->implace_allocate<2>(size, alignment) :
                    m_queue->external_allocate<2>(size, alignment);
                return push_data.m_element;
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
				commit_reentrant_put_impl(m_push_data.m_control_block);
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
				cancel_reentrant_put_impl(m_push_data.m_control_block);
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
                return m_push_data.m_element;
            }

            /** Returns the type of the object being added.

                \pre The behavior is undefined if this reentrant_put_transaction is empty, that is it has been used as source for a move operation. */
            const TYPE_ERASER_TYPE & type() const noexcept
            {
                DENSITY_ASSERT(!empty());
                return *type_after_control(m_push_data.m_control);
            }

            /** If this reentrant_put_transaction the destructor has no side effects. Otherwise it commits or cancels the transaction, depending
                on whether commit() has been called on not. */
            ~reentrant_put_transaction()
            {
                if (m_queue != nullptr)
                {
					cancel_reentrant_put_impl(m_push_data.m_control_block);
                }
            }

            #ifndef DOXYGEN_DOC_GENERATION

                // not part of the public interface
                reentrant_put_transaction(heterogeneous_queue * i_queue, PutData i_push_data, std::true_type, void *) noexcept
                    : m_queue(i_queue), m_push_data(i_push_data)
                        { }

                // not part of the public interface
                reentrant_put_transaction(heterogeneous_queue * i_queue, PutData i_push_data, std::false_type, COMMON_TYPE * i_element) noexcept
                    : m_queue(i_queue), m_push_data(i_push_data)
                {
                    m_push_data.m_element = i_element;
                    m_push_data.m_control_block->m_element = i_element;
                }

            #endif // #ifndef DOXYGEN_DOC_GENERATION

        private:
            heterogeneous_queue * m_queue;
            PutData m_push_data;
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
            using type_eraser_type = TYPE_ERASER_TYPE;
            using common_type = COMMON_TYPE;
            using value_type = std::pair<const type_eraser_type &, common_type * const>;
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

            /** Returns the TYPE_ERASER_TYPE associated to this element. The user may use the function type_info of TYPE_ERASER_TYPE
                (whenever supported) to obtain a const-reference to a std::type_info. */
            const TYPE_ERASER_TYPE & complete_type() const noexcept
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
            using type_eraser_type = TYPE_ERASER_TYPE;
            using common_type = COMMON_TYPE;
            using value_type = const std::pair<const type_eraser_type &, const common_type* const>;
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

            /** Returns the TYPE_ERASER_TYPE associated to this element. The user may use the function type_info of TYPE_ERASER_TYPE
                (whenever supported) to obtain a const-reference to a std::type_info. */
            const TYPE_ERASER_TYPE & complete_type() const noexcept
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
                noexcept(noexcept(i_func(std::declval<const TYPE_ERASER_TYPE &>(), std::declval<COMMON_TYPE *>())))
                    -> decltype(i_func(std::declval<const TYPE_ERASER_TYPE &>(), std::declval<COMMON_TYPE *>()))
        {
            auto transaction = start_consume();
            DENSITY_ASSERT(static_cast<bool>(transaction));
            auto const & type = transaction.complete_type();
            auto const element = transaction.element_ptr();

            auto res = i_func(type, element);

            type.destroy(element);
            transaction.commit();
            return res;
        }

        template <typename FUNC>
            unvoid_t<void> consume_impl(FUNC && i_func, std::true_type)
                noexcept(noexcept(i_func(std::declval<const TYPE_ERASER_TYPE &>(), std::declval<COMMON_TYPE *>())))
        {
            auto transaction = start_consume();
            DENSITY_ASSERT(static_cast<bool>(transaction));
            auto const & type = transaction.complete_type();
            auto const element = transaction.element_ptr();

            i_func(type, element);

            type.destroy(element);
            transaction.commit();

            return unvoid_t<void>();
        }

        template <typename FUNC>
            auto consume_if_any_impl(FUNC && i_func, std::false_type)
                noexcept(noexcept((i_func(std::declval<const TYPE_ERASER_TYPE>(), std::declval<COMMON_TYPE *>()))))
                    -> optional<decltype(i_func(std::declval<const TYPE_ERASER_TYPE &>(), std::declval<COMMON_TYPE *>()))>
        {
            using ReturnType = decltype(i_func(std::declval<const TYPE_ERASER_TYPE &>(), std::declval<COMMON_TYPE *>()));
            auto transaction = start_consume();
            if (transaction)
            {
                auto const & type = transaction.complete_type();
                auto const element = transaction.element_ptr();
                auto result = make_optional<ReturnType>(i_func(type, element));
                type.destroy(element);
                transaction.commit();
                return result;
            }
            return optional<ReturnType>();
        }

        template <typename FUNC>
            optional<unvoid_t<void>> consume_if_any_impl(FUNC && i_func, std::true_type)
                noexcept(noexcept((i_func(std::declval<const TYPE_ERASER_TYPE>(), std::declval<COMMON_TYPE *>()))))
        {
            auto transaction = start_consume();
            if (transaction)
            {
                auto const & type = transaction.complete_type();
                auto const element = transaction.element_ptr();
                i_func(type, element);
                type.destroy(element);
                transaction.commit();
                return make_optional<unvoid_t<void>>(unvoid_t<void>());
            }
            return optional<unvoid_t<void>>();
        }

        constexpr static auto s_invalid_control_block = ALLOCATOR_TYPE::page_size - 1;
        constexpr static size_t s_sizeof_ControlBlock = (sizeof(ControlBlock) + (min_alignment - 1)) & ~(min_alignment - 1);
        constexpr static size_t s_sizeof_RuntimeType = (sizeof(type_eraser_type) + (min_alignment - 1)) & ~(min_alignment - 1);
        constexpr static auto s_max_size_inpage = ALLOCATOR_TYPE::page_size - s_sizeof_ControlBlock - s_sizeof_RuntimeType;

        ControlBlock * first_valid(ControlBlock * i_from) const
        {
            for (auto curr = i_from; curr != m_tail; )
            {
                if ((curr->m_next & 3) == 0)
                {
                    return curr;
                }
                curr = reinterpret_cast<ControlBlock*>(curr->m_next & ~static_cast<uintptr_t>(7));
            }
            return nullptr;
        }

        ControlBlock * next_valid(ControlBlock * i_from) const
        {
            DENSITY_ASSERT_INTERNAL(i_from != m_tail);
            for (auto curr = reinterpret_cast<ControlBlock*>(i_from->m_next & ~static_cast<uintptr_t>(3)); curr != m_tail; )
            {
                if ((curr->m_next & 3) == 0)
                {
                    return curr;
                }
                curr = reinterpret_cast<ControlBlock*>(curr->m_next & ~static_cast<uintptr_t>(7));
            }
            return nullptr;
        }

        static type_eraser_type * type_after_control(ControlBlock * i_control)
        {
            return reinterpret_cast<type_eraser_type*>(address_add(i_control, s_sizeof_ControlBlock));
        }

        static void * get_unaligned_element(detail::QueueControl<void> * i_control)
        {
            auto result = address_add(i_control, s_sizeof_ControlBlock + s_sizeof_RuntimeType);
            if (i_control->m_next & 4)
            {
                result = static_cast<ExternalBlock*>(result)->m_block;
            }
            return result;
        }

        static void * get_element(detail::QueueControl<void> * i_control)
        {
            auto result = address_add(i_control, s_sizeof_ControlBlock + s_sizeof_RuntimeType);
            if (i_control->m_next & 4)
            {
                result = static_cast<ExternalBlock*>(result)->m_block;
            }
            else
            {
                result = address_upper_align(result, type_after_control(i_control)->alignment());
            }
            return result;
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
            size_t m_size, m_alignment;
        };

        template <uintptr_t CONTROL_BITS>
            PutData external_allocate(detail::SizeAndAlignment i_sa)
        {
            auto const external_block = ALLOCATOR_TYPE::allocate(i_sa.m_size, i_sa.m_alignment);
            auto const inplace_put = implace_allocate<CONTROL_BITS>(detail::SizeAndAlignment{sizeof(ExternalBlock), alignof(ExternalBlock)});

            new(inplace_put.m_element) ExternalBlock{external_block, i_sa.m_size, i_sa.m_alignment};

            DENSITY_ASSERT((inplace_put.m_control_block->m_next & 4) == 0);
            inplace_put.m_control_block->m_next |= 4;
            return PutData{ inplace_put.m_control_block, external_block };
        }

        template <uintptr_t CONTROL_BITS>
            PutData implace_allocate(detail::SizeAndAlignment i_sa)
        {
            DENSITY_ASSERT_INTERNAL(i_sa.m_alignment >= min_alignment && is_power_of_2(i_sa.m_alignment)
                && (i_sa.m_size % i_sa.m_alignment) == 0 && i_sa.m_size <= s_max_size_inpage);

            for(;;)
            {
                auto const control_block = m_tail;
                void * new_tail = address_add(control_block, s_sizeof_ControlBlock + s_sizeof_RuntimeType );

                new_tail = address_upper_align(new_tail, i_sa.m_alignment);
                void * new_element = new_tail;
                new_tail = address_add(new_tail, i_sa.m_size);

                if (DENSITY_LIKELY(are_in_same_page(address_add(new_tail, s_sizeof_ControlBlock), control_block)))
                {
                    DENSITY_ASSERT_INTERNAL(control_block != nullptr);
                    new(control_block) ControlBlock();

                    control_block->m_next = reinterpret_cast<uintptr_t>(new_tail) + CONTROL_BITS;
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

        DENSITY_NO_INLINE static void cancel_put_impl(ControlBlock * i_control_block)
        {
            const auto type_ptr = type_after_control(i_control_block);
            type_ptr->destroy(get_element(i_control_block));

            DENSITY_ASSERT_INTERNAL((i_control_block->m_next & 3) == 0);
            i_control_block->m_next+=2;
        }

        static void commit_reentrant_put_impl(ControlBlock * i_control_block)
        {
            DENSITY_ASSERT_INTERNAL((i_control_block->m_next & 3) == 1);
            i_control_block->m_next--;
        }

        DENSITY_NO_INLINE static void cancel_reentrant_put_impl(ControlBlock * i_control_block)
        {
            const auto type_ptr = type_after_control(i_control_block);
            type_ptr->destroy(get_element(i_control_block));

            DENSITY_ASSERT_INTERNAL((i_control_block->m_next & 3) == 1);
            i_control_block->m_next++;
        }

        ControlBlock * start_consume_impl() noexcept
        {
            auto curr = m_head;
            while (curr != m_tail)
            {
                if ((curr->m_next & 3) == 0)
                {
                    curr->m_next |= 1;
                    return curr;
                }

                curr = reinterpret_cast<ControlBlock*>(curr->m_next & ~static_cast<uintptr_t>(7));
            }

            return nullptr;
        }

        void commit_consume_impl(ControlBlock * i_control_block) noexcept
        {
            DENSITY_ASSERT_INTERNAL((i_control_block->m_next & 3) == 1);
            i_control_block->m_next++;

            if(i_control_block == m_head)
            {
                auto curr = i_control_block;
                DENSITY_ASSERT_INTERNAL(curr != m_tail);
                do {
                    if ((curr->m_next & 3) != 2)
                    {
                        break;
                    }

                    auto next = reinterpret_cast<ControlBlock*>(curr->m_next & ~static_cast<uintptr_t>(7));
                    if (curr->m_next & 4)
                    {
                        auto result = address_add(curr, s_sizeof_ControlBlock + s_sizeof_RuntimeType);
                        const auto & block = *static_cast<ExternalBlock*>(result);
                        ALLOCATOR_TYPE::deallocate(block.m_block, block.m_size, block.m_alignment);
                    }

                    if (!are_in_same_page(next, curr))
                    {
                        allocator_type::deallocate_page(address_lower_align(curr, allocator_type::page_size));
                    }

                    curr = next;

                } while (curr != m_tail);

                m_head = curr;
            }
        }

        void cancel_consume_impl(ControlBlock * i_control_block) noexcept
        {
            DENSITY_ASSERT_INTERNAL((i_control_block->m_next & 7) == 1);
            i_control_block->m_next--;
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

    template <typename COMMON_TYPE, typename TYPE_ERASER_TYPE, typename ALLOCATOR_TYPE>
        inline void swap(heterogeneous_queue<COMMON_TYPE, TYPE_ERASER_TYPE, ALLOCATOR_TYPE> & i_first,
            heterogeneous_queue<COMMON_TYPE, TYPE_ERASER_TYPE, ALLOCATOR_TYPE> & i_second) noexcept
    {
        i_first.swap(i_second);
    }

} // namespace density
