all: main

main: libtest
	gcc main.c -rdynamic -g -O0 -L. -ltest -ltrace -o main

trace: 
	gcc backtrace.c -shared -fpic -rdynamic -g -O0 -ldl -lbfd -o libtrace.so

libtest: trace
	gcc libtest.c -shared -fpic -rdynamic -g -O0 -o libtest.so

.PHONY:clean

clean:
	rm -rf main libtrace.so libtest.so
