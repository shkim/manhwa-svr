CC = gcc
CXX = g++
LD = g++

DBGFLAGS = -g -D_DEBUG
#DBGFLAGS = -DNDEBUG -O

CFLAGS = $(DBGFLAGS)

CXXFLAGS = $(DBGFLAGS) -std=c++11 -pthread -fexceptions\
	-Iextern/zlib -Iextern/unzip\
	-Iextern/tbb/include -Iextern/casablanca/Release/include

LDFLAGS = -Lextern/casablanca/Release/build.release/Binaries\
	-Lextern/tbb/lib/intel64/gcc4.4\
	-pthread -std=c++11

LDLIBS = -lcpprest -ltbb

EXTERN_OBJS = unzip.o ioapi.o\
	adler32.o crc32.o infback.o inflate.o trees.o zutil.o\
	compress.o deflate.o inffast.o inftrees.o uncompr.o

OBJS =	PicServer.o TitleVolume.o PicFile.o SerialJobQ.o ZipFile.o\
	readconf.o dbglog.o $(EXTERN_OBJS)

TARGET = PicServer

VPATH = extern/unzip:extern/zlib

all:	$(TARGET)

$(TARGET):	$(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)


clean:
	rm -f $(OBJS) $(TARGET) 
