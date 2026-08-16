CC ?= gcc
CFLAGS ?= -Wall -Wextra -g
INCLUDES = -Iinclude

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin

TARGET = hotspotctl
OBJS = main.o hostapd.o dnsmasq.o cli.o firewall.o auto.o docs.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(OBJS)

main.o: src/main.c include/hostapd.h include/dnsmasq.h
	$(CC) $(CFLAGS) $(INCLUDES) -c src/main.c -o main.o

hostapd.o: src/hostapd.c include/hostapd.h
	$(CC) $(CFLAGS) $(INCLUDES) -c src/hostapd.c -o hostapd.o

dnsmasq.o: src/dnsmasq.c include/hostapd.h
	$(CC) $(CFLAGS) $(INCLUDES) -c src/dnsmasq.c -o dnsmasq.o

cli.o: src/cli.c include/cli.h include/hostapd.h
	$(CC) $(CFLAGS) $(INCLUDES) -c src/cli.c -o cli.o

firewall.o: src/firewall.c include/firewall.h
	$(CC) $(CFLAGS) $(INCLUDES) -c src/firewall.c -o firewall.o

auto.o: src/auto.c include/auto.h include/hostapd.h 
	$(CC) $(CFLAGS) $(INCLUDES) -c src/auto.c -o auto.o

docs.o: src/docs.c include/docs.h  
	$(CC) $(CFLAGS) $(INCLUDES) -c src/docs.c -o docs.o

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

.PHONY: all clean install uninstall