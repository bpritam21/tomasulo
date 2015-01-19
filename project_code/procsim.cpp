#include "procsim.hpp"
#include "stdlib.h"
#include "stdio.h"
#include <iostream>
#include <vector>

using namespace std;

void execution(void);
void schedule(void);
bool find_free_FU(unsigned int sq_FU, uint64_t* score_row);
bool find_min_tag_ready(uint64_t* sq_row, bool first_time);
void state_update(void);
bool check_schedule_empty(void);
void dispatch(void);
bool check_schedule_full(uint64_t* sched_pos);
void clear_sched(void);
void print_log(void);
void cal_avg_disp_size(void);
double cal_avg_dq_size(void);

struct schedule_queue{
	bool scheduled;
	bool clean;
	bool execute;
	unsigned int instr_no;
	unsigned int FU;
	int dest_reg_no;
	unsigned int dest_reg_tag;
	unsigned int src1_tag;
	bool src1_ready;
	unsigned int src2_tag;
	bool src2_ready;
}*sq;

struct score_board{
	unsigned int instr_no;
	unsigned long cycle;	
	int dest_reg_no;
	unsigned int dest_reg_tag;
	unsigned int FU;
	bool busy;
}*score;

struct result_bus{
	unsigned int instr_no;
	unsigned int tag;
	int dest_reg_no;
	bool busy;
}*cdb;

struct register_file{
	unsigned int tag;
	bool ready;
}*rf;

struct dispatch_queue{
	unsigned int instr_no;
	unsigned int FU;
	int dest_reg;
	int src1_reg;
	int src2_reg;
};

struct log{
	unsigned int instr_no;
	unsigned long fetch_cycle;
	unsigned long dispatch_cycle;
	unsigned long schedule_cycle;
	unsigned long execute_cycle;
	unsigned long state_update_cycle;
};

vector<struct dispatch_queue> dq;
vector<struct log> out_log;
vector<unsigned int> dq_size;
uint64_t SQ_SIZE;
uint64_t SCORE_SIZE;
uint64_t tag_count;
uint64_t instr_count;
uint64_t actual_r;
uint64_t actual_k0;
uint64_t actual_k1;
uint64_t actual_k2;
uint64_t actual_f;
unsigned long cycle;
unsigned int* instr_no;
proc_stats_t* global_stats;
bool proc_end;


void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f) 
{
	uint64_t count = 0;
	
	SQ_SIZE = 2*(k0+k1+k2);
	SCORE_SIZE = (k0+k1+k2);
	instr_count = 1;
	tag_count = 1;
	cycle = 1;
	proc_end = F;
	actual_r = r;
	actual_k0 = k0;
	actual_k1 = k1;
	actual_k2 = k2;
	actual_f = f;

	sq = new schedule_queue[SQ_SIZE];
	score = new score_board[SCORE_SIZE];
	cdb = new result_bus[r];
	rf = new register_file[REG_SIZE];
	
	for(count = 0; count<SQ_SIZE; count++){
		sq[count].clean = F;
		sq[count].execute = F;
	}
	
	for(count = 0; count<REG_SIZE; count++){
		rf[count].ready = T;
	}
	
	for(count = 0; count<r; count++){
		cdb[count].busy = F;
	}
	
	for(count = 0; count<SCORE_SIZE; count++){
		if(count < actual_k0){
			score[count].FU = 0;
		
		}else if(count< actual_k0 + actual_k1){
			score[count].FU = 1;
		
		}else{
			score[count].FU = 2;
		
		} 
	}

}

void fetch(FILE* inFile){
	unsigned int instr_address;
	int dest_reg;
	int src1_reg;
	int src2_reg;
	
	int FU;
	uint64_t count=0;
	for(count=0;count<actual_f;count++){
		int ret = fscanf(inFile, "%x %d %d %d %d\n", &instr_address, &FU, &dest_reg, &src1_reg, &src2_reg); 
		if(ret == 5) {
			struct dispatch_queue temp_node;
			struct log temp_log;
			
			temp_log.fetch_cycle=cycle-1;
			temp_log.dispatch_cycle=cycle;
			temp_log.schedule_cycle=0;
			temp_log.execute_cycle=0;
			temp_log.state_update_cycle=0;

			temp_node.instr_no = instr_count;
			temp_log.instr_no = instr_count;
			
			//cout<<cycle-1<<"\tFETCHED\t"<<instr_count<<endl;
			
			if(FU==-1 || FU==1){
				temp_node.FU = 1;
			}else{
				temp_node.FU = FU;
			}
			temp_node.dest_reg = dest_reg;
			temp_node.src1_reg = src1_reg;
			temp_node.src2_reg = src2_reg;
			
			dq.push_back (temp_node);
			out_log.push_back (temp_log);
	        }
		instr_count++;
	}
}


