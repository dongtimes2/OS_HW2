//
// CPU Schedule Simulator Homework
// Student Number : B731165
// Name : 유동하
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>

#define SEED 10

// process states
#define S_IDLE 0
#define S_READY 1
#define S_BLOCKED 2
#define S_RUNNING 3
#define S_TERMINATE 4

int NPROC, NIOREQ, QUANTUM;

struct ioDoneEvent {
	int procid;
	int doneTime;
	int len;
	struct ioDoneEvent *prev;
	struct ioDoneEvent *next;
} ioDoneEventQueue, *ioDoneEvent;

struct process {
	int id;
	int len;		// for queue
	int targetServiceTime;
	int serviceTime;
	int startTime;
	int endTime;
	char state;
	int priority;
	int saveReg0, saveReg1;
	struct process *prev;
	struct process *next;
} *procTable;

struct process idleProc;
struct process readyQueue;
struct process blockedQueue;
struct process *runningProc;

int cpuReg0, cpuReg1;
int currentTime = 0;
int *procIntArrTime, *procServTime, *ioReqIntArrTime, *ioServTime;

void compute() {
	// DO NOT CHANGE THIS FUNCTION
	cpuReg0 = cpuReg0 + runningProc->id;
	cpuReg1 = cpuReg1 + runningProc->id;
	//printf("In computer proc %d cpuReg0 %d\n",runningProc->id,cpuReg0);
}

void initProcTable() {
	int i;
	for(i=0; i < NPROC; i++) {
		procTable[i].id = i;
		procTable[i].len = 0;
		procTable[i].targetServiceTime = procServTime[i];
		procTable[i].serviceTime = 0;
		procTable[i].startTime = 0;
		procTable[i].endTime = 0;
		procTable[i].state = S_IDLE;
		procTable[i].priority = 0;
		procTable[i].saveReg0 = 0;
		procTable[i].saveReg1 = 0;
		procTable[i].prev = NULL;
		procTable[i].next = NULL;
	}
}

//프로세스 큐를 위한 선언
struct process *front = &readyQueue;
struct process *rear = &readyQueue;

//io 큐를 위한 포인터 선언
struct ioDoneEvent *iofront = &ioDoneEventQueue;
struct ioDoneEvent *iorear = &ioDoneEventQueue;
struct ioDoneEvent *iopointer = &ioDoneEventQueue;

