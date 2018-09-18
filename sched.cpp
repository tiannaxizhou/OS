#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <list>
#include <array>
#include <vector>
#include <climits>
using namespace std;

typedef enum { 
	STATE_CREATED,
	STATE_READY,
	STATE_RUNNING, 
	STATE_BLOCKED,
	STATE_FINISHED
} process_state_t;

typedef enum {
	TRANS_TO_READY,
	TRANS_TO_RUN,
	TRANS_TO_BLOCK,
	TRANS_TO_PREEMPT
} transition_t; 
 
struct Process {
    int pid; //process identifier
    process_state_t state; //process state
    int AT; //arrival time
    int TC; //total CPU time
    int CB; //CPU burst
    int IO; //I/O burst
    int FT; //finishing time
    int TT; //turnaround time
    int IT; //I/O time
    int CW; //CPU waiting time
    int SPrio; //static priority
    int DPrio; //dynamic priority
    int TC_remain; //remaining total execution time
    int CB_remain; //remaining CPU burst time
    int timeInPrevState; //time in previous state 
    int state_ts; //time at current state
	Process(int procid, process_state_t procstate, int at, int tc, int cb, int io, int prio);
};

Process::Process(int procid, process_state_t procstate, int at, int tc, int cb, int io, int prio) {
	pid = procid;
	state = procstate;
	AT = at;
	TC = tc;
	CB = cb;
	IO = io;
	FT = 0;
	TT = 0;
	IT = 0;
	CW = 0;
	//important initialization
	SPrio = prio;
	DPrio = prio - 1; //***
	TC_remain = tc; //***
	CB_remain = 0;
	timeInPrevState = 0;
	state_ts = at; //***
}

struct Event {
	int time_stamp;
	transition_t transition;
	Process* proc;
	Event(Process* proc, int time_stamp, transition_t transition); //Time ordered
};

Event::Event(Process* p, int ts, transition_t trans) {
	time_stamp = ts;
	proc = p;
	transition = trans;
}

//Scheduling algorithms
class Scheduler {
	public:
		virtual void add_process(Process* proc) {}
		virtual Process* get_next_process() {}
};

//First Come First Served
class FCFS : public Scheduler {
	public:
		FCFS();
		void add_process(Process* proc);
		Process* get_next_process();
	private:
		list<Process*> runqueue;
};

FCFS::FCFS() {
	
}

void FCFS::add_process(Process* proc) {
	runqueue.push_back(proc);
}

Process* FCFS::get_next_process() {
	if(!runqueue.empty()) {
		Process* proc = runqueue.front();
		runqueue.pop_front();
		return proc;
	}
	else
		return NULL;
}

//Last Come First Served
class LCFS : public Scheduler {
	public:
		LCFS();
		void add_process(Process* proc);
		Process* get_next_process();
	private:
		list<Process*> runqueue;
};

LCFS::LCFS() {
	
}

void LCFS::add_process(Process* proc) {
	runqueue.push_back(proc);
}

Process* LCFS::get_next_process() {
	if(!runqueue.empty()) {
		Process* proc = runqueue.back();
		runqueue.pop_back();
		return proc;
	}
	else
		return NULL;
}

//Shortest Job First
class SJF : public Scheduler {
	public:
		SJF();
		void add_process(Process* proc);
		Process* get_next_process();
	private:
		list<Process*> runqueue;
};

SJF::SJF() {
	
} 

void SJF::add_process(Process* proc) {
	int flag = 0;
	list<Process*>::iterator it = runqueue.begin();
	for(it = runqueue.begin(); it != runqueue.end(); it++) {
		if((*it)->TC_remain > proc->TC_remain) {
			runqueue.insert(it, proc);
			flag = 1;
			break;
		}
	}
	//If current proc has the longest remaining time
	if(flag == 0)
		runqueue.push_back(proc);
}

Process* SJF::get_next_process() {
	if(!runqueue.empty()) {
		Process* proc = runqueue.front();
		runqueue.pop_front();
		return proc;
	}
	else
		return NULL;
}

//Round Robin
class RR : public Scheduler {
	public:
		RR(int tq);
		void add_process(Process* proc);
		Process* get_next_process();
	private:
		int time_quant;
		list<Process*> runqueue;
};

RR::RR(int tq) {
	time_quant = tq;
}

void RR::add_process(Process* proc) {
	runqueue.push_back(proc);
}

