#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#define N 3072
#define RDTSC(X) asm volatile("lfence;rdtsc; shlq $32, %%rdx; orq %%rdx, %%rax":"=r"(X) :: "%rdx")

struct st{
	int num;
	char name[60];
};

int main(){

	int i, j, k, n;
	struct st *buf;
	buf = (struct st*)malloc(sizeof(struct st)*N);
	for(i = 0; i < N; i++) buf[i].num = i;

	printf("%p\n",&buf[0]);	

	while(1){
		for(i = 0; i < N; i++){
			for(j = 0; j < 25000*10; j++) n = buf[0].num;
			if(i % 2 == 0){
				for(j = 0; j < N; j++){
					for(k = 1; k < 20*25000/N; k++) n = buf[j].num;
				}
			} else {
				for(j = 0; j < N; j++){
					for(k = 1; k < 2*20*25000/N; k++) n = buf[j].num;
				}
			}
		}
	}
	return 0;
}

