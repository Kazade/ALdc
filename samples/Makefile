
# Manipulate the CFLAGS to look our *our* version of the library and includes
INC_DIR = $(abspath ../include)
LIB_DIR = $(abspath ../)
export CFLAGS := $(CFLAGS) -I $(INC_DIR)
export OBJEXTRA := $(LIB_DIR)/libAL.a


all:
	$(KOS_MAKE) -C basic all
	$(KOS_MAKE) -C helicopter all

