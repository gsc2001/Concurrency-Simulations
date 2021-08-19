# Question 2 Back To College

## Assumptions

-   The max number of pharmaceutical companies (n),vaccination zones (m), students(o) <= 1000.
-   The total number of waiting students (according to which Vaccination Zone create slots) are the number of students who have not yet exited the vaccination drive (simulation).
-   Vaccination delivery time (time between when pharmaceutical company delivering and Vaccination Zone recieving vaccine) is 0 seconds (instant).
-   After entering vaccination phase, Vaccination zone takes 2 secs to vaccinate all the assigned students.

## Implementation Details

### Structs

#### Critic Int

A helper struct for integers which are critical.

```c
typedef struct CriticInt
{
    int value;
    pthread_mutex_t mutex;
} CriticInt;
```

-   `value`: The value of the int,
-   `mutex`: Mutex to handle the critical update/read of int

#### Company

Details for a Pharmaceutical Company

```c
typedef struct Company
{
    int id;
    int p;
    float x;
    CriticInt batches, total_vaccines;
    pthread_t tid;
} Company;
```

-   `id`: idx of the company in `comps` array
-   `p`: Number of vaccines in a batch
-   `x`: Probabily that vaccine will work
-   `batches`: Critical variable for number of batches which are not delivered
-   `total_vaccines`: Critical variable for total_vaccines which are not yet used.

#### Zone

```c
typedef struct Zone
{
    int id;
    int vac, compId;
    float compX;
    CriticInt slots_free;
    pthread_t tid;
} Zone;
```

-   `id`: index of the zone in `zones` array
-   `vac`: Number of vaccines currently in the vaccination zone
-   `compId`: Id of latest company which delivered the vaccinations to the zone
-   `compX`: Working probabilty of latest company which delivered the vaccinations to the zone
-   `slots_free`: Critical variable for slots which are free till now.

#### Student

```c
typedef struct Student
{
    int id;
    int zoneId, arrival_time;
    float pass_prob;

    // 0 if not waiting for vaccination 1 if waiting (after slot assignment)
    int vacWaiting;
    pthread_t tid;
} Student;
```

-   `id`: index of the student in `students` array
-   `zoneId`: Zone allocated to the student (initially -1).
-   `arrival_time`: Time of arrival of student (in seconds).
-   `arrival_time`: Time of arrival of student (in seconds).
-   `vacWaiting`: 0 if student not waiting for vaccination, 1 if waiting in slot on vaccination zone to be vaccinated.

### Global Variables

-   `s`: `CriticInt`, to track for number of students who need to be allocated in the slots.
-   `nStudents`: `int`, to track for number of students who have not left the simulation (i.e. number of students whose threads are active). Each student thread before exiting decrements this by 1.
-   `sim_running`: `int`, 1 if simulation is running, 0 when all student have
    compeleted the drive (successfully/unsuccessfully).
-   `assign_company_m`: `pthread_mutex_t`, Mutex to ensure only 1 Zone at a time enters the company assignment loop (for vaccination delivery).

-   `assign_zone_m`: `pthread_mutex_t`, Mutex to ensure only 1 student at a time enters the zone assignment loop (for slot allotment).

-   `comps`: `Company [1000]`, Array to store companies
-   `zones`: `Zone [1000]`, Array to store Vaccination zones.
-   `students`: `Student [1000]`, Array to store students.

`n`, `m`, `o` same defination as in question.

### Core Functions

-   **`void assign_company(int zoneId)`**

    This function first locks the `assign_company_m`, iterates over all the companies and finds a company which as 1 unassigned batch.
    The search loop is inside a while loop which keeps running till such a company is not found. When found it delivers the batch from company to the zone. The while loop is exited and and mutex is released.

-   **`void assign_zone(int studId)`**

    Same as `assign_company` but between student and a zone. Finds a zone with atleast 1 free zone. `assign_zone_m` mutex is used to ensure single assignment.

-   **`void make_slots(int zoneId)`**

    Lock `zone.slots_free.mutex`, find the `max = min(8,nStudents, zone.vac)`, and create `zone.slots_free.value = rand() % max + 1` slots.
    And finally unlock the mutex.

-   **`void vaccinate_students(int zoneId)`**

    Sleep for 2 seconds (to vaccinate students). Loop over all students and vaccinate all who have `students.zoneId == zoneId` with following.

    ```c
    students[i].zoneId = -1;
    students[i].vacWaiting = 0;
    ```

    Decrease the zone's vac count and zone's last company's total_vaccination who delivered it vaccines.

    ```c
    zones[zoneId].vac--;
    pthread_mutex_lock(&(comps[zones[zoneId].compId].mutex));
    comps[zones[zoneId].compId].total_vaccines.value--;
    pthread_mutex_unlock(&(comps[zones[zoneId].compId].total_vaccines.mutex));
    ```

    As total vaccines is a CriticInt, we need to first lock and then update.

-   **`void produce_batches(int compId)`**

    The basic need for this funciton is to create batches for a company.
    To do so, first lock the batches and total vaccines mutex, then create random number of batches, number of time to create, and capacity of each batch.
    Sleep for creation time and then update the vars in company struct.

-   **`void antibody_test(int studId)`**

    Create a random number in [0,100] check if that is less than `student.pass_prob`. If less then antibodies were detected otherwise not detected.

    ```c
    int ra = rand() % 101;
    int pass = 0;
    int pass_prob = (int)(students[studId].pass_prob * 100);
    if (ra <= pass_prob)
        pass = 1;
    ```

### Thread Functions

-   **`void *company_thread(void* arg)`**

    Get the id.

    Then till sim is running,

    1. Produce
    2. Wait for all vaccines to finish.
    3. Go to step 1.

    ```c
    while (sim_running)
    {
        produce_batches(compId);

        // wait for all vaccines to finish
        while (sim_running && comps[compId].total_vaccines.value > 0);
    }
    ```

-   **`void *zone_thread(void *arg)`**

    Get Id.

    Till simulation is running

    1. Check for vaccination 0. If vaccination is zero then call `assign_company` to get the vaccines.

    2. Check for free slots `if <= 0` and `s.value > 0` (there are students to be allocated to slots). Then make slots.

    3. Create a temp variable to store initial slots.
    4. Wait for slots to finish or number of students to be allocated to become zero.
    5. If the slots now are not equal to that stored in temp variable, it means some slot have been taken therefore call the vaccinate_students funciton otherwise continue in loop.

-   **`void *stud_thread(void *arg)`**

    1.  Get Id.
    2.  Sleep till time of arrival
    3.  Call `assign_zone` to get a slot
    4.  Busy wait till not vacinated (`student.vacWaiting == 1`)
    5.  Do test, If fail

            - Increase students to be allocated by 1.

            ```c
            pthread_mutex_lock(&(s.mutex));
            s.value++;
            pthread_mutex_unlock(&(s.mutex))
            ```

            - Repeat from step 3 (if current round != 3)

        Else,
        Decrease `nStudents` and exit.

-   **`main()`**
    All the initialization stuff like initializing mutexes, global vars and creating thread for each entity. And wait for all students to finish

## Running Instructions

-   Compile
    ```
    gcc q2.c -lpthread -o q2
    ```
-   Run
    ```
    ./q2
    ```
