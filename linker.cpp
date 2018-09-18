#include <iostream>
#include <fstream>
#include <cstdio>
#include <unistd.h>
#include <vector>
#include <string>
#include <utility>
#include <cctype>
#include <algorithm>
using namespace std;

class Symbol {
	public:
		string sym;
		int rel_addr;
		int abs_addr;
		int modnum;
		int defined;
		bool used;
		Symbol(int mn, string s, int ra);
};

Symbol::Symbol(int mn, string s, int ra) {
	modnum = mn;
	sym = s;
	rel_addr = ra;
	abs_addr = 0;
	defined = 1;
	used = false;
}

class Module {
	public:
		int base;
		int size;
		vector<pair<string, bool>> use_list; //bool to check symbols in use list
		Module(int n);
};

Module::Module(int n) {
	base = n;
	size = 0;
}

class Parser {
	public:
		void parse(string infile);
		void passOne();
		void passTwo();
		Parser();
		
	private:
		ifstream input;
		int linenum;
		int lineoffset;
		int cur_line;
		int cur_offset; 
		int last_line;
		char ends;
		
		vector<Module> mod_list;
		vector<Symbol> def_list;
		
		string getToken();
		bool isInt(string token);
		bool isSym(string token);
		bool isIAER(string token);
		int readInt();
		string readSym();
		char readIAER();
		void parseError(int errcode);
		void printSymTab();	
};

Parser::Parser() {
	linenum = 1;
	lineoffset = 1;
	cur_line = 1;
	cur_offset = 1;
	last_line = 0;
}

void Parser::parse(string infile) {
	input.open(infile);
	passOne();
	input.close();
	
	input.open(infile);
	passTwo();
	input.close();
	
}

string Parser::getToken() {
	string token = "";
	int n;
	while((n = input.get()) != -1) {
		char c = char(n);
		ends = c;
		//append char to token
		if(token.length() == 0) {
			if(c == ' ' || c == '\t') //in-line delimiter
				continue;
			else if(c == '\n') { //line delimiter
				last_line = cur_offset;
				linenum = cur_line;
				cur_line++;
				cur_offset = 0; //set to 0, automatically update after one loop
			}
			else {
				linenum = cur_line;
				lineoffset = cur_offset;
				token += c;
			}
		}
		else {
			//read token
			if(c == ' ' || c == '\t') {
				cur_offset++;
				return token;	
			}
			else if(c == '\n') { //
				last_line = cur_offset;
				cur_line++;
				cur_offset = 1;
				return token;
			}
			else {
				token += c;
			}
		}
		cur_offset++;
	}
	//eof ends with \n
	if(ends == '\n' ) {
		linenum = cur_line - 1;
		lineoffset = last_line;
	}
	else {
		linenum = cur_line;
		lineoffset = cur_offset;
	}	
}

bool Parser::isInt(string token) {
	for(int i = 0; i < token.length(); i++)
		if(!isdigit(token[i]))
			return false;
	return true;
}

bool Parser::isSym(string token) {
	if(!isalpha(token[0]))
		return false;
	for(int i = 1; i < token.length(); i++)
		if(!isalnum(token[i]))
			return false;
	return true;
}

bool Parser::isIAER(string token) {
	if(token[0] == 'I' || token[0] == 'A' || token[0] == 'E' || token[0] == 'R')
		return true;
	else
		return false;
}

int Parser::readInt() {
	string token = getToken();
	if(!isInt(token))
		parseError(0); //number expected
	return atoi(token.c_str());
}

string Parser::readSym() {
	string token = getToken();
	if(!isSym(token))
		parseError(1); //symbol expected
	if(token.length() > 16)
		parseError(3); //symbol name is too long
	return token;
}

char Parser::readIAER() {
	string token = getToken();
	if(!isIAER(token))
		parseError(2); //addressing expected
	return token[0];
}

//Pass 1
void Parser::passOne() {
	int total_inst = 0;
	int n = 0;
	int modnum = 1;
	while(!input.eof()) {
		Module module(n);
		//Read def list
		int defcount = readInt();
		if(defcount > 16)
			parseError(4); //too many def in module
		for(int i = 0; i < defcount; i++) {
			string sym = readSym();
			int val = readInt();
			Symbol symbol(modnum, sym, val);
			int flag = 0;
			int index  = -1;
			for(int j = 0; j < def_list.size(); j++) { //not find
				Symbol &s = def_list[j];
				if(s.sym.compare(sym) == 0) {
					index = j;
					flag = 1;
					break;
				}
			}
			if(flag == 0)
				def_list.push_back(symbol);
			else
				def_list[index].defined++;
		}
		
		//Read use list
		int usecount = readInt(); 
		if(usecount > 16)
			parseError(5); //too many use in module
		for (int i = 0; i < usecount; i++) {
			string sym = readSym();
			module.use_list.push_back(make_pair(sym, false));
		}
		
		//Read inst list
		int codecount = readInt();
		total_inst += codecount;
		if(total_inst > 512)
			parseError(6); //total num_instr exceeds memory size
		for(int i = 0; i < codecount; i++) {
			readIAER();
			readInt();
		}
		module.size = codecount;
		mod_list.push_back(module);
		
		n += codecount; //base update
		modnum++; //id update
	}
	
	//Check rule 5
	for(int i = 0; i < def_list.size(); i++) {
		Symbol &symbol = def_list[i];
		Module &module = mod_list[symbol.modnum - 1];
		if(symbol.rel_addr > module.size - 1) {
			printf("Warning: Module %d: %s too big %d (max=%d) assume zero relative\n", symbol.modnum, symbol.sym.c_str(), symbol.rel_addr, module.size - 1);
			symbol.rel_addr = 0;
		}	
		//Compute absolute address
		symbol.abs_addr = module.base + symbol.rel_addr;
	}
	
	printSymTab();
}

