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

#include <CUnit/Basic.h>

#include "main.h"
#include "read_config.h"
#include "buzzer.h"
#include "sender.h"

//extern struct cfg_t cfg;

//extern CURL *curl;

//extern volatile int stop;
//extern long pulseCount, rawCount;
//extern struct timespec tstart, tlast;
extern char send_buf[1024];
extern struct buzzer_config buzzer_config[2];
extern struct buzzer_power_queue pqueue[2];

extern int build_url_emoncms(struct server_entry *server);
extern int build_url_emonlight(struct server_entry *server);
extern double calc_power(struct source_entry *source, double dt);
extern struct send_entry * insert_entry(struct server_mapping_entry *map, struct timespec tlast, struct timespec trec, double dt, double power, double elapsed_kWh, long pulse_count, long raw_count);
extern void parse_opts(int argc, char **argv);
extern char* get_config_file(const char *config_file);
extern int read_config(const char *config_file);
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
    cfg.daemonize = 0;
    cfg.config = config;
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
    char **argv = (char *[]){"prg", "-v", "-c", "config_file"};
    parse_opts(4, argv);
    CU_ASSERT(cfg.verbose == 1);
    CU_ASSERT_STRING_EQUAL(cfg.config, "config_file");
}

void testREAD_CONFIG(void) {
    memset(&cfg, 0, sizeof (cfg));
    CU_ASSERT_PTR_NULL(cfg.config);
    CU_ASSERT_FALSE(access("tests/emonlight1.conf", F_OK));
    CU_ASSERT_EQUAL(init_config("tests/emonlight1.conf"), 1);
    CU_ASSERT_EQUAL(init_config("XXXX"), 1);
    CU_ASSERT_EQUAL_FATAL(init_config("tests/emonlight.conf"), 0);

    CU_ASSERT_EQUAL(cfg.sources_length, 2);
    CU_ASSERT_EQUAL(cfg.sources[0].pin, 17);
    CU_ASSERT_EQUAL(cfg.sources[0].ppkwh, 1000);
    CU_ASSERT_EQUAL(cfg.sources[0].maps_length, 2);
    CU_ASSERT_PTR_EQUAL(cfg.sources[0].maps[0], &cfg.servers[0].maps[0]);
    CU_ASSERT_PTR_EQUAL(cfg.sources[0].maps[1], &cfg.servers[1].maps[0]);    
    CU_ASSERT_EQUAL(cfg.sources[1].pin, 3);
    CU_ASSERT_EQUAL(cfg.sources[1].ppkwh, 1200);
    CU_ASSERT_EQUAL(cfg.sources[1].maps_length, 2);
    CU_ASSERT_PTR_EQUAL(cfg.sources[1].maps[0], &cfg.servers[0].maps[1]);
    CU_ASSERT_PTR_EQUAL(cfg.sources[1].maps[1], &cfg.servers[1].maps[1]);    

    CU_ASSERT_EQUAL(cfg.servers_length, 2);
    CU_ASSERT_STRING_EQUAL(cfg.servers[0].url, "http://emoncms.org")
    CU_ASSERT_EQUAL(cfg.servers[0].protocol, EMONCMS_REMOTE_ID);
    CU_ASSERT_EQUAL(cfg.servers[0].maps_length, 2);
    CU_ASSERT_EQUAL(cfg.servers[0].maps[0].node_id, 5);
    CU_ASSERT_EQUAL(cfg.servers[0].maps[0].queued, 0);
    CU_ASSERT_EQUAL(cfg.servers[0].maps[0].queue_length, 0);
    
    //    printf("P=%d, Q=%d\n", cfg.pin, cfg.queue_size);
}

