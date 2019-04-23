#include <time.h>
#include <stdio.h>

long long int nano_timeval(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp.tv_sec * 1000000000 + temp.tv_nsec;
}

#ifndef IN_JOB //for measurements only
int main()
{
    struct timespec time1, time2;
    int temp;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time1);
    for (int i = 0; i< 242000000*3; i++)
        temp+=temp;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time2);
    printf("tv=%lld",nano_timeval(time1,time2));
    return 0;
}
#endif