void procExecSim(struct process *(*scheduler)()) {
	int pid, qTime=0, cpuUseTime = 0, nproc=0, termProc = 0, nioreq=0;
	char schedule = 0, nextState = S_IDLE;
	int nextForkTime, nextIOReqTime;
	int i;
	int count = 0;
	nextForkTime = procIntArrTime[nproc];
	nextIOReqTime = ioReqIntArrTime[nioreq];

	//맨 처음 실행할 때 돌아가는 것은 idle한 프로세스이다.
	runningProc = &idleProc;

	while(1) {
		currentTime++;
		qTime++;
		runningProc->serviceTime++;
		if (runningProc != &idleProc ) cpuUseTime++;

		int process_created_sign = 0;
		int process_blocked_sign = 0;
		int process_terminated_sign = 0;
		int quantum_sign = 0;
		int ioDone_complete_sign = 0;

		if(qTime == QUANTUM) quantum_sign = 1;
		//printf("==============================================\n");
		//printf("current : %d, next fork time : %d, next io req time : %d, cpu use time : %d, io done time : %d\n", currentTime, nextForkTime, nextIOReqTime, cpuUseTime, iopointer->next->doneTime);
		//printf("queue_len : %d, block_queue_len : %d\n", readyQueue.len, ioDoneEventQueue.len);
		// printf("qtime : %d, QUANTUM : %d\n", qTime, QUANTUM);
		// printf("run serve : %d, runtarget : %d\n", runningProc->serviceTime, runningProc->targetServiceTime);
		// printf("현재 프로세스 ID : %d\n", runningProc->id);
		// printf("현재 프로세스 상태 : %d\n", runningProc->state);
		// printf("front : %d\n", iofront->procid);
		// printf("front->next : %d\n", iofront->next->procid);
		// printf("front->next->next : %d\n", iofront->next->next->procid);
		// MUST CALL compute() Inside While loop
		compute();
		runningProc->saveReg0 = cpuReg0;
		runningProc->saveReg1 = cpuReg1;

		//idle process는 절대 readyQueue에 들어가지 않는다!
		//runningProc의 상태전환 우선순위는 종료 > blocked > ready 이다.


		///////////////////////////////프로세스 종료////////////////////////////////////////////////////////////////////////
		if (runningProc->serviceTime == runningProc->targetServiceTime) { /* CASE 4 : the process job done and terminates */
			//사용시간이 지정된 수행시간에 도달하면 해당 프로세스는 수행을 중단하고 terminate가 된다.
			//printf("프로세스 종료 발생!\n");
			//printf("terminated 프로세스 id : %d\n", runningProc->id);
			runningProc->state = S_TERMINATE;
			runningProc->endTime = currentTime;
			termProc++;

			//만약 종료할 프로세스가 IO요청도 발생시킬 경우 프로세스는 종료시키되 IO 큐에는 정보를 삽입시킴
			//이후 IO DONE이 발생할 때 해당 프로세스가 종료인지 검사하는 작업을 수행하면 될 것 같음
			if(cpuUseTime == nextIOReqTime) {
				struct ioDoneEvent *iooper = &ioDoneEventQueue;
				//printf("해당 프로세스는 종료될 것이지만 IO 요청 정보는 queue에 삽입합니다\n");

				ioDoneEvent[nioreq].procid = runningProc->id;
				//printf("blocked queue로 이동할 id : %d\n", runningProc->id);

				ioDoneEvent[nioreq].doneTime = (currentTime + ioServTime[nioreq]);
				//printf("donetime계산 : %d\n", ioDoneEvent[nioreq].doneTime);

				//빈 큐에 삽입할 때
				if(iofront->next == NULL){
					iorear->next = &ioDoneEvent[nioreq];
					iorear = &ioDoneEvent[nioreq];
					ioDoneEventQueue.len +=1;
				}

				//원소가 있는 큐에 삽입할 때
				else{
					//iooper->next와 새로 추가할 것의 donetime을 비교한다.
					while(iooper->next != NULL && (iooper->next->doneTime <= ioDoneEvent[nioreq].doneTime)){
						iooper = iooper->next;
					}

					//만약 맨 끝에 삽입된다면
					if(iooper->next == NULL){
						iooper->next = &ioDoneEvent[nioreq];
						iorear->next = NULL;
						iorear = &ioDoneEvent[nioreq];
						ioDoneEventQueue.len +=1;
					}

					//그것이 아니라면
					else{
						ioDoneEvent[nioreq].next = iooper->next;
						iooper->next = &ioDoneEvent[nioreq];
						ioDoneEventQueue.len +=1;
					}
				}
			}

			if(currentTime != nextForkTime && iopointer->next->doneTime != currentTime){scheduler();}

			qTime=0;

			process_terminated_sign = 1;
		}




		////////////////////////////// IO 요청 /////////////////////////////////////////////////////////////////////////
		if (cpuUseTime == nextIOReqTime) { /* CASE 5: reqest IO operations (only when the process does not terminate) */
			//printf("IO요청\n");
			if(process_terminated_sign){}

			else{
				struct ioDoneEvent *ioInsert = &ioDoneEventQueue;

				if(runningProc->state != S_IDLE){
					runningProc->state = S_BLOCKED;

					if(quantum_sign==0) {runningProc->priority +=1;}
					else{runningProc->priority -=1;}
				}

				ioDoneEvent[nioreq].procid = runningProc->id;

				ioDoneEvent[nioreq].doneTime = (currentTime + ioServTime[nioreq]);

				if(iofront->next == NULL){
					iorear->next = &ioDoneEvent[nioreq];
					iorear = &ioDoneEvent[nioreq];
				}

				else{
					while(ioInsert->next != NULL && (ioInsert->next->doneTime <= ioDoneEvent[nioreq].doneTime)){ioInsert = ioInsert->next;}

					if(ioInsert->next == NULL){
						ioInsert->next = &ioDoneEvent[nioreq];
						iorear->next = NULL;
						iorear = &ioDoneEvent[nioreq];
						ioDoneEventQueue.len +=1;
					}

					else{
						ioDoneEvent[nioreq].next = ioInsert->next;
						ioInsert->next = &ioDoneEvent[nioreq];
						ioDoneEventQueue.len +=1;
					}
				}

				if(currentTime != nextForkTime && iopointer->next->doneTime != currentTime){scheduler();}
			}

			++nioreq;
			if(nioreq<NIOREQ) {nextIOReqTime += ioReqIntArrTime[nioreq];}
			else{nextIOReqTime = INT_MAX;}

			//printf("NIOREQ : %d, nioreq : %d\n", NIOREQ, nioreq);
			//printf("ioReqIntArrTime : %d\n", ioReqIntArrTime[nioreq]);
			//printf("%d- th nextIOReqTime : %d\n", nioreq, nextIOReqTime);

			qTime=0;
			process_blocked_sign = 1;
		}



		///////////////////////////// 새로운 프로세스 생성 /////////////////////////////////////////////////////////////
		if (currentTime == nextForkTime) { /* CASE 2 : a new process created */
			//printf("프로세스 생성\n");
			pid = nproc;
			procTable[pid].state = S_READY;
			procTable[pid].startTime = currentTime;

			struct process *temp = NULL;
			temp = rear;

			rear->next = &procTable[pid];
			rear = &procTable[pid];
			rear->next=NULL;

			rear->prev = temp;
			temp = NULL;
			readyQueue.len += 1;

			if(runningProc->serviceTime == runningProc->targetServiceTime && iopointer->next->doneTime != currentTime){scheduler();}

			else{
				//생성 O DONE X 요청 X
				if(iopointer->next->doneTime != currentTime && !process_blocked_sign){
					if(runningProc->state != S_IDLE){

						runningProc->state = S_READY;

						temp = rear;

						rear->next = runningProc;
						rear = runningProc;
						rear->next=NULL;

						rear->prev= temp;
						temp = NULL;
						readyQueue.len += 1;

						if(quantum_sign){runningProc->priority -=1;}
					}
					scheduler();
				}

				//생성 O DONE X 요청 O
				else if(iopointer->next->doneTime != currentTime && process_blocked_sign) {scheduler();}

				//생성O DONE O 요청 X
				else if (iopointer->next->doneTime == currentTime && !process_blocked_sign){}

				//생성 O DONE O 요청 O
				else if(iopointer->next->doneTime == currentTime && process_blocked_sign){}

				else{}
			}

			++nproc;
			if(nproc < NPROC){nextForkTime += procIntArrTime[nproc];}
			else{nextForkTime = INT_MAX;}

			qTime=0;

			process_created_sign = 1;
		}



		//////////////////////////////////////// 퀀텀 만료 /////////////////////////////////////////////////////////
		if (quantum_sign) { /* CASE 1 : The quantum expires */
			//printf("퀀텀만료\n");
			if(process_created_sign){}

			else if(process_blocked_sign){}

			//이미 종료되었기 때문에 우선순위 의미가 없음
			else if(process_terminated_sign){}

			else if(iopointer->next->doneTime == currentTime){}

			else{
				if(runningProc->state != S_IDLE){
					runningProc->state = S_READY;

					struct process *temp = NULL;
					temp = rear;

					rear->next = runningProc;
					rear = runningProc;
					rear->next=NULL;

					rear->prev = temp;
					temp = NULL;
					readyQueue.len += 1;

					runningProc->priority -=1;
				}
				scheduler();
			}

			qTime=0;
		}




		///////////////////////////////////////// IO DONE ////////////////////////////////////////////////////////
		while (iopointer->next->doneTime == currentTime) { /* CASE 3 : IO Done Event */
			//printf("IO DONE\n");
			int procid = iopointer->next->procid;

			if(procTable[procid].state == S_TERMINATE) {procTable[procid].next = NULL;}

			else{
				procTable[procid].state = S_READY;
				procTable[procid].next = NULL;

				struct process *temp = NULL;
				temp = rear;

				rear->next = &procTable[procid];
				rear = &procTable[procid];
				rear->next=NULL;

				rear->prev = temp;
				temp = NULL;
				readyQueue.len += 1;
			}

			//현재 프로세스 처리하는 구간
			if(process_terminated_sign || process_blocked_sign){
				if(iopointer->next->next != NULL){
					if(iopointer->next->next->doneTime != currentTime){
						scheduler();
					}
				}
			}

			else{
				if(iopointer->next->next != NULL){
					if(iopointer->next->next->doneTime != currentTime){

						if(runningProc->state != S_IDLE){
							runningProc->state = S_READY;

							struct process *temp = NULL;
							temp = rear;

							rear->next = runningProc;
							rear = runningProc;

							rear->next=NULL;
							rear->prev = temp;
							temp = NULL;
							readyQueue.len += 1;

							if(quantum_sign){
								runningProc->priority -=1;
							}
						}
						scheduler();
					}
				}
			}

			qTime=0;

			if(iopointer == iofront){
				iopointer = iopointer->next;
				ioDoneEventQueue.len -=1;
			}

			else{
				struct ioDoneEvent *ioDelete=NULL;
				ioDelete = iopointer;
				iopointer = iopointer->next;
				iofront->next = ioDelete->next;
				ioDelete = NULL;
				ioDoneEventQueue.len -=1;
			}
		}


		if(termProc >= NPROC) {break;}

		count++;
	} // while loop
}