void run_proc(proc_stats_t* p_stats)
{
	global_stats = p_stats;
	
	cal_avg_disp_size();
	state_update();
	execution();
	schedule();
	dispatch();
	clear_sched();
	
	//Signal the end of processing
	if(cycle>2 && check_schedule_empty() == T){
		proc_end = T;
	}else{
		cycle++;
	}
}

void cal_avg_disp_size(){
	unsigned int temp_size = dq.size();

	dq_size.push_back (temp_size);

	if(cycle==1){
		global_stats->max_disp_size = 0;
	
	}else if(global_stats->max_disp_size < temp_size){
		global_stats->max_disp_size = temp_size;
	
	}
}

void clear_sched(){
	uint64_t count;
	for(count = 0; count<SQ_SIZE; count++){
		if(sq[count].clean == T){
			sq[count].instr_no = 0;
			sq[count].clean = F;
		}
	}
}

////EXECUTION
void execution(){
	uint64_t count=0, innerCount=0, min_instr_row=0, min_instr=0, min_cycle=0;
	bool valid = F;
	
	for(count = 0; count < SQ_SIZE ; count++){
		if(sq[count].execute == T){
		//	cout<<cycle<<"\tEXECUTED\t"<<sq[count].instr_no<<endl;
			sq[count].execute = F;
		}			
	}
			
	for(count = 0; count < actual_r; count++){
		if(cdb[count].busy == F){
			min_instr = 0xFFFFFFFFFFFFFFFF;
			min_cycle = 0xFFFFFFFFFFFFFFFF;
			for(innerCount = 0; innerCount < SCORE_SIZE; innerCount++){
				if(score[innerCount].busy == T && score[innerCount].instr_no!= 0){
					if(min_cycle > score[innerCount].cycle){
						min_cycle = score[innerCount].cycle;
						min_instr_row = innerCount;
						min_instr = score[innerCount].instr_no;
					}else if(min_cycle == score[innerCount].cycle && min_instr > score[innerCount].instr_no){
						min_cycle = score[innerCount].cycle;
						min_instr_row = innerCount;
						min_instr = score[innerCount].instr_no;
					}
				}
			}
			
			if(score[min_instr_row].instr_no != 0 && score[min_instr_row].busy == T){
				valid = T;
			}
			
			if(valid == T){
				cdb[count].instr_no = score[min_instr_row].instr_no;
				cdb[count].tag = score[min_instr_row].dest_reg_tag;
				cdb[count].dest_reg_no = score[min_instr_row].dest_reg_no;
				cdb[count].busy = T;
				score[min_instr_row].busy = F;
				valid = F;
			}
		}
	}
}


