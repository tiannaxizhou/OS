#include <iostream>
#include <fstream>
#include <sstream> 
#include <cstring>
#include <vector>
#include <cstdlib>
#include <unistd.h>
#include <bitset>
#include <climits> 
using namespace std;

#define MAP 400
#define UNMAP 400
#define PAGE_IN 3000
#define PAGE_OUT 3000
#define FILE_IN 2500
#define FILE_OUT 2500
#define ZERO 150
#define SEGV 240
#define SEGPROT 300
#define READ_WRITE 1
#define CONTEXT_SWITCH 121

struct PTE { 
	unsigned int PRESENT : 1;
	unsigned int WRITE_PROTECT : 1;
	unsigned int MODIFIED : 1;
	unsigned int REFERENCED : 1;
	unsigned int PAGEDOUT : 1;
	unsigned int FRAMEINDEX : 7;
	unsigned int FILEMAPPED : 1;
	unsigned int OWNUSAGE : 19;
	
	PTE();	
};

PTE::PTE() {
	PRESENT = 0;
	WRITE_PROTECT = 0;
	MODIFIED = 0;
	REFERENCED = 0;
	PAGEDOUT = 0;
	FRAMEINDEX = 0;
	FILEMAPPED = 0;
}

class VMA {
	public:
		unsigned int starting_virtual_page : 7;
		unsigned int ending_virtual_page : 7;
		unsigned int write_protected : 1;
		unsigned int filemapped : 1;
		
		VMA(int start_page, int end_page, int write_prot, int file_map);
}; 

VMA::VMA(int start_page, int end_page, int write_prot, int file_map) {
	starting_virtual_page = start_page;
	ending_virtual_page = end_page;
	write_protected = write_prot;
	filemapped = file_map;
}

//Keep track of the inverse mapping: (frame --> <proc-id,vpage>) inside each frame's frame table entry
class Frame {
	public:
		int pid, vpage, index;
		Frame(int i);
};

Frame::Frame(int i) {
	index = i;
	pid = -1; //initialization
}

//A global frame_table describe the usage of each of its physical frames 
//where you maintain backward mappings to the address space(s) and the vpage that maps a particular frame
class FrameTable {
	public:
		vector<Frame> inverse_map;
		FrameTable();
		FrameTable(int num_frame);
		Frame* get_frame();
};

FrameTable::FrameTable() {
	
}

FrameTable::FrameTable(int num_frame) {
	for(int i = 0; i < num_frame; i++)
		inverse_map.push_back(Frame(i));
}

Frame* FrameTable::get_frame() {
	for(int i = 0; i < inverse_map.size(); i++) {
		Frame* frame = &(inverse_map[i]);
		if(frame->pid == -1) //first available
			return frame;
	}
	return NULL;
}

//Compute and print the summary statistics related to the VMM
//Track the number of instructions, segv, segprot, unmap, map, pageins (IN, FIN), pageouts (OUT, FOUT), and zero operations for each process
struct Pstats {
	//The cost calculation can overrun 2^32, use 64-bit counters
	unsigned long unmaps;
	unsigned long maps;
	unsigned long ins;
	unsigned long outs;
	unsigned long fins;
	unsigned long fouts;
	unsigned long zeros;
	unsigned long segv;
	unsigned long segprot;

	Pstats();
};

Pstats::Pstats() {
	unmaps = 0;
	maps = 0;
	ins = 0;
	outs = 0;
	fins = 0;
	fouts = 0;
	zeros = 0;
	segv = 0;
	segprot = 0;
}	

//Create process with its list of vmas and a page_table that 
//represents the translations from virtual pages to physical frames for that process.
class Process {
	public:
		int pid;
		vector<VMA> vmalist;
		vector<PTE> pageTable = vector<PTE>(64);
		Pstats pstats;
		Process(int index);
};

Process::Process(int index) {
	pid = index;
}

//Pager class
class Pager {
	public:
		Pager();
		virtual Frame* select_frame(vector<Process*>& proc_list, FrameTable* frame_table) {
			
		}
};

Pager::Pager() {
	
}

//Get random number for Random and NRU
class getRand {
	public:
		getRand(const string &rfile);
		int getRandomNumber(int size);
	private:
		int rcount, rofs;
		vector<int> randomValue;
};

