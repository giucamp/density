
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <assert.h>
#include <type_traits>
#include <limits>
#include "density_common.h"

namespace density
{
	/*
		Element Type Synopsis

		class RuntimeType
		{
			public:

				template <typename COMPLETE_TYPE> static RuntimeType make() noexcept;

				RuntimeType(const RuntimeType &) noexcept;
				RuntimeType(RuntimeType &&) noexcept;
				~RuntimeType() noexcept;

				size_t size() const noexcept;
				size_t alignment() const noexcept;

				void copy_construct(void * i_dest_element, const void * i_source_element) const
				void move_construct(void * i_dest_element, void * i_source_element) const noexcept
								
				void destroy(void * i_element) const noexcept;
		};
	
	*/
	/** Specifies the way in which the size and the alignment of elements of a DenseList are stored */
	enum SizeAlignmentMode
	{
		most_general, /**< Uses two separate size_t to store the size and the alignment. */
		compact, /**< Both size and alignment are stored in a single size_t word. The alignment uses the 25% of the bits
					of the size_t, while the alignment uses all the other bits. For example, if size_t is big 64-bits, the
					alignment is stored in 16 bits, while the size is stored in 48 bits.
					If the size or the alignment can't be represented with the given number of bits, the behaviour is undefined.
					The implementation may report this error with a debug assert.
					If size_t has not a binary representation (that is std::numeric_limits<size_t>::radix != 2), using this
					representation wi resut in a compile time error. */
		assume_normal_alignment /**< Use a size_t word to store the size, and do not store the alignment: just assume that
					every element does not need an alignment more strict than a void pointer (void*).
					If an element actually needs a more strict alignment , the behaviour is undefined.
					The implementation may report this error with a debug assert.*/
	};

	enum ElementTypeCaps
	{
		none = 0,
		nothrow_move_construtible = 1 << 1,
		copy_only = 1 << 0,		
		copy_and_move = nothrow_move_construtible | copy_only,
	};

	namespace detail
	{
		// This class is used by the default type-infos to store the size and the alignment according to the secified SizeAlignmentMode
		template <SizeAlignmentMode MODE>
			class ElementType_SizeAlign;
		template <> class ElementType_SizeAlign<SizeAlignmentMode::most_general>
		{
		public:

			ElementType_SizeAlign() = delete;
			ElementType_SizeAlign & operator = (const ElementType_SizeAlign &) = delete;
			ElementType_SizeAlign & operator = (ElementType_SizeAlign &&) = delete;

			ElementType_SizeAlign(const ElementType_SizeAlign &) DENSITY_NOEXCEPT = default;
			#if defined(_MSC_VER) && _MSC_VER < 1900
				ElementType_SizeAlign(ElementType_SizeAlign && i_source) DENSITY_NOEXCEPT // visual studio 2013 doesnt seems to support defauted move constructors
					: m_size(i_source.m_size), m_alignment(i_source.m_alignment) { }			
			#else
				ElementType_SizeAlign(ElementType_SizeAlign && i_source) DENSITY_NOEXCEPT = default;
			#endif

			ElementType_SizeAlign(size_t i_size, size_t i_alignment) DENSITY_NOEXCEPT
				: m_size(i_size), m_alignment(i_alignment) {}
			
			size_t size() const DENSITY_NOEXCEPT { return m_size; }
			size_t alignment() const DENSITY_NOEXCEPT { return m_alignment; }
						
		private:
			const size_t m_size, m_alignment;
		};
		template <> class ElementType_SizeAlign<
			std::enable_if<std::numeric_limits<size_t>::radix == 2, SizeAlignmentMode>::type::compact>
		{
		public:
			
			ElementType_SizeAlign() = delete;
			ElementType_SizeAlign & operator = (const ElementType_SizeAlign &) = delete;
			ElementType_SizeAlign & operator = (ElementType_SizeAlign &&) = delete;

			ElementType_SizeAlign(const ElementType_SizeAlign &) DENSITY_NOEXCEPT = default;
			#if defined(_MSC_VER) && _MSC_VER < 1900
				ElementType_SizeAlign(ElementType_SizeAlign && i_source) DENSITY_NOEXCEPT // visual studio 2013 doesnt seems to support defauted move constructors
					: m_size(i_source.m_size), m_alignment(i_source.m_alignment) { }			
			#else
				ElementType_SizeAlign(ElementType_SizeAlign && i_source) DENSITY_NOEXCEPT = default;
			#endif
			
			ElementType_SizeAlign(size_t i_size, size_t i_alignment) DENSITY_NOEXCEPT
				: m_size(i_size), m_alignment(i_alignment)
			{
				// check the correcteness of the narrowing conversion - a failure on this gives undefined behaviour
				assert(m_size == i_size && m_alignment == i_alignment);
			}
			size_t size() const DENSITY_NOEXCEPT { return m_size; }
			size_t alignment() const DENSITY_NOEXCEPT { return m_alignment; }

		private:
			//static_assert(std::numeric_limits<size_t>::radix == 2, "size_t is expected to be a binary number");
			const size_t m_size : std::numeric_limits<size_t>::digits - std::numeric_limits<size_t>::digits / 4;
			const size_t m_alignment : std::numeric_limits<size_t>::digits / 4;
		};
		template <> class ElementType_SizeAlign<SizeAlignmentMode::assume_normal_alignment>
		{
		public:

			ElementType_SizeAlign() = delete;
			ElementType_SizeAlign & operator = (const ElementType_SizeAlign &) = delete;
			ElementType_SizeAlign & operator = (ElementType_SizeAlign &&) = delete;

			ElementType_SizeAlign(const ElementType_SizeAlign &) DENSITY_NOEXCEPT = default;
			#if defined(_MSC_VER) && _MSC_VER < 1900
				ElementType_SizeAlign(ElementType_SizeAlign && i_source) DENSITY_NOEXCEPT // visual studio 2013 doesnt seems to support defauted move constructors
					: m_size(i_source.m_size) { }			
			#else
				ElementType_SizeAlign(ElementType_SizeAlign && i_source) DENSITY_NOEXCEPT = default;
			#endif

			ElementType_SizeAlign(size_t i_size, size_t i_alignment) DENSITY_NOEXCEPT
				: m_size(i_size)
			{
				// check the correcteness of the alignment - a failure on this gives undefined behaviour
				assert(i_alignment <= std::alignment_of<void*>::value );
				(void)i_alignment;
			}

			size_t size() const DENSITY_NOEXCEPT { return m_size; }
			size_t alignment() const DENSITY_NOEXCEPT { return std::alignment_of<void*>::value; }

		private:
			const size_t m_size;
		};

