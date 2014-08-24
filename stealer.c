#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

/* stealer maximum trial */
#define MAXTRIAL 100

/* number of stealer of moneyguy */
static int stealer_num = 5;
static int moneyguy_num = 5; /*currently this is not supported */
/* time for punishing unsucessful steal */
static int punish_time = 80000;
/* time for saving money */
static int save_time = 10000;
/* deposit time interval */
static int deposit_time = 20000;


/*
 * notify format the message with the name of given as first argument by the
 * caller.  Here use a trick of C compiler: neighbouring strings are concaten-
 * ated.  However, a macro function does not have type checking, which easily
 * makes bug (SEGFALT) when name is not provided.  but i don't want to use
 * stdarg.h, va_list, and the friends ... maybe there is a better workaround.
 */

/* !!!! remember to provide name !!! */
#define notify(mesg, ...) printf( "%15s:" mesg, __VA_ARGS__);

struct account
{
        int balance;
        int max;
        pthread_mutex_t mutex;
        /* moneyguys wait on condfull when the account space is not enough.  It
         * doesn't really mean full */
        pthread_cond_t cond_full;
};

static void *stealer(void *amountptr);
static void *moneyguy(void *amountptr);
static void punish(int amount);
static void save(int givenamount, int *place);
static void deposit(char *name, int amount, struct account *acc);
static void steal(char *name, int amount, struct account *acc);
static void adv_sleep(int nanosec);
static void opt_set(int *var, char *name);

static void usage(void)
{
        printf("./stealer --stealer=n --punish-time=n\n");
        printf("See README for more prarameters\n");
}
static void exit_error(char *msg)
{
        fputs(msg, stderr);
        usage();
        exit(EXIT_FAILURE);
}

static void opt_set(int *var, char *name)
{
        char *endptr;
        if (optarg != NULL)
                *var = strtol(optarg, &endptr, 10);
        if (!endptr || *endptr != '\0' || !optarg){
                /* variable length array, require C99 */
                char msg[strlen("invalid \n") + strlen(name) + 1];
                snprintf(msg, sizeof(msg), "invalid %s\n", name);
                exit_error(msg);
        }
}

static struct account acc1 =
{
        .balance = 10,
        .max = 15,
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .cond_full = PTHREAD_COND_INITIALIZER
};

/* number of currently sleeping moneyguy */
static int sleepedguy = 0;

void check(int ret, char *mesg)
{
        if (ret){
                fprintf(stderr, mesg);
                errno = ret;
                perror("");
                exit(1);
        }
}

int main(int argc, char **argv)
{
        while (1){
                int c;
                static struct option long_options[] =
                {
                        {"stealer", optional_argument, NULL, 's'},
                        {"moneyguy", optional_argument, NULL, 'm'},
                        {"punish-time", optional_argument, NULL, 'p'},
                        {"save-time", optional_argument, NULL, 'a'},
                        {"deposite-time", optional_argument, NULL, 'd'},
                        {0, 0, 0, 0}
                };

                int option_index = 0;
                c = getopt_long(argc, argv, "s:m:p:a:d:",
                                long_options, &option_index);
                
                if (c == -1)
                        break;
                switch (c){
                        case 's':
                                opt_set(&stealer_num, "stealer");
                                break;
                        case 'm':
                                opt_set(&moneyguy_num, "moneyguy");
                                break;
                        case 'p':
                                opt_set(&punish_time, "punish-time");
                                break;
                        case 'a':
                                opt_set(&save_time, "save-time");
                                break;
                        case 'd':
                                opt_set(&deposit_time, "deposit-time");
                                break;
                        default:
                                exit_error("invalid option\n");
                }
        }


        pthread_t steallist[stealer_num];
        pthread_t mguylist[moneyguy_num];

        /* steal_val[n][0] is money to steal each time. steal_val[n][1] is tatal
         * stolen amount.
         */
        int steal_val[stealer_num][2];
        int deposit_val[moneyguy_num];
        int ret;
        
        for (int i=0; i < stealer_num; i++){
                steal_val[i][0] = i + 1;
                ret = pthread_create(&steallist[i], NULL, stealer, &steal_val[i][0]);
                check(ret, "pthread_create");
        }

        for (int i=0; i < moneyguy_num; i++){
                deposit_val[i] = i + 1;
                ret = pthread_create(&mguylist[i], NULL, moneyguy, &deposit_val[i]);
                check(ret, "pthread_create");
        }

        for (int i=0; i < stealer_num; i++){
                int *stolen;
                pthread_join(steallist[i], (void**) &stolen);
                steal_val[i][1] = *stolen;
                free(stolen);
                check(ret, "pthread_join");
                printf("joined stealer %d\n\n", i+1);
        }

        /* some folks say that the mutex and condition variable need to be
         * destroyed (see Uderstanding and Using C Pointers, Page 188),
         * however, this guy
         * (http://www2.chrishardick.com:1099/Notes/Computing/C/pthreads/mutexes.html)
         * say it does not need to, for a statically allocated one.
         */

        fputs("\n========================\n"
              "main: all stealer joined\n"
              "========================\n\n",
              stdout);

        for (int i=0; i < moneyguy_num; i++){
                pthread_cancel(mguylist[i]);
                check(ret, "pthread_join");
        }

        fputs("\n=========================\n"
              "main: all moneyguy killed\n"
              "=========================\n\n",
              stdout);

        pthread_mutex_destroy(&acc1.mutex);
        for (int i=0; i < stealer_num; i++){
                printf("stealer %d stolen %d\n", i+1, steal_val[i][1]);
        }
}

