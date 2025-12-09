/****************************************************************************
 *
 * Copyright 2025 Samsung Electronics All Rights Reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sched.h>
#include <errno.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define HOG_MSEC       500
#define YIELD_MSEC     100
#define IMPOSSIBLE_CPU -1
#define CPU_ZERO(s) do { *(s) = 0; } while (0)
#define CPU_SET(c,s) do { *(s) |= (1 << (c)); } while (0)
#define CPU_CLR(c,s) do { *(s) &= ~(1 << (c)); } while (0)
#define CPU_ISSET(c,s) ((*(s) & (1 << (c))) != 0)

#define TEST_ITERATIONS 100

/****************************************************************************
 * Private Data
 ****************************************************************************/

static volatile int g_thread_cpu[CONFIG_TESTING_SMP_NBARRIER_THREADS + 1];
static uint8_t affinity = 0;
static volatile bool g_keep_running = true;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: show_cpu / show_cpu_conditional
 *
 * Description:
 *   These functions display CPU information for threads.
 *
 ****************************************************************************/

static void show_cpu(FAR const char *caller, int threadno)
{
    g_thread_cpu[threadno] = sched_getcpu();
    printf("%s[%d]: Running on CPU%d\n", caller, threadno, g_thread_cpu[threadno]);
}

static void show_cpu_conditional(FAR const char *caller, int threadno)
{
    int cpu = sched_getcpu();

    if (cpu != g_thread_cpu[threadno]) {
        g_thread_cpu[threadno] = cpu;
        printf("%s[%d]: Now running on CPU%d\n", caller, threadno, cpu);
    }
}

/****************************************************************************
 * Name: hog_milliseconds
 *
 * Description:
 *   Delay inline for the specified number of milliseconds.
 *
 ****************************************************************************/

static void hog_milliseconds(unsigned int milliseconds)
{
    volatile unsigned int i;
    volatile unsigned int j;

    for (i = 0; i < milliseconds; i++) {
        for (j = 0; j < CONFIG_BOARD_LOOPSPERMSEC; j++) {
        }
    }
}

/****************************************************************************
 * Name: hog_time
 *
 * Description:
 *   Delay for awhile, calling pthread_yield() now and then to allow other
 *   pthreads to get CPU time.
 *
 ****************************************************************************/

static void hog_time(FAR const char *caller, int threadno)
{
    unsigned int remaining = HOG_MSEC;
    unsigned int hogmsec;

    while (remaining > 0 && g_keep_running) {
        /* Hog some CPU */

        hogmsec = YIELD_MSEC;
        if (hogmsec > remaining) {
            hogmsec = remaining;
        }

        hog_milliseconds(hogmsec);
        remaining -= hogmsec;

        /* Let other threads run */
        pthread_yield();
        show_cpu_conditional(caller, threadno);
    }
}

/****************************************************************************
 * Name: worker_thread
 ****************************************************************************/

static pthread_addr_t worker_thread(pthread_addr_t parameter)
{
    int threadno  = (int)((intptr_t)parameter);
    
    printf("Thread[%d]: Started\n",  threadno);
    show_cpu("Thread", threadno);

    /* Hog some CPU time */
    while (g_keep_running) {
        hog_time("Thread", threadno);
    }

    printf("Thread[%d]: Done\n",  threadno);
    show_cpu_conditional("Thread", threadno);
    return NULL;
}

/****************************************************************************
 * Name: set_affinity
 ****************************************************************************/

static void set_affinity(void)
{
    cpu_set_t cpu_set;

    CPU_ZERO(&cpu_set);
    CPU_SET(affinity, &cpu_set);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set) != 0) {
        printf("Failed to set affinity for CPU %d\n", affinity);
    }
    affinity++;
    if (affinity == CONFIG_SMP_NCPUS) {
        affinity = 0;
    }
}

/****************************************************************************
 * Name: create_worker_threads
 ****************************************************************************/