		// ElementType_Destr class
		template <typename ELEMENT, SizeAlignmentMode SIZE_ALIGNMENT_MODE, 
			bool HAS_VIRTUAL_DESTRUCTOR = std::has_virtual_destructor<ELEMENT>::value> class ElementType_Destr;
		template <typename ELEMENT, SizeAlignmentMode SIZE_ALIGNMENT_MODE>
			class ElementType_Destr<ELEMENT, SIZE_ALIGNMENT_MODE, true> 
				: public ElementType_SizeAlign<SIZE_ALIGNMENT_MODE>
		{
		public:
			static_assert(std::is_nothrow_destructible<ELEMENT>::value, "the destructor must be nothrow");

			ElementType_Destr(size_t i_size, size_t i_alignment) DENSITY_NOEXCEPT
				: ElementType_SizeAlign<SIZE_ALIGNMENT_MODE>(i_size, i_alignment) { }

			void destroy(void * i_element) const DENSITY_NOEXCEPT
			{
				assert(i_element != nullptr);
				static_cast<ELEMENT*>(i_element)->~ELEMENT();
			}
		};
		template <typename ELEMENT, SizeAlignmentMode SIZE_ALIGNMENT_MODE>
			class ElementType_Destr<ELEMENT, SIZE_ALIGNMENT_MODE, false> 
				: public ElementType_SizeAlign<SIZE_ALIGNMENT_MODE>
		{
		public:
			static_assert(std::is_same<ELEMENT, void>::value || std::is_nothrow_destructible<ELEMENT>::value, "the destructor must be nothrow");

			ElementType_Destr(size_t i_size, size_t i_alignment) DENSITY_NOEXCEPT
				: ElementType_SizeAlign<SIZE_ALIGNMENT_MODE>(i_size, i_alignment) { }

			void destroy(void * i_element) const DENSITY_NOEXCEPT
			{
				assert(i_element != nullptr);
				static_cast<ELEMENT*>(i_element)->~ELEMENT();
			}
		};

