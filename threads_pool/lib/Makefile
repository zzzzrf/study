DIRS = .
FILES = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.c))
OBJ = $(patsubst %.c,%.o, $(FILES))
CFLAGS = -g

TARGET = $(TMPDIR)/libprocessor.so

$(TARGET):$(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) -shared -fpic $(LIBS) 

$(OBJ):%.o:%.c
	$(CC) -c $< -shared -fpic -o $@ $(CFLAGS)

clean:
	rm -rf $(OBJ) $(TARGET)