static int create_worker_threads(pthread_t *threadid, int num_threads)
{
    pthread_attr_t attr;
    int ret;
    int i;

    /* Initialize thread attributes */
    ret = pthread_attr_init(&attr);
    if (ret != 0) {
        printf("pthread_attr_init failed, ret=%d\n", ret);
        return -1;
    }

#ifdef CONFIG_AMP
    /* If AMP is enabled, then we will set the affinity of each thread
     * to run on a different cpu.
     */
    printf("Running with AMP configuration\n");
    uint32_t core_id = 0;
#endif

    for (i = 0; i < num_threads; i++) {
#ifdef CONFIG_AMP
        CPU_ZERO(&attr.affinity);
        CPU_SET(core_id, &attr.affinity);
        core_id++;
        if (core_id == CONFIG_SMP_NCPUS) {
            core_id = 0;
        }
#endif

        ret = pthread_create(&threadid[i], &attr, worker_thread, 
                (pthread_addr_t)((uintptr_t)i + 1));
        if (ret != 0) {
            printf("Error in thread %d create, ret=%d\n", i + 1, ret);
            return -1;
        } else {
            printf("Thread %d created\n", i + 1);
        }

        show_cpu_conditional("Main", 0);
    }

    return 0;
}

/****************************************************************************
 * Name: test_invalid_parameters
 *
 * Description:
 *   Test the sched_cpuon and sched_cpuoff APIs with invalid parameters
 *
 ****************************************************************************/

static int test_invalid_parameters(void)
{
    int ret;
    
    printf("\n=== Testing Invalid Parameters ===\n");
    
    /* Test 1: Try to turn off CPU 0 (should fail) */
    printf("Test 1: Attempting to turn off CPU 0 (should fail)\n");
    ret = sched_cpuoff(0/*, true*/);
    if (ret == 0) {
        printf("ERROR: sched_cpuoff(0) should have failed but succeeded\n");
        return -1;
    } else {
        printf("SUCCESS: sched_cpuoff(0) correctly failed with ret=%d\n", ret);
    }
    
    /* Test 2: Try to turn on CPU 0 (should fail) */
    printf("Test 2: Attempting to turn on CPU 0 (should fail)\n");
    ret = sched_cpuon(0);
    if (ret == 0) {
        printf("ERROR: sched_cpuon(0) should have failed but succeeded\n");
        return -1;
    } else {
        printf("SUCCESS: sched_cpuon(0) correctly failed with ret=%d\n", ret);
    }
    
    /* Test 3: Try to turn off invalid CPU number (should fail) */
    printf("Test 3: Attempting to turn off invalid CPU number (should fail)\n");
    ret = sched_cpuoff(CONFIG_SMP_NCPUS + 1/*, true*/);
    if (ret == 0) {
        printf("ERROR: sched_cpuoff(invalid_cpu) should have failed but succeeded\n");
        return -1;
    } else {
        printf("SUCCESS: sched_cpuoff(invalid_cpu) correctly failed with ret=%d\n", ret);
    }
    
    /* Test 4: Try to turn on invalid CPU number (should fail) */
    printf("Test 4: Attempting to turn on invalid CPU number (should fail)\n");
    ret = sched_cpuon(CONFIG_SMP_NCPUS + 1);
    if (ret == 0) {
        printf("ERROR: sched_cpuon(invalid_cpu) should have failed but succeeded\n");
        return -1;
    } else {
        printf("SUCCESS: sched_cpuon(invalid_cpu) correctly failed with ret=%d\n", ret);
    }
    
    /* Test 5: Try to turn off negative CPU number (should fail) */
    printf("Test 5: Attempting to turn off negative CPU number (should fail)\n");
    ret = sched_cpuoff(-1/*, true*/);
    if (ret == 0) {
        printf("ERROR: sched_cpuoff(-1) should have failed but succeeded\n");
        return -1;
    } else {
        printf("SUCCESS: sched_cpuoff(-1) correctly failed with ret=%d\n", ret);
    }
    
    /* Test 6: Try to turn on negative CPU number (should fail) */
    printf("Test 6: Attempting to turn on negative CPU number (should fail)\n");
    ret = sched_cpuon(-1);
    if (ret == 0) {
        printf("ERROR: sched_cpuon(-1) should have failed but succeeded\n");
        return -1;
    } else {
        printf("SUCCESS: sched_cpuon(-1) correctly failed with ret=%d\n", ret);
    }
    
    printf("=== Invalid Parameter Tests Completed Successfully ===\n\n");
    return 0;
}

