# Question 3 Musical Mayhem

## Implementation Details

### Structs

#### Musician

Structure to store musicians

```c
typedef struct Musician
{
    int id;
    char *name;
    char instrument;
    int time_arrival;
    int time_wait;
    int time_per;
    int stageId;

    // 0 for acoustic 1 for electric 2 for both
    int type;

    pthread_t tid;

    // for type 2 musicians
    int got_stage;
    pthread_mutex_t mut;
    sem_t done_allocation;

} Musician;
```

-   `id`: index of the musician in `mucs` array.
-   `name`: Name of the musician,
-   `instrument`: Intrument played by the musician
-   `time_arrival`: Arrival time of Musician
-   `time_wait`: Time for which a musician will wait before becomming impatient
-   `time_per`: Performance time
-   `stageId`: Id of the stage on which musician is performing
-   `type`: 0 for acoustic, 1 for electric, 2 for both
-   `tid`: Thread Id of the musician thread.
    The following are relevant only for a type 2 musician
-   `got_stage`: Critic variable to check if the musician required a stage (0 represent yes 1 represents no). (Used while allocation).
-   `mut`: Mutex to protext got_stage.
-   `done_allocation`: Semophore which will be signaled when a stage is allocated to it.

#### Singer

Struct to store a singer

```c
typedef struct Singer
{
    int id;
    char *name;
    int time_arrival;
    int time_wait;
    int time_per;
    int stageId;

    sem_t duet_finish;

    pthread_t tid;

} Singer;
```

`id`, `name`, `time_arrival`, `time_wait`, `time_per`, `stageId`, `tid` same as in `Musician`

-   `duet_finish`: Semophore that will be used if the singer performs with a musician

#### Stage

```c
typedef struct Stage
{
    // 0 for acoustic 1 for electric
    int type;
    int singerId;
    int musicianId;

    pthread_mutex_t allocate;

} Stage;
```

-   `type`: 0 for a acoustic stage, 1 for an electric stage
-   `singerId`: Id of the singer performing on the stage. -1 if none.
-   `musicianId`: Id of the musician performing on the stage, -1 if none.
-   `allocate`: Mutex to ensure only 1 thread at a time takes part in allocation/deallocation procedure.

### Global Variables

-   `stages`: `Stage [1000]`, Array to store stages
-   `singers`: `Singer [1000]`, Array to store singers
-   `mucs`: `Musician [1000]`, Array to store musicians
-   `nsing`: `int`, Number of singers
-   `nmucs`: `int`, Number of musicians
-   `stage_sems`: `sem_t [2]` stage_sems[0] semophore to store number of available acoustic stages and stage_sems[1] semophore to store number of electric stages.
-   `singers_sem`: `sem_t`, Store the number of stages where a singer can be allocated
-   `cords_sem`: `sem_t`, Store the number of available co-ordinators.
-   `stage_types`: `char *[]` => `stage_types[0] = "acoustic"` and `stage_types[1] = "electric"`. An array to help in printing

### Core Functions

-   **`int allocate_singer_on(int singId, int type)`**

    Function allocates a singer on stage type `type` if `type == 1 || type == 0`, if `type == 2` , allocate with a musician.
    It finds compatible stage. If not found returns -1 else returns the idx of stage allocated

-   **`int allocate_musician_on(int singId, int type)`**

    Function allocates a singer on stage type `type`.
    It finds compatible stage. If not found returns -1 else returns the idx of stage allocated

-   **`void deallocate_musician(int mucId)`**

    Lock allocate mutex of the stage on which musician performs.

    ```c
    int stageId = mucs[mucId].stageId;
    pthread_mutex_lock(&(stages[stageId].allocate));
    ```

    If a singer is also performing with the musician, unlock the mutex and perform for 2 secs more and Lock the mutex again and post singer that the performance has finished (using `singer.duet_finish` semophore)

    Deallocate stage

    Post `stage_sems[type]` (to increase available stages by 1)

    Unlock the mutex.