struct process* RRschedule() {

	if(front->next == NULL || front->next == &readyQueue){
		runningProc = &idleProc;
	}

	else{

		if(front->next==rear){
			runningProc = front->next;
			front->next->prev = NULL;
			front->next = NULL;
			rear = &readyQueue;
			readyQueue.len -= 1;
		}

		else{
			struct process *temp = NULL;
			temp = front->next->next;
			temp->prev = front;
			front->next->next = NULL;
			front->next->prev = NULL;
			runningProc = front->next;
			front->next = temp;
			temp=NULL;
			readyQueue.len -= 1;
		}
	}

	if(runningProc->state != S_IDLE){
		runningProc->state = S_RUNNING;
	}

	cpuReg0 = runningProc->saveReg0;
	cpuReg1 = runningProc->saveReg1;

}

struct process* SJFschedule() {
	//targetServiceTime이 제일 적은 프로세스가 CPU를 할당받는다.

	//printf("스케쥴링 실행\n");
	if(front->next == NULL || front->next == &readyQueue){
		//printf("큐가 비었으므로 idle을 러닝프로세스로 전환함\n");
		runningProc = &idleProc;
	}

	else{
		if(front->next==rear){
			//printf("원소가 한 개만 있을 때\n");
			runningProc = front->next;
			front->next->prev = NULL;
			front->next = NULL;
			rear = &readyQueue;
			readyQueue.len -= 1;
		}

		else{
			//printf("원소가 두 개 이상 있을 때\n");
			struct process *iter = NULL;
			struct process *backIter = NULL;

			struct process *temp = NULL;

			struct process *check = NULL;	//최소값 노드를 가리킨다.

			struct process *backCheck = NULL;	//최소값 노드를 가리킨다.

			int minValue = INT_MAX;
			int minId = -1;

			int backMinValue = INT_MAX;
			int backMinId = -1;

			int whileCount = 0;

			iter = front->next;
			backIter = rear;

			//가장 작은 값 탐색한 뒤 targetServiceTime이 가장 작은 값을 가진 id를 저장함.

			while(whileCount<readyQueue.len/2+1){
				//printf("백 ID : %d, 백 값 : %d, 정 ID : %d, 정 값 : %d\n", backMinId, backMinValue, minId, minValue);
				if((iter->targetServiceTime)<minValue){
					minValue = iter->targetServiceTime;
					minId = iter->id;
					check = iter;
				}

				if((backIter->targetServiceTime)<=backMinValue){
					backMinValue = backIter->targetServiceTime;
					backMinId = backIter->id;
					backCheck = backIter;
				}

				iter = iter->next;
				backIter = backIter->prev;
				whileCount++;
			}


			//printf("타겟 타임이 가장 적은 id : %d\n", minId);
			//printf("마지막 whileCount : %d\n", whileCount);
			//printf("타겟 타임이 가장 적은 id : %d\n", minId);

			iter=NULL;
			backIter=NULL;

			//뒤에서 찾은게 더 작을 경우 뒤의 값을 기준으로 변경시켜주면 된다.
			//printf("백 ID : %d, 백 값 : %d, 정 ID : %d, 정 값 : %d\n", backMinId, backMinValue, minId, minValue);
			if(backMinValue < minValue){
				//printf("백 ID : %d, 백 값 : %d, 정 ID : %d, 정 값 : %d\n", backMinId, backMinValue, minId, minValue);
				minValue = backMinValue;
				minId = backMinId;
				check = backCheck;
			}

			//찾은 노드가 맨 처음에 있을 경우
			if(minId == front->next->id){
				//printf("찾은 노드가 맨 처음에 있는 경우\n");
				temp = front->next->next;
				temp->prev = front;

				front->next->next = NULL;
				front->next->prev = NULL;

				runningProc = front->next;
				front->next = temp;

				temp = NULL;
				readyQueue.len -= 1;
			}

			//찾은 노드가 맨 나중에 있을 경우
			else if(minId == rear->id){
				//printf("찾은 노드가 맨 나중에 있는 경우\n");
				temp = rear->prev;
				rear->prev = NULL;
				temp->next = NULL;

				runningProc = rear;

				rear = temp;
				temp = NULL;
				readyQueue.len -= 1;
			}

			//찾은 노드가 중간에 있을 경우
			else{
				//printf("찾은 노드가 중간에 있는 경우\n");
				check->prev->next = check->next;
				check->next->prev = check->prev;

				check->next = NULL;
				check->prev = NULL;
				runningProc = check;

				check = NULL;
				readyQueue.len -= 1;
			}
		}
	}

