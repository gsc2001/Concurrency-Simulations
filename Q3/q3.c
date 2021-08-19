#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <math.h>
#include <string.h>

// colors
#define BLU "\033[34;1m"
#define GRN "\033[32;1m"
#define CYN "\033[1;36m"
#define YLW "\033[01;33m"
#define MGN "\033[1;35m"
#define RED "\033[31;1m"
#define CLR "\033[0m"

/********************* Structs **********************/
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

typedef struct Stage
{
    // 0 for acoustic 1 for electric
    int type;
    int singerId;
    int musicianId;

    pthread_mutex_t allocate;

} Stage;

/********************** Global vars *************************/

Stage stages[1000];
Singer singers[1000];
Musician mucs[1000];

int nsing, nmucs, a, e, c, t1, t2, t, nstages;

// for acoustic stages, electric stages, singers
sem_t stage_sems[2], singers_sem, cords_sem;

char *stage_types[] = {"acoustic", "electric"};

/********************** Core Functions **********************/

int allocate_singer_on(int singId, int type)
{
    // type = 0 for acoust, 1 for electric, 2 for with musician

    int allocate_idx = -1;
    for (int i = 0; i < nstages; i++)
    {
        pthread_mutex_lock(&(stages[i].allocate));
        if (stages[i].singerId == -1)
        {
            if (type == 2 && stages[i].musicianId != -1)
            {
                allocate_idx = i;
                stages[i].singerId = singId;
                singers[singId].stageId = i;
                Singer singer = singers[singId];
                printf(YLW "%s has joined %s's performance at stage idx: %d, performance extended by 2 sec\n" CLR,
                       singer.name, mucs[stages[i].musicianId].name, i);
                pthread_mutex_unlock(&(stages[i].allocate));
                break;
            }
            else if (stages[i].type == type && stages[i].musicianId == -1)
            {
                allocate_idx = i;
                stages[i].singerId = singId;
                singers[singId].stageId = i;
                Singer singer = singers[singId];
                printf(MGN "%s performing s at %s (stage idx: %d) for %d sec\n" CLR,
                       singer.name, stage_types[stages[i].type], i, singer.time_per);
                pthread_mutex_unlock(&(stages[i].allocate));
                break;
            }
        }
        pthread_mutex_unlock(&(stages[i].allocate));
    }
    return allocate_idx;
}

int allocate_musician_on(int mucId, int type)
{
    int allocate_idx = -1;
    for (int i = 0; i < nstages; i++)
    {
        pthread_mutex_lock(&(stages[i].allocate));
        if (stages[i].musicianId == -1 && stages[i].type == type && stages[i].singerId == -1)
        {
            allocate_idx = i;
            stages[i].musicianId = mucId;
            mucs[mucId].stageId = i;
            Musician muc = mucs[mucId];
            printf(MGN "%s performing %c at %s (stage idx: %d) for %d sec\n" CLR,
                   muc.name, muc.instrument, stage_types[type], i, muc.time_per);
            pthread_mutex_unlock(&(stages[i].allocate));
            break;
        }
        pthread_mutex_unlock(&(stages[i].allocate));
    }
    return allocate_idx;
}

void deallocate_musician(int mucId)
{
    int stageId = mucs[mucId].stageId;
    pthread_mutex_lock(&(stages[stageId].allocate));
    if (stages[stageId].singerId != -1)
    {
        pthread_mutex_unlock(&(stages[stageId].allocate));
        // perform for more 2 sec
        sleep(2);
        pthread_mutex_lock(&(stages[stageId].allocate));
        sem_post(&(singers[stages[stageId].singerId].duet_finish));
    }

    stages[stageId].musicianId = -1;
    stages[stageId].singerId = -1;

    printf(CYN "%s performance at %s stage (stage idx: %d) finished\n" CLR, mucs[mucId].name, stage_types[stages[stageId].type], stageId);
    pthread_mutex_unlock(&(stages[stageId].allocate));
    sem_post(&stage_sems[stages[stageId].type]);
}

void deallocate_singer(int singId)
{
    int stageId = singers[singId].stageId;
    pthread_mutex_lock(&(stages[stageId].allocate));
    stages[stageId].singerId = -1;
    stages[stageId].musicianId = -1;
    printf(CYN "%s performance at %s stage (stage idx: %d) finished\n" CLR, singers[singId].name, stage_types[stages[stageId].type], stageId);
    sem_post(&stage_sems[stages[stageId].type]);
    pthread_mutex_unlock(&(stages[stageId].allocate));
}

void get_tshirt(char *name)
{
    // Collect tshirt wait
    sem_wait(&cords_sem);

    // collect Tshirt
    printf(BLU "%s collecting T-Shirt\n" CLR, name);
    sleep(2);
    sem_post(&cords_sem);
}

void single_singer_end(int singId)
{
    // perform
    sleep(singers[singId].time_per);
    deallocate_singer(singId);
    sem_post(&singers_sem);
    get_tshirt(singers[singId].name);
}

void duet_end(int singId)
{
    // wait for duet to finish
    sem_wait(&(singers[singId].duet_finish));
    sem_post(&singers_sem);
    get_tshirt(singers[singId].name);
}

void initialize()
{
    srand(time(0));
    nstages = a + e;
    nmucs = 0;
    nsing = 0;
    sem_init(&stage_sems[0], 0, a);
    sem_init(&stage_sems[1], 0, e);
    sem_init(&singers_sem, 0, nstages);
    sem_init(&cords_sem, 0, c);

    // stage setup
    for (int i = 0; i < nstages; i++)
    {
        stages[i].type = i >= a;
        stages[i].singerId = -1;
        stages[i].musicianId = -1;
        pthread_mutex_init(&(stages[i].allocate), NULL);
    }
}

