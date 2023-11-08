DIRS = .
FILES = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.c))
OBJ = $(patsubst %.c,%.o, $(FILES))

TARGET = $(TMPDIR)/libleetcode_utils.so

$(TARGET):$(OBJ)
	$(CC) -o $@ $^ $(CLFAGS) -shared -fpic $(LIBS) 

$(OBJ):%.o:%.c
	$(CC) -c $< -shared -fpic -o $@ $(CLFAGS)

clean:
	rm -rf $(OBJ) $(TARGET)