//Pass 2
void Parser::passTwo() {
	int modnum = 0;
	int lcount = 0;
	cout<<"Memory Map"<<endl;
	while(!input.eof()) {
		Module module = mod_list[modnum];
		//Read def list
		int defcount = readInt();
		for(int i = 0; i < defcount; i++) {
			string sym = readSym();
			int val = readInt(); 
		}
		
		//Read use list
		int usecount = readInt(); 		
		for(int i = 0; i < usecount; i++) {
			string sym = readSym();
		}
		
		//Read inst list
		int codecount = readInt();
		for(int i = 0; i < codecount; i++) {
			char type = readIAER();
			int instr = readInt();
			int opcode = instr / 1000;
			int oprand = instr % 1000;
			switch(type) {
				case 'I':
					if(instr > 9999) {
						instr = 9999;
						printf("%03d: %04d Error: Illegal immediate value; treated as 9999", lcount, instr);
					}
					else {
						printf("%03d: %04d", lcount, instr);
					}
					break;
					
				case 'A':
					if(opcode > 9) {
						instr = 9999;
						printf("%03d: %04d Error: Illegal opcode; treated as 9999", lcount, instr);
					}
					else if(oprand > 511) { //rule 8
						oprand = 0;
						printf("%03d: %01d%03d Error: Absolute address exceeds machine size; zero used", lcount, opcode, oprand);
					}
					else {
						printf("%03d: %04d", lcount, instr);
					}
					break;
					
				case 'E':
					if(opcode > 9) {
						instr = 9999;
						printf("%03d: %04d Error: Illegal opcode; treated as 9999", lcount, instr);
					}
					else if(oprand > module.use_list.size() - 1) {
						printf("%03d: %04d Error: External address exceeds length of uselist; treated as immediate", lcount, instr);
					}
					else {
						string sym = module.use_list[oprand].first;
						module.use_list[oprand].second = true;
						int flag = 0;
						int index = -1;
						for(int j = 0; j < def_list.size(); j++) { //not find
							Symbol &s = def_list[j];
							if(s.sym.compare(sym) == 0) {
								flag = 1;
								index = j;
								break;
							}
						}	
						if(flag == 0) { //rule 3
							oprand = 0;
							printf("%03d: %01d%03d Error: %s is not defined; zero used", lcount, opcode, oprand, sym.c_str());
						}
						else {
							Symbol &symbol = def_list[index];
							oprand = symbol.abs_addr;
							symbol.used = true;
							printf("%03d: %01d%03d", lcount, opcode, oprand);
						}
					}
					break;
					
				case 'R':
					if(opcode > 9) {
						instr = 9999;
						printf("%03d: %04d Error: Illegal opcode; treated as 9999", lcount, instr);
					}
					else if(oprand > module.size - 1) { //rule 9
						oprand = module.base + 0;
						printf("%03d: %01d%03d Error: Relative address exceeds module size; zero used", lcount, opcode, oprand);
					}
					else {
						oprand += module.base;
						printf("%03d: %01d%03d", lcount, opcode, oprand);
					}
					break;
			}
			lcount++;
			cout<<endl;
		}
		modnum++;
		
		//Check rule 7
		for(int i = 0; i < module.use_list.size(); i++) {
			string sym = module.use_list[i].first;
			bool used = module.use_list[i].second;
			int flag = 0;
			int index = -1;
			for(int j = 0; j < def_list.size(); j++) { //not find
				Symbol &s = def_list[j];
				if(s.sym.compare(sym) == 0) {
					index = j;
					flag = 1;
					break;
				}
			}
			if(used == false) {
				printf("Warning: Module %d: %s appeared in the uselist but was not actually used\n", modnum, sym.c_str());
			}
		}
	}
	cout<<endl;
	//Check rule 4
	for(int i = 0; i < def_list.size(); i++) {
		Symbol &symbol = def_list[i];
		if(symbol.used == 0)
			printf("Warning: Module %d: %s was defined but never used\n", symbol.modnum, symbol.sym.c_str());
	}
	cout<<endl;
}

void Parser::parseError(int errcode) {
	const char* errstr[] = {
		"NUM_EXPECTED", // Number expect
		"SYM_EXPECTED", // Symbol Expected
		"ADDR_EXPECTED", // Addressing Expected which is A/E/I/R
		"SYM_TOO_LONG", // Symbol Name is too long
		"TOO_MANY_DEF_IN_MODULE", // > 16
		"TOO_MANY_USE_IN_MODULE", // > 16
		"TOO_MANY_INSTR" // total num_instr exceeds memory size (512)
	};
	printf("Parse Error line %d offset %d: %s\n", linenum, lineoffset, errstr[errcode]);
	exit(1);
}

void Parser::printSymTab() {
	cout<<"Symbol Table"<<endl;
	for(int i = 0; i < def_list.size(); i++) {
		Symbol &symbol = def_list[i];
		cout<<symbol.sym<<"="<<symbol.abs_addr;
		if(symbol.defined != 1) //rule 2
			cout<<" Error: This variable is multiple times defined; first value used";
		cout<<endl;
	}
	cout<<endl;
}

int main(int argc, char* argv[]) {
	string infile = argv[1];
	Parser parser;
	parser.parse(infile);
}