void schedule(){
	uint64_t sq_row=0, score_row=0;
	bool sq_ready = F, FU_free = F;
	
	sq_ready = find_min_tag_ready(&sq_row,T);
	FU_free = find_free_FU(sq[sq_row].FU, &score_row);
	
	while(sq_ready == T){
		if(FU_free == T){
			sq[sq_row].scheduled = T;
			sq[sq_row].execute = T;
			score[score_row].instr_no = sq[sq_row].instr_no;
			score[score_row].cycle = cycle;
			score[score_row].dest_reg_no = sq[sq_row].dest_reg_no;
			score[score_row].dest_reg_tag = sq[sq_row].dest_reg_tag;
			score[score_row].busy = T;
			out_log[sq[sq_row].instr_no-1].execute_cycle=cycle+1;
			
			//cout<<cycle<<"\tSCHEDULED\t"<<sq[sq_row].instr_no<<endl;
		sq_ready = find_min_tag_ready(&sq_row,F);
		FU_free = find_free_FU(sq[sq_row].FU, &score_row);
	}
}

bool find_free_FU(unsigned int sq_FU, uint64_t* score_row){
	uint64_t count=0;
	
	for(count = 0; count < SCORE_SIZE; count++){
		if(score[count].FU == sq_FU && score[count].busy == F){
			*score_row = count;
			return T;
		}
	}
	
	return F;
}

bool find_min_tag_ready(uint64_t* sq_row, bool first_time){
	uint64_t count = 0, min_row = 0, min = 0xFFFFFFFFFFFFFFFF, base;
	bool hehe = F;

	if(first_time == T){
		base = 0;
	}else{
		base = sq[*sq_row].instr_no;
	}
	
	for(count = 0; count < SQ_SIZE; count++){
		if(sq[count].clean == F && sq[count].scheduled == F && sq[count].src1_ready == T && sq[count].src2_ready == T && sq[count].instr_no != 0){
			if(min > sq[count].instr_no && sq[count].instr_no > base){
				hehe = T;
				min = sq[count].instr_no;
				min_row = count;
			}
		}
	}
	*sq_row = min_row;
	return hehe;
}

//////STATE UPDATE
void state_update(){
	uint64_t count=0, innerCount=0;
	
	for(count = 0; count < actual_r; count++){
		if(cdb[count].busy == T){
			global_stats->retired_instruction++;
			//cout<<cycle<<"\tSTATE UPDATE\t"<<cdb[count].instr_no<<endl;
			
			out_log[cdb[count].instr_no-1].state_update_cycle=cycle;
			
			if(cdb[count].dest_reg_no != -1 && rf[cdb[count].dest_reg_no].tag == cdb[count].tag){
				rf[cdb[count].dest_reg_no].ready = T;
			}
			
			for(innerCount = 0; innerCount < SQ_SIZE; innerCount++){
				if(sq[innerCount].scheduled == T && cdb[count].instr_no == sq[innerCount].instr_no){
					sq[innerCount].scheduled = F;
					sq[innerCount].clean = T;
				}
				
				if(sq[innerCount].src1_tag == cdb[count].tag){
					sq[innerCount].src1_ready = T;
				}
				if(sq[innerCount].src2_tag == cdb[count].tag){
					sq[innerCount].src2_ready = T;
			}
			cdb[count].busy = F;
		}
	}
}



bool check_schedule_empty(){
	uint64_t count = 0;
	
	for(count = 0; count < SQ_SIZE; count++){
		if(sq[count].instr_no != 0){
			return F;
		}
	}
	
	return T;
}

void dispatch(){
	bool sched_full;
	uint64_t sched_pos = 0;
	unsigned int boogaboo_tag = 0;
	bool boogaboo_ready = T;
	
	sched_full = check_schedule_full(&sched_pos);
	while(dq.size() != 0 && sched_full==F){
		out_log[dq[0].instr_no-1].schedule_cycle=cycle+1;
		
		sq[sched_pos].scheduled = F;
		sq[sched_pos].instr_no = dq[0].instr_no;	//Insert the instruction into the schedule queue
		sq[sched_pos].FU = dq[0].FU;			//Assign the FU type
		
		//Check for src1 register stuff
		boogaboo_tag = 0;
		boogaboo_ready = T;
		
		if(dq[0].src1_reg >-1){
			if(rf[dq[0].src1_reg].ready == F){
				boogaboo_tag = rf[dq[0].src1_reg].tag;
				boogaboo_ready = F;
			}
		}
		
		sq[sched_pos].src1_tag = boogaboo_tag;
		sq[sched_pos].src1_ready = boogaboo_ready;
		
		//Check for src2 register stuff
		boogaboo_tag = 0;
		boogaboo_ready = T;
		
		if(dq[0].src2_reg >-1){
			if(rf[dq[0].src2_reg].ready == F){
				boogaboo_tag = rf[dq[0].src2_reg].tag;
				boogaboo_ready = F;
			}
		}
		
		sq[sched_pos].src2_tag = boogaboo_tag;
		sq[sched_pos].src2_ready = boogaboo_ready;
		
		//check for dest register stuff
		boogaboo_tag = 0;
		
		if(dq[0].dest_reg >-1){
			rf[dq[0].dest_reg].tag = tag_count;
			rf[dq[0].dest_reg].ready = F;
			boogaboo_tag = tag_count;
			tag_count++;
		}
		sq[sched_pos].dest_reg_no = dq[0].dest_reg;
		sq[sched_pos].dest_reg_tag = boogaboo_tag;
		
		
		sched_full = check_schedule_full(&sched_pos);
		dq.erase (dq.begin());
	}
}

bool check_schedule_full(uint64_t* sched_pos){
	uint64_t count=0;
	
	for(count = *sched_pos; count < SQ_SIZE; count++){
		if(sq[count].instr_no == 0){
			*sched_pos = count;
			return F;
		}
	}
	
	*sched_pos = count;
	return T;
}


void complete_proc(proc_stats_t *p_stats) 
{
	p_stats->cycle_count = cycle;
	p_stats->avg_inst_fired = (float) p_stats->retired_instruction/p_stats->cycle_count;
	p_stats->avg_inst_retired = p_stats->avg_inst_fired;
	p_stats->avg_disp_size = cal_avg_dq_size();
	
	print_log();
	
	delete[] sq;
	sq = nullptr;
	
	delete[] score;
	score = nullptr;
	
	delete[] cdb;
	cdb = nullptr;
	
	delete[] rf;
	rf = nullptr;
}

double cal_avg_dq_size(){
	unsigned long long int sum = 0;
	double result;
	
	while(dq_size.size() != 0){
		sum = sum + dq_size[0];
		dq_size.erase (dq_size.begin());
	}
	
	result = (double) sum/ (double)cycle;
	return	result;
}

void print_log(){
	cout<<"INST\tFETCH\tDISP\tSCHED\tEXEC\tSTATE"<<endl;
	
	while(out_log.size() != 0){
		cout<< out_log[0].instr_no <<"\t"<< out_log[0].fetch_cycle <<"\t"<< out_log[0].dispatch_cycle <<"\t"<< out_log[0].schedule_cycle <<"\t"<< out_log[0].execute_cycle <<"\t"<< out_log[0].state_update_cycle <<endl;
		out_log.erase (out_log.begin());
	}
	cout<<endl;
}
