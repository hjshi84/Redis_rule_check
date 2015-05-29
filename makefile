LIBDIR= -L/usr/local/lib
LIBSO = -lhiredis -lm
CFLAG = -Wall -g -O2 
  
all:redis  
  
redis:redis.o list.o md5.o
	gcc ${CFLAG} -o $@ $^ ${LIBDIR} ${LIBSO}  
%o:%c
	gcc -O2 -g -c -o $@ $^  
clean:  
	rm -f *.o 

#LIBDIR= -L/home/hjshi/work/code/c/redis -L/usr/local/lib  
#LIBSO = -lta -lhiredis  
#CFLAG = -fPIC -g -O2
#
#all:redista
#
#redista:main.o libta.so
#	gcc ${CFLAG} -o $@ $< ${LIBDIR} ${LIBSO} 
#
#libta.so:redis.o list.o md5.o
#	gcc ${CFLAG} -shared -g -O2 -o $@ $^
#
#%o:%c
#	gcc ${CFLAG} -c -o $@ $^
#	
#clean:
#	rm -f *.o *.so redista
#