getRand::getRand(const string &rfile) {
	rofs = 0;
	ifstream rinput(rfile.c_str());
	rinput >> rcount; //First number is the total count of random numbers
	for(int i = 0; i < rcount; i++) {
		int rand;
		rinput >> rand;
		randomValue.push_back(rand);
	} 
	rinput.close();
}

int getRand::getRandomNumber(int size) {
	int rind = randomValue[rofs % rcount] % size;
	rofs++;
	return rind;
}

//First In First Out
class FIFO : public Pager {
	public:
		FIFO();
		Frame* select_frame(vector<Process*>& proc_list, FrameTable* frame_table);
	private:
		vector<Frame*> frame_queue; 
		//Use vector O(1) compared to queue O(n)
};

FIFO::FIFO() {
	
}

Frame* FIFO::select_frame(vector<Process*>& proc_list, FrameTable* frame_table) {
	Frame* frame = frame_table->get_frame();
	if(frame == NULL) {
		frame = frame_queue.front(); //first out
		frame_queue.erase(frame_queue.begin());
		frame_queue.push_back(frame); //Push back to the end of the queue
	} 
	else
		frame_queue.push_back(frame);
	return frame; 
}

//Second Chance
class SC : public Pager {
	public:
		SC();
		Frame* select_frame(vector<Process*>& proc_list, FrameTable* frame_table);
	private:
		vector<Frame*> frame_queue;
};

SC::SC() {
	
}

Frame* SC::select_frame(vector<Process*>& proc_list, FrameTable* frame_table) {
	Frame* frame = frame_table->get_frame();
	if(frame == NULL) {
		frame = frame_queue.front();	
		PTE* pte = &(proc_list[frame->pid]->pageTable[frame->vpage]);
		while(pte->REFERENCED) {
			pte->REFERENCED = 0; //Reset ref bit
			frame_queue.erase(frame_queue.begin());
			frame_queue.push_back(frame); //Push to the end
			frame = frame_queue.front(); //Check the next frame
			pte = &(proc_list[frame->pid]->pageTable[frame->vpage]); 
		}
		frame_queue.erase(frame_queue.begin());
		frame_queue.push_back(frame); //Push back to the end of the queue
	}
	else
		frame_queue.push_back(frame);
	return frame;
}

//Random
class Random : public Pager{
	public:
		Random(getRand* randNum);
		Frame* select_frame(vector<Process*>& proc_list, FrameTable* frame_table);
	private:
		getRand *randNum;
};

Random::Random(getRand* randNum) : randNum(randNum){
	
}

Frame* Random::select_frame(vector<Process*>& proc_list, FrameTable* frame_table) {
	Frame* frame = frame_table->get_frame();
	if(frame == NULL) {
		int size = frame_table->inverse_map.size();
		int index = randNum->getRandomNumber(size);
		frame = &(frame_table->inverse_map[index]);
	}
	return frame;
}

//Not Recently Used
class NRU : public Pager{
	public:
		NRU(getRand* randNum);
		Frame* select_frame(vector<Process*>& proc_list, FrameTable* frame_table);
	private:
		int clock;
		vector<Frame*> classes[4]; //Two dimensinal vector for each class and each frame
		getRand* randNum; //Use the random function to select a random frame from the lowest class identified
};

NRU::NRU(getRand* randNum) : randNum(randNum) {
	clock = 0;
}

Frame* NRU::select_frame(vector<Process*>& proc_list, FrameTable* frame_table) {
	Frame* frame = frame_table->get_frame();
	if(frame == NULL) {
		//Clear classes before each selection
		for(auto &cls : classes)
			cls.clear();
		
		//Classify the frames into their four classes
		for(auto &tag : frame_table->inverse_map) {
			int pid = tag.pid;
			int vpage = tag.vpage;
			if(pid != -1) { //Used frame 
				PTE &pte = proc_list[pid]->pageTable[vpage];
				int prior = pte.REFERENCED * 2 + pte.MODIFIED; //(0, 0) < (0, 1) < (1, 0) < (1, 1)
				classes[prior].push_back(&tag);
			}
		}
		
		//Identify the lowest class and randomly select a page from it
		for(int i = 0; i < 4; i++) {
			if(classes[i].size() > 0) { //First non-empty class -> lowest one
				frame = classes[i].at(randNum->getRandomNumber(classes[i].size()));
				break;
			}
		}
		
		//NRU requires that the REFERENCED-bit be periodically reset for all valid page table entries.
		clock++;
		if(clock == 10) {
			clock = 0; //Every 10th page replacement request
			for(int i = 0; i < 4; i++) {
				for(auto tag : classes[i]) {
					PTE &pte = proc_list[tag->pid]->pageTable[tag->vpage];
					pte.REFERENCED = 0;
				}
			} 
		} 
	}
	return frame;
}

