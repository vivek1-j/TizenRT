/****************************************************************************
 *
 * Copyright 2023 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/
/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>
#include <stdlib.h>
#include <debug.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <queue.h>
#include <sys/types.h>
#include <tinyara/mm/mm.h>
#include <tinyara/mm/heap_regioninfo.h>
#include <arch/chip/memory_region.h>
#include <tinyara/binfmt/elf.h>

#include "sched/sched.h"
#include "binary_manager/binary_manager_internal.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define MEM_USED   0
#define MEM_LEAK   1
#define MEM_BROKEN 2
#define CMN_BIN_IDX 0

#define MEM_ACCESS_UNIT    0x04
#define MAX_ALLOC_COUNT    CONFIG_MEM_LEAK_CHECKER_MAX_ALLOC_COUNT
#define HASH_SIZE          CONFIG_MEM_LEAK_CHECKER_HASH_TABLE_SIZE
#define MEM_DUMP_MAX_BYTES 32

#define MM_PREV_NODE_SIZE(x)            ((x)->preceding & ~MM_ALLOC_BIT)

struct alloc_node_info_s {
	volatile struct mm_allocnode_s *node;
	struct alloc_node_info_s *next;
};

static struct alloc_node_info_s **g_hash_table;
static struct alloc_node_info_s *g_node_info;

static int hash_init(void)
{
	int index;

	g_hash_table = (struct alloc_node_info_s **)malloc(sizeof(struct alloc_node_info_s *) * HASH_SIZE);
	if (!g_hash_table) {
		return ERROR;
	}

	g_node_info = (struct alloc_node_info_s*)malloc(sizeof(struct alloc_node_info_s) * MAX_ALLOC_COUNT);
	if (!g_node_info) {
		free(g_hash_table);
		return ERROR;
	}

	for (index = 0; index < HASH_SIZE; ++index) {
		g_hash_table[index] = NULL;
	}

	return OK;
}

static void hash_deinit(void)
{
	free(g_hash_table);
	free(g_node_info);
	g_hash_table = NULL;
	g_node_info = NULL;
}

static void add_hash(int index)
{
	long key;
	struct alloc_node_info_s *cur;

	key = (long)g_node_info[index].node % HASH_SIZE;
	if (g_hash_table[key] == NULL) {
		g_hash_table[key] = &g_node_info[index];
		return;
	}

	cur = g_hash_table[key];
	while (cur->next) {
		cur = cur->next;
	}
	cur->next = &g_node_info[index];
}

static bool search_hash(unsigned long value)
{
	long key = value % HASH_SIZE;
	struct alloc_node_info_s *cur = g_hash_table[key];

	while (cur != NULL) {
		if ((unsigned long)cur->node == value) {
			if (cur->node->reserved == MEM_USED) {
				return false;
			}
			cur->node->reserved = MEM_USED;
			return true;
		}
		if (cur->next == NULL) {
			return false;
		}
		cur = cur->next;
	}
	return false;
}

static int get_node_cnt(struct mm_heap_s *heap)
{
	volatile struct mm_allocnode_s *node;
	mmsize_t node_size;
	node_size = SIZEOF_MM_ALLOCNODE;

	int ret = 0;
	
#if CONFIG_KMM_REGIONS > 1
	int region;
#else
#define region 0
#endif

	/* Visit each region */

#if CONFIG_KMM_REGIONS > 1
	for (region = 0; region < heap->mm_nregions; region++)
#endif
	{
		node_size = SIZEOF_MM_ALLOCNODE;
		for (node = heap->mm_heapstart[region]; node < heap->mm_heapend[region]; node = (struct mm_allocnode_s *)((char *)node + node->size)) {
			ASSERT(node->size);
			/* Ignore the heap start checking, because there is a guard node in heap start */
			if (node == heap->mm_heapstart[region]) {
				continue;
			}
			/* Check broken link */
			if (node_size != MM_PREV_NODE_SIZE(node)) {
				continue;
			}
			node_size = node->size;
			/* Check if the node corresponds to an allocated memory chunk */
			if ((node->preceding & MM_ALLOC_BIT) != 0) {
				ret++;
			}
		}
	}

	return ret;
}