		// CopyConstructImpl<COMPLETE_TYPE>::invoke
		template <typename COMPLETE_TYPE, bool CAN = std::is_copy_constructible<COMPLETE_TYPE>::value> struct CopyConstructImpl;
		template <typename COMPLETE_TYPE> struct CopyConstructImpl<COMPLETE_TYPE, true>
		{
			static void invoke(void * i_first, void * i_second)
			{
				new (i_first) COMPLETE_TYPE(*static_cast<const COMPLETE_TYPE*>(i_second));
			}
		};
		template <typename COMPLETE_TYPE> struct CopyConstructImpl<COMPLETE_TYPE, false>
		{
			static void invoke(void *, void *)
			{
				throw std::exception("copy-construction not supported");
			}
		};

		// MoveConstructImpl<COMPLETE_TYPE>::invoke
		template <typename COMPLETE_TYPE, bool CAN = std::is_nothrow_move_constructible<COMPLETE_TYPE>::value > struct MoveConstructImpl;
		template <typename COMPLETE_TYPE> struct MoveConstructImpl<COMPLETE_TYPE, true>
		{
			static void invoke(void * i_first, void * i_second)
			{
				new (i_first) COMPLETE_TYPE(std::move(*static_cast<COMPLETE_TYPE*>(i_second)));
			}
		};
		template <typename COMPLETE_TYPE> struct MoveConstructImpl<COMPLETE_TYPE, false>
		{
			static void invoke(void *, void *)
			{
				throw std::exception("move-construction not supported");
			}
		};

		template <typename TYPE, 
			bool CAN_COPY = std::is_copy_constructible<TYPE>::value || std::is_same<void, TYPE>::value,
			bool CAN_MOVE = std::is_nothrow_move_constructible<TYPE>::value || std::is_same<void, TYPE>::value
				> struct GetAutoCopyMoveCap;

		template <typename TYPE > struct GetAutoCopyMoveCap<TYPE, false, false>
			{ static const ElementTypeCaps value = ElementTypeCaps::none; };
		template <typename TYPE > struct GetAutoCopyMoveCap<TYPE, false, true>
			{ static const ElementTypeCaps value = ElementTypeCaps::nothrow_move_construtible; };
		template <typename TYPE > struct GetAutoCopyMoveCap<TYPE, true, false>
			{ static const ElementTypeCaps value = ElementTypeCaps::copy_only; };
		template <typename TYPE > struct GetAutoCopyMoveCap<TYPE, true, true>
			{ static const ElementTypeCaps value = ElementTypeCaps::copy_and_move; };

	} // namespace detail


	template <typename ELEMENT, ElementTypeCaps COPY_MOVE = detail::GetAutoCopyMoveCap<ELEMENT>::value, SizeAlignmentMode SIZE_ALIGNMENT_MODE = SizeAlignmentMode::most_general>
		class RuntimeType;
	
	template <typename ELEMENT, SizeAlignmentMode SIZE_ALIGNMENT_MODE>
		class RuntimeType<ELEMENT, ElementTypeCaps::copy_and_move, SIZE_ALIGNMENT_MODE> : public detail::ElementType_Destr<ELEMENT, SIZE_ALIGNMENT_MODE>
	{
	public:

		using Element = ELEMENT;
		static const SizeAlignmentMode s_size_alignment_mode = SIZE_ALIGNMENT_MODE;
		static const ElementTypeCaps s_caps = ElementTypeCaps::copy_and_move;

		RuntimeType() = delete;
		RuntimeType & operator = (const RuntimeType &) = delete;
		RuntimeType & operator = (RuntimeType &&) = delete;

		RuntimeType(const RuntimeType &) DENSITY_NOEXCEPT = default;

		#if defined(_MSC_VER) && _MSC_VER < 1900
			RuntimeType(RuntimeType && i_source) DENSITY_NOEXCEPT // visual studio 2013 doesnt seems to support defauted move constructors
				: detail::ElementType_Destr<ELEMENT,SIZE_ALIGNMENT_MODE>(std::move(i_source)), m_function(i_source.m_function)
					{ }			
		#else
			RuntimeType(RuntimeType && i_source) DENSITY_NOEXCEPT = default;
		#endif
					
		template <typename COMPLETE_TYPE>
			static RuntimeType make() DENSITY_NOEXCEPT
		{
			static_assert(std::is_same<ELEMENT, COMPLETE_TYPE>::value || std::is_base_of<ELEMENT, COMPLETE_TYPE>::value || std::is_same<ELEMENT, void>::value, "not covariant type");
			return RuntimeType(sizeof(COMPLETE_TYPE), std::alignment_of<COMPLETE_TYPE>::value, &function_impl< COMPLETE_TYPE > );
		}

