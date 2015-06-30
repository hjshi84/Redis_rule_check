#LIBDIR= -L/usr/local/lib
#LIBSO = -lhiredis -lm
#CFLAG = -Wall -g 
#  
#all:redis  
#  
#redis:redis.o list.o md5.o cJSON.o
#	gcc ${CFLAG} -o $@ $^ ${LIBDIR} ${LIBSO}  
#%o:%c
#	gcc  -g -c -o $@ $^  
#clean:  
#	rm -f *.o 

LIBDIR= -L/home/hjshi/work/code/c/redis -L/usr/local/lib  
LIBSO = -lhiredis -lm -luuid
CFLAG = -fPIC -g 

all:libta.so redista

redista:redis.o libta.so md5.o  cJSON.o
	gcc ${CFLAG} -o $@ $< -lta  ${LIBDIR} ${LIBSO} 

libta.so:redis.o list.o md5.o cJSON.o
	gcc ${CFLAG} -shared -g -o $@ $^ ${LIBDIR} ${LIBSO} 

%o:%c
	gcc ${CFLAG} -c -o $@ $^
	
clean:
	rm -f *.o *.so redista
##
