#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include "PageTable.h"

void DapatSinyal(int);
int n_page;
int n_frame;
int SegmentID;
int SharedMemoryKey;
page_table_pointer PageTable;


int main(int argc, char* argv[]) {
	
	
	n_page = atoi(argv[1]);
	n_frame = atoi(argv[2]);

	SharedMemoryKey = getpid();
	SegmentID = shmget(SharedMemoryKey, n_page * sizeof(page_table_entry) , IPC_CREAT | 0666);
	PageTable = (page_table_pointer) shmat(SegmentID,NULL,0);
	for(int i = 0 ; i<n_page ; i++) {
		PageTable[i].Valid = 0;
		PageTable[i].Frame = -1;
		PageTable[i].Dirty = 0;
		PageTable[i].Requested =0;
	}



	printf("The shared memory key (PID) is %d \n", SharedMemoryKey);
	printf("Initialized page table \n");
	
	signal(SIGUSR1,DapatSinyal);

	while (true) {

	}

	return 0;
}

void DapatSinyal(int){	
	printf("Hore dapat sinyal!\n");
	int i = 0;
	int MMU_Pid;

	while ( i < n_page && PageTable[i].Requested == 0 ) {
		i++;	
	}
	
	if ( i != n_page) {
		MMU_Pid = PageTable[i].Requested;
		printf("%d \n", PageTable[i].Requested);
	}
	

	PageTable[i].Frame = 0;
	PageTable[i].Valid = 1;
	kill(MMU_Pid,SIGCONT);

} 
