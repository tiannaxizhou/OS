#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include <vector>
#include <cmath>
#include <climits> 
using namespace std;

struct IOrequest{
	public:
		int arrival_time;
		int start_time;
		int end_time;
		int index;
		int track;
		IOrequest(int id, int timeStep, int trackNum);
};

IOrequest::IOrequest(int id, int timeStep, int trackNum) {
	index = id;
	arrival_time = timeStep;
	track = trackNum;
	start_time = 0;
	end_time = 0;
}
 
class IOScheduler {
	public:
		IOScheduler();
		virtual void addIOrequest(IOrequest* IOreq) {}
		virtual	IOrequest* getIOrequest(int cur_track) {}
};

IOScheduler::IOScheduler() {
	
}

//First In First Out
class FIFO : public IOScheduler {
	public:
		FIFO();
		void addIOrequest(IOrequest* IOreq);
		IOrequest* getIOrequest(int cur_track);
	private:
		vector<IOrequest*> queue;
}; 

FIFO::FIFO() {
	
}

void FIFO::addIOrequest(IOrequest* IOreq) {
	queue.push_back(IOreq);
}

IOrequest* FIFO::getIOrequest(int cur_track) {
	if(!queue.empty()) {
		IOrequest* IOreq = queue.front();
		queue.erase(queue.begin());
		return IOreq;
	} 
	else
		return NULL;
} 

//Shortest Seek Time First
class SSTF : public IOScheduler {
	public:
		SSTF();	
		void addIOrequest(IOrequest* IOreq);
		IOrequest* getIOrequest(int cur_track);
	private:
		vector<IOrequest*> queue;
};

SSTF::SSTF() {
	
}

void SSTF::addIOrequest(IOrequest* IOreq) {
	queue.push_back(IOreq);
}

IOrequest* SSTF::getIOrequest(int cur_track) {
	if(!queue.empty()) {
		IOrequest* sstio;
		int min = INT_MAX, id = 0;
		for(int i = 0; i < queue.size(); i++) {
			IOrequest* temp = queue[i];
			if(abs(temp->track - cur_track) < min) {
				min = abs(temp->track - cur_track);
				sstio = temp;
				id = i;
			}
		}
		queue.erase(queue.begin() + id);
		return sstio;
	}
	else
		return NULL;
}

//No end looking SCAN
class LOOK : public IOScheduler {
	public:
		LOOK();
		void addIOrequest(IOrequest* IOreq);
		IOrequest* getIOrequest(int cur_track);
	private:
		int dir;
		vector<IOrequest*> queue;
};

LOOK::LOOK() {
	dir = 1; //The initial direction is from 0-tracks to higher tracks
}

void LOOK::addIOrequest(IOrequest* IOreq) {
	queue.push_back(IOreq); 
}

IOrequest* LOOK::getIOrequest(int cur_track) {
	if(!queue.empty()) {
		IOrequest* lookio = NULL;
		int min = INT_MAX, id = 0;
		for(int i = 0; i < queue.size(); i++) {
			IOrequest* temp = queue[i];
			if(dir == 1) {
				int dis = temp->track - cur_track;
				if(dis >= 0 && dis < min) { //dis >= 0 in case that two IOs have same track number
					min = dis;
					lookio = temp;
					id = i;
				}
			}
			if(dir == -1) {
				int dis = cur_track - temp->track;
				if(dis >= 0 && dis < min) {
					min = dis;
					lookio = temp;
					id = i;
				}
			}
		}
		if(lookio == NULL) {
			dir = -dir; //No proper outIO, reverse the direction
			lookio = getIOrequest(cur_track);
		}
		else { //Don't want to erase innocent IOs
			queue.erase(queue.begin() + id);
		}
		return lookio;
	} 
	else
		return NULL;
}

//No end looking C-SCAN
class CLOOK : public IOScheduler {
	public:
		CLOOK();
		void addIOrequest(IOrequest* IOreq);
		IOrequest* getIOrequest(int cur_track);
	private:
		vector<IOrequest*> queue;
};

CLOOK::CLOOK() {
	
}

void CLOOK::addIOrequest(IOrequest* IOreq) {
	queue.push_back(IOreq);
}

IOrequest* CLOOK::getIOrequest(int cur_track) {
	if(!queue.empty()) {
		IOrequest* clookio = NULL;
		int min = INT_MAX, id = 0;
		//From lower to higher, find the min IO
		for(int i = 0; i < queue.size(); i++) {
			IOrequest* temp = queue[i];
			int dis = temp->track - cur_track;
			if(dis >= 0 && dis < min) {
				min = dis;
				clookio = temp;
				id = i;
			}
		}
		if(clookio == NULL) { //Returns back to the beginning
			cur_track = 0;
			clookio = getIOrequest(cur_track);
		}
		else
			queue.erase(queue.begin() + id);
		return clookio;
	}
	else {
		return NULL;	
	}
}

//LOOK with two queues
class FLOOK : public IOScheduler {
	public:
		FLOOK();
		void addIOrequest(IOrequest* IOreq);
		IOrequest* getIOrequest(int cur_track);		
	private:
		//Use two queues add to one, retrieve from the other, when empty flip the pointers
		int dir;
		vector<IOrequest*> proc_queue;
		vector<IOrequest*> wait_queue;
};

FLOOK::FLOOK() {
	dir = 1;
}

void FLOOK::addIOrequest(IOrequest* IOreq) {
	wait_queue.push_back(IOreq);
}

