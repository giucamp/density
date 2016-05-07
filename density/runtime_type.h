
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <type_traits>
#include <limits>
#include <utility>
#include <typeinfo>
#include "density_common.h"

namespace density
{
    /*
        Runtime Type Synopsis

        class runtime_type
        {
            public:

                template <typename COMPLETE_TYPE> static runtime_type make() noexcept;

                runtime_type(const runtime_type &) noexcept;
                runtime_type(runtime_type &&) noexcept;
                ~runtime_type();

                size_t size() const noexcept;
                size_t alignment() const noexcept;

                // optional
                void * copy_construct(void * i_dest_element, const void * i_source_element) const;

                // optional
                void * move_construct_nothrow(void * i_dest_element, void * i_source_element) const noexcept;
                                
                void destroy(void * i_element) const noexcept;
        };
    
    */

	/** This struct template checks the requirements on a RUNTIME_TYPE. Vioations are detected with static_assert. */
    template <typename RUNTIME_TYPE>
        struct RuntimeTypeConceptCheck
    {
        static_assert(noexcept(std::declval<const RUNTIME_TYPE>().~RUNTIME_TYPE()),
            "The destructor of RUNTIME_TYPE must be noexcept"); // note: destructors are noexcept by default

        static_assert(noexcept(RUNTIME_TYPE(std::declval<const RUNTIME_TYPE>())),
            "The copy constructor of RUNTIME_TYPE must be declared as noexcept");

        static_assert(noexcept(std::declval<const RUNTIME_TYPE>().size()),
            "RUNTIME_TYPE::size must be declared as noexcept");

        static_assert(noexcept(std::declval<const RUNTIME_TYPE>().alignment()),
            "RUNTIME_TYPE::alignment must be declared as noexcept");
    };


	namespace detail
	{
		/* size_t invoke_hash(const TYPE & i_object) - Computes the hash of an object. 
			- If a the call hash_func(i_object) is legal, it is used to compute the hash. This function
			  should be defined in the namespace that contains TYPE (it uses ADL). If such function exits,
			  its return type must be size_t.
			- Otherwise std::hash<TYPE> is used to comute the hash
		see http://stackoverflow.com/questions/257288/is-it-possible-to-write-a-c-template-to-check-for-a-functions-existence */
		template <typename> struct sfinae_true : std::true_type {};
		template <typename TYPE> static sfinae_true<decltype(hash_func(std::declval<TYPE>()))> has_hash_func_impl(int);
		template <typename TYPE> static std::false_type has_hash_func_impl(long);
		template <typename TYPE> inline size_t invoke_hash_func_impl(const TYPE & i_object, std::true_type)
		{
			static_assert( std::is_same< decltype(hash_func(i_object)), size_t >::value, 
				"if the hash_func() exits for this type, the it must return a size_t" );
			return hash_func(i_object);
		}
		template <typename TYPE> inline size_t invoke_hash_func_impl(const TYPE & i_object, std::false_type)
		{
			return std::hash<TYPE>(i_object);
		}
		template <typename TYPE>
			inline size_t invoke_hash(const TYPE & i_object)
		{
			return invoke_hash_func_impl(i_object, decltype(has_hash_func_impl<TYPE>(0))() );
		}

		/** This struct template rapresent a list of features associated to a runtime_type.		
		
			struct FeatureX
			{
				using type = ...;

				template <typename BASE, typename TYPE> struct Impl
				{
					static const uintptr_t value = ...;
				};
			};
		*/

		/* This feature stores the size in the table of the type */
		struct FeatureSize
		{
			using type = size_t;

			template <typename BASE, typename TYPE> struct Impl
			{				
				static const uintptr_t value = sizeof(TYPE);
			};
		};

		/* This feature stores the alignment in the table of the type */
		struct FeatureAlignment
		{
			using type = size_t;

			template <typename BASE, typename TYPE> struct Impl
			{
				static const uintptr_t value = alignof(TYPE);
			};
		};

