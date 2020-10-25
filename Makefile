CC = gcc
CFLAGS = -c
all:cliandsrv
cliandsrv:tcp_echo_cli.o tcp_echo_srv.o
	$(CC) tcp_echo_cli.c -o tcp_echo_cli
	$(CC) tcp_echo_srv.o -o tcp_echo_srv
tcp_echo_cli.o: tcp_echo_cli.c
	$(CC) $(CFLAGS) $<
tcp_echo_srv.o: tcp_echo_srv.c
	$(CC) $(CFLAGS) $<
clean:
	rm -rf tcp_echo_cli tcp_echo_srv *.o