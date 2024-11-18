#include "lib.h"
#include "types.h"

int boundedBuffer(void);
int philosopher(void);
int reader_writer(void);

int uEntry(void)
{
	// For lab4.1
	// Test 'scanf'
  	int dec = 0;
	int hex = 0;
	char str[6];
	char cha = 0; 
	int ret = 0;
 	while (1)
	{
		printf("Input:\" Test %%c Test %%6s %%d %%x\"\n");
		ret = scanf(" Test %c Test %6s %d %x", &cha, str, &dec, &hex);
		printf("Ret: %d; %c, %s, %d, %x.\n", ret, cha, str, dec, hex);
		if (ret == 4)
			break;
	}  

	// For lab4.2
	// Test 'Semaphore'
  	int i = 4;

	sem_t sem;
	printf("Father Process: Semaphore Initializing.\n");
	ret = sem_init(&sem, 2);
	if (ret == -1)
	{
		printf("Father Process: Semaphore Initializing Failed.\n");
		exit();
	}

	ret = fork();
	if (ret == 0)
	{
		while (i != 0)
		{
			i--;
			printf("Child Process: Semaphore Waiting.\n");
			sem_wait(&sem);
			printf("Child Process: In Critical Area.\n");
		}
		printf("Child Process: Semaphore Destroying.\n");
		sem_destroy(&sem);
		exit();
	}
	else if (ret != -1)
	{
		while (i != 0)
		{
			i--;
			printf("Father Process: Sleeping.\n");
			sleep(128);
			printf("Father Process: Semaphore Posting.\n");
			sem_post(&sem);
		}
		printf("Father Process: Semaphore Destroying.\n");
		sem_destroy(&sem);
		exit();
	}  

	// For lab4.3
	// TODO: You need to design and test the problem.
	// Note that you can create your own functions.
	// Requirements are demonstrated in the guide.
	//boundedBuffer();
	//philosopher();
	return 0;
}

//bounded buffer
void deposit(sem_t* mutex, sem_t* fullBuffers, sem_t* emptyBuffers)
{
	//生产者
	int i = 2;
	while (i > 0)
	{
		sem_wait(emptyBuffers);
		sleep(128);
		sem_wait(mutex);
		sleep(128);
		int id=getpid()-1;
		printf("Producer %d : produce\n",id);
		sleep(128);
		sem_post(mutex);
		sleep(128);
		sem_post(fullBuffers);
		sleep(128);
		i--;
	}
}
void remove(sem_t* mutex, sem_t* fullBuffers, sem_t* emptyBuffers)
{
	int i = 8;
	while (i > 0)
	{
		sem_wait(fullBuffers);
		sleep(128);
		sem_wait(mutex);
		sleep(128);
		printf("Consumer : consume\n");
		sleep(128);
		sem_post(mutex);
		sleep(128);
		sem_post(emptyBuffers);
		sleep(128);
		i--;
	}
}

int boundedBuffer(void){
	printf("-----boundedBuffer-----\n");
	int n = 4;    	// buffer size	
	int producer = 4;
	int consumer = 1;
	sem_t mutex,fullBuffers,emptyBuffers;
	sem_init(&mutex,1);
	sem_init(&fullBuffers,0); 
	sem_init(&emptyBuffers,n);
	int ret;
	while(producer >0){
		ret = fork();		
		if(ret == 0){
			deposit(&mutex,&fullBuffers,&emptyBuffers);
			exit();
		}		
		producer-=1;
	}
	while(consumer >0){
		ret = fork();		
		if(ret == 0){
			remove(&mutex,&fullBuffers,&emptyBuffers);
			exit();
		}	
		consumer-=1;
	}
	exit();
	return 0;
}


int philosopher(void) {
	printf("----philosopher----\n");
	sem_t forks[5];
	for (int i = 0; i < 5; i++){
		sem_init(&forks[i], 1);
	}
	for(int i=0,ret=0;i<5;++i){
		ret = fork();
		if(ret == 0){
			int id = getpid()-1;
			while(1){
				printf("Philosopher %d : think\n",id);
				sleep(128);
				if(i%2 == 0){
					sem_wait(&forks[i]);
					sleep(128);
					sem_wait(&forks[(i+1)%5]);
					sleep(128);
				}
				else{
					sem_wait(&forks[(i+1)%5]);
					sleep(128);
					sem_wait(&forks[i]);
					sleep(128);
				}
				printf("Philosopher %d : eat\n",id);
				sleep(128);
				sem_post(&forks[i]);
				sleep(128);
				sem_post(&forks[(i+1)%5]);
				sleep(128);
			}
			exit();
		}

	}
	exit();
	return 0;
}