	if(runningProc->state != S_IDLE){
		runningProc->state = S_RUNNING;
	}

	cpuReg0 = runningProc->saveReg0;
	cpuReg1 = runningProc->saveReg1;
}

struct process* SRTNschedule() {
	//readyQueue에서 남아있는 수행시간(targetServiceTime - serviceTime)이 가장 작은 것이 선정된다.

	//printf("스케쥴링 실행\n");
	if(front->next == NULL || front->next == &readyQueue){
		//printf("큐가 비었으므로 idle을 러닝프로세스로 전환함\n");
		runningProc = &idleProc;
	}

	else{
		if(front->next==rear){
			//printf("원소가 한 개만 있을 때\n");
			runningProc = front->next;
			front->next->prev = NULL;
			front->next = NULL;
			rear = &readyQueue;
			readyQueue.len -= 1;
		}

		else{
			//printf("원소가 두 개 이상 있을 때\n");
			struct process *iter = NULL;
			struct process *backIter = NULL;

			struct process *temp = NULL;

			struct process *check = NULL;

			struct process *backCheck = NULL;

			int minValue = INT_MAX;
			int minId = -1;

			int backMinValue = INT_MAX;
			int backMinId = -1;

			int whileCount = 0;

			iter = front->next;
			backIter = rear;

			//가장 작은 값 탐색한 뒤 가장 작은 값을 가진 id를 저장함.
			while(whileCount<readyQueue.len/2+1){

				if((iter->targetServiceTime-iter->serviceTime)<minValue){
					minValue = (iter->targetServiceTime-iter->serviceTime);
					minId = iter->id;
					check = iter;
				}

				if((backIter->targetServiceTime-backIter->serviceTime)<=backMinValue){
					backMinValue = (backIter->targetServiceTime-backIter->serviceTime);
					backMinId = backIter->id;
					backCheck = backIter;
				}

				iter = iter->next;
				backIter = backIter->prev;
				whileCount++;
			}

			iter=NULL;
			backIter=NULL;

			if(backMinValue < minValue){
				//printf("백 ID : %d, 백 값 : %d, 정 ID : %d, 정 값 : %d\n", backMinId, backMinValue, minId, minValue);
				minValue = backMinValue;
				minId = backMinId;
				check = backCheck;
			}

			//찾은 노드가 맨 처음에 있을 경우
			if(minId == front->next->id){
				//printf("찾은 노드가 맨 처음에 있는 경우\n");
				temp = front->next->next;
				temp->prev = front;

				front->next->next = NULL;
				front->next->prev = NULL;

				runningProc = front->next;
				front->next = temp;

				temp = NULL;
				readyQueue.len -= 1;
			}

			//찾은 노드가 맨 나중에 있을 경우
			else if(minId == rear->id){
				//printf("찾은 노드가 맨 나중에 있는 경우\n");
				temp = rear->prev;
				rear->prev = NULL;
				temp->next = NULL;

				runningProc = rear;

				rear = temp;
				temp = NULL;
				readyQueue.len -= 1;
			}

			//찾은 노드가 중간에 있을 경우
			else{
				//printf("찾은 노드가 중간에 있는 경우\n");
				check->prev->next = check->next;
				check->next->prev = check->prev;

				check->next = NULL;
				check->prev = NULL;
				runningProc = check;

				check = NULL;
				readyQueue.len -= 1;
			}
		}
	}

