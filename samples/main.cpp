
namespace density
{
	void dense_queue_test();
	void list_benchmark();
	void list_test();
	void paged_queue_test();
}

#include <iostream>
#include <vector>
#include "..\density\density_common.h"

class A
{
public:
	A() { std::cout << "Constr" << std::endl; }
	~A() noexcept { std::cout << "Destr" << std::endl; }
	A(const A&) { std::cout << "Copy Constr" << std::endl; }
	A(A&&) noexcept { std::cout << "Move Constr" << std::endl; }
	A & operator = (const A&) { std::cout << "Copy Assign" << std::endl; }
	A & operator = (A&&) noexcept { std::cout << "Move Assign" << std::endl; }
};

int main()
{
	std::cout << density::MemSize<uint64_t>(5000) << std::endl;
	std::cout << density::MemSize<uint64_t>(1050) << std::endl;
	std::cout << density::MemSize<uint64_t>(900) << std::endl;
	std::cout << density::MemSize<uint64_t>(5000 * 1000) << std::endl;
	std::cout << density::MemSize<uint64_t>(1050 * 1000) << std::endl;
	std::cout << density::MemSize<uint64_t>(900 * 1000) << std::endl;
	std::cout << "\t" << density::MemSize<uint64_t>(1024*1024) << std::endl;

	std::cout << density::MemSize<uint64_t>(uint64_t(5000) * 1000000) << std::endl;
	std::cout << density::MemSize<uint64_t>(1050 * 1000000) << std::endl;
	std::cout << density::MemSize<uint64_t>(900 * 1000000) << std::endl;
	
	std::vector<A> vect;
	vect.resize(10);
	vect.reserve(20);

	using namespace density;

	paged_queue_test();
	dense_queue_test();
	list_test();
	list_benchmark();
	return 0;
}