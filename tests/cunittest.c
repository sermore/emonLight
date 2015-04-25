/*
 * File:   commoncunittest.c
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

extern struct cfg_t cfg;

extern struct send_queue send_q;
extern int send_queue_length;
//extern CURL *curl;

//extern volatile int stop;
//extern mqd_t mq;
//extern long pulseCount, rawCount;
//extern struct timespec tstart, tlast;
extern char send_buf[1024];

extern int build_url();
extern double power_interpolation(long t, struct send_entry *p0, struct send_entry *p1);
extern struct send_entry *insert_entry(struct timespec trec, double power, double elapsedkWh, long pulseCount);
extern mqd_t open_pulse_queue(int oflag);
extern void parse_opts(int argc, char **argv);
extern void cleanup_queue(mqd_t mq);
extern char* get_config_file(const char *config_file);

/*
 * CUnit Test Suite
 */

int init_suite(void) {
    return 0;
}

int clean_suite(void) {
    return 0;
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
    CU_ASSERT(cfg.pin == 32);
}

void testREAD_CONFIG(void) {
    bzero(&cfg, sizeof (cfg));
    CU_ASSERT_PTR_NULL(cfg.config);
    CU_ASSERT_EQUAL(cfg.pin, 0);
    CU_ASSERT_EQUAL(cfg.queue_size, 0);
    cfg.sender = 1;
    cfg.receiver = 1;
    CU_ASSERT_EQUAL(read_config(cfg.config), 1);
    bzero(&cfg, sizeof (cfg));
    cfg.sender = 1;
    cfg.receiver = 1;
    cfg.config = "XXXXX";
    CU_ASSERT_EQUAL(read_config(cfg.config), 2);
    bzero(&cfg, sizeof (cfg));
    cfg.config = "tests/emonlight.conf";
    cfg.sender = 1;
    cfg.receiver = 1;
    CU_ASSERT_EQUAL(read_config(cfg.config), 0);
    CU_ASSERT_STRING_EQUAL(cfg.api_key, "1234567890KK");
    CU_ASSERT_STRING_EQUAL(cfg.emocms_url, "http://xx.yy.zz");
    printf("P=%d, Q=%d\n", cfg.pin, cfg.queue_size);
    CU_ASSERT_EQUAL(cfg.pin, 12);
    CU_ASSERT_EQUAL(cfg.queue_size, 345);
    CU_ASSERT_EQUAL(cfg.node_id, 56);
}

void testINSERT_ENTRY(void) {
    send_queue_length = 0;
    TAILQ_INIT(&send_q);
    CU_ASSERT_PTR_NULL(TAILQ_FIRST(&send_q));
    struct timespec t = {1234567890, 123456789};
    insert_entry(t, 123.45, 0.003, 1);
    struct send_entry *e = TAILQ_FIRST(&send_q);
    CU_ASSERT_PTR_NOT_NULL(e);
    CU_ASSERT_EQUAL(e->trec.tv_sec, 1234567890);
    CU_ASSERT_EQUAL(e->trec.tv_nsec, 123456789);
    CU_ASSERT_DOUBLE_EQUAL(e->power, 123.45, 1e-6);
    CU_ASSERT_EQUAL(e->pulseCount, 1);
    t.tv_sec++;
    insert_entry(t, 250.45, 0.004, 2);
    e = e->entries.tqe_next;
    CU_ASSERT_PTR_NOT_NULL(e);
    //    printf("S=%ld\n", e->trec.tv_sec);
    CU_ASSERT_EQUAL(e->trec.tv_sec, 1234567891);
    CU_ASSERT_EQUAL(e->trec.tv_nsec, 123456789);
    CU_ASSERT_DOUBLE_EQUAL(e->power, 250.45, 1e-6);
    CU_ASSERT_EQUAL(e->pulseCount, 2);
}

