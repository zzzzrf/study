#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <execinfo.h>
#include <dlfcn.h>
#include <bfd.h>
#include <pthread.h>

#include "backtrace.h"
#include "utils/object.h"
#include "utils/uthash.h"

static pthread_mutex_t bfd_mutex;

typedef struct private_backtrace_t private_backtrace_t;

typedef struct {
	char *filename;
	bfd *abfd;
	asymbol **syms;
} bfd_entry_t;

typedef struct {
  void *filename;
  bfd_entry_t *entry;
  UT_hash_handle hh;
} el_t;

el_t *bfds = NULL;

typedef struct {
	bfd_entry_t *entry;
	bfd_vma vma;
	bool found;
	FILE *file;
} bfd_find_data_t;

struct private_backtrace_t
{
    backtrace_t public;
    int frame_count;
    void *frames[];
};

static void println(FILE *file, char *format, ...)
{
	char buf[512];
	va_list args;

	va_start(args, format);
	if (file)
	{
		vfprintf(file, format, args);
		fputs("\n", file);
	}
	else
	{
		vsnprintf(buf, sizeof(buf), format, args);
		fprintf(stderr, "%s", buf);
	}
	va_end(args);
}

static void find_addr(bfd *abfd, asection *section, bfd_find_data_t *data)
{
	bfd_size_type size;
	bfd_vma vma;
	const char *source;
	const char *function;
	char fbuf[512] = "", sbuf[512] = "";
	u_int line;
	int i;

	if (data->found || (bfd_section_flags(section) & SEC_ALLOC) == 0)
	{
		return;
	}
	vma = bfd_section_vma(section);
	if (data->vma < vma)
	{
		return;
	}
	size = bfd_section_size(section);
	if (data->vma >= vma + size)
	{
		return;
	}

	data->found = bfd_find_nearest_line(abfd, section, data->entry->syms,
										data->vma - vma, &source, &function,
										&line);
	if (!data->found)
	{
		return;
	}

	println(data->file, "    -> source file[%s] function[%s] line[%d]\n", source, function, line);
}

static void bfd_entry_destroy(bfd_entry_t *this)
{
	free(this->filename);
	free(this->syms);
	bfd_close(this->abfd);
	free(this);
}

static bfd_entry_t *get_bfd_entry(char *filename)
{
	bool dynamic = FALSE, ok = FALSE;
	bfd_entry_t *entry = NULL;
	long size;
    el_t *d;

	/* check cache */
    HASH_FIND_PTR(bfds, &filename, d);
	if (d)
	{
		return d->entry;
	}

	INIT(entry,
		.abfd = bfd_openr(filename, NULL),
	);

	if (!entry->abfd)
	{
		free(entry);
		return NULL;
	}

	entry->abfd->flags |= BFD_DECOMPRESS;

	if (bfd_check_format(entry->abfd, bfd_archive) == 0 &&
		bfd_check_format_matches(entry->abfd, bfd_object, NULL))
	{
		if (bfd_get_file_flags(entry->abfd) & HAS_SYMS)
		{
			size = bfd_get_symtab_upper_bound(entry->abfd);
			if (size == 0)
			{
				size = bfd_get_dynamic_symtab_upper_bound(entry->abfd);
				dynamic = TRUE;
			}
			if (size >= 0)
			{
				entry->syms = malloc(size);
				if (dynamic)
				{
					ok = bfd_canonicalize_dynamic_symtab(entry->abfd,
														 entry->syms) >= 0;
				}
				else
				{
					ok = bfd_canonicalize_symtab(entry->abfd,
												 entry->syms) >= 0;
				}
			}
		}
	}
	if (ok)
	{
        d = malloc(sizeof(*d));
        d->entry = entry;
        d->filename = strdup(filename);
		entry->filename = strdup(filename);
        HASH_ADD_PTR(bfds, filename, d);
		return entry;
	}
	bfd_entry_destroy(entry);
	return NULL;
}

static void lookup_addr(char *filename, bfd_find_data_t *data)
{
	bfd_entry_t *entry;
	bool old = FALSE;

    pthread_mutex_lock(&bfd_mutex);
	entry = get_bfd_entry(filename);
	if (entry)
	{
		data->entry = entry;
		bfd_map_over_sections(entry->abfd, (void*)find_addr, data);
	}
	pthread_mutex_unlock(&bfd_mutex);
}

static void print_sourceline(FILE *file, char *filename, void *ptr, void *base)
{
	bfd_find_data_t data = {
		.file = file,
		.vma = (uintptr_t)ptr,
	};

	lookup_addr(filename, &data);
}

METHOD(backtrace_t, log_, void,
	private_backtrace_t *this, FILE *file, bool detailed)
{
    size_t i;
    char **strings = NULL;

    println(file, " dumping %d stack frame addresses:\n", this->frame_count);
    for (i = 0; i < this->frame_count; i++)
    {
#ifndef USE_TRACE
        Dl_info info;

        if (dladdr(this->frames[i], &info))
		{
			void *ptr = this->frames[i];

			if (strstr(info.dli_fname, ".so"))
			{
				ptr = (void*)(this->frames[i] - info.dli_fbase);
			}
			if (info.dli_sname)
			{
				println(file, "  %s @ %p (%s+0x%tx) [%p]\n",
						info.dli_fname,
						info.dli_fbase,
						info.dli_sname,
						this->frames[i] - info.dli_saddr,
						this->frames[i]);
			}
			else
			{
				println(file, "  %s @ %p [%p]\n",
						info.dli_fname,
						info.dli_fbase, this->frames[i]);
			}
			if (detailed && info.dli_fname[0])
			{
				print_sourceline(file, (char*)info.dli_fname, ptr, info.dli_fbase);
			}
		}
#else
        if (!strings)
        {
            strings = backtrace_symbols(this->frames, this->frame_count);
        }

        if (strings)
        {
            println(file, "    %s\n", strings[i]);
        }
#endif

    }
    free(strings);

}

METHOD(backtrace_t, destroy, void,
	private_backtrace_t *this)
{
	free(this);
}

backtrace_t *backtrace_create(int skip)
{
    private_backtrace_t *this;
    void *frames[BT_BUF_SIZE];
    int frame_count = 0;

    frame_count = backtrace(frames, BT_BUF_SIZE);
    frame_count = frame_count - skip > 0 ? frame_count - skip : 0;
    this = malloc(sizeof(private_backtrace_t) + frame_count * sizeof(void*));
    memcpy(this->frames, frames + skip, frame_count * sizeof(void*));
    this->frame_count = frame_count;

    this->public.log = _log_;
    this->public.destroy = _destroy;

    return &this->public;
}

void backtrace_dump(char *label, FILE *file, bool detailed)
{
	backtrace_t *backtrace;

	backtrace = backtrace_create(2);

	if (label)
	{
		println(file, "Debug backtrace: %s\n", label);
	}
	backtrace->log(backtrace, file, detailed);
	backtrace->destroy(backtrace);
}

void suppress_bfd_errors (const char *fmt, va_list args)
{
}

void backtrace_deinit()
{
	el_t *p, *tmp;
	
	HASH_ITER(hh, bfds, p, tmp) {
      HASH_DEL(bfds, p);
	  bfd_entry_destroy(p->entry);
	  free(p->filename);
      free(p);
    }
	pthread_mutex_destroy(&bfd_mutex);
}

void backtrace_init()
{
    bfd_init();
    pthread_mutex_init(&bfd_mutex, NULL);
    bfd_set_error_handler(suppress_bfd_errors);
}