	//printf("running 프로세스 id : %d\n", runningProc->id);

	if(runningProc->state != S_IDLE){
		runningProc->state = S_RUNNING;
	}

	//printf("running 프로세스 상태 : %d\n", runningProc->state);

	//printf("큐에 저장된 front->next 프로세스 정보: ");
	//if(front->next==NULL) printf("없음\n");
	//else printf("id : %d, state : %d\n", front->next->id, front->next->state);

	cpuReg0 = runningProc->saveReg0;
	cpuReg1 = runningProc->saveReg1;

	//printf("스케쥴링 종료\n");
}


struct process* GSschedule() {
	//readyQueue에서 ratio(serviceTime/targetServiceTime)이 가장 작은 것이 선정된다.

	//printf("스케쥴링 실행\n");
	if(front->next == NULL || front->next == &readyQueue){
		//printf("큐가 비었으므로 idle을 러닝프로세스로 전환함\n");
		runningProc = &idleProc;
	}

	else{
		if(front->next==rear){
			//printf("원소가 한 개만 있을 때\n");
			runningProc = front->next;
			front->next->prev = NULL;
			front->next = NULL;
			rear = &readyQueue;
			readyQueue.len -= 1;
		}

		else{
			//printf("원소가 두 개 이상 있을 때\n");
			struct process *iter = NULL;
			struct process *backIter = NULL;

			struct process *temp = NULL;

			struct process *check = NULL;

			struct process *backCheck = NULL;

			double minValue = 9999.9999;
			int minId = -1;

			double backMinValue = 9999.9999;
			int backMinId = -1;

			int whileCount = 0;

			iter = front->next;
			backIter = rear;

			//가장 작은 값 탐색한 뒤 가장 작은 값을 가진 id를 저장함.
			while(whileCount<readyQueue.len/2+1){

				if(((double)iter->serviceTime / (double)iter->targetServiceTime) < minValue){
					minValue = ((double)iter->serviceTime / (double)iter->targetServiceTime);
					minId = iter->id;
					check = iter;
				}

				if(((double)backIter->serviceTime / (double)backIter->targetServiceTime) <= backMinValue){
					backMinValue = ((double)backIter->serviceTime / (double)backIter->targetServiceTime);
					backMinId = backIter->id;
					backCheck = backIter;
				}

				iter = iter->next;
				backIter = backIter->prev;
				whileCount++;
			}

			if(backMinValue < minValue){
				//printf("백 ID : %d, 백 값 : %d, 정 ID : %d, 정 값 : %d\n", backMinId, backMinValue, minId, minValue);
				minValue = backMinValue;
				minId = backMinId;
				check = backCheck;
			}

			iter=NULL;
			backIter=NULL;

			//찾은 노드가 맨 처음에 있을 경우
			if(minId == front->next->id){
				//printf("찾은 노드가 맨 처음에 있는 경우\n");
				temp = front->next->next;
				temp->prev = front;

				front->next->next = NULL;
				front->next->prev = NULL;

				runningProc = front->next;
				front->next = temp;

				temp = NULL;
				readyQueue.len -= 1;
			}

			//찾은 노드가 맨 나중에 있을 경우
			else if(minId == rear->id){
				//printf("찾은 노드가 맨 나중에 있는 경우\n");
				temp = rear->prev;
				rear->prev = NULL;
				temp->next = NULL;

				runningProc = rear;

				rear = temp;
				temp = NULL;
				readyQueue.len -= 1;
			}

			//찾은 노드가 중간에 있을 경우
			else{
				//printf("찾은 노드가 중간에 있는 경우\n");
				check->prev->next = check->next;
				check->next->prev = check->prev;

				check->next = NULL;
				check->prev = NULL;
				runningProc = check;

				check = NULL;
				readyQueue.len -= 1;
			}
		}
	}

