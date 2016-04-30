
#include <iostream>

namespace zoo
{
	class Animal
	{
		int m = 0;

	public:
		Animal & operator ++()
		{
			m++;
			return *this;
		}
	};

	class Mammalia : public Animal
	{
		int j = 0;

	public:
		Mammalia & operator ++()
		{
			j++;
			return *this;
		}

		virtual void gg() {}
	};

	
	class Dog : public Mammalia
	{
		int b = 0;

	public:
		Dog & operator ++()
		{
			b++;
			return *this;
		}

		virtual void gg() {}
	};
}

namespace type_experiment_
{
	struct Incrementable
	{
		template <typename TYPE>
			static void apply(void * i_dest)
		{
			++*static_cast<TYPE*>(i_dest);
		}
	};

	template <typename CONCEPT, typename TYPE>
		struct ConceptApply
	{
		static void apply(void * i_dest)
		{
			CONCEPT::apply<TYPE>(i_dest);
		}
	};

	template <typename... CONCEPTS>
		struct ConceptList { };

	namespace detail
	{
		/** FunctionTable<TYPE, ConceptList<CONCEPTS...> >::s_functions is a static array of pointers to all the ConceptApply<CONCEPTS, TYPE>::apply */
		template <typename TYPE, typename CONCEPT_LIST> struct FunctionTable;
		template <typename TYPE, typename... CONCEPTS>
		struct FunctionTable< TYPE, ConceptList<CONCEPTS...> >
		{
			static const void * const s_functions[sizeof...(CONCEPTS)];
		};
		template <typename TYPE, typename... CONCEPTS>
		const void * const FunctionTable< TYPE, ConceptList<CONCEPTS...> >::s_functions[sizeof...(CONCEPTS)]
			= { &ConceptApply<CONCEPTS, TYPE>::apply... };

	} // namespace detail
}

template <typename CONCEPT_LIST>
	class TypeEraser
{

};


template <typename FROM, typename TO>
	struct is_trivially_convertible;

template <typename FROM, typename TO>
	struct is_trivially_convertible<FROM*, TO*>
{
	constexpr static const bool value = std::is_trivially_constructible<TO*, FROM*>::value; // (TO*)((FROM*)(7)) == (TO*)(7);
};

template <typename FROM, typename TO>
	void print()
{
	bool can = is_trivially_convertible<FROM, TO>::value;
	std::cout << "from " << typeid(FROM).name() << " to " << typeid(TO).name() << ": " << can << std::endl;
};

void type_experiment()
{
	using namespace zoo;
	using namespace type_experiment_;

	auto table = detail::FunctionTable<Dog, ConceptList<Incrementable> >::s_functions;
	(void)table;

	Dog dog;


	print<Animal*, Mammalia*>();
	print<Mammalia*, Animal*>();

	print<Mammalia*, Dog*>();
	print<Dog*,Mammalia*>();
	
	print<Animal*, Dog*>();
	print<Dog*, Animal*>();
	
	print<Animal*, Animal*>();
	print<Mammalia*, Mammalia*>();
	print<Dog*, Dog*>();

	std::cout << std::endl;
}
