#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "utils.h"


static int cmp(const void *a, const void *b)
{
    return (*(int *)a > *(int *)b);
}

static void right_sort(int *arr, int size)
{
    qsort(arr, size, sizeof(int), cmp);
    return ;
}

static void print(int *arr, int size)
{
    for (int i = 0; i < size; i++)
        printf("arr[%d] = %d\n", i, arr[i]);
    putchar('\n');
}

static void swap(int *arr, int i, int j)
{
#if 0
    arr[i] = arr[i] ^ arr[j];
    arr[j] = arr[i] ^ arr[j];
    arr[i] = arr[i] ^ arr[j];
#else
    int tmp = arr[i];
    arr[i] = arr[j];
    arr[j] = tmp;
#endif

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

static void merge(int *arr, int left, int mid, int right)
{
    int tmp[right - left + 1];
    int i = 0;
    int p = left, q = mid + 1;
    while (p <= mid && q <= right)
    {
        tmp[i++] = arr[p] > arr[q] ? arr[q++] : arr[p++];
    }

    while (p <= mid)
        tmp[i++] = arr[p++];

    while (q <= right)
        tmp[i++] = arr[q++];

    memcpy(arr + left, tmp, (right - left + 1)*sizeof(int));

    return ;
}

static void merge_process(int *arr, int left, int right)
{
    if (left == right)
        return ;

    int mid = left + ((right - left) >> 1);
    merge_process(arr, left, mid);
    merge_process(arr, mid + 1, right);
    return merge(arr, left, mid, right);
}

/* 归并排序 */
static void merge_sort(int *arr, int size)
{
    if (arr == NULL || size < 2)
        return ;

    return merge_process(arr, 0, size - 1);
}

static void partation(int *arr, int left, int right, int *equal_left, int *equal_right)
{
    int less = left - 1;
    int more = right;
    int i = left;

    while (i < more)
    {
        if (arr[i] > arr[right])
        {
            swap(arr, i, more - 1);
            more--;
        }
        else if (arr[i] < arr[right])
        {
            swap(arr, i, less + 1);
            i++, less++;
        }
        else 
        {
            i++;    
        }
    }
    swap(arr, more, right);
    *equal_left = less + 1;
    *equal_right = more;
}

static void __quick_sort(int *arr, int left, int right)
{
    if (left >= right)
        return ;
    
    int rand_idx = left + (random() % (right - left));
    swap(arr, rand_idx, right);

    int equal_left, equal_right;
    partation(arr, left, right, &equal_left, &equal_right);
    __quick_sort(arr, left, equal_left - 1);
    __quick_sort(arr, equal_right + 1, right);
    return ;
}

static void quick_sort(int *arr, int size)
{
    if (arr == NULL || size < 2)
        return ;

    return __quick_sort(arr, 0, size - 1);
}

typedef void (*sort_func)(int *arr, int size);

typedef struct test_t
{ 
    void (*run)(struct test_t *this);
    void (*destroy)(struct test_t *this);
    char *test_func;

    sort_func right;
    sort_func test;
    int test_times;
    int max_size;
    int max_num;
}test_t;

static void test_destroy(test_t *this)
{
    free(this->test_func);
    free(this);
    return ;
}

static void test_run(test_t *this)
{
    int faild = 0;

    for (int i = 0; i < this->test_times; i++)
    {
        int len;
        int *arr = gen_random_arr(this->max_size, this->max_num, &len);
        int *copy = malloc(sizeof(int) * len);
        memcpy(copy, arr, sizeof(int) * len);
        this->right(copy, len);
        this->test(arr, len);
        if (memcmp(arr, copy, sizeof(int) * len))
            faild++;

        free(arr);
        free(copy);
    }
    printf("%s : %d pass, %d failed\n", this->test_func, this->test_times - faild, faild);

    return ;
}

static test_t *test_create_default(char *fname, sort_func right, sort_func test)
{
    test_t *ret = malloc(sizeof(*ret));

    ret->test_times = 1000;
    ret->max_num = 100;
    ret->max_size = 100;
    ret->test_func = strdup(fname);
    ret->right = right;
    ret->test = test;
    ret->run = test_run;
    ret->destroy = test_destroy;

    return ret;
}

int main()
{
    test_t *test_select_sort = test_create_default("select sort", right_sort, selection_sort);
    test_select_sort->run(test_select_sort);
    test_select_sort->destroy(test_select_sort);

    test_t *test_bubble_sort = test_create_default("bubble sort", right_sort, bubble_sort);
    test_bubble_sort->run(test_bubble_sort);
    test_bubble_sort->destroy(test_bubble_sort);

    test_t *test_insert_sort = test_create_default("insert sort", right_sort, insert_sort);
    test_insert_sort->run(test_insert_sort);
    test_insert_sort->destroy(test_insert_sort);

    test_t *test_merge_sort = test_create_default("merge  sort", right_sort, merge_sort);
    test_merge_sort->run(test_merge_sort);
    test_merge_sort->destroy(test_merge_sort);

    test_t *test_quick_sort = test_create_default("quick  sort", right_sort, quick_sort);
    test_quick_sort->run(test_quick_sort);
    test_quick_sort->destroy(test_quick_sort);

    return 0;
}