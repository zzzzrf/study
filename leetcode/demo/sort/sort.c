#include <stdio.h>
#include <string.h>

static void print(int *arr, int size)
{
    for (int i = 0; i < size; i++)
        printf("arr[%d] = %d\n", i, arr[i]);
    putchar('\n');
}

/* i != j*/
static void swap(int *arr, int i, int j)
{
    arr[i] = arr[i] ^ arr[j];
    arr[j] = arr[i] ^ arr[j];
    arr[i] = arr[i] ^ arr[j];
}

/* 选择排序 */
static void selection_sort(int *arr, int size)
{
    if (arr == NULL || size < 2)
        return ;

    for (int i = 0; i < size; i++)
    {
        int minIdx = i;
        for (int j = i + 1; j < size; j++)
        {
            if (arr[minIdx] > arr[j])
                minIdx = j;
        }
        if (i != minIdx)
            swap(arr, i, minIdx);
    }

    return ;
}

/* 冒泡排序 */
static void bubble_sort(int *arr, int size)
{
    if (arr == NULL || size < 2)
        return ;

    for (int i = size - 1; i >= 0; i--)
    {
        for (int j = 0; j < i; j++)
        {
            if (arr[j] > arr[j + 1])
                swap(arr, j, j + 1);
        }
    }

    return ;
}

/* 插入排序 */
static void insert_sort(int *arr, int size)
{
    if (arr == NULL || size < 2)
        return ;

    for (int i = 1; i < size; i++)
    {
        for (int j = i - 1; j >=0 && arr[j] > arr[j + 1]; j--)
            swap(arr, j, j + 1);
    }

    return ;
}

typedef struct test_vec
{
    int *in;
    int *expected;
    int size;
}test_vec_t;

static int arr0[2][0] =
{
    {},
    {},
};

static int arr1[2][1] =
{
    {1},
    {1},
};

static int arr2[2][2] =
{
    {4,2},
    {2,4},
};

static int arr3[2][3] = 
{
    {4,2,5},
    {2,4,5},
};

static int arr4[2][8] = 
{
    {7,6,5,4,3,2,1,0},
    {0,1,2,3,4,5,6,7},
};

static test_vec_t tests[] = 
{
    {.in = arr0[0], .expected = arr0[1], .size = 0},
    {.in = arr1[0], .expected = arr1[1], .size = 1},
    {.in = arr2[0], .expected = arr2[1], .size = 2},
    {.in = arr3[0], .expected = arr3[1], .size = 3},
    {.in = arr4[0], .expected = arr4[1], .size = 8},
};

typedef struct sort_func
{
    void (*sort)(int *arr, int size);
    void (*test)(struct sort_func *this);
    const char *func_name;
    test_vec_t **tests_vectors;
    int vectors_size;
}sort_func_t;

static void test_one(struct sort_func *this)
{
    
    test_vec_t *tests = *this->tests_vectors;
    int failed = 0;
    int vectors_size = this->vectors_size;

    printf("test %s\n", this->func_name);
    for (int i = 0; i < vectors_size; i++)
    {
        int size = tests[i].size;
        int tmp[size];
        memcpy(tmp, tests[i].in, size*sizeof(int));
        insert_sort(tmp, size);
        if (memcmp(tmp, tests[i].expected, size))
        {
            fprintf(stderr, "test%d failed\n", i);
            failed++;
        }
            
    }
    printf("%d tests, %d pass, %d failed\n\n", vectors_size, vectors_size - failed, failed);
    return ;
}

sort_func_t f_list[] = 
{
    [0] = {.sort = selection_sort,  .func_name = "selection sort",       
                .tests_vectors = (test_vec_t **)tests,   .vectors_size = 5, .test = test_one},

    [1] = {.sort = bubble_sort,     .func_name = "bubble sort",          
                .tests_vectors = (test_vec_t **)tests,   .vectors_size = 5, .test = test_one},

    [2] = {.sort = insert_sort,     .func_name = "insert sort",          
                .tests_vectors = (test_vec_t **)tests,   .vectors_size = 5, .test = test_one},
};

int main()
{
    for (int i = 0; i < 3; i++)
        f_list[i].test(&f_list[i]);

    return 0;
}