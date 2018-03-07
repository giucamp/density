export CC=clang-5.0
export CXX=clang++-5.0
rm test/build -r
bash scripts/test.sh TRUE Debug FALSE "" "-queue_tests_cardinality:3000" -fsanitize=undefined -fsanitize-stats

