TESTS = tester_flat
CXX ?= g++
CXXFLAGS = -std=c++11 -Wall
LDFLAGS = -pthread

run: $(TESTS)
	true$(foreach t,$(TESTS), && ./$(t))

tester_%: ../bson_%.cpp ../bson_%.hpp tester_%.cpp gtest/libgtest.a gtest/libgtest_main.a
	$(CXX) -o $@ $(CXXFLAGS) $(filter-out %.hpp,$^) $(LDFLAGS)

gtest/libgtest.a gtest/libgtest_main.a: /usr/src/gtest/CMakeLists.txt
	mkdir -p $(@D)
	cd $(@D) && cmake $(dir $<) && make