Process* RR::get_next_process() {
	if(!runqueue.empty()) {
		Process* proc = runqueue.front();
		runqueue.pop_front();
		return proc;
	}
	else
		return NULL;
}

//Priority
class PRIO : public Scheduler {
	public:
		PRIO(int tq);
		void add_process(Process* proc);
		Process* get_next_process();
	private:
		int time_quant;
		//list<Process*> prio_queues[2][4];
		array<list<Process*>, 4> active_queue;
		array<list<Process*>, 4> expired_queue;
};

PRIO::PRIO(int tq) {
	time_quant = tq;
}

void PRIO::add_process(Process* proc) {
	//When "-1" is reached the process is enqueued into the expired queue. 
	if(proc->DPrio == -1) {
		expired_queue[proc->SPrio - 1].push_back(proc);
	}
	else {	
		active_queue[proc->DPrio].push_back(proc);
	}
}

Process* PRIO::get_next_process() {
	//When the active queue is empty, active and expired are switched.
	bool flag = true;
	for(int i = 0; i < 4; i++) {
		if(!active_queue[i].empty()) {
			flag = false;
			break;
		}
	}
	if(flag) {
		auto null = active_queue;
		active_queue = expired_queue;
		expired_queue = null;
	}
	//Is active queue empty? 
	for(int i = 3; i >= 0; i--) {
		if(!active_queue[i].empty()) {
			Process* proc = active_queue[i].front();
			active_queue[i].pop_front();
			return proc;
		}
	}
	return NULL;
}

//Discrete Event Simulation
class DES {
	public:
		DES();
		void Simulation(string infile, string rfile, string scheAlg, int time_quant, bool vout);
		void readInputFile(string infile);
		void readRandomFile(string rfile);
		
	private:
		int rcount, rofs, pid, FINISH_TIME, totalTC, totalIT, totalCW, totalTT;
		double CPU_UTIL, IO_UTIL, AVG_TT, AVG_CW, THROUGHPUT;
		Scheduler* sche; 
		vector<int> randvals;
		list<Process*> proc_list;
		list<Event*> event_list;
		
		Event* get_event();
		void put_event(Event* event);
		void delete_event();
		int get_next_event_time();
		void printSummary();
		void printv(bool verbose, Event* evt, int curr_time, int io_burst);
		int myrandom(int burst);
};

DES::DES() {
	rcount = 0;
	rofs = 0;
	pid = 0;
	FINISH_TIME = 0;
	totalTC = 0;
	totalIT = 0;
	totalCW = 0;
	totalTT = 0; 
	CPU_UTIL = 0;
	IO_UTIL = 0;
	AVG_TT = 0;
	AVG_CW = 0;
	THROUGHPUT = 0;
}

Event* DES::get_event() {
	if(!event_list.empty())
		return event_list.front();
	else
		return NULL;
}

void DES::put_event(Event* event) {
	int flag = 0;
	list<Event*>::iterator it = event_list.begin();
	for(it = event_list.begin(); it != event_list.end(); it++) {
		if((*it)->time_stamp > event->time_stamp) { //find the first event timestamp in the list bigger than that of arriving event
			event_list.insert(it, event); //insert to the proper position
			flag = 1;
			break;
		}
	}
	if(flag == 0) {//timestamp bigger than all the events
		event_list.push_back(event);
	}
}

void DES::delete_event() {
	if(!event_list.empty())
		event_list.pop_front();
}

int DES::get_next_event_time() {
	if(!event_list.empty()) {
		Event* event = event_list.front();
		return event->time_stamp;
	}
	else
		return -1;
	
}

//Read input file
void DES::readInputFile(string infile) {
	ifstream input;
	input.open(infile);
	string line;
	while(getline(input, line)) {
		int at, tc, cb, io, prio;
		stringstream split(line);
		split >> at >> tc >> cb >> io; 
		prio = myrandom(4);
		Process *proc = new Process(pid, STATE_CREATED, at, tc, cb, io, prio);
		proc_list.push_back(proc);
		Event *event = new Event(proc, at, TRANS_TO_READY); //from CREATED(1)
		put_event(event);
		pid++;
	}
	input.close();
}

//Generate random numbers
void DES::readRandomFile(string rfile) {
	ifstream input;
	input.open(rfile);
	input >> rcount;
	for(int i = 0; i < rcount; i++) {
		int rand;
		input >> rand;
		randvals.push_back(rand);
	}
	input.close();
}