static void fill_hash_table(struct mm_heap_s *heap, int *leak_cnt, int *broken_cnt)
{
	volatile struct mm_allocnode_s *node;
	mmsize_t node_size;
	node_size = SIZEOF_MM_ALLOCNODE;

	mm_takesemaphore(heap);

#if CONFIG_KMM_REGIONS > 1
	int region;
#else
#define region 0
#endif

	/* Visit each region */

#if CONFIG_KMM_REGIONS > 1
	for (region = 0; region < heap->mm_nregions; region++)
#endif
	{
		node_size = SIZEOF_MM_ALLOCNODE;
		for (node = heap->mm_heapstart[region]; node < heap->mm_heapend[region]; node = (struct mm_allocnode_s *)((char *)node + node->size)) {
			ASSERT(node->size);
			/* Ignore the heap start checking, because there is a guard node in heap start */
			if (node == heap->mm_heapstart[region]) {
				continue;
			}

			/* Check broken link */
			if (node_size != MM_PREV_NODE_SIZE(node)) {
				node->reserved = MEM_BROKEN;
				(*broken_cnt)++;
				continue;
			}
			node_size = node->size;
			if ((unsigned long)node + (unsigned long)SIZEOF_MM_ALLOCNODE == (unsigned long)g_node_info || 
					(unsigned long)node + (unsigned long)SIZEOF_MM_ALLOCNODE == (unsigned long)g_hash_table) {
				continue;
			}
			/* Check if the node corresponds to an allocated memory chunk */
			if ((node->preceding & MM_ALLOC_BIT) != 0) {
				g_node_info[*leak_cnt].node = node;
				g_node_info[*leak_cnt].next = NULL;
				node->reserved = MEM_LEAK;
				add_hash(*leak_cnt);
				(*leak_cnt)++;
			}
		}
	}
	mm_givesemaphore(heap);
}

static void search_addr(void *start_addr, void *end_addr, int *leak_cnt)
{
	/* This function traverse the memory from start_addr to end_addr for comparing the address based on hash table. */
	void *leak_chk;

	/* Not to access over its region, subtract 0x04 from the end of the address. */
	for (leak_chk = start_addr; leak_chk < end_addr - MEM_ACCESS_UNIT; leak_chk++) {
		if (search_hash(*(unsigned long volatile *)leak_chk - (unsigned long)SIZEOF_MM_ALLOCNODE)) {
			(*leak_cnt)--;
		}
	}
}

static void heap_check(struct mm_heap_s *heap, int checker_pid, int *leak_cnt)
{
	void *leak_chk;
	struct mm_allocnode_s *visit_node;
	void *exclude_top;
	void *exclude_bottom;

	struct tcb_s *ctcb = sched_gettcb(checker_pid);
	ASSERT(ctcb != NULL);
	exclude_top = ctcb->adj_stack_ptr;
	exclude_bottom = ctcb->adj_stack_ptr - ctcb->adj_stack_size;

#if CONFIG_KMM_REGIONS > 1
	int region;
#else
#define region 0
#endif

	/* Visit each region */

#if CONFIG_KMM_REGIONS > 1
	for (region = 0; region < heap->mm_nregions; region++)
#endif
	{
		for (visit_node = heap->mm_heapstart[region]; visit_node < heap->mm_heapend[region]; visit_node = (struct mm_allocnode_s *)((char *)visit_node + visit_node->size)) {
			if ((visit_node->preceding & MM_ALLOC_BIT) != 0) {
				if ((void *)((char *)visit_node + SIZEOF_MM_ALLOCNODE) == (void *)g_node_info) {
					continue;
				}
				for (leak_chk = (void *)visit_node; leak_chk < (void *)(((char *)visit_node) + visit_node->size); leak_chk++) {
					if ((leak_chk >= exclude_bottom && leak_chk <= exclude_top)) {
						continue;
					}
					if (search_hash(*(unsigned long volatile *)leak_chk - (unsigned long)SIZEOF_MM_ALLOCNODE)) {
						(*leak_cnt)--;
					}
				}
			}
		}
	}
}

static struct mm_heap_s * init_mem_leak_checker(int checker_pid, char *bin_name);

