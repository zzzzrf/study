#ifndef __MY_MUTEX_T__
#define __MY_MUTEX_T__

typedef struct mutex_t mutex_t;
typedef enum mutex_type_t mutex_type_t;
typedef enum condvar_type_t condvar_type_t;

enum mutex_type_t {
	MUTEX_TYPE_DEFAULT	= 0,
	MUTEX_TYPE_RECURSIVE	= 1,
};

struct mutex_t {
	void (*lock)(mutex_t *this);
	void (*unlock)(mutex_t *this);
	void (*destroy)(mutex_t *this);
};

mutex_t *mutex_create(mutex_type_t type);

#endif