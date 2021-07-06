#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <assert.h>
#include <sys/mman.h>
#include "rmalloc.h"

#define FIRSTFIT_CHECK (policy == FirstFit)
#define WORSTFIT_CHECK (policy == WorstFit)
#define BESTFIT_CHECK (policy == BestFit)
#define NEED_MMAP (now_chunk == NULL || now_chunk->size < s)
#define CAN_SHRINK (ori_size[i] - sizeof(rm_header) == iterator->size && address[i] == iterator)
#define SCAN_FREE (iterator = rm_free_list.next; iterator != NULL; iterator = iterator->next)
#define SCAN_USED (iterator = rm_used_list.next; iterator != NULL; iterator = iterator->next)
#define Unlinked_left_AND_Linked_right ((rm_header_ptr)(_data(past) + past->size) != target_chunk && (rm_header_ptr)(_data(target_chunk) + target_chunk->size) == iterator)
#define Unlinked_left_AND_Unlinked_right ((rm_header_ptr)(_data(past) + past->size) < target_chunk && (rm_header_ptr)(_data(target_chunk) + target_chunk->size) < iterator)
#define Linked_left_AND_Linked_right ((rm_header_ptr)(_data(past) + past->size) == target_chunk && (rm_header_ptr)(_data(target_chunk) + target_chunk->size) == iterator)
#define Linked_left_AND_Unlined_right ((rm_header_ptr)(_data(past) + past->size) == target_chunk && (rm_header_ptr)(_data(target_chunk) + target_chunk->size) != iterator)
#define Scan_Free_Unlinked_right (iterator == NULL)
#define FIND_MMAP_LIST (int i = 0; i < mmap_count; i++)
#define NEED_SPLIT (s < now_chunk->size)
#define FIT_CHUNK (s == now_chunk->size)

rm_option policy = FirstFit;
rm_header rm_free_list = {0x0, 0};
rm_header rm_used_list = {0x0, 0};
int mmap_count = 0;

void *address[5000];
size_t ori_size[5000];

static void *_data(rm_header_ptr e)
{
	return ((void *)e) + sizeof(rm_header);
}

void *rmalloc(size_t s)
{
	if (s <= 0)
		return NULL;

	rm_header_ptr now_chunk = NULL;
	rm_header_ptr iterator = NULL;
	rm_header_ptr trace_chunk = &rm_free_list;
	rm_header_ptr past_chunk = &rm_free_list;
	int best = (int)sysconf(_SC_PAGESIZE);
	int worst = -1;
	iterator = rm_free_list.next;

	for SCAN_FREE
		{
			if FIRSTFIT_CHECK
			{
				if ((int)(s <= iterator->size - sizeof(rm_header)))
				{
					now_chunk = iterator;
					break;
				}
				trace_chunk = iterator;
			}
			if WORSTFIT_CHECK
			{
				if ((int)(iterator->size - s - sizeof(rm_header)) >= worst)
				{
					worst = (int)(iterator->size - s);
					trace_chunk = past_chunk;
					now_chunk = iterator;
				}
			}
			if BESTFIT_CHECK
			{
				if ((int)(iterator->size - s - sizeof(rm_header)) <= best && (int)(iterator->size - s - sizeof(rm_header)) >= 0)
				{
					best = (int)(iterator->size - s);
					trace_chunk = past_chunk;
					now_chunk = iterator;
				}
			}
			past_chunk = iterator;
		}

	if NEED_MMAP
	{
		size_t mmap_size = get_size(s);
		now_chunk = mmap(NULL, mmap_size, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0); //page size만큼 메모리 할당받고 그 메모리의 시작주소를 가져옴
		if (now_chunk == NULL)
			return NULL; //mmap이 실패했을 때를 의미한다.
		change_value(now_chunk, mmap_size);

		now_chunk->size = mmap_size - sizeof(rm_header);
		now_chunk->next = trace_chunk->next;
		trace_chunk->next = now_chunk;
	}

	if NEED_SPLIT
		split_chunk(trace_chunk, now_chunk, s); //chunk가 클 때
	else if FIT_CHUNK
		insert_chunk(trace_chunk, now_chunk, s); //free list에 있는 사이즈와 동일할때

	return _data(now_chunk);
}

static void insert_chunk(rm_header_ptr past_chunk, rm_header_ptr now_chunk, size_t s)
{
	past_chunk->next = now_chunk->next;
	now_chunk->next = rm_used_list.next;
	rm_used_list.next = now_chunk;
}

static void split_chunk(rm_header_ptr past_chunk, rm_header_ptr big_chunk, size_t s)
{
	rm_header_ptr surplus = (rm_header_ptr)(s + _data(big_chunk));
	rm_header_ptr iterator = NULL;
	rm_header_ptr temp = &rm_used_list;
	surplus->size = big_chunk->size - s - sizeof(rm_header);
	surplus->next = big_chunk->next;
	past_chunk->next = surplus;
	big_chunk->size = s;

	big_chunk->size = s;
	big_chunk->next = rm_used_list.next;
	rm_used_list.next = big_chunk;
}

