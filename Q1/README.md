# Question 1

-   `arg`

    -   Used to pass arguments in threaded merge sore
    -   Sort the start, end array itself

-   `int *shareMem(size_t size)`

    -   Return a shared memory size of array

-   `selectionSort`

    -   Selection sort the array

-   `merge`

    -   Merge function for mergesort

-   `mergesort`

    -   Basic mergesort

-   `mpMergeSort`

    -   Multiprocesses mergesort
    -   Here each part of array is taken by different processes

-   `threaded_mergesort`

    -   Threaded mergesort,
    -   Here each part of array is taken by each thread

-   `runSorts`
    -   Run different sorts and take time

## Findings and Conclusion

| n             | 10       | 100      | 1000     |
| ------------- | -------- | -------- | -------- |
| normal        | 0.000002 | 0.000028 | 0.000106 |
| threaded      | 0.001028 | 0.005717 | 0.005681 |
| mutiprocessed | 0.004356 | 0.015394 | 0.017943 |

### Explanation for findings

-   normal merge sort is faster than concurrent because:

    -   Continously creating threads/processes required a lot of time.
    -   Context Switching also requires time
    -   The processor loads the left half in cache when executing it, but just after a few instructions it switches to the right half. This causes a lot of cache misses and slows down the program

-   Threaded mergesort is faster than mergesort using process because:
    -   creating/destroying thread is much faster than creating/destroying

## Logic

### Multithreaded

As in mergesort, a array is broken into 2 from the middle. 2 threads created to call mergesort on them.
Finally the 2 thread are joined in parent and merge is called

### Multiprocessed

As in mergesort, a array is broken into 2 from the middle. 2 process created to call mergesort on them.
Finally the 2 process are waited in parent and merge is called.