		void copy_construct(void * i_destination, const void * i_source_element) const
		{
			assert(i_destination != nullptr && i_source_element != nullptr && i_destination != i_source_element);
			(*m_function)(Operation::copy, i_destination, const_cast<void*>(i_source_element));
		}

		void move_construct(void * i_destination, void * i_source_element) const DENSITY_NOEXCEPT
		{
			assert(i_destination != nullptr && i_source_element != nullptr && i_destination != i_source_element);
			(*m_function)(Operation::move, i_destination, i_source_element);
		}

	private:

		enum class Operation { copy, move, destroy };
 
		using FunctionPtr = void(*)(Operation i_operation, void * i_first, void * i_second );

		RuntimeType(size_t i_size, size_t i_alignment, FunctionPtr i_function) DENSITY_NOEXCEPT
			: detail::ElementType_Destr<ELEMENT,SIZE_ALIGNMENT_MODE>(i_size, i_alignment), m_function(i_function) { }

		template <typename COMPLETE_TYPE>
			static void function_impl(Operation i_operation, void * i_first, void * i_second)
		{
			switch (i_operation )
			{
				case Operation::copy:
					detail::CopyConstructImpl<COMPLETE_TYPE>::invoke(i_first, i_second);
					break;

				case Operation::move:
					detail::MoveConstructImpl<COMPLETE_TYPE>::invoke(i_first, i_second);
					break;
			}
		}

	private:
		const FunctionPtr m_function;
	};

	template <typename ELEMENT, SizeAlignmentMode SIZE_ALIGNMENT_MODE>
		class RuntimeType<ELEMENT, ElementTypeCaps::copy_only, SIZE_ALIGNMENT_MODE> : public detail::ElementType_Destr<ELEMENT, SIZE_ALIGNMENT_MODE>
	{
	public:

		using Element = ELEMENT;
		static const SizeAlignmentMode s_size_alignment_mode = SIZE_ALIGNMENT_MODE;
		static const ElementTypeCaps s_caps = ElementTypeCaps::copy_only;

		RuntimeType() = delete;
		RuntimeType & operator = (const RuntimeType &) = delete;
		RuntimeType & operator = (RuntimeType &&) = delete;

		RuntimeType(const RuntimeType &) DENSITY_NOEXCEPT = default;

		#if defined(_MSC_VER) && _MSC_VER < 1900
			RuntimeType(RuntimeType && i_source) DENSITY_NOEXCEPT // visual studio 2013 doesnt seems to support defauted move constructors
				: detail::ElementType_Destr<ELEMENT,SIZE_ALIGNMENT_MODE>(std::move(i_source)), m_function(i_source.m_function)
					{ }			
		#else
			RuntimeType(RuntimeType && i_source) DENSITY_NOEXCEPT = default;
		#endif
					
		template <typename COMPLETE_TYPE>
			static RuntimeType make() DENSITY_NOEXCEPT
		{
			static_assert(std::is_same<ELEMENT, COMPLETE_TYPE>::value || std::is_base_of<ELEMENT, COMPLETE_TYPE>::value || std::is_same<ELEMENT, void>::value, "not covariant type");
			return RuntimeType(sizeof(COMPLETE_TYPE), std::alignment_of<COMPLETE_TYPE>::value, &function_impl< COMPLETE_TYPE > );
		}

		void copy_construct(void * i_destination, const void * i_source_element) const
		{
			assert(i_destination != nullptr && i_source_element != nullptr && i_destination != i_source_element);
			(*m_function)(i_destination, const_cast<void*>(i_source_element));
		}

	private:

		enum class Operation { copy, move, destroy };
 
		using FunctionPtr = void(*)(void * i_first, void * i_second );

		RuntimeType(size_t i_size, size_t i_alignment, FunctionPtr i_function) DENSITY_NOEXCEPT
			: detail::ElementType_Destr<ELEMENT, SIZE_ALIGNMENT_MODE>(i_size, i_alignment), m_function(i_function) { }

		template <typename COMPLETE_TYPE>
			static void function_impl(void * i_first, void * i_second)
		{
			detail::CopyConstructImpl<COMPLETE_TYPE>::invoke(i_first, i_second);
		}

	private:
		const FunctionPtr m_function;
	};

