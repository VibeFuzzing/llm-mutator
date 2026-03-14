CFLAGS = -fPIC -IAFLplusplus/include -MMD
LDFLAGS = -shared
LDLIBS = -lcurl -lcjson
TARGET = libllmmutator.so
DEP = ollama.d mutator.d

.PHONY: all clean

all: $(TARGET)

clean:
	rm -f libllmmutator.so ollama.o mutator.o $(DEP)

libllmmutator.so: ollama.o mutator.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

-include $(DEP)

mutator.c: AFLplusplus/include/afl-fuzz.h

AFLplusplus/include/afl-fuzz.h: AFLplusplus

AFLplusplus:
	git clone https://github.com/AFLplusplus/AFLplusplus
