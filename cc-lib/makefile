
default : heap_test.exe

heap_test.o : heap_test.cc heap.h
	$(CXX) $< -o $@ -c

heap_test.exe : heap_test.o
	$(CXX) $^ -o $@
