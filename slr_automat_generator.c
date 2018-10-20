#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "datastructs.h"
#include "lexer.h"
#include "parser.h"
#include "slr_automat_generator.h"

int state_item_counter;

char* rules_register = NULL;
void rules_register_reset(){
	for(int i = 0; i < rules_counter; i++) rules_register[i] = 0;
}
int rules_register_sum(){
	int sum = 0;
	for(int i = 0; i < rules_counter; i++) sum += rules_register[i];
	return sum;
}

int* terminal_transitions;
void terminal_transitions_reset(){
	for(int i = 0; i < terminals_counter; i++) terminal_transitions[i] = -1;
}
int terminal_transitions_sum(){
	int sum = 0;
	for(int i = 0; i < terminals_counter; i++){
		if(terminal_transitions[i] > -1) sum += 1;
	}
	return sum;
}

int* nonterminal_transitions;
void nonterminal_transitions_reset(){
	for(int i = 0; i < nonterminals_counter; i++) nonterminal_transitions[i] = -1;
}
int nonterminal_transitions_sum(){
	//printf("nonterminal_transitions_sum\n");			
	int sum = 0;
	for(int i = 0; i < nonterminals_counter; i++){
		if(nonterminal_transitions[i] > -1) sum += 1;
	}
	return sum;
}

void lr_automat_generator_initialize(){
	//printf("lr_automat_generator_initialize()\n");		
	lr_states = malloc(sizeof(LR_state*) * LR_STATES_MAX_COUNTER);
	for(int i = 0; i < LR_STATES_MAX_COUNTER; i++) lr_states[i] = NULL;
	rules_register = malloc(rules_counter);
	rules_register_reset();
	terminal_transitions = malloc(sizeof(int*) * terminals_counter);
	terminal_transitions_reset();
	nonterminal_transitions = malloc(sizeof(int*) * nonterminals_counter);
	nonterminal_transitions_reset();
}

/* items */

Item* new_item(int rule, int position){
	//printf("new_item\n");		
	Item* item = malloc(sizeof(Item));
	item->rule = rule;
	item->position = position;
	item->lookahead = NULL;
	return item;
}

int compare_items(Item* item1, Item* item2){
	//printf("compare_items\n");	
	if((item1->rule == item2->rule)&&(item1->position == item2->position)) return 1;
	return 0;
}

/********************* LR states ************************/

static int lr_states_max_counter = LR_STATES_MAX_COUNTER;
void add_lr_state(LR_state* new_lr_state){
	if(lr_states_counter >= lr_states_max_counter){
		lr_states_max_counter *= 2;
		LR_state** new_lr_states = malloc(sizeof(LR_state*) * lr_states_max_counter);
		for(int i = 0; i < lr_states_counter; i++) new_lr_states[i] = lr_states[i];
		for(int i = lr_states_counter; i < lr_states_max_counter; i++) new_lr_states[i] = NULL;
		free(lr_states);
		lr_states = new_lr_states;
	}
	lr_states[lr_states_counter++] = new_lr_state;
}


int compare_lr_states(LR_state* state1, LR_state* state2){
	//printf("compare_lr_states\n");	
	int kernel_items_count_comparison = 0;
	if(state1->kernel_items_count == state2->kernel_items_count){
		for(int i = 0; i < state1->kernel_items_count; i++){
			for(int j = 0; j < state2->kernel_items_count; j++){
				if(compare_items(state1->kernel_items[i], state2->kernel_items[j])){
					kernel_items_count_comparison++;
					break;
				}
			}
		}
		if(kernel_items_count_comparison == state1->kernel_items_count) return 1;
		kernel_items_count_comparison = 0;
	}
	return 0;
}


int get_lr_state(LR_state* lr_state){
	//printf("get_lr_state\n");
	for(int i = 0; i < lr_states_counter; i++){	
		if(compare_lr_states(lr_states[i],lr_state)) return i;
	}
	return -1;
}

LR_state* new_lr_state(int kernel_items_count, int nonkernel_items_count){
	//printf("new_lr_state\n");	

	if(kernel_items_count > max_kernel_items_count) max_kernel_items_count = kernel_items_count;
	
	LR_state* lr_state = malloc(sizeof(LR_state));
	if(kernel_items_count < 1){
		lr_state->kernel_items = NULL;
		lr_state->kernel_items_count = 0;		
	} else {
		lr_state->kernel_items = malloc(sizeof(Item*)*kernel_items_count);
		lr_state->kernel_items_count = kernel_items_count;
	}
	if(nonkernel_items_count < 1){
		lr_state->nonkernel_items = NULL;
		lr_state->nonkernel_items_count = 0; 		
	} else {
		lr_state->nonkernel_items = malloc(sizeof(Item*)*nonkernel_items_count);
		lr_state->nonkernel_items_count = nonkernel_items_count; 
	}
	return lr_state;
}