static void ram_check(struct mm_heap_s *heap, int checker_pid, char *bin_name, int *leak_cnt)
{
	
#ifdef CONFIG_APP_BINARY_SEPARATION
	bin_addr_info_t *info;
	struct mm_heap_s *kheap;
	int bin_idx;

	info = (bin_addr_info_t *)get_bin_addr_list();
	for (bin_idx = 0; bin_idx <= CONFIG_NUM_APPS; bin_idx++) {
		if (strncmp(BIN_NAME(bin_idx), bin_name, strlen(bin_name)) == 0) {
			break;
		}
	}
#endif
	/* Visit all the data regions
	 */
	int mem_region_idx;
	for (mem_region_idx = 0; mem_region_idx < MEM_VAR_REGION_COUNT; mem_region_idx++) {
		search_addr(variable_region_start_addr[mem_region_idx], variable_region_end_addr[mem_region_idx], leak_cnt);
	}

#ifdef CONFIG_APP_BINARY_SEPARATION
	if (strncmp(bin_name, "kernel", strlen("kernel") + 1) == 0) {
		/* do nothing */
	} else {
#ifdef CONFIG_SUPPORT_COMMON_BINARY
		search_addr((void *)info[CMN_BIN_IDX].data_addr, (void *)(info[CMN_BIN_IDX].data_addr + info[CMN_BIN_IDX].data_size), leak_cnt);
		search_addr((void *)info[CMN_BIN_IDX].bss_addr, (void *)(info[CMN_BIN_IDX].bss_addr + info[CMN_BIN_IDX].bss_size), leak_cnt);
#endif
		/* search the bss and data region of the loadable app */
		search_addr((void *)info[bin_idx].data_addr, (void *)(info[bin_idx].data_addr + info[bin_idx].data_size), leak_cnt);
		search_addr((void *)info[bin_idx].bss_addr, (void *)(info[bin_idx].bss_addr + info[bin_idx].bss_size), leak_cnt);
		/* search the kernel heap first */
		kheap = kmm_get_baseheap();
		heap_check(kheap, checker_pid, leak_cnt);
	}
#endif

	/* Visit heap region */
	heap_check(heap, checker_pid, leak_cnt);
}

static void print_mem_hex_dump(void *addr, size_t alloc_size)
{
	unsigned char *ptr = (unsigned char *)addr;
	size_t dump_size = (alloc_size < MEM_DUMP_MAX_BYTES) ? alloc_size : MEM_DUMP_MAX_BYTES;
	size_t i;

	printf("[DATA] ");
	for (i = 0; i < dump_size; i++) {
		printf("%02x ", ptr[i]);
		if ((i + 1) % 16 == 0 && (i + 1) < dump_size) {
			printf("\n       ");
		}
	}
	printf("\n");
}

static void print_info(struct mm_heap_s *heap, int leak_cnt, int broken_cnt)
{
	volatile struct mm_allocnode_s *node;
	uint32_t owner_addr;	

	if (leak_cnt > 0 || broken_cnt > 0) {
		printf("Type   |    Addr    | Size(byte) |    Owner   | PID \n");
		printf("---------------------------------------------------\n");

		mm_takesemaphore(heap);

#if CONFIG_KMM_REGIONS > 1
		int region;
#else
#define region 0
#endif

		/* Visit each region */

#if CONFIG_KMM_REGIONS > 1
		for (region = 0; region < heap->mm_nregions; region++)
#endif
		{
			for (node = heap->mm_heapstart[region]; node <  heap->mm_heapend[region]; node = (struct mm_allocnode_s *)((char *)node + node->size)) {
				ASSERT(node->size);
				if (node->reserved == MEM_LEAK) {
					/* alloc_call_addr can be from kernel, app or common binary.
					* based on the text addresses printed, user needs to check the
					* corresponding binaries accordingly
					*/
					owner_addr = (uint32_t)node->alloc_call_addr;
					pid_t pid = node->pid;
					if (pid < 0) {
						/* For stack allocated node, pid is negative value.
						* To use the pid, change it to original positive value.
						*/
						pid = (-1) * pid;
					}
					printf("LEAK   | %10p |  %8d  | %10p | %d\n", (void *)((char *)node + SIZEOF_MM_ALLOCNODE), node->size - SIZEOF_MM_ALLOCNODE, owner_addr, pid);
					print_mem_hex_dump((void *)((char *)node + SIZEOF_MM_ALLOCNODE), node->size - SIZEOF_MM_ALLOCNODE);
				} else if (node->reserved == MEM_BROKEN) {
					printf("BROKEN | %p\n", node);
				}
			}
		}

		mm_givesemaphore(heap);

		printf("*** %d LEAKS, %d BROKENS.\n", leak_cnt, broken_cnt);
	} else {
		printf("*** NO MEMORY LEAK.\n");
	}
}