IOrequest* FLOOK::getIOrequest(int cur_track) {
	if(proc_queue.empty()) {
		proc_queue = wait_queue;
		wait_queue.clear();
	}
	//proc_queue is empty on first access, so switch the queues
	if(!proc_queue.empty()) {
		IOrequest* flookio = NULL;
		int min = INT_MAX, id = 0;
		//continue in the direction you were going from the current position
		for(int i = 0; i < proc_queue.size(); i++) {
			IOrequest* temp = proc_queue[i];
			if(dir == 1) {
				int dis = temp->track - cur_track;
				if(dis >= 0 && dis < min) {
					min = dis;
					flookio = temp;
					id = i;
				}
			}
			if(dir == -1) {
				int dis = cur_track - temp->track;
				if(dis >= 0 && dis < min) {
					min = dis;
					flookio = temp;
					id = i;
				}
			}
		}
		//then switch direction until empty 
		if(flookio == NULL && !proc_queue.empty()) {
			dir = -dir;
			flookio = getIOrequest(cur_track);
		}
		else {
			proc_queue.erase(proc_queue.begin() + id);
		}
		return flookio;
	}
	else
		return NULL;
}

class Simulator {
	public:
		int id, total_time, tot_movement, max_waittime, total_turnaround, total_waittime;
		double avg_turnaround, avg_waittime;
		IOScheduler* IOsche;
		vector<IOrequest*> IO_list;
		
		Simulator();
		void readInputFile(string infile);
		void scheduling(string infile, string scheAlg);
};

Simulator::Simulator() {
	id = 0;
	total_time = 0;
	tot_movement = 0;
	max_waittime = 0;
	total_turnaround = 0;
	total_waittime = 0;
	avg_turnaround = 0;
	avg_waittime = 0;
}

void Simulator::readInputFile(string infile) {
	ifstream input;
	input.open(infile);
	string line;
	while(getline(input, line)) {
		if(line[0] != '#') {
			stringstream split(line);
			int timeStep, trackNum;
			split >> timeStep >> trackNum;
			IO_list.push_back(new IOrequest(id, timeStep, trackNum));
			id++;
		}
	}
}

void Simulator::scheduling(string infile, string scheAlg) {
	readInputFile(infile);
	//Choose IO scheduler
	char alg = scheAlg[0];
	if(alg == 'i')
		IOsche = new FIFO();
	if(alg == 'j')
		IOsche = new SSTF();
	if(alg == 's')
		IOsche = new LOOK();
	if(alg == 'c')
		IOsche = new CLOOK();
	if(alg == 'f')
		IOsche = new FLOOK();
	
	int simTime = 0, trackAt = 0, num_IOreq = 0;
	IOrequest* cur_IOreq = NULL;
	while(num_IOreq < IO_list.size() || cur_IOreq != NULL) {
		simTime++; //Increment
		//1) Did a new I/O arrive to the system at this time, if so add to IO-queue
		if(num_IOreq < IO_list.size()) {
			IOrequest* newIO = IO_list[num_IOreq];
			if(newIO != NULL && newIO->arrival_time == simTime) {
				IOsche->addIOrequest(newIO);
				num_IOreq++;
			}
		}
		
		//2) Is an IO active and completed at this time
		//if(cur_IOreq != NULL) cout<<cur_IOreq->end_time<<"  "<<simTime<<endl;
		if(cur_IOreq != NULL && cur_IOreq->end_time == simTime) {
			total_time = simTime;
			trackAt = cur_IOreq->track;
			cur_IOreq = NULL;
		}
		
		//3) Is an IO active but did not complete, then move the head by one sector/track/unit in the direction it is going (to simulate seek)
		if(cur_IOreq != NULL && cur_IOreq->end_time > simTime) {
			continue;
		}
		
		//4) Is no IO request active now (after (2)) but IO requests are pending? Fetch and start a new IO.
		if(cur_IOreq == NULL) {
			//Fetch
			cur_IOreq = IOsche->getIOrequest(trackAt);
			if(cur_IOreq == NULL) //DON'T miss NULL cases
				continue;
			//Start
			cur_IOreq->start_time = simTime;
			int move_time = abs(trackAt - cur_IOreq->track); //No SCAN
			cur_IOreq->end_time = simTime + move_time;
			tot_movement += move_time;
			total_turnaround += (cur_IOreq->end_time - cur_IOreq->arrival_time);
			int wait_time = (cur_IOreq->start_time - cur_IOreq->arrival_time);
			total_waittime += wait_time;
			if(wait_time > max_waittime)
				max_waittime = wait_time;
		}
		
		//*Special case
		if(cur_IOreq->track == trackAt) //The head does not need to move (input0 - SSTF)
			simTime--;
	}
	//Print summary
	for(int i = 0; i < IO_list.size(); i++) {
		IOrequest* r = IO_list[i];
		printf("%5d: %5d %5d %5d\n", i, r->arrival_time, r->start_time, r->end_time);
	}
	avg_turnaround = (double)total_turnaround / IO_list.size();
	avg_waittime = (double)total_waittime / IO_list.size();
	printf("SUM: %d %d %.2lf %.2lf %d\n", total_time, tot_movement, avg_turnaround, avg_waittime, max_waittime);
}

int main(int argc, char* argv[]) {
	string scheAlg;
	int c;
	while((c = getopt(argc, argv, "s:")) != -1) {
		scheAlg = optarg;
	} 
	string infile = argv[optind];
	Simulator sim;
	sim.scheduling(infile, scheAlg);
}
