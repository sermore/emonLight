/*
 * emonLight: home energy monitor for Raspberry Pi.
 *  Copyright (C) 2015  Sergio Moretti
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * File:   cunittest.c
 * Author: Sergio
 *
 * Created on Apr 16, 2015, 8:08:56 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <sys/queue.h>

#include <CUnit/Basic.h>

#include "../main.h"

//extern struct cfg_t cfg;

extern struct pulse_queue send_q;
extern int send_queue_length;
//extern CURL *curl;

//extern volatile int stop;
//extern mqd_t mq;
//extern long pulseCount, rawCount;
//extern struct timespec tstart, tlast;
extern char send_buf[1024];
extern struct buzzer_config buzzer_config[2];
extern struct buzzer_power_queue pqueue[2];

extern int build_url_emoncms();
extern int build_url_emonlight();
extern double calc_power(double dt);
extern struct pulse_entry *insert_entry(struct timespec tlast, struct timespec trec, double dt, double power, double elapsedkWh, long pulseCount);
extern mqd_t open_pulse_queue(int oflag);
extern void parse_opts(int argc, char **argv);
extern void cleanup_queue(mqd_t mq);
extern char* get_config_file(const char *config_file);
extern int read_config(const char *config_file);
extern void buzzer_setup();
extern void buzzer_pqueue_push(struct buzzer_power_queue *q, double elapsedkWh, double time);
extern void buzzer_pqueue_pop(struct buzzer_power_queue *q, double time_interval, double tnow);
extern double buzzer_pqueue_delta(struct buzzer_power_queue *q, double power_threshold_kwh, double time_threshold_sec);
extern int buzzer_calc_pulses(struct buzzer_power_queue *q, struct buzzer_config *t);

/*
 * CUnit Test Suite
 */

int init_suite(void) {
    return 0;
}

int clean_suite(void) {
    return 0;
}

int init_config(char * config) {
    bzero(&cfg, sizeof (cfg));
    cfg.verbose = -1;
    cfg.pulse_pin = -1;
    cfg.node_id = -1;
    cfg.sender = 1;
    cfg.receiver = 1;
    cfg.daemonize = 0;
    cfg.config = config;
    send_queue_length = 0;
    return read_config(cfg.config);
}

void testGET_CONFIG_FILE(void) {
    bzero(&cfg, sizeof (cfg));
    cfg.daemonize = 1;
    CU_ASSERT_STRING_EQUAL(get_config_file(NULL), "/etc/emonlight.conf");
    cfg.daemonize = 0;
    CU_ASSERT_EQUAL(strncmp(get_config_file(NULL), "/home/", 6), 0);
}

void testPARSE_OPTS(void) {
    char **argv = (char *[]){"prg", "--queue-size", "1005", "-v", "-p", "32"};
    parse_opts(6, argv);
    CU_ASSERT(cfg.queue_size == 1005);
    CU_ASSERT(cfg.verbose == 1);
    CU_ASSERT(cfg.pulse_pin == 32);
}

void testREAD_CONFIG(void) {
    memset(&cfg, 0, sizeof (cfg));
    CU_ASSERT_PTR_NULL(cfg.config);
    CU_ASSERT_EQUAL(cfg.pulse_pin, 0);
    CU_ASSERT_EQUAL(cfg.queue_size, 0);
    CU_ASSERT_FALSE(access("tests/emonlight1.conf", F_OK));
    CU_ASSERT_EQUAL(init_config("tests/emonlight1.conf"), 1);
    CU_ASSERT_EQUAL(init_config("XXXX"), 2);
    CU_ASSERT_EQUAL_FATAL(init_config("tests/emonlight.conf"), 0);
    CU_ASSERT_STRING_EQUAL(cfg.api_key, "1234567890KK");
    CU_ASSERT_STRING_EQUAL(cfg.url, "http://xx.yy.zz");
    //    printf("P=%d, Q=%d\n", cfg.pin, cfg.queue_size);
    CU_ASSERT_EQUAL(cfg.pulse_pin, 12);
    CU_ASSERT_EQUAL(cfg.queue_size, 345);
    CU_ASSERT_EQUAL(cfg.node_id, 56);
}

