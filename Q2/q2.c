// #define _POSIX_C_SOURCE 199309L //required for clock
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>
#include <limits.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <inttypes.h>
#include <math.h>

#define BLU "\033[0;34m"
#define GRN "\033[0;32m"
#define CYN "\033[0;36m"
#define YLW "\033[0;33m"
#define MGN "\033[1;35m"
#define RED "\033[0;31m"
#define CLR "\033[0m"

/*************************** STRUCTS *********************************/

// helper struct for critical int vars
typedef struct CriticInt
{
    int value;
    pthread_mutex_t mutex;
} CriticInt;

typedef struct Company
{
    int id;
    int p;
    float x;
    CriticInt batches, total_vaccines;
    pthread_t tid;
} Company;

typedef struct Zone
{
    int id;
    int vac, compId;
    float compX;
    CriticInt slots_free;
    pthread_t tid;
} Zone;

typedef struct Student
{
    int id;
    int zoneId, arrival_time;
    float pass_prob;

    // 0 if not waiting for vaccination 1 if waiting (after slot assignment)
    int vacWaiting;
    pthread_t tid;
} Student;

/*************************** Global vars *********************************/

// number of students left to wait
CriticInt s;

int nStudents;

int sim_running = 1;

int n, m, o;

// mutexs for company and zone assignment;
pthread_mutex_t assign_company_m, assign_zone_m;

Company comps[1000];
Zone zones[1000];
Student students[1000];

/************************** Core Functions **************************/

void assign_company(int zoneId)
{
    pthread_mutex_lock(&assign_company_m);
    int assigned = 0;
    while (sim_running && !assigned)
    {
        for (int i = 0; i < n; i++)
        {
            if (comps[i].batches.value > 0)
            {
                pthread_mutex_lock(&(comps[i].batches.mutex));
                printf(CYN "Pharmaceutical Company %d is delivering a vaccine batch to Vaccination Zone %d which has success probability %f\n" CLR, i, zoneId, comps[i].x);
                printf(CYN "Pharmaceutical Company %d has delivered vaccines to Vaccination zone %d, resuming vaccinations now\n" CLR, i, zoneId);
                assigned = 1;
                zones[zoneId].compId = i;
                zones[zoneId].compX = comps[i].x;
                zones[zoneId].vac = comps[i].p;
                comps[i].batches.value--;
                pthread_mutex_unlock(&(comps[i].batches.mutex));
                break;
            }
        }
    }
    pthread_mutex_unlock(&assign_company_m);
}

void assign_zone(int studId)
{
    printf(YLW "Student %d is waiting to be allocated a slot on a Vaccination Zone\n" CLR, studId);
    pthread_mutex_lock(&assign_zone_m);
    int assigned = 0;
    while ((assigned == 0) && sim_running)
    {
        for (int i = 0; i < m; i++)
        {
            if (zones[i].slots_free.value > 0)
            {
                pthread_mutex_lock(&(zones[i].slots_free.mutex));
                pthread_mutex_lock(&(s.mutex));
                printf(CYN "Student %d assigned a slot on the Vaccination Zone %d and waiting to be vaccinated\n" CLR, studId, i);
                assigned = 1;
                students[studId].pass_prob = zones[i].compX;
                students[studId].zoneId = i;
                students[studId].vacWaiting = 1;
                s.value--;
                zones[i].slots_free.value--;
                pthread_mutex_unlock(&(s.mutex));
                pthread_mutex_unlock(&(zones[i].slots_free.mutex));
                break;
            }
        }
    }
    pthread_mutex_unlock(&assign_zone_m);
}

void make_slots(int zoneId)
{
    pthread_mutex_lock(&(zones[zoneId].slots_free.mutex));
    int max = 8;
    if (max > nStudents)
        max = nStudents;

    if (max > zones[zoneId].vac)
        max = zones[zoneId].vac;
    int k = rand() % max + 1;
    // printf("slots->%d\n", k);

    printf(GRN "Vaccination Zone %d is ready to vaccinate with %d slots​\n" CLR, zoneId, k);
    zones[zoneId].slots_free.value = k;
    pthread_mutex_unlock(&(zones[zoneId].slots_free.mutex));
}

void vaccinate_students(int zoneId)
{
    // vaccinate students
    sleep(2);

    for (int i = 0; i < o; i++)
    {
        if (students[i].zoneId == zoneId)
        {
            printf(MGN "Student %d on Vaccination Zone %d has been vaccinated which has success probability %f\n" CLR, i, zoneId, students[i].pass_prob);
            students[i].zoneId = -1;
            zones[zoneId].vac--;
            pthread_mutex_lock(&(comps[zones[zoneId].compId].total_vaccines.mutex));
            comps[zones[zoneId].compId].total_vaccines.value--;
            pthread_mutex_unlock(&(comps[zones[zoneId].compId].total_vaccines.mutex));
            students[i].vacWaiting = 0;
        }
    }
}