void testINSERT_ENTRY(void) {
    struct server_entry *server = cfg.servers;
    struct server_mapping_entry *map = server->maps;
    CU_ASSERT_EQUAL(map->queue_length, 0);
    CU_ASSERT_EQUAL(map->queued, 0);
    CU_ASSERT_TRUE(TAILQ_EMPTY(&map->queue));
//    printf("S=%d\n", sizeof(struct send_entry));
    CU_ASSERT_EQUAL(sizeof(struct send_entry), 56);
    CU_ASSERT_PTR_NULL(TAILQ_FIRST(&map->queue));
    struct timespec t0 = {1234567890, 123456789};
    struct timespec t1 = {1234567900, 123456789};
    insert_entry(map, t0, t1, time_diff(t1, t0), calc_power(map->source, 10), 0.003, 1, 1);
    struct send_entry *e = TAILQ_FIRST(&map->queue);
    CU_ASSERT_PTR_NOT_NULL(e);
    CU_ASSERT_EQUAL(e->tlast.tv_sec, 1234567890);
    CU_ASSERT_EQUAL(e->tlast.tv_nsec, 123456789);
    CU_ASSERT_EQUAL(e->trec.tv_sec, 1234567900);
    CU_ASSERT_EQUAL(e->trec.tv_nsec, 123456789);
    CU_ASSERT_DOUBLE_EQUAL(e->dt, 10, 1e-6);
    CU_ASSERT_DOUBLE_EQUAL(e->power, calc_power(map->source, 10), 1e-6);
    CU_ASSERT_EQUAL(e->pulse_count, 1);
    t0.tv_sec += 30;
    insert_entry(map, t1, t0, 20, calc_power(map->source, 20), 0.004, 2, 2);
    e = e->entries.tqe_next;
    CU_ASSERT_PTR_NOT_NULL(e);
    //    printf("S=%ld\n", e->trec.tv_sec);
    CU_ASSERT_EQUAL(e->tlast.tv_sec, 1234567900);
    CU_ASSERT_EQUAL(e->tlast.tv_nsec, 123456789);
    CU_ASSERT_EQUAL(e->trec.tv_sec, 1234567920);
    CU_ASSERT_EQUAL(e->trec.tv_nsec, 123456789);
    CU_ASSERT_DOUBLE_EQUAL(e->dt, 20, 1e-6);
    CU_ASSERT_DOUBLE_EQUAL(e->power, calc_power(map->source, 20), 1e-6);
    CU_ASSERT_EQUAL(e->pulse_count, 2);
}