void testINSERT_ENTRY(void) {
    send_queue_length = 0;
    TAILQ_INIT(&send_q);
    printf("S=%d\n", sizeof(struct pulse_entry));
    CU_ASSERT_EQUAL(sizeof(struct pulse_entry), 56);
    CU_ASSERT_PTR_NULL(TAILQ_FIRST(&send_q));
    struct timespec t0 = {1234567890, 123456789};
    struct timespec t1 = {1234567900, 123456789};
    insert_entry(t0, t1, 10, calc_power(10), 0.003, 1);
    struct pulse_entry *e = TAILQ_FIRST(&send_q);
    CU_ASSERT_PTR_NOT_NULL(e);
    CU_ASSERT_EQUAL(e->tlast.tv_sec, 1234567890);
    CU_ASSERT_EQUAL(e->tlast.tv_nsec, 123456789);
    CU_ASSERT_EQUAL(e->trec.tv_sec, 1234567900);
    CU_ASSERT_EQUAL(e->trec.tv_nsec, 123456789);
    CU_ASSERT_DOUBLE_EQUAL(e->dt, 10, 1e-6);
    CU_ASSERT_DOUBLE_EQUAL(e->power, calc_power(10), 1e-6);
    CU_ASSERT_EQUAL(e->pulseCount, 1);
    t0.tv_sec += 30;
    insert_entry(t1, t0, 20, calc_power(20), 0.004, 2);
    e = e->entries.tqe_next;
    CU_ASSERT_PTR_NOT_NULL(e);
    //    printf("S=%ld\n", e->trec.tv_sec);
    CU_ASSERT_EQUAL(e->tlast.tv_sec, 1234567900);
    CU_ASSERT_EQUAL(e->tlast.tv_nsec, 123456789);
    CU_ASSERT_EQUAL(e->trec.tv_sec, 1234567920);
    CU_ASSERT_EQUAL(e->trec.tv_nsec, 123456789);
    CU_ASSERT_DOUBLE_EQUAL(e->dt, 20, 1e-6);
    CU_ASSERT_DOUBLE_EQUAL(e->power, calc_power(20), 1e-6);
    CU_ASSERT_EQUAL(e->pulseCount, 2);
}

