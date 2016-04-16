

namespace density
{
	void dense_fixed_queue_test();
	void dense_list_benchmark();
	void dense_list_test();
}

int main()
{
	using namespace density;
	dense_fixed_queue_test();
	dense_list_test();
	dense_list_benchmark();
	return 0;
}