EXE:=check_upgrade 

#EXTRA_CFLAGS += -O2 -Wall -g $(CFLAGS)
EXTRA_CFLAGS += -O2 -Wall 


OBJS = check_upgrade.o rc4.o

all: $(EXE) 

$(EXE): $(OBJS)
	$(CC)  -o $@ $(OBJS)

clean:
	-@rm -f *~ *.o $(EXE) *.d

$(OBJS): %.o : %.c
	$(CC) -c $< -o $@