//Clock
class Clock : public Pager{
	public:
		Clock();
		Frame* select_frame(vector<Process*>& proc_list, FrameTable* frame_table);
	private:
		int hand;
		vector<Frame*> circle;
};

Clock::Clock() {
	hand = 0;
}

Frame* Clock::select_frame(vector<Process*>& proc_list, FrameTable* frame_table) {
	Frame* frame = frame_table->get_frame();
	if(frame == NULL) {
		//hand points to the frame number to be considered next
		frame = circle[hand];
		PTE* pte = &(proc_list[frame->pid]->pageTable[frame->vpage]);
		while(pte->REFERENCED) { //Same as second chance
			pte->REFERENCED = 0;
			hand = (hand + 1) % circle.size();
			frame = circle[hand];
			pte = &(proc_list[frame->pid]->pageTable[frame->vpage]);
		}
		hand = (hand + 1) % circle.size(); //Point to next frame
	}
	else
		circle.push_back(frame);
	return frame;
}

//Aging
class Aging : public Pager{
	public:
		Aging(int size);
		Frame* select_frame(vector<Process*>& proc_list, FrameTable* frame_table);
	private:
		vector<unsigned int> age; //32 bits
};

Aging::Aging(int size) {
	for(int i = 0; i < size; i++)
        age.push_back(0);
}

Frame* Aging::select_frame(vector<Process*>& proc_list, FrameTable* frame_table) {
	Frame* frame = frame_table->get_frame();
	if(frame == NULL) {
		unsigned long min = ULONG_MAX;
		for(int i = 0; i < frame_table->inverse_map.size(); i++) {
			Frame &tag = frame_table->inverse_map[i];
			if(tag.pid != -1) { //Same as NRU
				PTE &pte = proc_list[tag.pid]->pageTable[tag.vpage];
				unsigned int ref = pte.REFERENCED;
				age[i] = ((ref << 31) | (age[i] >> 1)); //Shift 1 bit to the right and set ref bit at the front
				pte.REFERENCED = 0; //Reset
			}
		}

		//Choose the page whose counter is lowest
		for(int i = 0; i < frame_table->inverse_map.size(); i++) {
			Frame &newframe = frame_table->inverse_map[i];
			if(newframe.pid != -1) { //Present
				if(age[i] < min) {
					min = age[i];
					frame = &newframe;
				}
			}
		}
		age[frame->index] = 0; //Reset the counter of victim page
	}
	return frame;
}

//Virtual Memory Management
class VMM {
	public:
		VMM();
		//Instruction* get_next_instruction();
		void readInputFile(string infile);
		void paging(string input, getRand *rand, string pagealg, bool Oop, bool Pop, bool Fop, bool Sop, int num_frames);
		void printPageTable();
		void printFrameTable();
		void printSummary();
	private:		
		int ctx_switches, inst_count;
		long long cost; 
		getRand* rand;
		Pager* pager;
		FrameTable* frameTable;
		//vector<Instruction> insList;
		vector<Process*> procList;
		Pstats pstats;
};

VMM::VMM() {
	ctx_switches = 0;
	inst_count = 0;
	cost = 0;
}

void VMM::printFrameTable() {
	//Show which frame is mapped at the end to which <pid:virtual page> or '*' if not currently mapped by any virtual page
	cout << "FT: ";
	for(auto tag : frameTable->inverse_map) {
		if(tag.pid != -1)
			cout<<tag.pid<<":"<<tag.vpage<<" ";
		else
			cout<<"* ";
	}
	cout<<endl;
}

