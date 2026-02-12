/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <errno.h>

#ifdef CONFIG_SHARED_MULTI_HEAP
#include <inttypes.h>
#include <stdlib.h>
#include <zephyr/multi_heap/shared_multi_heap.h>
#include <zephyr/sys/mem_stats.h>
#include <zephyr/sys/sys_heap.h>

#define HEAP_SIZE 4096
static uint8_t heap_mem[HEAP_SIZE] __aligned(sizeof(void *));

/*
 * For shared_multi_heap, we could have such an API like this:

#include <zephyr/sys/mem_stats.h>

void get_smh_stats_for_attr(enum shared_multi_heap_attr attr) {
    struct sys_memory_stats stats;
    size_t total_max = 0, total_alloc = 0, total_free = 0;

    // You'd need access to smh_data (currently static in shared_multi_heap.c)
    for (size_t i = 0; i < smh_data[attr].heap_cnt; i++) {
        sys_heap_runtime_stats_get(&smh_data[attr].heap_pool[i], &stats);
        total_max += stats.max_allocated_bytes;
        total_alloc += stats.allocated_bytes;
        total_free += stats.free_bytes;
    }

    printk("Attr %d: max=%zu alloc=%zu free=%zu\n",
           attr, total_max, total_alloc, total_free);
}

 * However, to avoid touching that subsystem, implement a workaround.
 */

/*
 * Keep a local sys_heap reference to track stats.
 *
 * We maintain our own sys_heap structure that mirrors the one used by
 * shared_multi_heap. This allows us to call sys_heap_runtime_stats_get()
 * to monitor heap usage including max_allocated_bytes (peak usage).
 */
static struct sys_heap local_heap;

static void print_heap_stats(const char *label)
{
#ifdef CONFIG_SYS_HEAP_RUNTIME_STATS
	struct sys_memory_stats stats;

	int ret = sys_heap_runtime_stats_get(&local_heap, &stats);
	if (ret == 0) {
		printf("%s:\n", label);
		printf("  Free bytes:          %zu\n", stats.free_bytes);
		printf("  Allocated bytes:     %zu\n", stats.allocated_bytes);
		printf("  Max allocated bytes: %zu\n", stats.max_allocated_bytes);
		printf("  Min free bytes:      %zu\n",
		       stats.free_bytes + stats.allocated_bytes - stats.max_allocated_bytes);
	} else {
		printf("%s: Failed to get stats (err: %d)\n", label, ret);
	}
#else
	printf("%s: Stats not available (CONFIG_SYS_HEAP_RUNTIME_STATS not enabled)\n", label);
#endif
}

static void demo_shared_multi_heap_stats(void)
{
	void *ptr1, *ptr2, *ptr3;
	int ret;

	printf("\n=== Shared Multi-Heap Runtime Stats Demo ===\n\n");

	/* Initialize the shared multi-heap pool */
	ret = shared_multi_heap_pool_init();
	if (ret != 0 && ret != EALREADY) {
		printf("Failed to init pool: %d\n", ret);
		return;
	}

	/*
	 * Initialize our local heap for stats tracking.
	 * Note: We init first, then shared_multi_heap_add() will re-init the
	 * same memory. Both heap structures will point to the same underlying
	 * z_heap in the memory region, so stats will be synchronized.
	 */
	sys_heap_init(&local_heap, (void *)heap_mem, HEAP_SIZE);

	/* Add the heap region to shared multi-heap (will re-init the same memory) */
	struct shared_multi_heap_region region = {
		.attr = SMH_REG_ATTR_CACHEABLE,
		.addr = (uintptr_t)heap_mem,
		.size = HEAP_SIZE,
	};

	ret = shared_multi_heap_add(&region, NULL);
	if (ret != 0) {
		printf("Failed to add heap: %d\n", ret);
		return;
	}

	printf("Added %d byte heap with CACHEABLE attribute\n\n", HEAP_SIZE);

	/* Show initial stats */
	print_heap_stats("Initial state");

	/* Allocate some memory */
	printf("\nAllocating 256 bytes...\n");
	ptr1 = shared_multi_heap_alloc(SMH_REG_ATTR_CACHEABLE, 256);
	if (ptr1) {
		print_heap_stats("After first allocation");
	}

	/* Allocate more memory */
	printf("\nAllocating 512 bytes...\n");
	ptr2 = shared_multi_heap_alloc(SMH_REG_ATTR_CACHEABLE, 512);
	if (ptr2) {
		print_heap_stats("After second allocation");
	}

	/* Allocate even more */
	printf("\nAllocating 1024 bytes...\n");
	ptr3 = shared_multi_heap_alloc(SMH_REG_ATTR_CACHEABLE, 1024);
	if (ptr3) {
		print_heap_stats("After third allocation (peak usage)");
	}

	/* Free some memory */
	printf("\nFreeing second allocation (512 bytes)...\n");
	shared_multi_heap_free(ptr2);
	print_heap_stats("After freeing middle allocation");

	printf("\nNote: max_allocated_bytes shows peak usage!\n");
	printf("      min_free_bytes = total_size - max_allocated_bytes\n");

	/* Clean up */
	if (ptr1) {
		shared_multi_heap_free(ptr1);
	}
	if (ptr3) {
		shared_multi_heap_free(ptr3);
	}

	printf("\n=== Demo Complete ===\n");
}
#endif /* CONFIG_SHARED_MULTI_HEAP */

int main(void)
{
	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);

#ifdef CONFIG_SHARED_MULTI_HEAP
	demo_shared_multi_heap_stats();
#endif

	return 0;
}