	//printf("running 프로세스 id : %d\n", runningProc->id);

	if(runningProc->state != S_IDLE){
		runningProc->state = S_RUNNING;
	}

	//printf("running 프로세스 상태 : %d\n", runningProc->state);

	//printf("큐에 저장된 front->next 프로세스 정보: ");
	//if(front->next==NULL) printf("없음\n");
	//else printf("id : %d, state : %d\n", front->next->id, front->next->state);

	cpuReg0 = runningProc->saveReg0;
	cpuReg1 = runningProc->saveReg1;

	//printf("스케쥴링 종료\n");
}

struct process* SFSschedule() {
	if(front->next == NULL || front->next == &readyQueue){
		runningProc = &idleProc;
	}

	else{
		if(front->next==rear){
			runningProc = front->next;
			front->next->prev = NULL;
			front->next = NULL;
			rear = &readyQueue;
			readyQueue.len -= 1;
		}

		else{
			struct process *iter = NULL;
			struct process *backIter = NULL;
			struct process *temp = NULL;
			struct process *check = NULL;
			struct process *backCheck = NULL;

			int maxValue = INT_MIN;
			int maxId = -1;

			int backMaxValue = INT_MIN;
			int backMaxId = -1;

			int whileCount = 0;

			iter = front->next;
			backIter = rear;

			while(whileCount<readyQueue.len/2+1){

				if(iter->priority > maxValue){
					maxValue = iter->priority;
					maxId = iter->id;
					check = iter;
				}

				if(backIter->priority >= backMaxValue){
					backMaxValue = backIter->priority;
					backMaxId = backIter->id;
					backCheck = backIter;
				}

				iter = iter->next;
				backIter = backIter->prev;
				whileCount++;
			}

			iter=NULL;
			backIter=NULL;

			if(backMaxValue > maxValue){
				maxValue = backMaxValue;
				maxId = backMaxId;
				check = backCheck;
			}

			//찾은 노드가 맨 처음에 있을 경우
			if(maxId == front->next->id){
				//printf("찾은 노드가 맨 처음에 있는 경우\n");
				temp = front->next->next;
				temp->prev = front;

				front->next->next = NULL;
				front->next->prev = NULL;

				runningProc = front->next;
				front->next = temp;

				temp = NULL;
				readyQueue.len -= 1;
			}

			//찾은 노드가 맨 나중에 있을 경우
			else if(maxId == rear->id){
				//printf("찾은 노드가 맨 나중에 있는 경우\n");
				temp = rear->prev;
				rear->prev = NULL;
				temp->next = NULL;

				runningProc = rear;

				rear = temp;
				temp = NULL;
				readyQueue.len -= 1;
			}

			//찾은 노드가 중간에 있을 경우
			else{
				//printf("찾은 노드가 중간에 있는 경우\n");
				check->prev->next = check->next;
				check->next->prev = check->prev;

				check->next = NULL;
				check->prev = NULL;
				runningProc = check;

				check = NULL;
				readyQueue.len -= 1;
			}
		}
	}