	template <typename ELEMENT, SizeAlignmentMode SIZE_ALIGNMENT_MODE>
		class RuntimeType<ELEMENT, ElementTypeCaps::nothrow_move_construtible, SIZE_ALIGNMENT_MODE> 
			: public detail::ElementType_Destr<ELEMENT, SIZE_ALIGNMENT_MODE>
	{
	public:

		using Element = ELEMENT;
		static const SizeAlignmentMode s_size_alignment_mode = SIZE_ALIGNMENT_MODE;
		static const ElementTypeCaps s_caps = ElementTypeCaps::nothrow_move_construtible;

		RuntimeType() = delete;
		RuntimeType & operator = (const RuntimeType &) = delete;
		RuntimeType & operator = (RuntimeType &&) = delete;

		RuntimeType(const RuntimeType &) DENSITY_NOEXCEPT = default;

		#if defined(_MSC_VER) && _MSC_VER < 1900
			RuntimeType(RuntimeType && i_source) DENSITY_NOEXCEPT // visual studio 2013 doesnt seems to support defauted move constructors
				: detail::ElementType_Destr<ELEMENT, SIZE_ALIGNMENT_MODE>(std::move(i_source)), m_function(i_source.m_function)
					{ }			
		#else
			RuntimeType(RuntimeType && i_source) DENSITY_NOEXCEPT = default;
		#endif
					
		template <typename COMPLETE_TYPE>
			static RuntimeType make() DENSITY_NOEXCEPT
		{
			static_assert(std::is_same<ELEMENT, COMPLETE_TYPE>::value || std::is_base_of<ELEMENT, COMPLETE_TYPE>::value || std::is_same<ELEMENT, void>::value, "not covariant type");
			return RuntimeType(sizeof(COMPLETE_TYPE), std::alignment_of<COMPLETE_TYPE>::value, &function_impl< COMPLETE_TYPE > );
		}

		void move_construct(void * i_destination, void * i_source_element) const DENSITY_NOEXCEPT
		{
			assert(i_destination != nullptr && i_source_element != nullptr && i_destination != i_source_element);
			(*m_function)(i_destination, i_source_element);
		}

	private:

		using FunctionPtr = void(*)(void * i_first, void * i_second );

		RuntimeType(size_t i_size, size_t i_alignment, FunctionPtr i_function) DENSITY_NOEXCEPT
			: detail::ElementType_Destr<ELEMENT,SIZE_ALIGNMENT_MODE>(i_size, i_alignment), m_function(i_function) { }

		template <typename COMPLETE_TYPE>
			static void function_impl(void * i_first, void * i_second)
		{
			detail::MoveConstructImpl<COMPLETE_TYPE>::invoke(i_first, i_second);
		}

	private:
		const FunctionPtr m_function;
	};

	template <typename ELEMENT, SizeAlignmentMode SIZE_ALIGNMENT_MODE>
		class RuntimeType<ELEMENT, ElementTypeCaps::none, SIZE_ALIGNMENT_MODE>
			: public detail::ElementType_Destr<ELEMENT, SIZE_ALIGNMENT_MODE>
	{
	public:

		using Element = ELEMENT;
		static const SizeAlignmentMode s_size_alignment_mode = SIZE_ALIGNMENT_MODE;
		static const ElementTypeCaps s_caps = ElementTypeCaps::none;

		RuntimeType() = delete;
		RuntimeType & operator = (const RuntimeType &) = delete;
		RuntimeType & operator = (RuntimeType &&) = delete;

		RuntimeType(const RuntimeType &) DENSITY_NOEXCEPT = default;

		#if defined(_MSC_VER) && _MSC_VER < 1900
			RuntimeType(RuntimeType && i_source) DENSITY_NOEXCEPT // visual studio 2013 doesnt seems to support defauted move constructors
				: detail::ElementType_Destr<ELEMENT, SIZE_ALIGNMENT_MODE>(std::move(i_source)) { }
		#else
			RuntimeType(RuntimeType && i_source) DENSITY_NOEXCEPT = default;
		#endif
					
		template <typename COMPLETE_TYPE>
			static RuntimeType make() DENSITY_NOEXCEPT
		{
			static_assert(std::is_same<ELEMENT, COMPLETE_TYPE>::value || std::is_base_of<ELEMENT, COMPLETE_TYPE>::value || std::is_same<ELEMENT, void>::value, "not covariant type");
			return RuntimeType(sizeof(COMPLETE_TYPE), std::alignment_of<COMPLETE_TYPE>::value );
		}

	private:

		RuntimeType(size_t i_size, size_t i_alignment ) DENSITY_NOEXCEPT
			: detail::ElementType_Destr<ELEMENT, SIZE_ALIGNMENT_MODE>(i_size, i_alignment) { }
	};
}
