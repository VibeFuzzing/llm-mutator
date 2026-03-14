CFLAGS=-fPIC -IAFLplusplus/include
LDFLAGS=-shared
LDLIBS=-lcurl -lcjson
TARGET=libllmmutator.so

.PHONY: all clean

all: $(TARGET)

clean:
	rm -f libllmmutator.so ollama.o mutator.o

libllmmutator.so: ollama.o mutator.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

mutator.c: AFLplusplus/include/afl-fuzz.h

AFLplusplus/include/afl-fuzz.h: AFLplusplus

AFLplusplus:
	git clone https://github.com/AFLplusplus/AFLplusplus