void testPOWER_INTERPOLATION(void) {
    struct send_entry *p0 = malloc(sizeof (struct send_entry));
    //copy_time(&p0.trec, t0);
    p0->trec.tv_nsec = 500000000;
    p0->trec.tv_sec = 1L;
    p0->power = 500.0;
    p0->pulseCount = 0;

    struct send_entry *p1 = malloc(sizeof (struct send_entry));
    p1->trec.tv_nsec = 500000000;
    p1->trec.tv_sec = 2L;
    p1->power = 1500.0;
    p1->pulseCount = 1;
    double p = power_interpolation(2L, p0, p1);
    //    printf("P=%f\n", p);
    CU_ASSERT_DOUBLE_EQUAL(p, 1000.0, 1e-3);
    p0->trec.tv_nsec = 750000000;
    p1->trec.tv_nsec = 750000000;
    p = power_interpolation(2L, p0, p1);
    //    printf("P=%f\n", p);
    CU_ASSERT_DOUBLE_EQUAL(p, 750.0, 1e-3);
    p0->trec.tv_nsec = 250000000;
    p1->trec.tv_nsec = 250000000;
    p = power_interpolation(2L, p0, p1);
    //    printf("P=%f\n", p);
    CU_ASSERT_DOUBLE_EQUAL(p, 1250.0, 1e-3);
}

void testBUILD_URL(void) {
    send_queue_length = 0;
    TAILQ_INIT(&send_q);
    struct timespec t = {1234567890, 123456789};
    insert_entry(t, 123.45, 0.003, 1);
    int cnt = build_url();
    CU_ASSERT_EQUAL(cnt, 0);

    t.tv_sec++;
    insert_entry(t, 250.45, 0.004, 2);
    cnt = build_url();
    double p = power_interpolation(t.tv_sec, TAILQ_FIRST(&send_q), TAILQ_FIRST(&send_q)->entries.tqe_next);
    printf("P=%f, L=%d, QQ=%s\n", p, strlen(send_buf), send_buf);
    CU_ASSERT_EQUAL(cnt, 1);
    CU_ASSERT_STRING_EQUAL(send_buf, "http://emoncms.org/input/bulk.json?apikey=1234567890KK&data=[[0,1,234.770988,0.003000,1]]&time=1234567891");

    t.tv_sec++;
    insert_entry(t, 787.45, 0.005, 3);
    cnt = build_url();
    p = power_interpolation(t.tv_sec, TAILQ_FIRST(&send_q)->entries.tqe_next, TAILQ_FIRST(&send_q)->entries.tqe_next->entries.tqe_next);
    printf("P=%f, L=%d, QQ=%s\n", p, strlen(send_buf), send_buf);
    CU_ASSERT_EQUAL(cnt, 2);
    CU_ASSERT_STRING_EQUAL(send_buf, "http://emoncms.org/input/bulk.json?apikey=1234567890KK&data=[[0,1,234.770988,0.003000,1],[1,1,721.153704,0.004000,2]]&time=1234567891");

    t.tv_nsec += 500000000;
    insert_entry(t, 1000, 0.006, 4);
    cnt = build_url();
    printf("P=%f, CNT=%d, L=%d, QQ=%s\n", p, cnt, strlen(send_buf), send_buf);
    CU_ASSERT_EQUAL(cnt, 3);
    CU_ASSERT_STRING_EQUAL(send_buf, "http://emoncms.org/input/bulk.json?apikey=1234567890KK&data=[[0,1,234.770988,0.003000,1],[1,1,721.153704,0.004000,2]]&time=1234567891");

    t.tv_nsec -= 500000000;
    t.tv_sec++;
    insert_entry(t, 1200, 0.007, 5);
    cnt = build_url();
    struct send_entry *q = TAILQ_LAST(&send_q, send_queue);
    p = power_interpolation(t.tv_sec, TAILQ_PREV(q, send_queue, entries), q);
    printf("P=%f, CNT=%d, L=%d, QQ=%s\n", p, cnt, strlen(send_buf), send_buf);
    CU_ASSERT_EQUAL(cnt, 4);
    CU_ASSERT_STRING_EQUAL(send_buf, "http://emoncms.org/input/bulk.json?apikey=1234567890KK&data=[[0,1,234.770988,0.003000,1],[1,1,721.153704,0.004000,2],[2,1,1150.617284,0.006000,4]]&time=1234567891");
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
            || (NULL == CU_add_test(pSuite, "testPOWER_INTERPOLATION", testPOWER_INTERPOLATION))
            || (NULL == CU_add_test(pSuite, "testBUILD_URL", testBUILD_URL))
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
