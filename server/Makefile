# Makefile.in generated by automake 1.12.1 from Makefile.am.
# upnp/sample/Makefile.  Generated from Makefile.in by configure.

# Copyright (C) 1994-2012 Free Software Foundation, Inc.

# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.



#
# "Makefile.am" for "libupnp/upnp/sample"
#
# Copyright (C) 2007 Marcelo Roberto Jimenez <mroberto@users.sourceforge.net>
#


.SUFFIXES : .o.c

OBJS = server_main.o sample_util.o tizen_ctrl.o
SRCS = server_main.c sample_util.c tizen_ctrl.c

CC = g++
INCS = -I$(PWD)/../upnp/include
LIBRARY_DIR = $(PWD)/../upnp/lib/x86
LIBRARIES = -lupnp -lthreadutil -lpthread -lixml
CFLAGS = -g -c $(INCS) -DTIZEN -DX86
CPPFLAGS= $(CFLAGS)

SERVER = server


all : $(SERVER) 

$(SERVER) : $(OBJS)
	$(CC) -o $(SERVER) $(OBJS) -L$(LIBRARY_DIR) $(LIBRARIES)

.c.o :
	$(CC) -c $@ $^ $(CFLAGS) 


clean:
	rm -rf $(SERVER) *.o

