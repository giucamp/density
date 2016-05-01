
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

void type_experiment()
{
	using namespace zoo;

	std::cout << std::endl;
}