void testBUILD_URL_EMONCMS(void) {
    memset(send_buf, 0, sizeof(send_buf));
    CU_ASSERT_EQUAL(init_config("tests/emonlight.conf"), 0);
    cfg.verbose = 0;
    TAILQ_INIT(&send_q);
    struct timespec t0 = {1234567890, 123456789};
    struct timespec t1 = {1234567900, 123456789};
    insert_entry(t0, t1, 10, calc_power(10), 0.003, 1);
    int cnt = build_url_emoncms();
    //printf("P=%f, L=%d, QQ=%s\n", calc_power(10), strlen(send_buf), send_buf);
    CU_ASSERT_EQUAL(cnt, 1);
    CU_ASSERT_STRING_EQUAL(send_buf, "http://xx.yy.zz/input/bulk.json?apikey=1234567890KK&data=[[0,56,360.000000,0.003000,1],[9,56,360.000000,0.003000,1]]&time=1234567891");

    t0.tv_sec = t1.tv_sec + 1;
    insert_entry(t1, t0, 1, calc_power(1), 0.004, 2);
    CU_ASSERT_EQUAL(TAILQ_LAST(&send_q, pulse_queue)->tlast.tv_sec, 1234567900);
    CU_ASSERT_EQUAL(TAILQ_LAST(&send_q, pulse_queue)->trec.tv_sec, 1234567901);
    cnt = build_url_emoncms();
    //printf("P=%f, L=%d, QQ=%s\n", calc_power(1), strlen(send_buf), send_buf);
    CU_ASSERT_EQUAL(cnt, 2);
    CU_ASSERT_STRING_EQUAL(send_buf, "http://xx.yy.zz/input/bulk.json?apikey=1234567890KK&data=[[0,56,360.000000,0.003000,1],[9,56,360.000000,0.003000,1],[10,56,3600.000000,0.004000,2]]&time=1234567891");

    t1.tv_sec = t0.tv_sec + 3;
    insert_entry(t0, t1, 3, calc_power(3), 0.005, 3);
    CU_ASSERT_EQUAL(TAILQ_LAST(&send_q, pulse_queue)->tlast.tv_sec, 1234567901);
    CU_ASSERT_EQUAL(TAILQ_LAST(&send_q, pulse_queue)->trec.tv_sec, 1234567904);
    cnt = build_url_emoncms();
    //printf("P=%f, L=%d, QQ=%s\n", calc_power(3), strlen(send_buf), send_buf);
    CU_ASSERT_EQUAL(cnt, 3);
    CU_ASSERT_STRING_EQUAL(send_buf, "http://xx.yy.zz/input/bulk.json?apikey=1234567890KK&data=[[0,56,360.000000,0.003000,1],[9,56,360.000000,0.003000,1],[10,56,3600.000000,0.004000,2],[11,56,1200.000000,0.005000,3],[13,56,1200.000000,0.005000,3]]&time=1234567891");

    t0.tv_sec = t1.tv_sec;
    t0.tv_nsec += 500000000;
    insert_entry(t1, t0, 0.5, calc_power(0.5), 0.006, 4);
    CU_ASSERT_EQUAL(TAILQ_LAST(&send_q, pulse_queue)->tlast.tv_sec, 1234567904);
    CU_ASSERT_EQUAL(TAILQ_LAST(&send_q, pulse_queue)->trec.tv_sec, 1234567904);
    cnt = build_url_emoncms();
    //printf("P=%f, CNT=%d, L=%d, QQ=%s\n", calc_power(0.5), cnt, strlen(send_buf), send_buf);
    CU_ASSERT_EQUAL(cnt, 4);
    CU_ASSERT_STRING_EQUAL(send_buf, "http://xx.yy.zz/input/bulk.json?apikey=1234567890KK&data=[[0,56,360.000000,0.003000,1],[9,56,360.000000,0.003000,1],[10,56,3600.000000,0.004000,2],[11,56,1200.000000,0.005000,3],[13,56,1200.000000,0.005000,3],[14,56,7200.000000,0.006000,4]]&time=1234567891");

    cfg.verbose = 0;
    t1.tv_sec++;
    t1.tv_nsec -= 100000000;
    insert_entry(t0, t1, 0.4, calc_power(0.4), 0.007, 5);
    CU_ASSERT_EQUAL(TAILQ_LAST(&send_q, pulse_queue)->tlast.tv_sec, 1234567904);
    CU_ASSERT_EQUAL(TAILQ_LAST(&send_q, pulse_queue)->trec.tv_sec, 1234567905);
    cnt = build_url_emoncms();
    //    printf("P=%f, CNT=%d, L=%d, QQ=%s\n", calc_power(0.5), cnt, strlen(send_buf), send_buf);
    CU_ASSERT_EQUAL(cnt, 5);
    CU_ASSERT_STRING_EQUAL(send_buf, "http://xx.yy.zz/input/bulk.json?apikey=1234567890KK&data=[[0,56,360.000000,0.003000,1],[9,56,360.000000,0.003000,1],[10,56,3600.000000,0.004000,2],[11,56,1200.000000,0.005000,3],[13,56,1200.000000,0.005000,3],[14,56,7200.000000,0.006000,4],[14,56,9000.000000,0.007000,5]]&time=1234567891");
}