	//printf("running 프로세스 id : %d\n", runningProc->id);

	if(runningProc->state != S_IDLE){
		runningProc->state = S_RUNNING;
	}

	//printf("running 프로세스 상태 : %d\n", runningProc->state);

	//printf("큐에 저장된 front->next 프로세스 정보: ");
	//if(front->next==NULL) printf("없음\n");
	//else printf("id : %d, state : %d\n", front->next->id, front->next->state);

	cpuReg0 = runningProc->saveReg0;
	cpuReg1 = runningProc->saveReg1;

	//printf("스케쥴링 종료\n");
}

void printResult() {
	// DO NOT CHANGE THIS FUNCTION
	int i;
	long totalProcIntArrTime=0,totalProcServTime=0,totalIOReqIntArrTime=0,totalIOServTime=0;
	long totalWallTime=0, totalRegValue=0;
	for(i=0; i < NPROC; i++) {
		totalWallTime += procTable[i].endTime - procTable[i].startTime;
		/*
		printf("proc %d serviceTime %d targetServiceTime %d saveReg0 %d\n",
			i,procTable[i].serviceTime,procTable[i].targetServiceTime, procTable[i].saveReg0);
		*/
		totalRegValue += procTable[i].saveReg0+procTable[i].saveReg1;
		/* printf("reg0 %d reg1 %d totalRegValue %d\n",procTable[i].saveReg0,procTable[i].saveReg1,totalRegValue);*/
	}
	for(i = 0; i < NPROC; i++ ) {
		totalProcIntArrTime += procIntArrTime[i];
		totalProcServTime += procServTime[i];
	}
	for(i = 0; i < NIOREQ; i++ ) {
		totalIOReqIntArrTime += ioReqIntArrTime[i];
		totalIOServTime += ioServTime[i];
	}

	printf("Avg Proc Inter Arrival Time : %g \tAverage Proc Service Time : %g\n", (float) totalProcIntArrTime/NPROC, (float) totalProcServTime/NPROC);
	printf("Avg IOReq Inter Arrival Time : %g \tAverage IOReq Service Time : %g\n", (float) totalIOReqIntArrTime/NIOREQ, (float) totalIOServTime/NIOREQ);
	printf("%d Process processed with %d IO requests\n", NPROC,NIOREQ);
	printf("Average Wall Clock Service Time : %g \tAverage Two Register Sum Value %g\n", (float) totalWallTime/NPROC, (float) totalRegValue/NPROC);

}

