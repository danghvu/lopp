HWLOC = /home/danghvu/

all:
	gcc -g3 aloc.c test.c -I$(HWLOC)/include -L$(HWLOC)/lib -lhwloc -lpthread -o test `pkg-config --libs libconfig`
clean:
	rm -f test