void testBUILD_URL_EMONLIGHT(void) {
    memset(send_buf, 0, sizeof(send_buf));
    CU_ASSERT_EQUAL(init_config("tests/emonlight.conf"), 0);
    cfg.verbose = 0;
    TAILQ_INIT(&send_q);
    struct timespec t0 = {1234567890, 123456789};
    struct timespec t1 = {1234567900, 123456789};
    insert_entry(t0, t1, 10, calc_power(10), 0.003, 1);
    int cnt = build_url_emonlight();
//    printf("Q='%s'\n", send_buf);
    CU_ASSERT_EQUAL(cnt, 1);
    CU_ASSERT_STRING_EQUAL(send_buf, "token=1234567890KK&node_id=56&epoch_time[]=1234567900%2C123456789&power=360.000000");

    t0.tv_sec = t1.tv_sec + 1;
    insert_entry(t1, t0, 1, calc_power(1), 0.004, 2);
    CU_ASSERT_EQUAL(TAILQ_LAST(&send_q, pulse_queue)->tlast.tv_sec, 1234567900);
    CU_ASSERT_EQUAL(TAILQ_LAST(&send_q, pulse_queue)->trec.tv_sec, 1234567901);
    cnt = build_url_emonlight();
//    printf("Q='%s'\n", send_buf);
    CU_ASSERT_EQUAL(cnt, 2);
    CU_ASSERT_STRING_EQUAL(send_buf, "token=1234567890KK&node_id=56&epoch_time[]=1234567900%2C123456789&power=360.000000&epoch_time[]=1234567901%2C123456789&power=3600.000000");

    t1.tv_sec = t0.tv_sec + 3;
    insert_entry(t0, t1, 3, calc_power(3), 0.005, 3);
    CU_ASSERT_EQUAL(TAILQ_LAST(&send_q, pulse_queue)->tlast.tv_sec, 1234567901);
    CU_ASSERT_EQUAL(TAILQ_LAST(&send_q, pulse_queue)->trec.tv_sec, 1234567904);
    cnt = build_url_emonlight();
//    printf("Q='%s'\n", send_buf);
    CU_ASSERT_EQUAL(cnt, 3);
    CU_ASSERT_STRING_EQUAL(send_buf, "token=1234567890KK&node_id=56&epoch_time[]=1234567900%2C123456789&power=360.000000&epoch_time[]=1234567901%2C123456789&power=3600.000000&epoch_time[]=1234567904%2C123456789&power=1200.000000");

    t0.tv_sec = t1.tv_sec;
    t0.tv_nsec += 500000000;
    insert_entry(t1, t0, 0.5, calc_power(0.5), 0.006, 4);
    CU_ASSERT_EQUAL(TAILQ_LAST(&send_q, pulse_queue)->tlast.tv_sec, 1234567904);
    CU_ASSERT_EQUAL(TAILQ_LAST(&send_q, pulse_queue)->trec.tv_sec, 1234567904);
    cnt = build_url_emonlight();
//    printf("Q='%s'\n", send_buf);
    CU_ASSERT_EQUAL(cnt, 4);
    CU_ASSERT_STRING_EQUAL(send_buf, "token=1234567890KK&node_id=56&epoch_time[]=1234567900%2C123456789&power=360.000000&epoch_time[]=1234567901%2C123456789&power=3600.000000&epoch_time[]=1234567904%2C123456789&power=1200.000000&epoch_time[]=1234567904%2C623456789&power=7200.000000");

    cfg.verbose = 0;
    t1.tv_sec++;
    t1.tv_nsec -= 100000000;
    insert_entry(t0, t1, 0.4, calc_power(0.4), 0.007, 5);
    CU_ASSERT_EQUAL(TAILQ_LAST(&send_q, pulse_queue)->tlast.tv_sec, 1234567904);
    CU_ASSERT_EQUAL(TAILQ_LAST(&send_q, pulse_queue)->trec.tv_sec, 1234567905);
    cnt = build_url_emonlight();
//    printf("Q='%s'\n", send_buf);
    CU_ASSERT_EQUAL(cnt, 5);
    CU_ASSERT_STRING_EQUAL(send_buf, "token=1234567890KK&node_id=56&epoch_time[]=1234567900%2C123456789&power=360.000000&epoch_time[]=1234567901%2C123456789&power=3600.000000&epoch_time[]=1234567904%2C123456789&power=1200.000000&epoch_time[]=1234567904%2C623456789&power=7200.000000&epoch_time[]=1234567905%2C23456789&power=9000.000000");
}

