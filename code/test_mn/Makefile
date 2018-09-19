CC	= gcc
CFLAGS	= -I/usr/include/libnl3 -W -Wall
LDFLAGS	= -L/usr/local/lib -lnl-3 -lnl-genl-3 -lnl-route-3

all : mn-agent

mn-agent : mn-agent.c util.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean : 
	@rm -rf *.o mn-agent
