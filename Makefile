PROGRAMS = libdecm.so

CC = $(CROSS)gcc
CXX = $(CROSS)g++
LINKER = $(CROSS)g++
STRIP = $(CROSS)strip
AR = $(CROSS)ar

SRCDIRS = .

SRCS = $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.c))
SRCS1= $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.cpp))
OBJS = $(SRCS:.c=.o) $(SRCS1:.cpp=.o)

CFLAGS := -O2 -Wundef -Wall -Wchar-subscripts -Wsign-compare -Wuninitialized -O -Wno-missing-braces -Wnested-externs -Wmissing-declarations -Wmissing-prototypes -Werror -Wno-aggressive-loop-optimizations -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -fPIC

CXXFLAGS := -O2 -Wundef -Wall -Wchar-subscripts -Wsign-compare -Wuninitialized -O -Wno-missing-braces -Wmissing-declarations -Werror -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -fPIC

ifeq ($(CROSS), arm-linux-androideabi-)
CFLAGS += -DARM_LINUX_ANDROIDEABI
CXXFLAGS += -DARM_LINUX_ANDROIDEABI
INCLUDE=-Ilibkdm -Itinyxml
LINKLIBS=tinyxml/libtinyxml.a libkdm/libkdm.a mbedtls/library/libmbedcrypto.a
else
INCLUDE=-Ilibkdm -Itinyxml
LINKLIBS=tinyxml/libtinyxml.a libkdm/libkdm.a mbedtls/library/libmbedcrypto.a
endif

all: $(PROGRAMS)

$(PROGRAMS): $(OBJS)
	@echo linking $@
	$(CXX) -shared -o $@ $^ $(LINKLIBS)
	$(STRIP) $@

%.o: %.c
	@echo compiling $<
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

%.o: %.cpp
	@echo compiling $<
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

clean: 
	rm -f $(OBJS) *~ $(PROGRAMS)