/****************************************************************************
 * Name: cpu_hotplug_test
 ****************************************************************************/

static int cpu_hotplug_test(void)
{
    int ret;
    int i;
    
    printf("\n=== CPU Hotplug Test ===\n");
    printf("Testing CPU 1 hotplug for %d iterations\n", TEST_ITERATIONS);
    
    for (i = 0; i < TEST_ITERATIONS; i++) {
        /* Turn off CPU 1 */
        printf("Iteration %d: Turning off CPU 1\n", i+1);
        ret = sched_cpuoff(1/*, true*/);  // true to migrate tasks
        if (ret != 0) {
            printf("ERROR: Failed to turn off CPU 1, ret=%d\n", ret);
            return -1;
        }
        
        sleep(1);  // Wait a bit
        
        /* Turn on CPU 1 */
        printf("Iteration %d: Turning on CPU 1\n", i+1);
        ret = sched_cpuon(1);
        if (ret != 0) {
            printf("ERROR: Failed to turn on CPU 1, ret=%d\n", ret);
            return -1;
        }
        
        sleep(1);  // Wait a bit
        
        printf("Iteration %d: Completed\n", i+1);
    }
    
    printf("=== CPU Hotplug Test Completed Successfully ===\n\n");
    return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * sched_test_main
 ****************************************************************************/

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int cputest_main(int argc, char *argv[])
#endif
{
    pthread_t *threadid;
    int num_threads = CONFIG_TESTING_SMP_NBARRIER_THREADS;
    int ret;
    int i;
	g_keep_running = true;

    /* Initialize data */
    memset(g_thread_cpu, IMPOSSIBLE_CPU, sizeof(g_thread_cpu));
    
    show_cpu("Main", 0);
    printf("Scheduler API Test Application\n");
    printf("==============================\n");
    
    /* Allocate memory for thread IDs */
    threadid = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);
    if (threadid == NULL) {
        printf("Failed to allocate memory for thread IDs\n");
        return -1;
    }
    memset(threadid, 0, sizeof(pthread_t) * num_threads);

    /* Create worker threads on each CPU */
    printf("\n=== Creating Worker Threads ===\n");
    ret = create_worker_threads(threadid, num_threads);
    if (ret != 0) {
        printf("Failed to create worker threads\n");
        free(threadid);
        return -1;
    }

    /* Run invalid parameter tests */
    ret = test_invalid_parameters();
    if (ret != 0) {
        printf("Invalid parameter tests failed\n");
        g_keep_running = false;
        
        /* Wait for threads to finish */
        for (i = 0; i < num_threads; i++) {
            if (threadid[i] != 0) {
                pthread_join(threadid[i], NULL);
            }
        }
        
        free(threadid);
        return -1;
    }

    /* Run CPU hotplug test */
    ret = cpu_hotplug_test();
    if (ret != 0) {
        printf("CPU hotplug test failed\n");
        g_keep_running = false;
        
        /* Wait for threads to finish */
        for (i = 0; i < num_threads; i++) {
            if (threadid[i] != 0) {
                pthread_join(threadid[i], NULL);
            }
        }
        
        free(threadid);
        return -1;
    }

    /* Clean up */
    g_keep_running = false;
    
    /* Wait for threads to finish */
    printf("=== Cleaning up threads ===\n");
    for (i = 0; i < num_threads; i++) {
        if (threadid[i] != 0) {
            pthread_join(threadid[i], NULL);
        }
    }
    
    free(threadid);
    
    printf("=== Test Completed Successfully ===\n");
    show_cpu_conditional("Main", 0);
    return 0;
}