void delete_lr_state(LR_state* lr_state){
	//printf("delete_lr_state\n");	
	for(int i = 0; i < lr_state->kernel_items_count; i++){ 
		if(lr_state->kernel_items[i] != NULL) free(lr_state->kernel_items[i]);
	}	
	if(lr_state->kernel_items != NULL) free(lr_state->kernel_items);
	
	for(int i = 0; i < lr_state->nonkernel_items_count; i++){
		//printf("nonkernel item %i\n", i);//////////////////////////////////
		if(lr_state->nonkernel_items[i] != NULL) free(lr_state->nonkernel_items[i]);
	}
	if(lr_state->nonkernel_items != NULL) free(lr_state->nonkernel_items);	
	
	free(lr_state);
}

void closure(int rule, int position){	
	//printf("closure\n");		
	if(position > rules[rule]->body_length-1) return;	
	Token* token = (rules[rule]->body)[position];
	
	if(token->tag == NONTERMINAL){
		for(int i = 0; i < rules_counter; i++){							
			if(rules[i]->head == token && (int)rules_register[i] != 1 ){
				rules_register[i] = 1;
				closure(i, 0);
			}
		}
	}
}

int go2(int lr_state, int token, int token_type){
	//printf("go2, %i\n", lr_state);	
	LR_state* _new_lr_state = NULL;
	int rule;
	int position;
	Token* symbol;
	int new_kernel_items_count = 0;
	int new_nonkernel_items_count = 0;
	
	if(token_type == TERMINAL)
		symbol = terminals[token];
	else if(token_type == NONTERMINAL)
		symbol = nonterminals[token];
	else 
		return -1;
	for(int i = 0; i < lr_states[lr_state]->kernel_items_count; i++){
		rule = lr_states[lr_state]->kernel_items[i]->rule;
		position = lr_states[lr_state]->kernel_items[i]->position;	
		if(position < rules[rule]->body_length){ 		
			if(((rules[rule]->body)[position]) == symbol){ 
				new_kernel_items_count++;
				closure(rule, position + 1);
			}
		}
	}
	for(int i = 0; i < lr_states[lr_state]->nonkernel_items_count; i++){
		rule = lr_states[lr_state]->nonkernel_items[i]->rule;
		position = lr_states[lr_state]->nonkernel_items[i]->position;	
		if(position < rules[rule]->body_length){ 
			if(((rules[rule]->body)[position]) == symbol){ 
				new_kernel_items_count++;
				closure(rule, position + 1);
			}
		}
	}	
	if(new_kernel_items_count > 0){
		new_nonkernel_items_count = rules_register_sum();
		_new_lr_state = new_lr_state(new_kernel_items_count, new_nonkernel_items_count);
		for(int i = lr_states[lr_state]->nonkernel_items_count-1; i >= 0; i--){
			rule = lr_states[lr_state]->nonkernel_items[i]->rule;
			position = lr_states[lr_state]->nonkernel_items[i]->position;
			if(position < rules[rule]->body_length){ 	
				if(((rules[rule]->body)[position]) == symbol) 
					_new_lr_state->kernel_items[--new_kernel_items_count] = new_item(rule, position + 1);
			}
		}	
		for(int i = lr_states[lr_state]->kernel_items_count-1; i >= 0; i--){
			rule = lr_states[lr_state]->kernel_items[i]->rule;
			position = lr_states[lr_state]->kernel_items[i]->position;
			if(position < rules[rule]->body_length){ 	
				if(((rules[rule]->body)[position]) == symbol) 
					_new_lr_state->kernel_items[--new_kernel_items_count] = new_item(rule, position + 1);
			}
		}
		int lr_state1 = get_lr_state(_new_lr_state);
		
		//if state already exists
		if(lr_state1 > -1){
			_new_lr_state->nonkernel_items_count = 0;
			delete_lr_state(_new_lr_state);
			rules_register_reset();
			if(token_type == TERMINAL) terminal_transitions[token] = lr_state1; else nonterminal_transitions[token] = lr_state1;				
			return lr_state1;
		}
		
		new_nonkernel_items_count = 0;
		for(int i = 0; i < rules_counter; i++){
			if(rules_register[i]){
				_new_lr_state->nonkernel_items[new_nonkernel_items_count++] = new_item(i,0);
			}
		}
		rules_register_reset();
		
		if(token_type == TERMINAL) terminal_transitions[token] = lr_states_counter; else nonterminal_transitions[token] = lr_states_counter;
		add_lr_state(_new_lr_state);
		return lr_states_counter;	
		
	}
	return -1;	
}

LR_state_trans* new_lr_state_trans(int token, int state){
	//printf("new_lr_state_trans\n");
	LR_state_trans* lr_state_trans = malloc(sizeof(LR_state_trans));
	lr_state_trans->token = token;
	lr_state_trans->state = state;
	return lr_state_trans;
}

