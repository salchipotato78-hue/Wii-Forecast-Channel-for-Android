# Builds and runs the forecast.bin / short.bin parser tests.
#   make test     generates sample files, builds, runs the tests
#   make sanitize builds with ASan and UBSan and runs the tests
#   make fuzz     runs the mutation fuzzer under ASan and UBSan
#   make clean
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
SANFLAGS := -std=c++17 -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -Wall -Wextra -Wpedantic
INCLUDES := -Iinclude -Itests
SAMPLE   := tests/sample
SRC      := src/forecast_data.cpp

.PHONY: test sanitize fuzz clean
test: $(SAMPLE)/forecast.bin test_forecast_data
	./test_forecast_data $(SAMPLE)

sanitize: $(SAMPLE)/forecast.bin
	$(CXX) $(SANFLAGS) $(INCLUDES) $(SRC) tests/test_forecast_data.cpp -o test_asan
	./test_asan $(SAMPLE)

fuzz: $(SAMPLE)/forecast.bin
	$(CXX) $(SANFLAGS) -Iinclude $(SRC) tests/fuzz_forecast.cpp -o fuzz_asan
	./fuzz_asan $(SAMPLE)

$(SAMPLE)/forecast.bin: tests/make_sample.py
	python3 tests/make_sample.py $(SAMPLE)

test_forecast_data: $(SRC) tests/test_forecast_data.cpp include/forecast_data/forecast_data.h tests/expected.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRC) tests/test_forecast_data.cpp -o $@

clean:
	rm -f test_forecast_data test_asan fuzz_asan
