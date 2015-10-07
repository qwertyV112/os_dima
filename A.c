#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <curses.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#define die(msg) { perror(msg); exit(EXIT_FAILURE); }
#define FUEL_TIME 5
#define ENTRY_TIME 5
#define KASSA_TIME 5

// ñòðóêòóðà, îïèñûâàþùàÿ ñòàíöèè
struct _stations {
        sem_t sem1, sem2;
        int N1, N2;
};
// ñòðóêòóðà, îïèñûâàþùàÿ êàññó
struct _kassa {
        sem_t sem;
        int queue_length;
};

// ñòðóêòóðà, îïèñûâàþùàÿ ñîñòîÿíèå ìàøèíû
struct car_state {
        char name; // èìÿ
        int state, offset; // ñîñò., ñìåùåíèå ïî Õ
};

struct _stations stations;
struct _kassa kassa;
struct car_state *states;
int max;
sem_t console;
WINDOW * mainwin;

void redraw_state () {
    int i,j, num_queue=0, num_kassa=0;
    char buf[10];

    // æäåì ñåìàôîð äëÿ ðàáîòû ñ êîíñîëüþ(íåñêîëüêî ïîòîêîâ íå äîëæíû ïèñàòü îäíîâðåìåííî)
  //  sem_wait(&console);
    clear();


    // ïðîáåãàåì ïî âñåì ìàøèíàì
    for (j=0;j<max;j++) {
        // è â çàâèñèìîñòè îò ñîñòîÿíèÿ, â êîòîðîì íàõ. ìàøèíà, äåëàåì îòðèñîâêó
        if (states[j].state == 0) {
            sprintf(buf, "%c\n", states[j].name);
            move(33, states[j].offset);
            addch(states[j].name);
        }
        if (states[j].state == 1) {
            move(33, 27+num_queue);
            addch(states[j].name);
            num_queue += 1;
        }
        if (states[j].state == 2) {
            move(30, 35);
            addch(states[j].name);
        }
        if (states[j].state == 3) {
            move(36, 35);
            addch(states[j].name);
        }
        if (states[j].state == 4) {
            move(33, 50+num_kassa);
            addch(states[j].name);
            num_kassa += 1;
        }

        if (states[j].state == 5) {
            move(33, 60);
            addch(states[j].name);
        }



    }

    // âûâîäèò èíôîðìàöèþ
    move(34,27);
    addstr("QUEUE TO BENZ");

    move(31,35);
    addstr("BENZ1");

    move(37,35);
    addstr("BENZ2");
    move(34,45);
    addstr("QUEUE TO KASSA");
    move(34,60);
    addstr("KASSA");


    refresh();
    // îñâîáîæäàåì ñåìàôîð
    sem_post(&console);
}

// ïîòîê, îïèñûâàþùèé ïîâåäåíèå ìàøèíû
static void
car_thread(int car_num)
{
    int i;

    // ïåðåðèñîâûâàåì
    redraw_state();

    // íà÷àëüíîå ñîñòîÿíèå
    states[car_num].state = -1;

    // æäåì ÷èñëî ñåêóíä, ðàâíîå ¹ ìàøèíû
    sleep(car_num);

    // ñîñòîÿíèå - ïîäúåçæàåì
    states[car_num].state = 0;
    for (i=0;i<3;i++)
    {
        sleep(1);
        states[car_num].offset += 6;
        redraw_state();
    }
    // ñîñòîÿíèå - æäåì îñâîáîæäåíèÿ îäíîé èç ñòàíöèé
    states[car_num].state = 1;

    while(1) {
        if(sem_trywait(&stations.sem1) != -1) {
		// çàíÿëè 1þ ñòàíèöèþ
                states[car_num].state = 2;
                break;
        }
        if(sem_trywait(&stations.sem2) != -1) {
   		// çàíÿëè 2-þ ñòàíöèþ
                states[car_num].state = 3;
                break;
       }
    }
    // ïåðåðèñîâûâàåì
    redraw_state();
    // æäåì íåêîòîðîå âðåìÿ íà çàïðàâî÷íîé ñòàíöèè(çàïðàâëÿåìñÿ)
    sleep(FUEL_TIME);

    // îñâîáîæäàåì ñòàíöèþ
    if (states[car_num].state == 2){
        sem_post(&stations.sem1);
    } else {
        sem_post(&stations.sem2);
    }
    // ñîñòîÿíèå - æäåì êàññó
    states[car_num].state = 4;
    redraw_state();
    if (sem_wait(&kassa.sem) == -1) die("sem_post");
    // ñîñòîÿíèå - çàíÿëè êàññó
    states[car_num].state = 5;
    redraw_state();
    // æäåì íêîòîðîå âðåìÿ íà êàññå
    sleep(KASSA_TIME);

    // îñâîáîæäàåì êàññó
    sem_post(&kassa.sem);
    // óåõàëè
    states[car_num].state = 6;
    redraw_state();


} /* handler */



int main() {
        int i;
        pthread_t *car;
        stations.N1 = stations.N2 = 0;

        printf("Input number of cars: ");
        scanf("%d", &max);
	// âûäåëÿåì ïàìÿòü äëÿ õðàíåíèÿ ìàññèâû ñòðóêòóð, îïèñûâàþùèõ ïîòîêè
        car = (pthread_t *)malloc(max*sizeof(pthread_t));
        // âûäåëÿåì ïàìÿòü äëÿ õðåíåíèÿ ñòðóêòóð, îïèñûâàþùèõ ñîñòîÿíèÿ
        states = (struct car_state *)malloc(max*sizeof(struct car_state));

	// èíèöèàëèçèðóåì ncurses
        if ( (mainwin = initscr()) == NULL ) {
            fprintf(stderr, "Error initialising ncurses.\n");
            exit(EXIT_FAILURE);
        }
        cbreak();

	// èíèöèàëèçèðóåì ñåìàôîðû
        if (sem_init(&stations.sem1, 0, 1) == -1) die("sem_init");
        if (sem_init(&stations.sem2, 0, 1) == -1) die("sem_init");
        if (sem_init(&console, 0, 1) == -1) die("sem_init");
        if (sem_init(&kassa.sem, 0, 1) == -1) die("sem_init");

	// èíèöèàëèçèðóåì ñòðóêòóðû, îïèñûâàþùèå ñîñòîÿíèÿ
        for (i = 0;i<max;i++) {
                states[i].name = 'A'+i;
                states[i].state = 0;
                states[i].offset = 0;
        }
        // ñîçäàåì ïîòîêè
        for (i = 0;i<max;i++) {
                pthread_create( &car[0], NULL, &car_thread, i);
        }

//      for (i = 0;i<max;i++) {
//              pthread_join( car[i], NULL);
//      }
        while(1) {
            sleep(10);
        }

        free(car);
        delwin(mainwin);
        endwin();
        refresh();


}
