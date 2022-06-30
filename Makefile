TARGET = stat_collector_v1
PREFIX = /usr/local/bin

.PHONY: all clean install uninstall

all: stat_collector_v1 stat_collector_v2 stat_displayer
	
clean:
			rm -rf stat_collector_v1 stat_collector_v2 stat_displayer *.o
stat_collector_v1:  
			gcc stat_collector_v1.c -o stat_collector_v1 -lpthread -lrt
stat_collector_v2:
			gcc stat_collector_v2.c -o stat_collector_v2 -lpthread -lrt
stat_displayer:
			gcc stat_displayer.c -o stat_displayer -lrt
install: ins_collector_v1 ins_collector_v2 ins_displayer
ins_collector_v1:
			install stat_collector_v1 $(PREFIX)
ins_collector_v2:
			install stat_collector_v2 $(PREFIX)
ins_displayer:
			install stat_displayer $(PREFIX)
uninstall: un_collector_v1 un_collector_v2 un_displayer
un_collector_v1:
			rm -rf $(PREFIX)/stat_collector_v1
un_collector_v2:
			rm -rf $(PREFIX)/stat_collector_v2
un_displayer:
			rm -rf $(PREFIX)/stat_displayer

deb:
	dpkg-buildpackage -D