void testBUILD_URL_EMONCMS(void) {
    memset(send_buf, 0, sizeof(send_buf));
    CU_ASSERT_EQUAL(init_config("tests/emonlight.conf"), 0);
    struct server_entry *server = cfg.servers;
    struct server_mapping_entry *map = server->maps;
    cfg.verbose = 1;
    struct timespec t0 = {1234567890, 123456789};
    struct timespec t1 = {1234567900, 123456789};
    insert_entry(map, t0, t1, 10, calc_power(map->source, 10), 0.003, 1, 1);
    int cnt = build_url_emoncms(server);
    //printf("P=%f, L=%d, QQ=%s\n", calc_power(map->source, 10), strlen(send_buf), send_buf);
    CU_ASSERT_EQUAL(cnt, 1);
    CU_ASSERT_STRING_EQUAL(send_buf, "http://emoncms.org/input/bulk.json?data=[[0,5,360.000000,0.003000,1],[9,5,360.000000,0.003000,1]]&time=1234567891&apikey=key-1");

    t0.tv_sec = t1.tv_sec + 1;
    insert_entry(map, t1, t0, 1, calc_power(map->source, 1), 0.004, 2, 2);
    CU_ASSERT_EQUAL(TAILQ_LAST(&map->queue, send_queue)->tlast.tv_sec, 1234567900);
    CU_ASSERT_EQUAL(TAILQ_LAST(&map->queue, send_queue)->trec.tv_sec, 1234567901);
    cnt = build_url_emoncms(server);
    //printf("P=%f, L=%d, QQ=%s\n", calc_power(map->source, 1), strlen(send_buf), send_buf);
    CU_ASSERT_EQUAL(cnt, 2);
    CU_ASSERT_STRING_EQUAL(send_buf, "http://emoncms.org/input/bulk.json?data=[[0,5,360.000000,0.003000,1],[9,5,360.000000,0.003000,1],[10,5,3600.000000,0.004000,2]]&time=1234567891&apikey=key-1");

    t1.tv_sec = t0.tv_sec + 3;
    insert_entry(map, t0, t1, 3, calc_power(map->source, 3), 0.005, 3, 3);
    CU_ASSERT_EQUAL(TAILQ_LAST(&map->queue, send_queue)->tlast.tv_sec, 1234567901);
    CU_ASSERT_EQUAL(TAILQ_LAST(&map->queue, send_queue)->trec.tv_sec, 1234567904);
    cnt = build_url_emoncms(server);
    //printf("P=%f, L=%d, QQ=%s\n", calc_power(map->source, 3), strlen(send_buf), send_buf);
    CU_ASSERT_EQUAL(cnt, 3);
    CU_ASSERT_STRING_EQUAL(send_buf, "http://emoncms.org/input/bulk.json?data=[[0,5,360.000000,0.003000,1],[9,5,360.000000,0.003000,1],[10,5,3600.000000,0.004000,2],[11,5,1200.000000,0.005000,3],[13,5,1200.000000,0.005000,3]]&time=1234567891&apikey=key-1");

    t0.tv_sec = t1.tv_sec;
    t0.tv_nsec += 500000000;
    insert_entry(map, t1, t0, 0.5, calc_power(map->source, 0.5), 0.006, 4, 4);
    CU_ASSERT_EQUAL(TAILQ_LAST(&map->queue, send_queue)->tlast.tv_sec, 1234567904);
    CU_ASSERT_EQUAL(TAILQ_LAST(&map->queue, send_queue)->trec.tv_sec, 1234567904);
    cnt = build_url_emoncms(server);
    //printf("P=%f, CNT=%d, L=%d, QQ=%s\n", calc_power(map->source, 0.5), cnt, strlen(send_buf), send_buf);
    CU_ASSERT_EQUAL(cnt, 4);
    CU_ASSERT_STRING_EQUAL(send_buf, "http://emoncms.org/input/bulk.json?data=[[0,5,360.000000,0.003000,1],[9,5,360.000000,0.003000,1],[10,5,3600.000000,0.004000,2],[11,5,1200.000000,0.005000,3],[13,5,1200.000000,0.005000,3],[14,5,7200.000000,0.006000,4]]&time=1234567891&apikey=key-1");

    cfg.verbose = 0;
    t1.tv_sec++;
    t1.tv_nsec -= 100000000;
    insert_entry(map, t0, t1, 0.4, calc_power(map->source, 0.4), 0.007, 5, 5);
    CU_ASSERT_EQUAL(TAILQ_LAST(&map->queue, send_queue)->tlast.tv_sec, 1234567904);
    CU_ASSERT_EQUAL(TAILQ_LAST(&map->queue, send_queue)->trec.tv_sec, 1234567905);
    cnt = build_url_emoncms(server);
    //    printf("P=%f, CNT=%d, L=%d, QQ=%s\n", calc_power(map->source, 0.5), cnt, strlen(send_buf), send_buf);
    CU_ASSERT_EQUAL(cnt, 5);
    CU_ASSERT_STRING_EQUAL(send_buf, "http://emoncms.org/input/bulk.json?data=[[0,5,360.000000,0.003000,1],[9,5,360.000000,0.003000,1],[10,5,3600.000000,0.004000,2],[11,5,1200.000000,0.005000,3],[13,5,1200.000000,0.005000,3],[14,5,7200.000000,0.006000,4],[14,5,9000.000000,0.007000,5]]&time=1234567891&apikey=key-1");
}