int main(int argc, char *argv[]) {
	// DO NOT CHANGE THIS FUNCTION
	int i;
	int totalProcServTime = 0, ioReqAvgIntArrTime;
	int SCHEDULING_METHOD, MIN_INT_ARRTIME, MAX_INT_ARRTIME, MIN_SERVTIME, MAX_SERVTIME, MIN_IO_SERVTIME, MAX_IO_SERVTIME, MIN_IOREQ_INT_ARRTIME;

	if (argc < 12) {
		printf("%s: SCHEDULING_METHOD NPROC NIOREQ QUANTUM MIN_INT_ARRTIME MAX_INT_ARRTIME MIN_SERVTIME MAX_SERVTIME MIN_IO_SERVTIME MAX_IO_SERVTIME MIN_IOREQ_INT_ARRTIME\n",argv[0]);
		exit(1);
	}

	SCHEDULING_METHOD = atoi(argv[1]);
	NPROC = atoi(argv[2]);
	NIOREQ = atoi(argv[3]);
	QUANTUM = atoi(argv[4]);
	MIN_INT_ARRTIME = atoi(argv[5]);
	MAX_INT_ARRTIME = atoi(argv[6]);
	MIN_SERVTIME = atoi(argv[7]);
	MAX_SERVTIME = atoi(argv[8]);
	MIN_IO_SERVTIME = atoi(argv[9]);
	MAX_IO_SERVTIME = atoi(argv[10]);
	MIN_IOREQ_INT_ARRTIME = atoi(argv[11]);

	printf("SIMULATION PARAMETERS : SCHEDULING_METHOD %d NPROC %d NIOREQ %d QUANTUM %d \n", SCHEDULING_METHOD, NPROC, NIOREQ, QUANTUM);
	printf("MIN_INT_ARRTIME %d MAX_INT_ARRTIME %d MIN_SERVTIME %d MAX_SERVTIME %d\n", MIN_INT_ARRTIME, MAX_INT_ARRTIME, MIN_SERVTIME, MAX_SERVTIME);
	printf("MIN_IO_SERVTIME %d MAX_IO_SERVTIME %d MIN_IOREQ_INT_ARRTIME %d\n", MIN_IO_SERVTIME, MAX_IO_SERVTIME, MIN_IOREQ_INT_ARRTIME);

	srandom(SEED);

	// allocate array structures
	procTable = (struct process *) malloc(sizeof(struct process) * NPROC);
	ioDoneEvent = (struct ioDoneEvent *) malloc(sizeof(struct ioDoneEvent) * NIOREQ);
	procIntArrTime = (int *) malloc(sizeof(int) * NPROC);
	procServTime = (int *) malloc(sizeof(int) * NPROC);
	ioReqIntArrTime = (int *) malloc(sizeof(int) * NIOREQ);
	ioServTime = (int *) malloc(sizeof(int) * NIOREQ);

	// initialize queues
	readyQueue.next = readyQueue.prev = &readyQueue;

	blockedQueue.next = blockedQueue.prev = &blockedQueue;
	ioDoneEventQueue.next = ioDoneEventQueue.prev = &ioDoneEventQueue;
	ioDoneEventQueue.doneTime = INT_MAX;
	ioDoneEventQueue.procid = -1;
	ioDoneEventQueue.len  = readyQueue.len = blockedQueue.len = 0;

	// generate process interarrival times
	for(i = 0; i < NPROC; i++ ) {
		procIntArrTime[i] = random()%(MAX_INT_ARRTIME - MIN_INT_ARRTIME+1) + MIN_INT_ARRTIME;
	}

	// assign service time for each process
	for(i=0; i < NPROC; i++) {
		procServTime[i] = random()% (MAX_SERVTIME - MIN_SERVTIME + 1) + MIN_SERVTIME;
		totalProcServTime += procServTime[i];
	}

	ioReqAvgIntArrTime = totalProcServTime/(NIOREQ+1);
	assert(ioReqAvgIntArrTime > MIN_IOREQ_INT_ARRTIME);

	// generate io request interarrival time
	for(i = 0; i < NIOREQ; i++ ) {
		ioReqIntArrTime[i] = random()%((ioReqAvgIntArrTime - MIN_IOREQ_INT_ARRTIME)*2+1) + MIN_IOREQ_INT_ARRTIME;
	}

	// generate io request service time
	for(i = 0; i < NIOREQ; i++ ) {
		ioServTime[i] = random()%(MAX_IO_SERVTIME - MIN_IO_SERVTIME+1) + MIN_IO_SERVTIME;
	}

#ifdef DEBUG
	// printing process interarrival time and service time
	printf("Process Interarrival Time :\n");
	for(i = 0; i < NPROC; i++ ) {
		printf("%d ",procIntArrTime[i]);
	}
	printf("\n");
	printf("Process Target Service Time :\n");
	for(i = 0; i < NPROC; i++ ) {
		printf("%d ",procTable[i].targetServiceTime);
	}
	printf("\n");
#endif

	// printing io request interarrival time and io request service time
	printf("IO Req Average InterArrival Time %d\n", ioReqAvgIntArrTime);
	printf("IO Req InterArrival Time range : %d ~ %d\n",MIN_IOREQ_INT_ARRTIME,
			(ioReqAvgIntArrTime - MIN_IOREQ_INT_ARRTIME)*2+ MIN_IOREQ_INT_ARRTIME);

#ifdef DEBUG
	printf("IO Req Interarrival Time :\n");
	for(i = 0; i < NIOREQ; i++ ) {
		printf("%d ",ioReqIntArrTime[i]);
	}
	printf("\n");
	printf("IO Req Service Time :\n");
	for(i = 0; i < NIOREQ; i++ ) {
		printf("%d ",ioServTime[i]);
	}
	printf("\n");
#endif

	struct process* (*schFunc)();
	switch(SCHEDULING_METHOD) {
		case 1 : schFunc = RRschedule; break;
		case 2 : schFunc = SJFschedule; break;
		case 3 : schFunc = SRTNschedule; break;
		case 4 : schFunc = GSschedule; break;
		case 5 : schFunc = SFSschedule; break;
		default : printf("ERROR : Unknown Scheduling Method\n"); exit(1);
	}
	initProcTable();
	procExecSim(schFunc);
	printResult();

}