int run_mem_leak_checker(int checker_pid, char *bin_name)
{
	int leak_cnt = 0;
	int node_cnt = 0;
	int broken_cnt = 0;
	struct mm_heap_s *heap = NULL;

	if (strncmp(bin_name, "kernel", strlen("kernel") + 1) == 0) {
		heap = kmm_get_baseheap();
	} 
#ifdef CONFIG_APP_BINARY_SEPARATION
	else {
		heap = mm_get_app_heap_with_name(bin_name);
	}
#endif

	if (!heap) {
		printf("Can't found heap, bin name: %s", bin_name);
		return ERROR;
	}

	node_cnt = get_node_cnt(heap);
	if (MAX_ALLOC_COUNT < node_cnt) {
		printf("Available buffer size (%d) is small.\nPlease increase CONFIG_MEM_LEAK_CHECKER_MAX_ALLOC_COUNT value more than %d.\n", MAX_ALLOC_COUNT, node_cnt);
		return ERROR;
	}

	if (g_hash_table || g_node_info) {
		printf("mem_leak_checker is already running.\n");
		return ERROR;
	}

	if (hash_init() != OK) {
		printf("hash table initialization failed.\n");
		return ERROR;
	}

	fill_hash_table(heap, &leak_cnt, &broken_cnt);

	/* Visit RAM region */
	ram_check(heap, checker_pid, bin_name, &leak_cnt);

	print_info(heap, leak_cnt, broken_cnt);

	hash_deinit();
	return OK;
}

int run_all_mem_leak_checker(int checker_pid)
{
	int ret;
	printf("\nKernel :\n");
	ret = run_mem_leak_checker(checker_pid, "kernel");

	if (ret != OK) {
		return ERROR;
	}

#ifdef CONFIG_APP_BINARY_SEPARATION
	printf("\nBelow are text addresses of loadable apps (and common binary if enabled) :\n");
	printf("The pc value of the allocation can be obtained by subtracting the text start address of the appropriate binary\n\n");
	bin_addr_info_t *bin_addr_info = (bin_addr_info_t *)get_bin_addr_list();
	int bin_idx;
	for (bin_idx = 0; bin_idx <= CONFIG_NUM_APPS; bin_idx++) {
		if (bin_addr_info[bin_idx].text_addr != 0) {
			printf("[%s] Text Addr : %p, Text Size : %u\n", BIN_NAME(bin_idx), bin_addr_info[bin_idx].text_addr, bin_addr_info[bin_idx].text_size);
		}
	}
	printf("\n");
	/* bin_idx value zero is always reserved for common binary, so
	 * skip checking common binary and start checking from index one
	 */
	for (bin_idx = 1; bin_idx <= CONFIG_NUM_APPS; bin_idx++) {
		if (bin_addr_info[bin_idx].text_addr != 0) {
			printf("%s :\n", BIN_NAME(bin_idx));
			ret = run_mem_leak_checker(checker_pid, BIN_NAME(bin_idx));
			if (ret != OK) {
				return ERROR;
			}
		}
	}
#endif
	return OK;
}

#ifdef CONFIG_AUTO_FREE_TASK_MEMORY_ON_EXIT

/****************************************************************************
 * Name: fill_hash_table_for_pid
 *
 * Description:
 *   Fill hash table with allocated nodes matching the specified PID
 *
 ****************************************************************************/

