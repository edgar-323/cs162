#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

/* Function pointers to hw3 functions */
void* (*mm_malloc)(size_t);
void* (*mm_realloc)(void*, size_t);
void (*mm_free)(void*);
void (*print_list)(void);

void load_alloc_functions() {
    void *handle = dlopen("hw3lib.so", RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    char* error;
    mm_malloc = dlsym(handle, "mm_malloc");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    mm_realloc = dlsym(handle, "mm_realloc");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    mm_free = dlsym(handle, "mm_free");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    print_list = dlsym(handle, "print_list");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

}

int main() {
    load_alloc_functions();
    /**************************************************************************/
    int *data = (int*) mm_malloc(sizeof(int));
    printf("data = %p\n", data);
    print_list();
    assert(data != NULL);
    data[0] = 0x162;
    mm_free(data);
    print_list();
    printf("malloc test 0 successful!\n");
    /**************************************************************************/
    size_t big_block_size = 20 * sizeof(void);
    printf("%s%d\n", "big_block_size = ", (int) big_block_size);
    size_t small_block_size = 5 * sizeof(void);
    void* data0 = mm_malloc(big_block_size);
    print_list();
    void* data1 =  mm_malloc(big_block_size);
    print_list();
    int d = (data1 - data0);
    printf("data1 - data0 = %d\n", d);
    printf("data0 = %p\n", data0);
    printf("data1 = %p\n", data1);

    mm_free(data0);
    print_list();
    mm_free(data1);
    print_list();

    void* data2 = mm_malloc(small_block_size);
    print_list();
    void* data3 = mm_malloc(small_block_size);
    print_list();
    printf("data2 = %p\n", data2);
    printf("data3 = %p\n", data3);
    printf("data3 - data2 = %d\n", (int) (data3 - data2));
    printf("data1 - data2 = %d\n", (int) (data1 - data2));
    mm_free(data2);
    print_list();
    mm_free(data3);
    print_list();
    return 0;
}
