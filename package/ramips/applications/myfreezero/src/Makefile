EXE:=freezero.cgi

#EXTRA_CFLAGS += -O2 -Wall -g $(CFLAGS)
EXTRA_CFLAGS += -O2 -Wall 


OBJS = base64.o fzkey.o rc4.o main.o md5.o

all: $(EXE) 

$(EXE): $(OBJS)
	$(CC)  -o $@ $(OBJS)

clean:
	-@rm -f *~ *.o $(EXE) *.d

$(OBJS): %.o : %.c
	$(CC) -c $< -o $@

