typedef enum
{
	BestFit,
	WorstFit,
	FirstFit
} rm_option;

struct _rm_header
{
	struct _rm_header *next;
	size_t size;
};

typedef struct _rm_header rm_header;

typedef struct _rm_header *rm_header_ptr;

void *rmalloc(size_t s);

static void change_value(rm_header_ptr now_chunk, size_t mmap_size);

static void insert_chunk(rm_header_ptr past_chunk, rm_header_ptr now_chunk, size_t s);

static void split_chunk(rm_header_ptr past_chunk, rm_header_ptr big_chunk, size_t s);

size_t get_size(size_t s);

void rfree(void *p);

void *rrealloc(void *p, size_t s);

void rmshrink();

void rmconfig(rm_option opt);

void rmprint();