static void adv_sleep(int usec)
{
        /*
         * usleep is depracated by linux, but nanosleep is a bit clumpsy.
         * here implement usleep interface by nanosleep . nanosleep accept
         * long, but it cannot prevent passing in a signed value, because the
         * compiler perform implict type conversion. So, assert
         *
         * From the man page, nanosleep only sleep a thread.
         */

        assert(usec >= 0);
        struct timespec req = { .tv_sec = 0,
                                .tv_nsec = usec * 1e3 };
        nanosleep(&req, NULL);
}

static void *stealer(void *amountptr)
{
        int amount = *(int*)amountptr;
        char name[15];
        sprintf(name, "stealer %d", amount);

        notify("born\n", name);
        int *mypocketptr = malloc(sizeof(int));    /* no money, so have to steal */
        int trial = 0;

        while (trial < MAXTRIAL){
                pthread_mutex_lock(&acc1.mutex);
                trial++;
                notify("start trial %d\n", name, trial);
                if (amount > acc1.balance){
                        notify("failed to steal\n", name);
                        pthread_mutex_unlock(&acc1.mutex);
                        punish(amount);
                } else {
                        steal(name, amount, &acc1);
                        if (sleepedguy != 0){
                                pthread_cond_broadcast(&acc1.cond_full);
                        }
                        pthread_mutex_unlock(&acc1.mutex);
                        save(amount, mypocketptr);
                }
        }
        return mypocketptr;
}

static void *moneyguy(void *amountptr)
{
        int amount = *(int*)amountptr;
        char name[15];
        sprintf(name, "moneyguy %d", amount);

        notify("born\n", name);
        while (1){
                pthread_mutex_lock(&acc1.mutex);

                while ((acc1.max - acc1.balance) < amount){
                        /* not enough space to deposit */
                        sleepedguy++;
                        notify("go sleep\n", name);
                        /* sleep and drop mutex in a atomic functon */
                        pthread_cond_wait(&acc1.cond_full, &acc1.mutex);
                        /* here mutex is automatically aquired again */
                        notify("waked!\n", name);
                }

                deposit(name, amount, &acc1);
                pthread_mutex_unlock(&acc1.mutex);
                adv_sleep(deposit_time * (double) random() / RAND_MAX);
                sleep(0.1 * ((double) random()) / RAND_MAX);
        }
}

static void punish(int amount)
{
        adv_sleep(punish_time * random() / RAND_MAX);
}

static void save(int givenamount, int *place)
{
        *place += givenamount;
        adv_sleep(save_time * random() / RAND_MAX);
}
static void steal(char *name, int amount, struct account *acc)
{
        acc->balance -= amount;
        notify("stealed $%d\n", name, amount);
}

static void deposit(char *name, int amount, struct account *acc)
{
        acc->balance += amount;
        notify("deposited $%d\n", name, amount);
        printf("\nbalance: %d\n\n", acc->balance);
}