void produce_batches(int compId)
{
    pthread_mutex_lock(&(comps[compId].batches.mutex));
    pthread_mutex_lock(&(comps[compId].total_vaccines.mutex));
    int r = rand() % 5 + 1, p = rand() % 11 + 10, w = rand() % 4 + 2;
    printf(BLU "Pharmaceutical Company %d is preparing %d batches of vaccines which have success probability %f\n" CLR, compId, r, comps[compId].x);

    sleep(w);

    printf(GRN "Pharmaceutical Company %d has prepared %d batches of vaccines which have success probability %f. Waiting for all the vaccines to be used to resume production\n" CLR,
           compId, r, comps[compId].x);
    comps[compId].p = p;
    comps[compId].batches.value = r;
    comps[compId].total_vaccines.value = r * p;
    pthread_mutex_unlock(&(comps[compId].total_vaccines.mutex));
    pthread_mutex_unlock(&(comps[compId].batches.mutex));
}

int antibody_test(int studId)
{
    int ra = rand() % 101;
    int pass = 0;
    int pass_prob = (int)(students[studId].pass_prob * 100);
    if (ra <= pass_prob)
        pass = 1;
    printf("%sStudent %d has tested %s for antibodies\n" CLR, pass ? GRN : RED, studId, pass ? "positive" : "negative");

    return pass;
}

/************************** Thread Functions **************************/

void *company_thread(void *(arg))
{
    int compId = *(int *)arg;
    while (sim_running)
    {
        produce_batches(compId);

        // wait for all vaccines to finish

        while (sim_running && comps[compId].total_vaccines.value > 0)
            ;
        if (sim_running)
            printf(MGN "All the vaccines prepared by Pharmaceutical Company %d are emptied. Resuming production now.\n" CLR, compId);
    }
    return NULL;
}

void *zone_thread(void *arg)
{

    int zoneId = *(int *)arg;
    int first_time = 1;

    while (sim_running)
    {
        if (zones[zoneId].vac == 0)
        {
            if (first_time)
            {
                first_time = 0;
            }
            else
                printf(RED "Vaccination Zone %d has run out of vaccines\n" CLR, zoneId);
            assign_company(zoneId);
        }

        if (zones[zoneId].slots_free.value <= 0 && s.value > 0)
            make_slots(zoneId);
        int slots = zones[zoneId].slots_free.value;
        while (sim_running && s.value > 0 && zones[zoneId].slots_free.value > 0)
            ;

        if (zones[zoneId].slots_free.value != slots)
            printf("Vaccination Zone %d entering Vaccination Phase\n", zoneId);
        vaccinate_students(zoneId);
    }

    return NULL;
}

void *stud_thread(void *arg)
{
    int studId = *(int *)arg;
    sleep(students[studId].arrival_time);
    int i = 1;
    while (i <= 3)
    {
        printf(CYN "Student %d has arrived for his %d round of Vaccination\n" CLR, studId, i);
        assign_zone(studId);
        // wait while vaccinating
        while (sim_running && students[studId].vacWaiting > 0)
            ;

        int pass = antibody_test(studId);
        if (pass)
            break;
        else if (i < 3)
        {
            pthread_mutex_lock(&(s.mutex));
            s.value++;
            pthread_mutex_unlock(&(s.mutex));
        }
        i++;
    }
    nStudents--;
    return NULL;
}

int main()
{
    srand(time(0));
    scanf("%d%d%d", &n, &m, &o);
    pthread_mutex_init(&(s.mutex), NULL);
    pthread_mutex_init(&assign_company_m, NULL);
    pthread_mutex_init(&assign_zone_m, NULL);
    s.value = o;
    sim_running = 1;
    nStudents = o;

    for (int i = 0; i < n; i++)
    {
        scanf("%f", &(comps[i].x));
        pthread_mutex_init(&(comps[i].batches.mutex), NULL);
        pthread_mutex_init(&(comps[i].total_vaccines.mutex), NULL);
        comps[i].id = i;
        comps[i].batches.value = 0;
        comps[i].total_vaccines.value = 0;
    }
    for (int i = 0; i < m; i++)
    {
        pthread_mutex_init(&(zones[i].slots_free.mutex), NULL);
        zones[i].vac = 0;
        zones[i].slots_free.value = 0;
        zones[i].id = i;
    }
    for (int i = 0; i < o; i++)
    {
        students[i].arrival_time = rand() % 6;
        students[i].zoneId = -1;
        students[i].id = i;
        students[i].vacWaiting = 0;
    }

    for (int i = 0; i < n; i++)
    {
        pthread_create(&(comps[i].tid), NULL, company_thread, (void *)(&(comps[i].id)));
    }
    for (int i = 0; i < m; i++)
    {
        pthread_create(&(zones[i].tid), NULL, zone_thread, (void *)(&(zones[i].id)));
    }
    for (int i = 0; i < o; i++)
    {
        pthread_create(&(students[i].tid), NULL, stud_thread, (void *)(&(students[i].id)));
    }

    for (int i = 0; i < o; i++)
    {
        pthread_join(students[i].tid, NULL);
    }

    sim_running = 0;
    printf(RED "Simulation Over\n" CLR);

    // teardown
    for (int i = 0; i < n; i++)
    {
        pthread_mutex_destroy(&(comps[i].batches.mutex));
        pthread_mutex_destroy(&(comps[i].total_vaccines.mutex));
    }
    for (int i = 0; i < m; i++)
    {
        pthread_mutex_destroy(&(zones[i].slots_free.mutex));
    }
}