static int fill_hash_table_for_pid(struct mm_heap_s *heap, pid_t target_pid, int *leak_cnt)
{
	volatile struct mm_allocnode_s *node;
	mmsize_t node_size;
	node_size = SIZEOF_MM_ALLOCNODE;

#if CONFIG_KMM_REGIONS > 1
	int region;
#else
#define region 0
#endif

	/* Visit each region */

#if CONFIG_KMM_REGIONS > 1
	for (region = 0; region < heap->mm_nregions; region++)
#endif
	{
		node_size = SIZEOF_MM_ALLOCNODE;
		for (node = heap->mm_heapstart[region]; node < heap->mm_heapend[region]; node = (struct mm_allocnode_s *)((char *)node + node->size)) {
			ASSERT(node->size);
			/* Ignore the heap start checking, because there is a guard node in heap start */
			if (node == heap->mm_heapstart[region]) {
				continue;
			}

			/* Check broken link */
			if (node_size != MM_PREV_NODE_SIZE(node)) {
				continue;
			}
			node_size = node->size;
			if ((unsigned long)node + (unsigned long)SIZEOF_MM_ALLOCNODE == (unsigned long)g_node_info || 
					(unsigned long)node + (unsigned long)SIZEOF_MM_ALLOCNODE == (unsigned long)g_hash_table) {
				continue;
			}
			/* Check if the node corresponds to an allocated memory chunk */
			if ((node->preceding & MM_ALLOC_BIT) != 0) {
				/* Only include nodes matching the target PID */
				pid_t node_pid = node->pid;
				if (node_pid < 0) {
					node_pid = (-1) * node_pid; /* Convert negative stack PID to positive */
				}
				
				if (node_pid == target_pid) {
					g_node_info[*leak_cnt].node = node;
					g_node_info[*leak_cnt].next = NULL;
					node->reserved = MEM_LEAK;
					add_hash(*leak_cnt);
					(*leak_cnt)++;
				}
			}
		}
	}
	
	return OK;
}

/****************************************************************************
 * Name: search_task_stack
 *
 * Description:
 *   Helper function for sched_foreach to search task stacks
 *
 ****************************************************************************/

static void search_task_stack(FAR struct tcb_s *tcb, FAR void *arg)
{
	struct task_memcheck_context_s {
		pid_t exiting_pid;
		int *leak_cnt;
	} *ctx = (struct task_memcheck_context_s *)arg;
	
	/* Skip exiting task's stack */
	if (tcb->pid == ctx->exiting_pid) {
		return;
	}
	
	/* Search this task's stack */
	void *stack_top = tcb->adj_stack_ptr;
	void *stack_bottom = tcb->adj_stack_ptr - tcb->adj_stack_size;
	search_addr(stack_bottom, stack_top, ctx->leak_cnt);
}

/****************************************************************************
 * Name: free_leaks_for_pid
 *
 * Description:
 *   Free all allocated nodes marked as leaks (no references found)
 *
 ****************************************************************************/

static int free_leaks_for_pid(struct mm_heap_s *heap, int *leak_cnt)
{
	volatile struct mm_allocnode_s *node;
	int freed_count = 0;
	size_t freed_bytes = 0;
	mmsize_t node_size;
	node_size = SIZEOF_MM_ALLOCNODE;

#if CONFIG_KMM_REGIONS > 1
	int region;
#else
#define region 0
#endif

	/* Visit each region */

#if CONFIG_KMM_REGIONS > 1
	for (region = 0; region < heap->mm_nregions; region++)
#endif
	{
		node_size = SIZEOF_MM_ALLOCNODE;
		for (node = heap->mm_heapstart[region]; node < heap->mm_heapend[region]; node = (struct mm_allocnode_s *)((char *)node + node->size)) {
			ASSERT(node->size);
			if (node == heap->mm_heapstart[region]) {
				continue;
			}

			if (node_size != MM_PREV_NODE_SIZE(node)) {
				continue;
			}
			node_size = node->size;
			
			/* Free nodes still marked as leak (no references found) */
			if (node->reserved == MEM_LEAK) {
				void *mem_to_free = (void *)((char *)node + SIZEOF_MM_ALLOCNODE);
				size_t mem_size = node->size - SIZEOF_MM_ALLOCNODE;
				
				mm_free(heap, mem_to_free);
				freed_count++;
				freed_bytes += mem_size;
				(*leak_cnt)--;
			}
		}
	}

	if (freed_count > 0) {
		printf("[PID %d] Freed %d allocations (%zu bytes)\n", 
		       heap == g_kmmheap ? -1 : 0, freed_count, freed_bytes);
	}

	return OK;
}

/****************************************************************************
 * Name: check_and_free_task_memory
 *
 * Description:
 *   Check for and free memory allocated by an exiting task that has no
 *   remaining references in the system.
 *
 ****************************************************************************/