void testBUILD_URL_EMONLIGHT(void) {
    memset(send_buf, 0, sizeof(send_buf));
    CU_ASSERT_EQUAL(init_config("tests/emonlight.conf"), 0);
    struct server_entry *server = cfg.servers;
    struct server_mapping_entry *map = server->maps;
    cfg.verbose = 1;
    struct timespec t0 = {1234567890, 123456789};
    struct timespec t1 = {1234567900, 123456789};
    insert_entry(map, t0, t1, 10, calc_power(map->source, 10), 0.003, 1, 1);
    CU_ASSERT_EQUAL(map->queue_length, 1);
    int cnt = build_url_emonlight(server);
//    printf("Q='%s'\n", send_buf);
    CU_ASSERT_EQUAL(cnt, 1);
    CU_ASSERT_EQUAL(map->queue_length, 1);
    CU_ASSERT_EQUAL(map->queued, 1);
    CU_ASSERT_STRING_EQUAL(send_buf, "{nodes: [{k:'key-1',id:5,d:[[1234567900,123456789,360.000000]]}]}");

    map->queued = 0;
    t0.tv_sec = t1.tv_sec + 1;
    insert_entry(map, t1, t0, 1, calc_power(map->source, 1), 0.004, 2, 2);
    CU_ASSERT_EQUAL(TAILQ_LAST(&map->queue, send_queue)->tlast.tv_sec, 1234567900);
    CU_ASSERT_EQUAL(TAILQ_LAST(&map->queue, send_queue)->trec.tv_sec, 1234567901);
    cnt = build_url_emonlight(server);
//    printf("Q='%s'\n", send_buf);
    CU_ASSERT_EQUAL(cnt, 2);
    CU_ASSERT_EQUAL(map->queue_length, 2);
    CU_ASSERT_EQUAL(map->queued, 2);
    CU_ASSERT_STRING_EQUAL(send_buf, "{nodes: [{k:'key-1',id:5,d:[[1234567900,123456789,360.000000],[1234567901,123456789,3600.000000]]}]}");

    map->queued = 0;
    t1.tv_sec = t0.tv_sec + 3;
    insert_entry(map, t0, t1, 3, calc_power(map->source, 3), 0.005, 3, 3);
    CU_ASSERT_EQUAL(TAILQ_LAST(&map->queue, send_queue)->tlast.tv_sec, 1234567901);
    CU_ASSERT_EQUAL(TAILQ_LAST(&map->queue, send_queue)->trec.tv_sec, 1234567904);
    cnt = build_url_emonlight(server);
//    printf("Q='%s'\n", send_buf);
    CU_ASSERT_EQUAL(cnt, 3);
    CU_ASSERT_STRING_EQUAL(send_buf, "{nodes: [{k:'key-1',id:5,d:[[1234567900,123456789,360.000000],[1234567901,123456789,3600.000000],[1234567904,123456789,1200.000000]]}]}");

    map->queued = 0;
    t0.tv_sec = t1.tv_sec;
    t0.tv_nsec += 500000000;
    insert_entry(map, t1, t0, 0.5, calc_power(map->source, 0.5), 0.006, 4, 4);
    CU_ASSERT_EQUAL(TAILQ_LAST(&map->queue, send_queue)->tlast.tv_sec, 1234567904);
    CU_ASSERT_EQUAL(TAILQ_LAST(&map->queue, send_queue)->trec.tv_sec, 1234567904);
    cnt = build_url_emonlight(server);
//    printf("Q='%s'\n", send_buf);
    CU_ASSERT_EQUAL(cnt, 4);
    CU_ASSERT_STRING_EQUAL(send_buf, "{nodes: [{k:'key-1',id:5,d:[[1234567900,123456789,360.000000],[1234567901,123456789,3600.000000],[1234567904,123456789,1200.000000],[1234567904,623456789,7200.000000]]}]}");

    cfg.verbose = 0;
    map->queued = 0;
    t1.tv_sec++;
    t1.tv_nsec -= 100000000;
    insert_entry(map, t0, t1, 0.4, calc_power(map->source, 0.4), 0.007, 5, 5);
    CU_ASSERT_EQUAL(TAILQ_LAST(&map->queue, send_queue)->tlast.tv_sec, 1234567904);
    CU_ASSERT_EQUAL(TAILQ_LAST(&map->queue, send_queue)->trec.tv_sec, 1234567905);
    cnt = build_url_emonlight(server);
//    printf("Q='%s'\n", send_buf);
    CU_ASSERT_EQUAL(cnt, 5);
    CU_ASSERT_STRING_EQUAL(send_buf, "{nodes: [{k:'key-1',id:5,d:[[1234567900,123456789,360.000000],[1234567901,123456789,3600.000000],[1234567904,123456789,1200.000000],[1234567904,623456789,7200.000000],[1234567905,23456789,9000.000000]]}]}");
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
    buzzer_init();
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