		/* This feature computes the hash of an object*/
		struct FeatureHash
		{
			using type = size_t (*) (const void * i_source);

			template <typename BASE, typename TYPE> struct Impl
			{				
				static const uintptr_t value;

				static size_t invoke(const void * i_source)
				{
					return invoke_hash( *static_cast<const TYPE*>(i_source) );
				}
			};
		};

		template <typename BASE, typename TYPE>
			const uintptr_t FeatureHash::Impl<BASE, TYPE>::value = reinterpret_cast<uintptr_t>(invoke);

		/* This feature stores a pointer to the std::type_info in the table of the type */
		struct FeatureRTTI
		{
			using type = const std::type_info*;

			template <typename BASE, typename TYPE> struct Impl
			{
				static const uintptr_t value;
			};
		};
		template <typename BASE, typename TYPE>
			const uintptr_t FeatureRTTI::Impl<BASE,TYPE>::value = reinterpret_cast<uintptr_t>(&typeid(TYPE));

		/* This feature stores a pointer to the copy constructor in the table of the type */
		struct FeatureCopyConstruct
		{
			using type = void * (*) (void * i_complete_dest, const void * i_base_source);

			template <typename BASE, typename TYPE> struct Impl
			{
				/* Copy-constructs an object of type TYPE, and returns a pointer (of type BASE) to it.
					@param i_complete_dest pointer to the storage in which the TYPE must be constructed.
					@param i_base_source pointer to a subobjct (of type BASE) of an object whose complete
						type is TYPE, that is to be used as source of the copy.
					@return pointer to a subobjct (of type BASE) of the new object. */
				static void * invoke(void * i_complete_dest, const void * i_base_source)
				{
					auto const base_source = static_cast<const BASE*>(i_base_source);
					// DENSITY_ASSERT( dynamic_cast<const TYPE*>(base_source) != nullptr ); to do: implement a detail::IsInstanceOf
					BASE * const base_result = new(i_complete_dest) TYPE(*static_cast<const TYPE*>(base_source));
					return base_result;
				}

				static const uintptr_t value;
			};
		};
		template <typename TYPE, typename BASE>
			const uintptr_t FeatureCopyConstruct::Impl<TYPE, BASE>::value = reinterpret_cast<uintptr_t>(invoke);

		/* This feature stores a pointer to the move constructor in the table of the type */
		struct FeatureMoveConstruct
		{
			using type = void * (*)(void * i_dest, void * i_source);

			template <typename BASE, typename TYPE> struct Impl
			{
				/* Move-constructs an object of type TYPE, and returns a pointer (of type BASE) to it.
					@param i_complete_dest pointer to the storage in which the TYPE must be constructed.
					@param i_base_source pointer to a subobjct (of type BASE) of an object whose complete
						type is TYPE, that is to be used as source of the move.
					@return pointer to a subobjct (of type BASE) of the new object. */
				static void * invoke(void * i_complete_dest, void * i_base_source) noexcept
				{
					BASE * base_source = static_cast<BASE*>(i_base_source);
					// DENSITY_ASSERT(dynamic_cast<const TYPE*>(base_source) != nullptr); to do: implement a detail::IsInstanceOf
					BASE * base_result = new(i_complete_dest) TYPE(std::move(*static_cast<TYPE*>(base_source)));
					return base_result;
				}
				static const uintptr_t value;
			};			
		};
		template <typename TYPE, typename BASE>
			const uintptr_t FeatureMoveConstruct::Impl<TYPE, BASE>::value = reinterpret_cast<uintptr_t>(invoke);


		struct FeatureDestroy
		{
			using type = void (*)(void * i_dest);

			template <typename BASE, typename TYPE> struct Impl
			{
				static void invoke(void * i_base_dest) noexcept
				{
					const auto base_dest = static_cast<BASE*>(i_base_dest);
					static_cast<TYPE*>(base_dest)->TYPE::~TYPE();
				}
				static const uintptr_t value;
			};			
		};
		template <typename TYPE, typename BASE>
			const uintptr_t FeatureDestroy::Impl<TYPE, BASE>::value = reinterpret_cast<uintptr_t>(invoke);