void testBUZZER_CALC_PULSES(void) {
    cfg.verbose = 1;
    CU_ASSERT_EQUAL(sizeof(buzzer_config), 2 * sizeof(struct buzzer_config));
    CU_ASSERT_EQUAL(buzzer_config[0].power_threshold_kwh, 0);
    CU_ASSERT_EQUAL(buzzer_config[0].time_threshold_sec, 0);
    CU_ASSERT_EQUAL(cfg.power_soft_threshold, 3300);
    CU_ASSERT_EQUAL(cfg.power_hard_threshold, 4000);
    CU_ASSERT_EQUAL(cfg.power_soft_threshold_time, 3600*3);
    CU_ASSERT_EQUAL(cfg.power_hard_threshold_time, 240);
    buzzer_setup();
    CU_ASSERT_EQUAL(buzzer_config[0].power_threshold_kwh, cfg.power_soft_threshold / 3.6e6);
    CU_ASSERT_EQUAL(buzzer_config[0].time_threshold_sec, cfg.power_soft_threshold_time);
    CU_ASSERT_EQUAL(buzzer_config[0].pulses_init, 1);
    CU_ASSERT_EQUAL(buzzer_config[1].power_threshold_kwh, cfg.power_hard_threshold / 3.6e6);
    CU_ASSERT_EQUAL(buzzer_config[1].time_threshold_sec, cfg.power_hard_threshold_time);
    CU_ASSERT_EQUAL(buzzer_config[1].pulses_init, 4);
    struct buzzer_config *t = &buzzer_config[0];
    struct buzzer_power_queue *q = &pqueue[0];
    double tnow = 500000;
    double elapsedkwh = 5.001;
    buzzer_pqueue_push(q, elapsedkwh, tnow);
    buzzer_pqueue_pop(q, t->time_threshold_sec, tnow);
    CU_ASSERT_EQUAL(q->tqh_first->power_acc_kwh, elapsedkwh);
    CU_ASSERT_EQUAL(q->tqh_first->time_sec, tnow);
    CU_ASSERT_EQUAL(buzzer_calc_pulses(q, t), 1);
    
    tnow += 3600;
    elapsedkwh += 3300.0 / 3.6e6 * 3600;
    buzzer_pqueue_push(q, elapsedkwh, tnow);
    buzzer_pqueue_pop(q, t->time_threshold_sec, tnow);
    CU_ASSERT_EQUAL(buzzer_calc_pulses(q, t), 2);

    tnow += 3600;
    elapsedkwh += 3301.0 / 3.6e6 * 3600;
    buzzer_pqueue_push(q, elapsedkwh, tnow);
    buzzer_pqueue_pop(q, t->time_threshold_sec, tnow);
    CU_ASSERT_EQUAL(buzzer_calc_pulses(q, t), 3);

    tnow += 3700;
    elapsedkwh += 3300.0 / 3.6e6;
    buzzer_pqueue_push(q, elapsedkwh, tnow);
    buzzer_pqueue_pop(q, t->time_threshold_sec, tnow);
    CU_ASSERT_EQUAL(buzzer_calc_pulses(q, t), 2);

    tnow += 3600;
    elapsedkwh += 3300.0 / 3.6e6;
    buzzer_pqueue_push(q, elapsedkwh, tnow);
    buzzer_pqueue_pop(q, t->time_threshold_sec, tnow);
    CU_ASSERT_EQUAL(buzzer_calc_pulses(q, t), 1);

    tnow += 3600;
    elapsedkwh += 3600 * 3900.0 / 3.6e6;
    buzzer_pqueue_push(q, elapsedkwh, tnow);
    buzzer_pqueue_pop(q, t->time_threshold_sec, tnow);
    CU_ASSERT_EQUAL(buzzer_calc_pulses(q, t), 2);
}

int main() {
    CU_pSuite pSuite = NULL;

    /* Initialize the CUnit test registry */
    if (CUE_SUCCESS != CU_initialize_registry())
        return CU_get_error();

    /* Add a suite to the registry */
    pSuite = CU_add_suite("cunittest", init_suite, clean_suite);
    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    /* Add the tests to the suite */
    if (
            (NULL == CU_add_test(pSuite, "testPARSE_OPTS", testPARSE_OPTS))
            || (NULL == CU_add_test(pSuite, "testGET_CONFIG_FILE", testGET_CONFIG_FILE))
            || (NULL == CU_add_test(pSuite, "testREAD_CONFIG", testREAD_CONFIG))
            || (NULL == CU_add_test(pSuite, "testINSERT_ENTRY", testINSERT_ENTRY))
            || (NULL == CU_add_test(pSuite, "testBUILD_URL_EMONCMS", testBUILD_URL_EMONCMS))
            || (NULL == CU_add_test(pSuite, "testBUILD_URL_EMONLIGHT", testBUILD_URL_EMONLIGHT))
            || (NULL == CU_add_test(pSuite, "testBUZZER_CALC_PULSES", testBUZZER_CALC_PULSES))
            ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    /* Run all tests using the CUnit Basic interface */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    return CU_get_error();
}
