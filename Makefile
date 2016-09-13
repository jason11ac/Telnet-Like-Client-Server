#Compiling with gcc
CC = gcc

#-g is for debugging purposes (gdb) and -pthread is because the program uses threads and -lmcrypt is for encrypting and decrypting
CFLAGS = -g -pthread -lmcrypt

TARGET = client
TARGET2 = server


all:: #Make both files
	$(CC) $(CFLAGS) $(TARGET).c -o $(TARGET)
	$(CC) $(CFLAGS) $(TARGET2).c -o $(TARGET2)


client:: $(TARGET).c  #Make just client from client.c
	$(CC) $(CFLAGS) $(TARGET).c -o $(TARGET)

server:: $(TARGET2).c  #Make just server from server.c
	$(CC) $(CFLAGS) $(TARGET2).c -o $(TARGET2)


clean: $(TARGET)  #make clean
	rm $(TARGET) $(TARGET2)

dist: $(TARGET)  #make dist
	tar -cvzf lab1b-504487052.tar.gz $(TARGET).c $(TARGET2).c Makefile README.txt my.key