static void change_value(rm_header_ptr now_chunk, size_t mmap_size)
{
	ori_size[mmap_count] = mmap_size;
	address[mmap_count] = now_chunk;
	mmap_count++;
	if (mmap_count > 1000)
	{
		printf("If you wanna get a address by usning rmalloc, you should rfree() to other pointer\n");
		exit(1);
	}
}

void rfree(void *p)
{
	rm_header_ptr iterator = NULL, target_chunk = NULL, past = &rm_used_list, used = &rm_used_list;
	for SCAN_USED
		{
			if (p == _data(iterator))
			{ // used를 free로 보내기
				target_chunk = iterator;
				break;
			}
			past = iterator;
		}

	if (target_chunk == NULL)
	{	
		printf("Improper Accessing Address\n");
		exit(1);
	}
	past->next = target_chunk->next;

	past = &rm_free_list;
	used = &rm_used_list;

	for SCAN_FREE
		{
			if (Unlinked_left_AND_Linked_right)
			{
				target_chunk->size = target_chunk->size + sizeof(rm_header) + iterator->size;
				target_chunk->next = iterator->next;
				past->next = target_chunk;
				return;
			}
			if (Unlinked_left_AND_Unlinked_right)
			{
				target_chunk->next = iterator;
				past->next = target_chunk;
				return;
			}
			if (Linked_left_AND_Linked_right)
			{
				past->size = past->size + sizeof(rm_header) + target_chunk->size + sizeof(rm_header) + iterator->size;
				past->next = iterator->next;
				return;
			}
			if (Linked_left_AND_Unlined_right)
			{
				past->size = past->size + sizeof(rm_header) + target_chunk->size;
				return;
			}
			past = iterator;
		}

	if (Scan_Free_Unlinked_right)
	{
		past->next = target_chunk;
		target_chunk->next = iterator;
		return;
	}

	target_chunk->next = rm_free_list.next;
	rm_free_list.next = target_chunk;
	return;
}

void *rrealloc(void *p, size_t s)
{
	if (s == 0)
	{
		rfree(p);
		return NULL;
	}
	void *oldptr = p;
	void *newptr;
	size_t copySize = 0;
	newptr = rmalloc(s);
	if (newptr == NULL)
		return NULL;

	rm_header_ptr iterator = NULL;
	for SCAN_USED
		{
			if (_data(iterator->next) == oldptr)
				copySize = iterator->next->size;
		}
	if (copySize == 0)
	{
		p = rmalloc(s);
		return p;
	}

	if (s == copySize)
		return p;

	if (s < copySize)
		copySize = s;

	memcpy(newptr, oldptr, copySize);
	rfree(oldptr);
	return newptr;
}

void rmshrink()
{
	for FIND_MMAP_LIST
	{
		rm_header_ptr temp = &rm_free_list;
		rm_header_ptr iterator = NULL;

		for SCAN_FREE
			{
				if CAN_SHRINK
				{
					printf("Shrink Executed\nAddress == %p    Size == %ld (Bytes) \n", _data(address[i]), ori_size[i]);
					temp->next = iterator->next; //free list 이어주기
					munmap(address[i], ori_size[i]);
					break;
				}
				temp = iterator;
			}
	}
}

void rmconfig(rm_option option) //fistfit, bestfit, worstfit  => default = firstfit.
{
	printf("Changing policy\n");
	policy = option;
}

void rmprint()
{
	rm_header_ptr itr;
	int i;

	printf("==================== rm_free_list ====================\n");
	for (itr = rm_free_list.next, i = 0; itr != 0x0; itr = itr->next, i++)
	{
		printf("%3d:%p:%8d:", i, ((void *)itr) + sizeof(rm_header), (int)itr->size);

		int j;
		char *s = ((char *)itr) + sizeof(rm_header);
		for (j = 0; j < (itr->size >= 8 ? 8 : itr->size); j++)
			printf("%02x ", s[j]);
		printf("\n");
	}
	printf("\n");

	printf("==================== rm_used_list ====================\n");
	for (itr = rm_used_list.next, i = 0; itr != 0x0; itr = itr->next, i++)
	{
		printf("%3d:%p:%8d:", i, ((void *)itr) + sizeof(rm_header), (int)itr->size);

		int j;
		char *s = ((char *)itr) + sizeof(rm_header);
		for (j = 0; j < (itr->size >= 8 ? 8 : itr->size); j++)
			printf("%02x ", s[j]);
		printf("\n");
	}
	printf("\n");
}

size_t get_size(size_t s)
{
	size_t page_size = sysconf(_SC_PAGESIZE);
	size_t size_with_header = s + sizeof(rm_header);
	size_t pages = size_with_header / page_size + 1;
	size_t mmap_size = pages * page_size;
	return mmap_size;
}