void VMM::printPageTable() {
	//Print the content of the pagetable pte entries: R (referenced), M (modified), S (swapped out)
	//Pages that are not valid are represented by a '#' if they have been swapped out, or a '*' if it does not have a swap area associated with. 
	//Otherwise (valid) indicates the virtual page index and RMS bits with бо-бо indicated that that bit is not set.
	for(int pid = 0; pid < procList.size(); pid++) {
		cout<<"PT["<<pid<<"]: ";
		for(int i = 0; i < procList[pid]->pageTable.size(); i++) {
			PTE &pte = procList[pid]->pageTable[i];
			if(!pte.PRESENT) { 
				if(pte.PAGEDOUT)
					cout<<"#";
				else
					cout<<"*"; 
			}
			else {
				cout<<i<<":";
				if(pte.REFERENCED)
					cout<<"R";
				else
					cout<<"-";
				if(pte.MODIFIED)
					cout<<"M";
				else
					cout<<"-";
				if(pte.PAGEDOUT)
					cout<<"S";
				else
					cout<<"-";
			}
			cout<<" ";
		}
		cout<<endl;
	}
} 

void VMM::printSummary() {
	for(int i = 0; i < procList.size(); i++) {
		printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
				procList[i]->pid,
				procList[i]->pstats.unmaps, procList[i]->pstats.maps, procList[i]->pstats.ins, procList[i]->pstats.outs,
				procList[i]->pstats.fins, procList[i]->pstats.fouts, procList[i]->pstats.zeros, procList[i]->pstats.segv, procList[i]->pstats.segprot);	
	}
	printf("TOTALCOST %lu %lu %llu\n", ctx_switches, inst_count, cost);
}