/********************* Thread functions *********************/

typedef struct AllocationStruct
{
    int mucId, type;
} AllocationStruct;

void *allocate_musician(void *arg)
{
    AllocationStruct in = *(AllocationStruct *)arg;

    int mucId = in.mucId;
    int type = in.type;
    sem_wait(&stage_sems[type]);

    pthread_mutex_lock(&(mucs[mucId].mut));
    if (mucs[mucId].got_stage == 0)
    {
        mucs[mucId].got_stage = 1;
        sem_post(&(mucs[mucId].done_allocation));
        mucs[mucId].type = type;
        allocate_musician_on(mucId, type);
        pthread_mutex_unlock(&(mucs[mucId].mut));
        return NULL;
    }
    else
    {
        sem_post(&stage_sems[type]);
        pthread_mutex_unlock(&(mucs[mucId].mut));
        return NULL;
    }
}

void *musician_thread(void *arg)
{
    int mucId = *(int *)arg;

    sleep(mucs[mucId].time_arrival);

    // arrived
    printf(GRN "%s %c arrived\n" CLR, mucs[mucId].name, mucs[mucId].instrument);

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
        return NULL;

    ts.tv_sec += mucs[mucId].time_wait;

    // allocate
    if (mucs[mucId].type == 2)
    {
        mucs[mucId].got_stage = 0;
        pthread_mutex_init(&(mucs[mucId].mut), NULL);
        sem_init(&(mucs[mucId].done_allocation), 0, 0);
        pthread_t tid1, tid2;
        AllocationStruct a1, a2;
        a1.mucId = mucId;
        a2.mucId = mucId;
        a1.type = 0;
        a2.type = 1;

        pthread_create(&tid1, NULL, allocate_musician, (void *)(&a1));
        pthread_create(&tid2, NULL, allocate_musician, (void *)(&a2));
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
    }
    else
    {
        int allocated = sem_timedwait(&stage_sems[mucs[mucId].type], &ts);
        if (allocated != 0)
        {
            printf(RED "%s %c left because of impatience\n" CLR, mucs[mucId].name, mucs[mucId].instrument);
            return NULL;
        }
        allocate_musician_on(mucId, mucs[mucId].type);
    }

    // perform
    sleep(mucs[mucId].time_per);

    // deallocate
    deallocate_musician(mucId);

    // tshirt
    get_tshirt(mucs[mucId].name);

    return NULL;
}

void *singer_thread(void *arg)
{
    int singId = *(int *)arg;
    int got_stage = 0;

    sleep(singers[singId].time_arrival);

    // arrived
    printf(GRN "%s s arrived\n" CLR, singers[singId].name);

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
        return NULL;

    ts.tv_sec += singers[singId].time_wait;

    int allocated = sem_timedwait(&singers_sem, &ts);

    if (allocated != 0)
    {
        printf(RED "%s s left because of impatience\n" CLR, singers[singId].name);
        return NULL;
    }

    // NOTE: This while loop only run very few times!
    while (!got_stage)
    {
        int ac_avail = sem_trywait(&stage_sems[0]);
        if (ac_avail == 0)
        {
            got_stage = 1;
            allocate_singer_on(singId, 0);
            single_singer_end(singId);
        }
        else
        {
            int el_avail = sem_trywait(&stage_sems[1]);
            if (el_avail == 0)
            {
                got_stage = 1;
                allocate_singer_on(singId, 1);
                single_singer_end(singId);
            }
            else
            {
                // duet
                int got = allocate_singer_on(singId, 2);
                if (got != -1)
                {
                    got_stage = 1;
                    sem_init(&(singers[singId].duet_finish), 0, 0);
                    duet_end(singId);
                }
            }
        }
    }
    return NULL;
}

int main()
{
    int k;
    scanf("%d%d%d%d%d%d%d", &k, &a, &e, &c, &t1, &t2, &t);
    initialize();
    char _name[100], ic;
    int ta;

    for (int i = 0; i < k; i++)
    {
        scanf("%s %c %d", _name, &ic, &ta);

        if (ic == 's')
        {
            singers[nsing].id = nsing;
            singers[nsing].name = (char *)malloc(strlen(_name));
            strcpy(singers[nsing].name, _name);
            singers[nsing].stageId = -1;
            singers[nsing].time_arrival = ta;
            singers[nsing].time_per = rand() % (t2 - t1 + 1) + t1;
            singers[nsing].time_wait = t;
            nsing++;
        }
        else
        {
            mucs[nmucs].id = nmucs;
            mucs[nmucs].name = (char *)malloc(strlen(_name));
            strcpy(mucs[nmucs].name, _name);
            mucs[nmucs].stageId = -1;
            mucs[nmucs].time_arrival = ta;
            mucs[nmucs].time_per = rand() % (t2 - t1 + 1) + t1;
            mucs[nmucs].time_wait = t;
            mucs[nmucs].instrument = ic;

            int type;
            switch (ic)
            {
            case 'p':
            case 'g':
                type = 2;
                break;
            case 'v':
                type = 0;
                break;
            case 'b':
                type = 1;
                break;
            }
            mucs[nmucs].type = type;
            nmucs++;
        }
    }

    for (int i = 0; i < nmucs; i++)
        pthread_create(&(mucs[i].tid), NULL, musician_thread, (void *)(&(mucs[i].id)));
    for (int i = 0; i < nsing; i++)
        pthread_create(&(singers[i].tid), NULL, singer_thread, (void *)(&(singers[i].id)));

    for (int i = 0; i < nmucs; i++)
        pthread_join(mucs[i].tid, NULL);
    for (int i = 0; i < nsing; i++)
        pthread_join(singers[i].tid, NULL);

    printf(RED "Finished\n" CLR);
}