		template <typename... FEATURES> struct FeatureList
		{
			static const size_t size = sizeof...(FEATURES);
		};

		/** (1) FeatureConcat< FeatureList<FEATURES_1...>, FeatureList<FEATURES_2...> >::type
		    (2) FeatureConcat< FeatureList<FEATURES_1...>, FEATURE_2 >::type
		*/
		template <typename...> struct FeatureConcat;
		template <typename... FIRST_FEATURES, typename... SECOND_FEATURES>
			struct FeatureConcat<FeatureList<FIRST_FEATURES...>, FeatureList<SECOND_FEATURES...>>
		{
			using type = FeatureList<FIRST_FEATURES..., SECOND_FEATURES...>;
		};
		template <typename... FIRST_FEATURES, typename SECOND_FEATURE>
			struct FeatureConcat<FeatureList<FIRST_FEATURES...>, SECOND_FEATURE>
		{
			using type = FeatureList<FIRST_FEATURES..., SECOND_FEATURE>;
		};

		/** FeatureTable<TYPE, FeatureList<FEATURES...> >::s_table is a static array of all the FEATURES::s_value */
		template <typename BASE, typename TYPE, typename FEATURE_LIST> struct FeatureTable;
		template <typename BASE, typename TYPE, typename... FEATURES>
			struct FeatureTable< BASE, TYPE, FeatureList<FEATURES...> >
		{
			static void * const s_table[sizeof...(FEATURES)];
		};
		template <typename BASE, typename TYPE, typename... FEATURES>
			void * const FeatureTable< BASE, TYPE, FeatureList<FEATURES...> >::s_table[sizeof...(FEATURES)]
				= { reinterpret_cast<void *>( FEATURES::template Impl<BASE, TYPE>::value )... };

		/* IndexOfFeature<0, TARGET_FEATURE, FeatureList<FEATURES...> >::value is the index of TARGET_FEATURE in FeatureList<FEATURES...>,
			or FeatureList<FEATURES...>::size if TARGET_FEATURE is not present */
		template <size_t START_INDEX, typename TARGET_FEATURE, typename FEATURE_LIST>
			struct IndexOfFeature;
		template <size_t START_INDEX, typename TARGET_FEATURE, typename... OTHER_FEATURES>
			struct IndexOfFeature<START_INDEX, TARGET_FEATURE, FeatureList<OTHER_FEATURES...> >
		{
			static const size_t value = START_INDEX;
		};
		template <size_t START_INDEX, typename TARGET_FEATURE, typename FIRST_FEATURE, typename... OTHER_FEATURES>
			struct IndexOfFeature<START_INDEX, TARGET_FEATURE, FeatureList<FIRST_FEATURE, OTHER_FEATURES...> >
		{
			static const size_t value = std::is_same<TARGET_FEATURE, FIRST_FEATURE>::value ?
				START_INDEX : IndexOfFeature<START_INDEX + 1, TARGET_FEATURE, FeatureList<OTHER_FEATURES...> >::value;
		};

		/** AutoGetFeatures<TYPE>::type */
		template <typename TYPE,
			bool CAN_COPY = std::is_copy_constructible<TYPE>::value || std::is_same<void, TYPE>::value,
			bool CAN_MOVE = std::is_nothrow_move_constructible<TYPE>::value || std::is_same<void, TYPE>::value
		> struct AutoGetFeatures;

		template <typename TYPE> struct AutoGetFeatures<TYPE, false, false >
		{
			using type = FeatureList<FeatureSize, FeatureAlignment, FeatureRTTI, FeatureDestroy>;
		};
		template <typename TYPE> struct AutoGetFeatures<TYPE, true, false >
		{
			using type = FeatureList<FeatureSize, FeatureAlignment, FeatureRTTI, FeatureDestroy, FeatureCopyConstruct>;
		};
		template <typename TYPE> struct AutoGetFeatures<TYPE, false, true >
		{
			using type = FeatureList<FeatureSize, FeatureAlignment, FeatureRTTI, FeatureDestroy, FeatureMoveConstruct>;
		};
		template <typename TYPE> struct AutoGetFeatures<TYPE, true, true >
		{
			using type = FeatureList<FeatureSize, FeatureAlignment, FeatureRTTI, FeatureDestroy, FeatureMoveConstruct, FeatureCopyConstruct>;
		};