int DES::myrandom(int burst)  {
	int rand = 1 + (randvals[rofs % rcount] % burst);
	rofs++;
	return rand;
}

//Simulation
void DES::Simulation(string infile, string rfile, string scheAlg, int time_quant, bool vout) {
	readRandomFile(rfile);
	readInputFile(infile);
	char alg = scheAlg[0];
	if(alg == 'F') {
		sche = new FCFS();
		cout<<"FCFS"<<endl;
	}
	if(alg == 'L') {
		sche = new LCFS();
		cout<<"LCFS"<<endl;
	}
	if(alg == 'S') {
		sche = new SJF();
		cout<<"SJF"<<endl;
	}
	if(alg == 'R') {
		sche = new RR(time_quant);
		cout<<"RR "<<time_quant<<endl;
	}
	if(alg == 'P') {
		sche = new PRIO(time_quant);
		cout<<"PRIO "<<time_quant<<endl;
	}
	
	Event* evt;
	int IO_BURST, IO_START = 0, IO_NUM = 0, CPU_BURST;
	bool CALL_SCHEDULER = false;
	Event* new_evt; //Avoid cross initialization
	Process* CURRENT_RUNNING_PROCESS = NULL;
	
	while((evt = get_event())) {
		Process *proc = evt->proc; // this is the process the event works on
		int CURRENT_TIME = evt->time_stamp;
		proc->timeInPrevState = CURRENT_TIME - proc->state_ts;
		
		switch(evt->transition) { // which state to transition to?
			case TRANS_TO_READY:
				// must come from BLOCKED or from PREEMPTION
				// must add to run queue
				//if(proc->state == STATE_CREATED)
					//printv(vout, evt, CURRENT_TIME, 0);
				proc->state = STATE_READY;
				sche->add_process(proc); //2
				CALL_SCHEDULER = true; // conditional on whether something is run
			break;
			
			case TRANS_TO_RUN:
				// create event for either preemption or blocking
				proc->TC_remain -= proc->timeInPrevState;
				if(proc->TC_remain > 0) { //not finished
					proc->CB_remain -= proc->timeInPrevState;
					if(proc->CB_remain > 0) {
						//cpu burst not finished, preempted
						new_evt = new Event(proc, CURRENT_TIME, TRANS_TO_PREEMPT); //5
						put_event(new_evt);
						//change current process state
						proc->state_ts = CURRENT_TIME;
						//printv(vout, new_evt, CURRENT_TIME, 0);
						proc->state = STATE_READY;
					}
					else {
						//cpu burst finished, to i/o burst
						IO_BURST = myrandom(proc->IO); //random number between [1...IO]
						proc->IT += IO_BURST;
						new_evt = new Event(proc, CURRENT_TIME + IO_BURST, TRANS_TO_BLOCK); //3
						put_event(new_evt);
						//change cur proc state
						proc->state_ts = CURRENT_TIME;
						//printv(vout, new_evt, CURRENT_TIME, IO_BURST);
						proc->state = STATE_BLOCKED; 
						if(IO_NUM == 0)
							IO_START = CURRENT_TIME;
						IO_NUM++; 
					}
				}
				else { //finished
					proc->state_ts = CURRENT_TIME;
					proc->FT = CURRENT_TIME;
					proc->TT = CURRENT_TIME - proc->AT;
					proc->state = STATE_FINISHED;
					//printv(vout, evt, CURRENT_TIME, 0);
				}
				CALL_SCHEDULER = true; //CALL SCHEDULER BUT NO CURRENT RUNNING PROCESS
				CURRENT_RUNNING_PROCESS = NULL;
			break;
			
			case TRANS_TO_BLOCK:
				// create an event for when process becomes READY again
				// When a process returns from I/O its dynamic priority is reset to (static_priority-1)
				proc->DPrio = proc->SPrio - 1;
				new_evt = new Event(proc, CURRENT_TIME, TRANS_TO_READY); //4
				put_event(new_evt);
				proc->state_ts = CURRENT_TIME;
				//printv(vout, new_evt, CURRENT_TIME, 0);
				proc->state = STATE_READY;
				IO_NUM--;
				if(IO_NUM == 0) //compute total io time
					totalIT += (CURRENT_TIME - IO_START);
				CALL_SCHEDULER = true;
			break;
			
			case TRANS_TO_PREEMPT:
				// add to runqueue (no event is generated)
				// With every quantum expiration the dynamic priority decreases by one.
				proc->DPrio--;
				sche->add_process(proc); //2
				//add to runqueue first so that sche can add it into expired queue
				if(proc->DPrio == -1) //When "-1" is reached the prio is reset to (static_priority-1).
					proc->DPrio = proc->SPrio - 1;
				CALL_SCHEDULER = true;
			break;
		}
		//remove current event object from Memory
		delete_event();
		evt = NULL;
		
		if(CALL_SCHEDULER) {
			if(get_next_event_time() == CURRENT_TIME) {
				continue; //process next event from event queue
			}
			CALL_SCHEDULER = false;
			if(CURRENT_RUNNING_PROCESS == NULL) {
				CURRENT_RUNNING_PROCESS = sche->get_next_process();
				if(CURRENT_RUNNING_PROCESS == NULL) {
					continue;
				}
				//Is cpu burst finished?
				if(CURRENT_RUNNING_PROCESS->CB_remain == 0) {
					//get a new random number
					int new_cb = myrandom(CURRENT_RUNNING_PROCESS->CB); //a random number between [1..CB]
					if(new_cb > CURRENT_RUNNING_PROCESS->TC_remain)
						new_cb = CURRENT_RUNNING_PROCESS->TC_remain;
					CURRENT_RUNNING_PROCESS->CB_remain = new_cb;
					if(new_cb > time_quant) { //treat preemption
						CPU_BURST = time_quant; 
					}
					else
						CPU_BURST = new_cb;						
				}
				else {
					//continue the previous process
					if(CURRENT_RUNNING_PROCESS->CB_remain > time_quant)
						CPU_BURST = time_quant;
					else
						CPU_BURST = CURRENT_RUNNING_PROCESS->CB_remain;
				}
				// create event to make process runnable for same time.
				new_evt = new Event(CURRENT_RUNNING_PROCESS, CURRENT_TIME + CPU_BURST, TRANS_TO_RUN);
				put_event(new_evt);
				CURRENT_RUNNING_PROCESS->timeInPrevState = CURRENT_TIME - CURRENT_RUNNING_PROCESS->state_ts;
				CURRENT_RUNNING_PROCESS->state_ts = CURRENT_TIME;
				CURRENT_RUNNING_PROCESS->state = STATE_RUNNING;
				CURRENT_RUNNING_PROCESS->CW += CURRENT_RUNNING_PROCESS->timeInPrevState; //CPU Waiting time (time in Ready state)
			}
		}
	}
	printSummary();
}

