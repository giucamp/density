
#pragma once
#include "histogram.h"

namespace density_tests
{
} // namespace density_test

#if 0

#include <iostream>
#include <random>

void histogram_test()
{
	using namespace density_tests;

	{
		
		histogram<int> hist;
		hist.title() << "This is an histogram";

		for (int i = 0; i < 10; i++)
			for (int j = 0; j <= i; j++)
				hist << j;
		hist.write(std::cout);
	}
	{
		
		std::mt19937 rand(55);
		auto dice = [&] { return std::uniform_int_distribution<int>(1, 6)(rand); };
		
		histogram<int> hist("Throwing two dices 2000 times");
		for(int i = 0; i < 2000; i++)
			hist << dice() + dice();

		std::cout << hist;
	}
	{
		histogram<int> hist("Single value");
		hist << 2;
		std::cout << hist;
	}
	{
		histogram<int> hist("No values");
		std::cout << hist;
	}
	{
		
		std::mt19937 rand(55);
		auto dice = [&] { return std::uniform_int_distribution<int>(1, 6)(rand); };
		
		histogram<int> hist("Throwing two dices 2000 times", 1);
		for(int i = 0; i < 2000; i++)
			hist << dice() + dice();

		std::cout << hist;
	}
}
#endif