		// CopyConstructImpl<COMPLETE_TYPE>::invoke
		template <typename COMPLETE_TYPE, bool CAN = std::is_copy_constructible<COMPLETE_TYPE>::value> struct CopyConstructImpl;
		template <typename COMPLETE_TYPE> struct CopyConstructImpl<COMPLETE_TYPE, true>
		{
			static void * invoke(void * i_first, void * i_second) DENSITY_NOEXCEPT
			{
				return new (i_first) COMPLETE_TYPE(*static_cast<const COMPLETE_TYPE*>(i_second));
			}
		};
		template <typename COMPLETE_TYPE> struct CopyConstructImpl<COMPLETE_TYPE, false>
		{
			static void * invoke(void *, void *)
			{
				return throw std::exception("copy-construction not supported");
			}
		};

		// MoveConstructImpl<COMPLETE_TYPE>::invoke
		template <typename COMPLETE_TYPE, bool CAN = std::is_nothrow_move_constructible<COMPLETE_TYPE>::value > struct MoveConstructImpl;
		template <typename COMPLETE_TYPE> struct MoveConstructImpl<COMPLETE_TYPE, true>
		{
			static void * invoke(void * i_first, void * i_second) DENSITY_NOEXCEPT
			{
				return new (i_first) COMPLETE_TYPE(std::move(*static_cast<COMPLETE_TYPE*>(i_second)));
			}
		};
		template <typename COMPLETE_TYPE> struct MoveConstructImpl<COMPLETE_TYPE, false>
		{
			static void * invoke(void *, void *)
			{
				throw std::exception("move-construction not supported");
			}
		};

	} // namespace detail
	
	template <typename BASE, typename FEATURE_LIST = typename detail::AutoGetFeatures<BASE>::type >
		class runtime_type
	{
	public:

		runtime_type() = delete;
		runtime_type(runtime_type && ) DENSITY_NOEXCEPT = default;
		runtime_type(const runtime_type &) DENSITY_NOEXCEPT = default;

		template <typename TYPE>
			static runtime_type make()
		{
			return runtime_type(detail::FeatureTable<BASE, TYPE, FEATURE_LIST>::s_table);
		}

		size_t size() const DENSITY_NOEXCEPT
		{
			return get_feature<detail::FeatureSize>();
		}

		size_t alignment() const DENSITY_NOEXCEPT
		{
			return get_feature<detail::FeatureAlignment>();
		}

		void * copy_construct(void * i_dest, const void * i_source) const
		{
			return get_feature<detail::FeatureCopyConstruct>()(i_dest, i_source);
		}

		void * move_construct_nothrow(void * i_dest, void * i_source) const DENSITY_NOEXCEPT
		{
			return get_feature<detail::FeatureMoveConstruct>()(i_dest, i_source);
		}

		void destroy(void * i_dest) const noexcept
		{
			get_feature<detail::FeatureDestroy>()(i_dest);
		}

		const std::type_info & type_info() const noexcept
		{
			return *get_feature<detail::FeatureRTTI>();
		}

		template <typename FEATURE>
			typename FEATURE::type get_feature() const noexcept
		{
			const size_t feature_index = detail::IndexOfFeature<0, FEATURE, FEATURE_LIST>::value;
			static_assert(feature_index < FEATURE_LIST::size, "feature not available in FEATURE_LIST");
			return reinterpret_cast<typename FEATURE::type>(m_table[feature_index]);
		}

	private:
		runtime_type(void * const * i_table) : m_table(i_table) { }

	private:
		void * const * m_table;
	};
}