-   **`void deallocate_musician(int mucId)`**

    Lock allocate mutex of the stage on which singer performs.

    ```c
    int stageId = mucs[mucId].stageId;
    pthread_mutex_lock(&(stages[stageId].allocate));
    ```

    Deallocate stage

    Post `stage_sems[type]` (to increase available stages by 1)

    Unlock the mutex.

-   **`void get_tshirt(char *name)`**

    -   Wait for a co-ordinator to get free by waiting `cords_sem`
    -   Wait for 2 secs
    -   Make the co-ordinator free by posting `cords_sem`

-   **`single_singer_end(int singId)`**

    Function to make a single singer (without musician) to perform and finish

    -   Sleep for performance time
    -   Call `deallocate_singer`
    -   Add stage to stage available to singer by posting `singer_sem`
    -   Take tshirt be calling `get_tshirt`

-   **`duet_end(int singId)`**
    Function to make a duet singer(with musician) perform and finish

    -   Wait for performance to end to finish (wait for `singer.duet_finish`)

    Continue with step 3 for `single_singer_end`

-   **`initialize()`**

    Function to perform initialization tasks like initalizing stages, semophores

### Thread Functions

-   `void *allocate_musician(void *arg)`

    Thread to allocate type 2 musician.`arg` in a pointer of type

    ```c
    typedef struct AllocationStruct
    {
        int mucId, type;
    } AllocationStruct;
    ```

    mucId is the musician id and type is the type of stage thread is trying to allocate musician on.
    The threads wait for the type of stage to complete

    ```c
    sem_wait(&stage_sems[type]);
    ```

    then it locks a `muc.mut` mutex and make `muc.got_stage = 1` and post `muc.done_allocation` so that other thread can simply signal its stage and exit.

    After changing got_stage it allocates the musician on that stage using `allocate_musician_on`

-   **`void *musician_thread(void *arg)`**

    Thread for a musician.

    1. Wait for time_arrival seconds
    2. Allocate musician

    -   If type == 0 or 1, timedwait for corresponding stage to get empty and call `allocate_musician_on`

    ```c

    int allocated = sem_timedwait(&stage_sems[mucs[mucId].type], &ts);
    if (allocated != 0)
    {
        printf(RED "%s %c left because of impatience\n" CLR, mucs[mucId].name, mucs[mucId].instrument);
        return NULL;
    }
    allocate_musician_on(mucId, mucs[mucId].type);
    ```

    -   If `type == 2`
        -   create 2 thread of `allocate_musician` for each stage type.
        -   timedwait on `muc.done_allocation` for time_wait
        -   if not got in time, make `muc.got_stage = 1` (after locking `muc.muc`) so that if that those 2 threads dont try to allocate now.

    ```c
    int allocated = sem_timedwait(&(mucs[mucId].done_allocation), &ts);
    pthread_mutex_lock(&(mucs[mucId].mut));
    if (allocated != 0 && mucs[mucId].got_stage == 0)
    {
        mucs[mucId].got_stage = 1;
        printf(RED "%s %c left because of impatience\n" CLR, mucs[mucId].name, mucs[mucId].instrument);
        pthread_mutex_unlock(&(mucs[mucId].mut));
        return NULL;
    }
    pthread_mutex_unlock(&(mucs[mucId].mut));
    ```

-   Perform by sleep for `time_per` seconds

-   Call `deallocate_musician` to deallocate
-   Take tshirt, call `get_tshirt`

-   **`void *singer_thread(void *arg)`**

    -   Sleep for time arrival
    -   timed wait for a stage for singer to get empty
    -   Try waiting on both types of stage. If gets any allocate on that, otherwise allocate on a stage with musician.
    -   There may be a case when the both locks failed but be did not get and stage with musician on it. (If the musician exited just before allocation of singer). To handle this case we create a local variable got_stage initialized to 0, and turn it to 1 on stage allocation.

    Do step 2 for while got_stage == 0 this loop will run very few times (1 in worse case 2)

-   **`main()`**
    -   Take input and
    -   Call `initalize()`
    -   Create singers and musicians
    -   Create thread for each
    -   Join all threads.
