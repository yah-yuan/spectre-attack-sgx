#include <time.h>
#include <stdio.h>

#define NANO 1000000000

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
    return temp.tv_sec * NANO + temp.tv_nsec;
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

void fprint_time(FILE *fd, char * desc){

    time_t tmpcal_ptr;
    struct tm *tmp_ptr;

    tmpcal_ptr = time(NULL);
    tmp_ptr = localtime(&tmpcal_ptr);
    fprintf(fd,"%s",desc);
    fprintf (fd,":%d.%d.%d ", (1900+tmp_ptr->tm_year), (1+tmp_ptr->tm_mon), tmp_ptr->tm_mday);
    fprintf(fd,"%d:%d:%d\n", tmp_ptr->tm_hour, tmp_ptr->tm_min, tmp_ptr->tm_sec);
}

void print_time(char * desc){

    time_t tmpcal_ptr;
    struct tm *tmp_ptr;

    tmpcal_ptr = time(NULL);
    tmp_ptr = localtime(&tmpcal_ptr);
    printf("%s",desc);
    printf (":%d.%d.%d ", (1900+tmp_ptr->tm_year), (1+tmp_ptr->tm_mon), tmp_ptr->tm_mday);
    printf("%d:%d:%d\n", tmp_ptr->tm_hour, tmp_ptr->tm_min, tmp_ptr->tm_sec);
}