void build_first_state(){
	//printf("build_first_state\n");
	closure(0,0);
	int nonkernel_items_count = rules_register_sum();
	LR_state* frst_state = new_lr_state(1, nonkernel_items_count);	
	frst_state->kernel_items[0] = new_item(0, 0);
	
	nonkernel_items_count = 0;
	for(int i = 0; i < rules_counter; i++){
		if(rules_register[i]){
			frst_state->nonkernel_items[nonkernel_items_count++] = new_item(i,0);
		}
	}	
	
	add_lr_state(frst_state);
	rules_register_reset();
}

void build_lr_automat(){
	//printf("build_lr_automat\n");
	lr_automat_generator_initialize();
	build_first_state();
	
	int i = 0;
	int temp = 0;
	
	int local_terminals_counter = 0;
	int local_nonterminals_counter = 0;

	while(lr_states[i] != NULL){
		
		for(int j = 0; j < terminals_counter; j++) go2(i, j, TERMINAL);
		for(int j = 0; j < nonterminals_counter; j++) go2(i, j, NONTERMINAL);

		temp = terminal_transitions_sum();
		lr_states[i]->terminals = malloc(sizeof(LR_state_trans*) * temp);
		lr_states[i]->terminals_count = temp;
		
		for(int j = 0; j < terminals_counter; j++){
			if(terminal_transitions[j] > -1) (lr_states[i]->terminals)[local_terminals_counter++] = new_lr_state_trans(j,terminal_transitions[j]);
		}
		local_terminals_counter = 0;
		terminal_transitions_reset();
		
		
		temp = nonterminal_transitions_sum();
		lr_states[i]->nonterminals = malloc(sizeof(LR_state_trans*) * temp);
		lr_states[i]->nonterminals_count = temp;
		
		for(int j = 0; j < nonterminals_counter; j++){
			if(nonterminal_transitions[j] > -1) (lr_states[i]->nonterminals)[local_nonterminals_counter++] = new_lr_state_trans(j,nonterminal_transitions[j]);
		}
		local_nonterminals_counter = 0;
		nonterminal_transitions_reset();
		i++;
	}	
}

/*************** LR automat printing ******************/

void print_lr_state_items(int i){
	printf("STATE %i\n", i);
	
	printf("\tKERNEL ITEMS %i:\n", lr_states[i]->kernel_items_count);	
	for(int j = 0; j < lr_states[i]->kernel_items_count; j++){
		printf("\t\t %i\t", lr_states[i]->kernel_items[j]->position); print_rule(lr_states[i]->kernel_items[j]->rule);
		if(lr_states[i]->kernel_items[j]->lookahead != NULL){
			printf("\t\t\tLOOKAHEAD SYMBOLS: ");
			for(int l = 0; l < lr_states[i]->kernel_items[j]->lookahead->terminals_count; l++){
				if(lr_states[i]->kernel_items[j]->lookahead->terminals[l] < 0){
					printf("%i ", lr_states[i]->kernel_items[j]->lookahead->terminals[l]);
				} else {
					printf("%s ", terminals[lr_states[i]->kernel_items[j]->lookahead->terminals[l]]->lexeme);
				}
			}
			printf("\n");
		}
	}
	
	printf("\tNONKERNEL ITEMS %i:\n", lr_states[i]->nonkernel_items_count);	
	for(int j = 0; j < lr_states[i]->nonkernel_items_count; j++){
		printf("\t\t"); print_rule(lr_states[i]->nonkernel_items[j]->rule);
		if(lr_states[i]->nonkernel_items[j]->lookahead != NULL){
			printf("\t\t\tLOOKAHEAD SYMBOLS: ");
			for(int l = 0; l < lr_states[i]->nonkernel_items[j]->lookahead->terminals_count; l++)
				if(lr_states[i]->nonkernel_items[j]->lookahead->terminals[l] < 0){
					printf("%i ", lr_states[i]->nonkernel_items[j]->lookahead->terminals[l]);	
				} else {
					printf("%s ", terminals[lr_states[i]->nonkernel_items[j]->lookahead->terminals[l]]->lexeme);
				}
			printf("\n");
		}	
	}
}

void print_lr_automat_items(){
	for(int i = 0; i < lr_states_counter; i++)
		print_lr_state_items(i);
}

void print_lr_state(int i){
	printf("STATE %i\n", i);
	printf("\tTERMINAL TRANSITIONS %i:\n", lr_states[i]->terminals_count);	
	
	for(int j = 0; j < lr_states[i]->terminals_count; j++)
		printf("\t\t\"%s\"  ->  %i\n", terminals[lr_states[i]->terminals[j]->token]->lexeme, lr_states[i]->terminals[j]->state);	

	printf("\tNONTERMINAL TRANSITIONS %i:\n", lr_states[i]->nonterminals_count);	
	
	for(int j = 0; j < lr_states[i]->nonterminals_count; j++)
		printf("\t\t\"%s\"  ->  %i\n", nonterminals[lr_states[i]->nonterminals[j]->token]->lexeme, lr_states[i]->nonterminals[j]->state);		
}

void print_lr_automat(){
	for(int i = 0; i < lr_states_counter; i++)
		print_lr_state(i);
}