int check_and_free_task_memory(pid_t exiting_pid, const char *bin_name)
{
	int leak_cnt = 0;
	struct mm_heap_s *heap = NULL;

	if (!bin_name) {
		printf("Error: bin_name is NULL\n");
		return ERROR;
	}

	/* Determine which heap to check */
	if (strncmp(bin_name, "kernel", strlen("kernel") + 1) == 0) {
		heap = kmm_get_baseheap();
	} 
#ifdef CONFIG_APP_BINARY_SEPARATION
	else {
		heap = mm_get_app_heap_with_name((char *)bin_name);
	}
#endif

	if (!heap) {
		printf("Error: Cannot find heap for bin_name: %s\n", bin_name);
		return ERROR;
	}

	/* Verify heap is actually ready/initialized */
#if CONFIG_KMM_REGIONS > 1
	if (heap->mm_nregions == 0) {
		printf("[PID %d] Skipping - heap has no regions\n", exiting_pid);
		return ERROR;
	}
#endif

	if (!heap->mm_heapstart[0] || !heap->mm_heapend[0]) {
		printf("[PID %d] Skipping - heap region 0 not initialized\n", exiting_pid);
		return ERROR;
	}

	/* Initialize hash table */
	if (hash_init() != OK) {
		/* Memory allocation failed - likely during early system boot
		 * Skip memory cleanup to avoid system crash
		 */
		printf("Warning: Hash table initialization failed - skipping memory cleanup for PID %d\n", exiting_pid);
		return ERROR;
	}

	/* Fill hash table with allocations matching the PID */
	fill_hash_table_for_pid(heap, exiting_pid, &leak_cnt);

	if (leak_cnt == 0) {
		printf("[PID %d] No allocated memory found in %s heap\n", exiting_pid, bin_name);
		hash_deinit();
		return OK;
	}

	/* Check for references in all stacks (excluding exiting task's stack) */
	struct tcb_s *tcb;
	void *exclude_top = NULL;
	void *exclude_bottom = NULL;
	
	/* Get exiting task's stack info to exclude from search */
	tcb = sched_gettcb(exiting_pid);
	if (tcb) {
		exclude_top = tcb->adj_stack_ptr;
		exclude_bottom = tcb->adj_stack_ptr - tcb->adj_stack_size;
	}

	/* Search all task stacks for references */
	mm_takesemaphore(heap);
	
	sched_lock();
	
	/* Setup context for stack search */
	struct task_memcheck_context_s {
		pid_t exiting_pid;
		int *leak_cnt;
	} g_task_memcheck_context;
	
	g_task_memcheck_context.exiting_pid = exiting_pid;
	g_task_memcheck_context.leak_cnt = &leak_cnt;
	
	/* Iterate through all tasks and search their stacks */
	sched_foreach(search_task_stack, &g_task_memcheck_context);
	
	sched_unlock();

	/* Search data regions */
	int mem_region_idx;
	for (mem_region_idx = 0; mem_region_idx < MEM_VAR_REGION_COUNT; mem_region_idx++) {
		search_addr(variable_region_start_addr[mem_region_idx], 
		          variable_region_end_addr[mem_region_idx], &leak_cnt);
	}

#ifdef CONFIG_APP_BINARY_SEPARATION
	/* Search app data regions if applicable */
	bin_addr_info_t *info;
	int bin_idx;
	info = (bin_addr_info_t *)get_bin_addr_list();
	
	if (info) {
		for (bin_idx = 0; bin_idx <= CONFIG_NUM_APPS; bin_idx++) {
			if (info[bin_idx].data_addr != 0) {
				search_addr((void *)info[bin_idx].data_addr, 
				          (void *)(info[bin_idx].data_addr + info[bin_idx].data_size), 
				          &leak_cnt);
			}
			if (info[bin_idx].bss_addr != 0) {
				search_addr((void *)info[bin_idx].bss_addr, 
				          (void *)(info[bin_idx].bss_addr + info[bin_idx].bss_size), 
				          &leak_cnt);
			}
		}
	}
#endif

	mm_givesemaphore(heap);

	/* Search heap region for pointers */
	heap_check(heap, exiting_pid, &leak_cnt);

#ifdef CONFIG_APP_BINARY_SEPARATION
	/* If checking app memory, also search kernel heap */
	if (strncmp(bin_name, "kernel", strlen("kernel") + 1) != 0) {
		struct mm_heap_s *kheap = kmm_get_baseheap();
		if (kheap) {
			heap_check(kheap, exiting_pid, &leak_cnt);
		}
	}
#endif

	/* Free all allocations still marked as leaks */
	free_leaks_for_pid(heap, &leak_cnt);

	/* Cleanup hash table */
	hash_deinit();

	return OK;
}

#endif /* CONFIG_AUTO_FREE_TASK_MEMORY_ON_EXIT */
