CFLAGS = -fPIC -IAFLplusplus/include -MMD
LDFLAGS = -shared
LDLIBS = -lcurl -lcjson
TARGET = libllmmutator.so
DEP = ollama.d mutator.d

.PHONY: all clean model

all: $(TARGET) model

model: model/model_q4km.gguf

clean:
	rm -f libllmmutator.so ollama.o mutator.o $(DEP)

libllmmutator.so: ollama.o mutator.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

-include $(DEP)

mutator.c: AFLplusplus/include/afl-fuzz.h

AFLplusplus/include/afl-fuzz.h: AFLplusplus

AFLplusplus:
	git clone https://github.com/AFLplusplus/AFLplusplus

model/model_q4km.gguf:
	./fetch_and_merge.sh