void DES::printSummary() {
	for(auto proc : proc_list) {
		if(proc->FT > FINISH_TIME)	FINISH_TIME = proc->FT;
		totalTC += proc->TC;
		totalCW += proc->CW;
		totalTT += proc->TT;
		printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n", proc->pid, proc->AT, proc->TC, proc->CB, proc->IO, proc->SPrio,
				proc->FT, proc->TT, proc->IT, proc->CW);
	}
	CPU_UTIL = (double) totalTC / FINISH_TIME * 100; //percentage (0.0 ¨C 100.0) of time at least one process is running
	IO_UTIL = (double) totalIT / FINISH_TIME * 100; //percentage (0.0 ¨C 100.0) of time at least one process is performing IO
	AVG_TT = (double) totalTT / proc_list.size();
	AVG_CW = (double) totalCW / proc_list.size();
	THROUGHPUT = (double) proc_list.size() / FINISH_TIME * 100; //Throughput of number processes per 100 time units
	printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n", FINISH_TIME, CPU_UTIL, IO_UTIL, AVG_TT, AVG_CW, THROUGHPUT);
}

int main(int argc, char* argv[]) {
	string str, alg;
	bool vout = 0;
	int c, tq;
	while((c = getopt(argc, argv, "vs:")) != -1) {
		switch(c) {
	 		case 'v':
	 			vout = 1;
	 			break;
	 		case 's': //-s[ FLS | R<num> | P<num> ]
	 			str = optarg;
	 			if(str[0] == 'R' || str[0] == 'P') {
	 				alg = str.substr(0, 1);
	 				tq = atoi(str.substr(1).c_str());
				}
				else if(str[0] == 'F' || str[0] == 'L' || str[0] == 'S') {
					alg = str;
					tq = INT_MAX;
				}
				else
					break;
	 			break;
		 }
	}
	string infile = argv[optind];
	string rfile = argv[optind + 1];
	DES sim;
	sim.Simulation(infile, rfile, alg, tq, vout);
} 
