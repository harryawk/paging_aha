/*	Simulasi Virtual Memory Paging dengan C 
	Tugas OS 
	13514006 - Adi Purnama
	13514030 - Aditio Pangestu
	13514036 - Harry Alvin W K

*/

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

int SharedMemoryKey;			//Key [Shared Memory]
int SegmentID;					//SegmentID [Shared Memory]
page_table_pointer PageTable;	//Page Table
frame_table_pointer FrameTable;	//Frame Table
int n_page;						//Jumlah Page
int n_frame;					//Jumlah Frame
int n_diskAccess;				//Jumlah disk access


//Inisialisasi program OS berdasarkan parameter yang diberikana
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


//Membuat shared memory , dengan key = OSPID, ukuran sesuai dengan besar page number
void createSharedMemory() {
	n_diskAccess = 0;
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

//Inisialisasi shared memory yang telah dibuat
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



//Mendapatkan indeks PageTable yang diminta oleh MMU
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

//Mendapatkan indeks PageTable yang akan digantikan
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

//Mendapatkan indeks FrameTable yang masih kosong
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

//Mencetak page table pada layar
void PrintPageTable(page_table_entry PageTable[],int NumberOfPages) {
    int Index;
    for (Index =  0;Index < NumberOfPages;Index++) {
        printf("%2d: Valid=%1d Frame=%2d Dirty=%1d Requested=%1d\n",Index,
	PageTable[Index].Valid,PageTable[Index].Frame,PageTable[Index].Dirty,
	PageTable[Index].Requested);
    }
}

//Analisis sinyal yang diterma OS
//Apakah sinyal proses atau sinyal berhenti
void initializeSignalProcess(int* MMU_Pid, int* Page_Request  ){
	int i = 0;
	
	i = getRequestedPageTableIdx();
	if ( i == NOTFOUND ) {
		printf("The MMU has finished \n");
		PrintPageTable(PageTable,n_page);
		printf("%d disk accesses required \n", n_diskAccess);
		exit(0);
	}
	else {
		*Page_Request = i;
		*MMU_Pid = PageTable[i].Requested;
		printf("Process %d has requested page %d \n", PageTable[i].Requested,i);
		PageTable[*Page_Request].Dirty = 0;
		PageTable[*Page_Request].Requested = 0;
		n_diskAccess++;
	}

}


//Tempatkan page pada Free Frame
void putInFreeFrame(int i, int Page_Request){
	printf("Put it in free frame %d \n", i); 
	sleep(1);
	PageTable[Page_Request].Valid = 1;
	PageTable[Page_Request].Frame = i;
	FrameTable[i] = true;
	
}


//Tempatkan page pada Victim Page
void putInVictimFrame(int Page_Request){
	int Victim;
	Victim = getVictimIdx();
		printf("Chose a victim page %d \n", Victim);
		PageTable[Victim].LastUsed = 0;
		PageTable[Victim].Valid = 0;
		if ( PageTable[Victim].Dirty ) {
			
			PageTable[Victim].Dirty = 0;
			printf("Victim is dirty , write out \n");
			sleep(1);	
			n_diskAccess++;
		}
		printf("Put in victim's frame %d \n", PageTable[Victim].Frame);
		PageTable[Page_Request].Valid = 1;
		PageTable[Page_Request].Frame = PageTable[Victim].Frame;
		PageTable[Victim].Frame = -1;	

}

//Mengirim sinyal kepada MMU bahwa proses sudah selesai
void sendSignalToMMU(int MMU_Pid, int PageRequest){
	printf("Unblock MMU \n");
	kill(MMU_Pid,SIGCONT);
}



//Proses sinyal yang diterma OS
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

//Menunggu sinyal datang
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

