all:nyuenc
nyuenc:nyuenc.o queue.o thread_pool.o
	gcc -mcmodel=medium -o  nyuenc nyuenc.o queue.o thread_pool.o -lpthread
queue.o:queue.c
	gcc -mcmodel=medium -c queue.c
thread_pool.o:thread_pool.c
	gcc -mcmodel=medium -c thread_pool.c
nyuenc.o:nyuenc.c
	gcc -mcmodel=medium -c nyuenc.c -std=c99
clean:
	rm -rf *.o nyuenc
