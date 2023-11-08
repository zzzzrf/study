CC = gcc
TARGET = $(TMPDIR)/sort_test
LIBS = -I$(LIBDIR) -L$(TMPDIR) -lleetcode_utils 
CLFAGS = -g 
DIRS = .
FILES = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.c))
OBJ = $(patsubst %.c,%.o, $(FILES))

$(TARGET):$(OBJ)
	$(CC) -o $@ $^ $(CLFAGS) $(LIBS) 

$(OBJ):%.o:%.c
	$(CC) -c $< -o $@ $(CLFAGS) $(LIBS) 

clean:
	rm -rf $(OBJ) $(TARGET)