void VMM::paging(string infile, getRand *rand, string pagealg, bool Oop, bool Pop, bool Fop, bool Sop, int num_frames) {
	//readInputFile(infile);
	//Process instructions while reading
	ifstream input;
	input.open(infile.c_str());
	string line;
	char instr;
	int num_proc, num_VMA, vpage, start_page, end_page, write_prot, file_map;
	while(getline(input, line) && line[0] == '#'); //All lines starting with '#' must be ignored
	//First line not starting with a '#' is the number of processes
	num_proc = atoi(line.c_str());
	//Read in VMAs for each process
	for(int i = 0; i < num_proc; i++) {
		Process* proc = new Process(i);
		while(getline(input, line) && line[0] == '#');
		num_VMA = atoi(line.c_str());
		for(int j = 0; j < num_VMA; j++) {
			getline(input, line);
			stringstream split(line);
			split >> start_page >> end_page >> write_prot >> file_map;
			VMA vma = VMA(start_page, end_page, write_prot, file_map);
			proc->vmalist.push_back(vma);
		}
		procList.push_back(proc);
	}
	char alg = pagealg[0];
	//Choose paging algorithm 
	if(alg == 'f')
		pager = new FIFO();
	if(alg == 's')
		pager = new SC();
	if(alg == 'r')
		pager = new Random(rand);
	if(alg == 'n')
		pager = new NRU(rand);
	if(alg == 'c')
		pager = new Clock();
	if(alg == 'a')
		pager = new Aging(num_frames);
		
	frameTable = new FrameTable(num_frames);
	//int totalIns = insList.size();
	
	while(getline(input, line)) {
		//Instruction* ins = get_next_instruction(); 
		if(!line.empty() && line[0] != '#') {
			stringstream split(line);
			split >> instr >> vpage;			
			//char instr = ins->instruction;
			Process* cur_proc;
			if(Oop) {
				cout << inst_count << ": ==> " << instr << " " << vpage << endl;
			}
			inst_count++;
			
			//First instruction is a context switch, a pointer to the page table
			if(instr == 'c') {
				int pid = vpage;
				cur_proc = procList[pid];
				ctx_switches++;
				cost += CONTEXT_SWITCH;
				continue;
			}
			
			//int vpage = ins->virtual_page;
			PTE &pte = cur_proc->pageTable[vpage];
			Pstats &pstats = cur_proc->pstats;
			cost += READ_WRITE;
			//Check the page is present
			if(!pte.PRESENT) {
			//Page fault
				//1. Look up anthor table to decide
				bool find = 0;
				for(auto &vma : cur_proc->vmalist) {
					if(vpage >= vma.starting_virtual_page && vpage <= vma.ending_virtual_page) {
						//See if the virtual page is valid
						pte.WRITE_PROTECT = vma.write_protected;
						pte.FILEMAPPED = vma.filemapped;
						find = 1;
						break;
					}
				}
				
				if(!find) { //Invalid reference => abort
					pstats.segv++;
					cost += SEGV;
					if(Oop) {
						cout<<"  SEGV"<<endl;
					}
					continue; //Get next instruction
				}
				
				//2. Find free frame
				//Page replacement
				Frame* frame = pager->select_frame(procList, frameTable);
				int vic_pid = frame->pid;
				int vic_vpage = frame->vpage;
				//cout<<"select"<<vic_pid;
				//Figure out if/what to do with old frame if it was mapped
				if(vic_pid != -1) {
					PTE &vic_pte = procList[vic_pid]->pageTable[vic_vpage];
					Pstats &vic_pstats = procList[vic_pid]->pstats;
					//Unmap
					vic_pte.PRESENT = 0;
					vic_pstats.unmaps++;
					cost += UNMAP;
					if(Oop) {
						cout<<" UNMAP "<<vic_pid<<":"<<vic_vpage<<endl;
					}
					
					//Check if the page was dirty (modified)
					if(vic_pte.MODIFIED) {
						if(vic_pte.FILEMAPPED) {
							vic_pstats.fouts++;
							cost += FILE_OUT;
							if(Oop) {
								cout<<" FOUT"<<endl;
							}
						}
						else {
							vic_pte.PAGEDOUT = 1;
							vic_pstats.outs++;
							cost += PAGE_OUT;
							if(Oop) {
								cout<<" OUT"<<endl;
							}
						}
						vic_pte.MODIFIED = 0; //reset
					}
				}
					
				//3.Swap page into frame via scheduled disk operation
				if(pte.PAGEDOUT) { //Page in
					pstats.ins++;
					cost += PAGE_IN;
					if(Oop) {
						cout<<" IN"<<endl;
					}
				}
				else if(pte.FILEMAPPED) { //File in
					pstats.fins++;
					cost += FILE_IN;
					if(Oop) {
						cout<<" FIN"<<endl;
					}
				}
				else { //Zeroed
					pstats.zeros++;
					cost += ZERO;
					if(Oop) {
						cout<<" ZERO"<<endl;
					}
				}
					
				//Map
				pstats.maps++;
				cost += MAP;
				
				//4.Reset tables to indicate page now in memorySet validation bit = v
				pte.PRESENT = 1;
				pte.FRAMEINDEX = frame->index;
				frame->pid = cur_proc->pid;
				frame->vpage = vpage;
				if(Oop) {
					cout<<" MAP "<<pte.FRAMEINDEX<<endl;
				}
				//5.Restart the instruction that caused the page fault
			}
			
			//Update page table 
			pte.REFERENCED = 1;
			if(instr == 'w') {
				if(pte.WRITE_PROTECT) {
					pstats.segprot++;
					cost += SEGPROT;
					if(Oop) {
						cout<<" SEGPROT"<<endl;
					}
				}
				else {
					pte.MODIFIED = 1;
				}
			}
		}	
	}
	//Print options
	if(Pop)
		printPageTable();
	if(Fop)
		printFrameTable();
	if(Sop)
		printSummary();
}


int main(int argc, char* argv[]) {
	string alg, opt, fnum;
	bool Oop = 0, Pop = 0, Fop = 0, Sop = 0;
	int c, num_frames;
	
	//Provide optional arguments in arbitrary order
	//https://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html
	while((c = getopt(argc, argv, "a:o:f:")) != -1) {
		switch(c) {
			case 'a': //[-a<algo>]
				alg = optarg;
				break;
			case 'o': //[-o<options>]
				opt = optarg;
				if(opt.find('O') != string::npos) Oop = 1;
				if(opt.find('P') != string::npos) Pop = 1;
				if(opt.find('F') != string::npos) Fop = 1;
				if(opt.find('S') != string::npos) Sop = 1;
				break;
			case 'f': //[-f<num_frames>]
				fnum = optarg;
				num_frames = atoi(fnum.c_str());
				break;
			case '?':
 	      	 	if (optopt == 'a' || optopt == 'o' || optopt == 'f')
    	      		fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        		else if (isprint (optopt))
          			fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        		else
          			fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
        		return 1;
      		default:
        		abort ();
		}
	}
	
	getRand rand(argv[optind + 1]);
    VMM sim;
    sim.paging(argv[optind], &rand, alg, Oop, Pop, Fop, Sop, num_frames);
    
    return 0;
}
