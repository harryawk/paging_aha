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

void ProcessSignal(int);
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

void UpdateLastUsedTime(page_table_entry PageTable[], int NumberOfPages){
	for (int i = 0 ; i < NumberOfPages ; i++) {
		if (PageTable[i].Valid) {
			PageTable[i].LastUsed++;					
		}
	}
}

int GetVictim() {
	int i = 0;
	int idxmax;
	int max;

	while ( i < n_page && PageTable[i].Valid == false ) {
		i++;
	}
	idxmax =i;
	max = PageTable[i].LastUsed;

	for (int i = idxmax ; i< n_page ; i++) {
		if ( (PageTable[i].LastUsed > max) && (PageTable[i].Valid)) {
			max = PageTable[i].LastUsed;			
			idxmax = i;	
		}
	}
	return idxmax;
}





int main(int argc, char* argv[]) {
	
	//Inisialisasi
	DiskAccess = 0;
	n_page = atoi(argv[1]);
	n_frame = atoi(argv[2]);
	SharedMemoryKey = getpid();

	//Buat FrameTable dan Shared Memory PageTable
	SegmentID = shmget(SharedMemoryKey, n_page * sizeof(page_table_entry) , IPC_CREAT | 0666);
	FrameTable = (bool * ) malloc ( n_page* sizeof(bool));
	PageTable = (page_table_pointer) shmat(SegmentID,NULL,0);

	//Inisialisasi FrameTable & Shared Memory
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

	//OS Siap,loop menunggu sinyal
	printf("The shared memory key (PID) is %d \n", SharedMemoryKey);
	printf("Initialized page table \n");
	signal(SIGUSR1,ProcessSignal);
	while (true) {}

	return 0;
}





void ProcessSignal(int){	
	int i = 0;
	int MMU_Pid;
	int Page_Request;
	int Victim;

	UpdateLastUsedTime(PageTable,n_page);	

	
	while ( i < n_page && PageTable[i].Requested == 0 ) {
		i++;	
	}
	if ( i != n_page) {
		Page_Request = i;
		MMU_Pid = PageTable[i].Requested;
		printf("Process %d has requested page %d \n", PageTable[i].Requested,i);
	}
	else {
		printf("The MMU has finished \n");
		PrintPageTable(PageTable,n_page);
		printf("%d disk accesses required \n", DiskAccess);
		exit(0);
	}


	i = 0;
	while ( i < n_frame && FrameTable[i] == true) {
		i++;
	}
	if ( i != n_frame ) {
		printf("Put it in free frame %d \n", i); 
		sleep(1);
		PageTable[Page_Request].Valid = 1;
		PageTable[Page_Request].Frame = i;
		FrameTable[i] = true;
		printf("Unblock MMU \n");	
	}
	else {
		Victim = GetVictim();
		printf("Chose a victim page %d \n", Victim);
		if ( PageTable[Victim].Dirty ) {
			printf("Victim is dirty , write out \n");
			sleep(1);	
			DiskAccess++;
		}
		printf("Put in victim's frame %d \n", PageTable[Victim].Frame);
		PageTable[Victim].Valid = 0;
		PageTable[Victim].Dirty = 0;
		PageTable[Page_Request].Valid = 1;
		PageTable[Page_Request].Frame = PageTable[Victim].Frame;
		PageTable[Victim].Frame = -1;		
	}
	PageTable[Page_Request].Dirty = 0;
	PageTable[Page_Request].Requested = 0;
	DiskAccess++;
	kill(MMU_Pid,SIGCONT);
} 
