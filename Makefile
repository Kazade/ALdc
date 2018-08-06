# KallistiOS ##version##
#
# kos-ports/libgl Makefile
# Copyright (C) 2013, 2014 Josh Pearson
# Copyright (C) 2014 Lawrence Sebald
# Copyright (C) 2018 Luke Benstead

TARGET = libAL.a
OBJS = AL/devices.o
OBJS += containers/stack.o containers/named_array.o containers/aligned_vector.o

SUBDIRS =

KOS_CFLAGS += -ffast-math -O3 -Iinclude

link:
	$(KOS_AR) rcs $(TARGET) $(OBJS)

build: $(OBJS) link


samples: build
	$(KOS_MAKE) -C samples all

defaultall: create_kos_link $(OBJS) subdirs linklib samples

include $(KOS_BASE)/addons/Makefile.prefab

# creates the kos link to the headers
create_kos_link:
	rm -f ../include/AL
	ln -s ../ALdc/include/AL ../include/
