/*
 * There are some ATM cracked by aome stealer. some guy are crazily depositing
 * money into the account.
 * With some reason, the ATM cannot tell how much $ left. The stealer each own 1
 * ATM, and the have to decide a fixed amount to retrieve from the machine for a
 * maximum number of frial. If the money left is not enough, he canot get a
 * mosquito.
 * The crazy money giver goto sleep when the ATM is full of money. The stealer
 * wake them up after stealing.
 *
 * It seems that a higher amount to steal may not be good if the guys have to
 * spend a lot od\f time to deposit money (the account is always empty)
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

/* number of stealer */
#define NOSTEALER 5
/* stealer maximum trial */
#define MAXTRIAL 100
/* deposit time interval */
#define DEPINTERVAL 80000
/* time for saving money */
#define SAVINTERVAL 10000
/* time for punishing unsucessful steal */
#define PUNINTERVAL 20000

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

int main(void)
{
        pthread_t steallist[NOSTEALER];
        pthread_t mguylist[NOSTEALER];
        int amountlist[NOSTEALER][2];
        int ret;
        
        for (int i=0; i < NOSTEALER; i++){
                amountlist[i][0] = i + 1;
                ret = pthread_create(&steallist[i], NULL, stealer, &amountlist[i][0]);
                check(ret, "pthread_create");
                ret = pthread_create(&mguylist[i], NULL, moneyguy, &amountlist[i][0]);
                check(ret, "pthread_create");
        }

        for (int i=0; i < NOSTEALER; i++){
                int *stolen;
                pthread_join(steallist[i], (void**) &stolen);
                amountlist[i][1] = *stolen;
                free(stolen);
                check(ret, "pthread_join");
                printf("joined stealer %d\n\n", i+1);
        }
        fputs("\n========================\n"
              "main: all stealer joined\n"
              "========================\n\n",
              stdout);

        for (int i=0; i < NOSTEALER; i++){
                pthread_cancel(mguylist[i]);
                check(ret, "pthread_join");
        }
        fputs("\n========================\n"
              "main: all moneyguy killed\n"
              "========================\n\n",
              stdout);
        for (int i=0; i < NOSTEALER; i++){
                printf("stealer %d stolen %d\n", i+1, amountlist[i][1]);
        }
}

static void adv_sleep(int usec)
{
        /*
         * usleep is depracated by linux, but nanosleep is a bit clumpsy
         * here implement usleep interface by nanosleep . nanosleep accept
         * long, but it cannot prevent passing in a signed value, because the
         * compiler perform implict type conversion. So, assert
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
                adv_sleep(DEPINTERVAL * random() / RAND_MAX);
                sleep(0.1 * ((double) random()) / RAND_MAX);
        }
}

static void punish(int amount)
{
        adv_sleep(PUNINTERVAL * random() / RAND_MAX);
}

static void save(int givenamount, int *place)
{
        *place += givenamount;
        adv_sleep(SAVINTERVAL * random() / RAND_MAX);
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

