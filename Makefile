HWLOC = /home/danghvu/

all:
	g++ aloc.c test.cxx -I$(HWLOC)/include -L$(HWLOC)/lib -lhwloc -lpthread -o test
clean:
	rm -f test
