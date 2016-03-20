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

#define NOTFOUND -1


int n_page;
int n_frame;
int SegmentID;
int SharedMemoryKey;
page_table_pointer PageTable;
bool * FrameTable;
int DiskAccess;


void PrintPageTable(page_table_entry PageTable[],int NumberOfPages) {
    int Index;
    for (Index =  0;Index < NumberOfPages;Index++) {
        printf("%2d: Valid=%1d Frame=%2d Dirty=%1d Requested=%1d\n",Index,
	PageTable[Index].Valid,PageTable[Index].Frame,PageTable[Index].Dirty,
	PageTable[Index].Requested);
    }
}

void initializeParameter(int argc, char* argv[]){
	if (argc <2) {
		printf("Warning : Wrong parameter \n");
		printf("OS <numberOfPages> <numberofFrame>\n");
		exit(-1);
	}
	else {
		n_page = atoi(argv[1]);
		n_frame = atoi(argv[2]);
		if ((n_page <= 0) || (n_frame <= 0)) {
			printf("Warning : Wrong parameter \n");
			printf("Number of pages & frames should be an integer, greater than zero.. \n");
			exit(-1);
		}
	}

}

void createSharedMemory() {
	DiskAccess = 0;
	SharedMemoryKey = getpid();

	if ( 
	((SegmentID = shmget(SharedMemoryKey, n_page * sizeof(page_table_entry) , IPC_CREAT | 0666)) == -1) ||
	(((PageTable = (page_table_pointer) shmat(SegmentID,NULL,0)) == NULL) ||
	((FrameTable = (bool * ) malloc ( n_frame* sizeof(bool))) == NULL )	
	)
	) {
	     printf("Warning : Shared Memory Error \n");
	 	 printf("Shared memory couldn't be initialized.\n");
	     exit(0);    
	}
	else {
		printf("The shared memory key (PID) is %d \n", SharedMemoryKey);
	}

}

void initializeSharedMemory(){
	for(int i = 0 ; i<n_page ; i++) {
		PageTable[i].Valid = 0;
		PageTable[i].Frame = -1;
		PageTable[i].Dirty = 0;
		PageTable[i].Requested =0;
		PageTable[i].LastUsed = 0;
	}
	for(int i = 0 ; i<n_frame ; i++) {
		FrameTable[i] = false;
	}
	printf("Initialized page table \n");
}




int getRequestedPageTableIdx(){
	int i = 0;
	while ( i < n_page && PageTable[i].Requested == 0 ) {
		i++;	
	}
	if ( i == n_page){
		i = -1;
	}
	return i;
}

int getVictimIdx() {
	int i = 0;
	int idxmax;
	int max;
	while ( i < n_page && PageTable[i].Valid == false ) {
		i++;
	}
	idxmax = i;
	max = PageTable[i].LastUsed;
	for (int i = idxmax ; i< n_page ; i++) {
		if ( (PageTable[i].LastUsed > max) && (PageTable[i].Valid)) {
			max = PageTable[i].LastUsed;			
			idxmax = i;	
		}
	}
	return idxmax;
}

int getFreeFrameIdx(bool FrameTable[] , int n_frame){
	int i = 0;
	while ( i < n_frame && FrameTable[i] == true) {
		i++;
	}
	if (i== n_frame){
		i = -1;
	}
	return i;
}


void initializeSignalProcess(int* MMU_Pid, int* Page_Request  ){
	int i = 0;
	
	i = getRequestedPageTableIdx();
	if ( i == NOTFOUND ) {
		printf("The MMU has finished \n");
		PrintPageTable(PageTable,n_page);
		printf("%d disk accesses required \n", DiskAccess);
		exit(0);
	}
	else {
		*Page_Request = i;
		*MMU_Pid = PageTable[i].Requested;
		printf("Process %d has requested page %d \n", PageTable[i].Requested,i);
		PageTable[*Page_Request].Dirty = 0;
		PageTable[*Page_Request].Requested = 0;
		DiskAccess++;
	}

}


void putInFreeFrame(int i, int Page_Request){
	printf("Put it in free frame %d \n", i); 
	sleep(1);
	PageTable[Page_Request].Valid = 1;
	PageTable[Page_Request].Frame = i;
	FrameTable[i] = true;
	printf("Unblock MMU \n");	
}


void putInVictimFrame(int Page_Request){
	int Victim;
	Victim = getVictimIdx();
		printf("Chose a victim page %d \n", Victim);
		if ( PageTable[Victim].Dirty ) {
			PageTable[Victim].Valid = 0;
			PageTable[Victim].Dirty = 0;
			printf("Victim is dirty , write out \n");
			sleep(1);	
			DiskAccess++;
		}
		printf("Put in victim's frame %d \n", PageTable[Victim].Frame);
		PageTable[Page_Request].Valid = 1;
		PageTable[Page_Request].Frame = PageTable[Victim].Frame;
		PageTable[Victim].Frame = -1;	

}

void sendSignalToMMU(int MMU_Pid, int PageRequest){
	kill(MMU_Pid,SIGCONT);
}


void processSignal(int){	
	int freeFrameIdx;
	int MMU_Pid;
	int Page_Request;
	
	initializeSignalProcess(&MMU_Pid,&Page_Request);
	freeFrameIdx = getFreeFrameIdx(FrameTable, n_frame);
	if ( freeFrameIdx == NOTFOUND ) {
		putInVictimFrame(Page_Request);
	}
	else {
		putInFreeFrame(freeFrameIdx,Page_Request);
	}
	sendSignalToMMU(MMU_Pid,Page_Request);
} 

void waitForSignal(){
	signal(SIGUSR1,processSignal);
	while (true) {}	
}


int main(int argc, char* argv[]) {

	initializeParameter(argc,argv);
	createSharedMemory();
	initializeSharedMemory();
	waitForSignal();
	return 0;
	
}

