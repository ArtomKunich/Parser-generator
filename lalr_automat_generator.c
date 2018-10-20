#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "datastructs.h"
#include "lexer.h"
#include "parser.h"
#include "slr_automat_generator.h"

char* terminals_register;
char* kernel_items_register;
char empty_register = 0;


static void throw_exception(char* msg, ...){
	va_list ptr;
	va_start(ptr, msg);
	printf("LALR automat generator error: ");
	vprintf(msg, ptr);
	va_end(ptr);
	getchar();
	exit(-1);
}

void terminals_register_reset(){for(int i = 0; i < terminals_counter; i++) terminals_register[i] = 0;}
int terminals_register_sum(){
	int sum = 0;
	for(int i = 0; i < terminals_counter; i++) sum += terminals_register[i];
	return sum;
}

void kernel_items_register_reset(){for(int i = 0; i < max_kernel_items_count; i++) kernel_items_register[i] = 0;}
int kernel_items_register_sum(){
	int sum = 0;
	for(int i = 0; i < max_kernel_items_count; i++) sum += kernel_items_register[i];
	return sum;
}

void write_lookahead_2_registers(Lookahead* lookahead){
	if(lookahead == NULL) return;
	for(int i = 0; i < lookahead->terminals_count; i++){
		if(lookahead->terminals[i] > -1) terminals_register[lookahead->terminals[i]] = 1;
		else kernel_items_register[(-1) * lookahead->terminals[i] - 1] = 1;
	}	
}

void all_registers_reset(){
	terminals_register_reset();
	kernel_items_register_reset();	
	empty_register = 0;
}

/*
int get_terminal_num(Token* token){
	if(token->tag == TERMINAL){
		for(int i = 0; i < terminals_counter; i++) if(terminals[i] == token) return i;
	} else if (token->tag == NONTERMINAL){
		for(int i = 0; i < nonterminals_counter; i++) if(nonterminals[i] == token) return i;
	}
	return -1;
}*/

int first_4_token_terminal(Token* token){
	if(token->tag == EMPTY) return 1;
	if(token->tag == TERMINAL){
		terminals_register[token->offset] = 1;
		return 2;
	}
	return 0;
}
int first_4_token_nonterminal(Token* token){	

	int local_empty_register = 0;
	int result;
	
	if(token->tag == NONTERMINAL){
		for(int i = 0; i < rules_counter; i++){
			if(rules[i]->head == token) result = first_4_token_terminal(rules[i]->body[0]);
			if(result == 1) local_empty_register = 1;
		}		
		
		for(int i = 0; i < rules_counter; i++){
			if(rules[i]->head == token){				
				for(int j = 0; j < rules[i]->body_length; j++){
					if(rules[i]->body[j] == token) {if(local_empty_register) continue; else break;}
					
					if(j != 0){ 
						result = first_4_token_terminal(rules[i]->body[j]);						
						if(result == 2) break;
						else if(result == 1) continue;
						else if(result == 0) result = first_4_token_nonterminal(rules[i]->body[j]);
					} else {
						result = first_4_token_nonterminal(rules[i]->body[j]);
						if(result == 0) break; 
					}
					if(result == 2) break;
					else if(result == 1) continue;
					else if(result == 0) throw_exception("unknown token");					
				}
				if(result == 1) local_empty_register = 1;
			}
		}
		if(local_empty_register) return 1; else return 2;
	}
	return 0;
}

void first_4_token(Token* token){
	
	int result = first_4_token_terminal(token);
	
	if(result){
		if(result == 1) empty_register = 1;
	} else {
		for(int i = 0; i < rules_counter; i++){
			if(rules[i]->head == token) result = first_4_token_terminal(rules[i]->body[0]);
			if(result == 1) empty_register = 1;
		}			
		
		for(int i = 0; i < rules_counter; i++){
			if(rules[i]->head == token){				
				for(int j = 0; j < rules[i]->body_length; j++){
					if(rules[i]->body[j] == token) {if(empty_register) { continue;} else break;}
					
					if(j != 0){ 
						result = first_4_token_terminal(rules[i]->body[j]);
						if(result == 2) break;
						else if(result == 1) continue;
						else if(result == 0) result = first_4_token_nonterminal(rules[i]->body[j]);
					} else {
						result = first_4_token_nonterminal(rules[i]->body[j]);
						if(result == 0) { break;}
					}
					if(result == 2) break;
					else if(result == 1) continue;
					else if(result == 0) throw_exception("unknown token");	
				}
				if(result == 1) empty_register = 1;
			}
		}
	}
}

/*
void first_4_token(Token* token){
	int result;
	local_empty_register = 0;
	if(token->tag == TERMINAL){
		result = token->offset;
		terminals_register[result] = 1;
		return;
	}
	if(token->tag == EMPTY){
		empty_register = 1;
		return;
	}	
	if(token->tag == NONTERMINAL){
		printf("here\n");
		for(int i = 0; i < rules_counter; i++){
			if(rules[i]->head == token){
				for(int j = 0; j < rules[i]->body_length; j++){
					first_4_token(rules[i]->body[j]); ////////////////////////
					if(local_empty_register && j < (rules[j]->body_length - 1)) local_empty_register = 0; else break;
				}
				if(local_empty_register){
					local_empty_register = 0;
					empty_register = 1;
				}
			}
		}
		return;
	}
}
*/
void first_4_item(Item* item){
	if((rules[item->rule]->body_length - item->position) <= 1){
		empty_register = 1;
		return;
	}
	for(int i = 1; i < (rules[item->rule]->body_length - item->position); i++){
		first_4_token((rules[item->rule]->body)[item->position + i]);
		if(empty_register && i < (rules[item->rule]->body_length - item->position - 1)){
			empty_register = 0;
		} else {
			break;
		}
	}
}

/**************************lookahead operations*********************************/

Lookahead* new_lookahead(int* terminals, int terminals_counter){
	Lookahead* lookahead = malloc(sizeof(Lookahead));
	lookahead->terminals = terminals;
	lookahead->terminals_count = terminals_counter;
	return lookahead;
}
void delete_lookahead(Lookahead* lookahead){
	if(lookahead == NULL) return;
	free(lookahead->terminals);
	free(lookahead);
}

int compare_lookaheads(Lookahead* lookahead1, Lookahead* lookahead2){
	if(lookahead1 == NULL || lookahead2 == NULL) return 0;
	if(lookahead1->terminals_count == lookahead2->terminals_count){
		for(int i = 0; i < lookahead1->terminals_count; i++) if(lookahead1->terminals[i] != lookahead2->terminals[i]) return 0;
		return 1;
	}
	return 0;
}

Lookahead* build_new_lookahead(){	
	int loc_terminals_counter = 0;
	int terminals_count = terminals_register_sum();
	int kernel_items_count = kernel_items_register_sum();
	
	int* loc_terminals = malloc(sizeof(int) * (terminals_count + kernel_items_count));
	
	for(int i = 0; i < terminals_counter; i++) if(terminals_register[i]) loc_terminals[loc_terminals_counter++] = i;	
	for(int i = 0; i < max_kernel_items_count; i++) if(kernel_items_register[i]) loc_terminals[loc_terminals_counter++] = (-1) * (i + 1);	
	
	return new_lookahead(loc_terminals, terminals_count + kernel_items_count);
}

/*****************************set lookaheads 2 nonkernel********************************/

void set_lookahead_nonkernel(LR_state* lr_state, int nonkernel_item, Lookahead* lookahead){
	Rule* rule = NULL;
	Lookahead* child_lookahead;
	Lookahead* new_lookahead;
	
	//print_rule(lr_state->nonkernel_items[nonkernel_item]->rule); printf("position: %i\n", lr_state->nonkernel_items[nonkernel_item]->position);
	/*for(int i = 0; i < lookahead->terminals_count; i++){
		if(lookahead->terminals[i] > -1) printf("%s ", terminals[lookahead->terminals[i]]->lexeme);
	}
	printf("\n");	
if(lr_state->nonkernel_items[nonkernel_item]->lookahead != NULL){
	for(int i = 0; i < lr_state->nonkernel_items[nonkernel_item]->lookahead->terminals_count; i++){
		
			if(lr_state->nonkernel_items[nonkernel_item]->lookahead->terminals[i] > -1) printf("%s ", terminals[lr_state->nonkernel_items[nonkernel_item]->lookahead->terminals[i]]->lexeme);

	}
	printf("\n");
}	else { printf("NULL\n");}*/
	
	write_lookahead_2_registers(lr_state->nonkernel_items[nonkernel_item]->lookahead);
	write_lookahead_2_registers(lookahead);
	new_lookahead = build_new_lookahead();
	
	if(compare_lookaheads(lr_state->nonkernel_items[nonkernel_item]->lookahead, new_lookahead)){
		delete_lookahead(new_lookahead);
		return;
	}
	
	lr_state->nonkernel_items[nonkernel_item]->lookahead = new_lookahead;
	all_registers_reset();
	rule = rules[lr_state->nonkernel_items[nonkernel_item]->rule];

	if(lr_state->nonkernel_items[nonkernel_item]->position < rule->body_length){	
		
		first_4_item(lr_state->nonkernel_items[nonkernel_item]);
		if(empty_register) write_lookahead_2_registers(new_lookahead);
		child_lookahead = build_new_lookahead();	
		all_registers_reset();	

		for(int i = 0; i < lr_state->nonkernel_items_count; i++ ) if(rules[lr_state->nonkernel_items[i]->rule]->head == rule->body[0]) set_lookahead_nonkernel(lr_state, i, child_lookahead);
		delete_lookahead(child_lookahead);
	}
}

void set_temporary_lookahead_kernel(LR_state* lr_state, int kernel_item, Lookahead* lookahead){
	Rule* rule = NULL;
	int position = -1;
	Lookahead* child_lookahead;
	
	lr_state->kernel_items[kernel_item]->lookahead = lookahead;
	rule = rules[lr_state->kernel_items[kernel_item]->rule];
	position = lr_state->kernel_items[kernel_item]->position;
	
	if(position < rule->body_length){
		first_4_item(lr_state->kernel_items[kernel_item]);		
		if(empty_register) write_lookahead_2_registers(lookahead);
		child_lookahead = build_new_lookahead();
		all_registers_reset();				
		for(int i = 0; i < lr_state->nonkernel_items_count; i++ ) if(rules[lr_state->nonkernel_items[i]->rule]->head == rule->body[position]) set_lookahead_nonkernel(lr_state, i, child_lookahead);	
		delete_lookahead(child_lookahead);
	}
}

void set_lookaheads_2_nonkernel(){
	terminals_register = malloc(sizeof(char) * terminals_counter);
	terminals_register_reset();
	kernel_items_register = malloc(sizeof(char) * max_kernel_items_count);
	kernel_items_register_reset();
	
	int counter = -1;
	int* temp_symbol = NULL;

	for(int i = 0; i < lr_states_counter; i++){
		for(int j = 0; j < lr_states[i]->kernel_items_count; j++){
			temp_symbol = malloc(sizeof(int));
			temp_symbol[0] = counter--;
			set_temporary_lookahead_kernel( lr_states[i], j, new_lookahead(temp_symbol, 1));
		}
		counter = -1;
	}
}

/*****************************set_lookaheads_2_kernel********************************/

int kernel_lookahead_table_size;
int kernel_lookahead_table_register_sum;

Lookahead** kernel_lookahead_table;
char* kernel_lookahead_table_register;

char get_kernel_lookahead_table_register(int lr_state, int kernel_item){
	return *(kernel_lookahead_table_register + lr_state * max_kernel_items_count + kernel_item);
}
void set_kernel_lookahead_table_register(int lr_state, int kernel_item, char value){
	if(get_kernel_lookahead_table_register(lr_state, kernel_item)){
		if(value){
			return;
		} else { 
			*(kernel_lookahead_table_register + lr_state * max_kernel_items_count + kernel_item) = 0;
			kernel_lookahead_table_register_sum --;
		}
	} else {
		if(value){
			*(kernel_lookahead_table_register + lr_state * max_kernel_items_count + kernel_item) = 1;
			kernel_lookahead_table_register_sum++;			
		} else { 
			return;
		}		
	}
}

void set_kernel_lookahead_table(int lr_state, int kernel_item, Lookahead* lookahead){
	delete_lookahead(*(kernel_lookahead_table + lr_state * max_kernel_items_count + kernel_item));
	*(kernel_lookahead_table + lr_state * max_kernel_items_count + kernel_item) = lookahead;
}
Lookahead* get_kernel_lookahead_table(int lr_state, int kernel_item){
	return *(kernel_lookahead_table + lr_state * max_kernel_items_count + kernel_item);
}


int update_lookahead_of_kernel_item(int lr_state, int kernel_item, Lookahead* lookahead){
	Lookahead* current_lookahead = get_kernel_lookahead_table(lr_state, kernel_item);
	write_lookahead_2_registers(current_lookahead);
	write_lookahead_2_registers(lookahead);
	kernel_items_register_reset();
	Lookahead* new_lookahead = build_new_lookahead();
	terminals_register_reset();				

	if(compare_lookaheads(current_lookahead, new_lookahead)){
		delete_lookahead(new_lookahead);
		set_kernel_lookahead_table_register(lr_state, kernel_item, 1);
		return 0;
	}
	set_kernel_lookahead_table(lr_state, kernel_item, new_lookahead);
	set_kernel_lookahead_table_register(lr_state, kernel_item, 0);
	return 1;
}

Item* search_item(int lr_state, Rule* rule, int position){
	for(int i = 0; i < lr_states[lr_state]->kernel_items_count; i++){
		if(rules[lr_states[lr_state]->kernel_items[i]->rule] == rule && lr_states[lr_state]->kernel_items[i]->position == position){
			return lr_states[lr_state]->kernel_items[i];
		}
	}
	for(int i = 0; i < lr_states[lr_state]->nonkernel_items_count; i++){
		if(rules[lr_states[lr_state]->nonkernel_items[i]->rule] == rule && lr_states[lr_state]->nonkernel_items[i]->position == position){
			return lr_states[lr_state]->nonkernel_items[i];
		}
	}		
	return NULL;
}

Lookahead* get_updated_lookahead(int lr_state, Lookahead* lookahead){
	Lookahead* new_lookahead;
	write_lookahead_2_registers(lookahead);
	for(int i = 0; i < max_kernel_items_count; i++) if(kernel_items_register[i]) write_lookahead_2_registers(get_kernel_lookahead_table(lr_state,i));
	kernel_items_register_reset();
	new_lookahead = build_new_lookahead();
	terminals_register_reset();
	return new_lookahead;
}

void update_lookahead_of_kernel_items_in_next_states(int lr_state){	
	if(kernel_lookahead_table_size == kernel_lookahead_table_register_sum) return;

	int next_state = -1;
	int position;
	int update_flag = 0;
	
	Token* next_symbol = NULL;
	Item* found_item;
	Lookahead* updated_lookahead;
	
	for(int i = 0; i < lr_states[lr_state]->terminals_count; i++){
		next_symbol = terminals[lr_states[lr_state]->terminals[i]->token];
		next_state = lr_states[lr_state]->terminals[i]->state;
		
		for(int j = 0; j < lr_states[next_state]->kernel_items_count; j++){
			position = lr_states[next_state]->kernel_items[j]->position - 1;
			if(rules[lr_states[next_state]->kernel_items[j]->rule]->body[position] == next_symbol){
				
				found_item = search_item(lr_state,rules[lr_states[next_state]->kernel_items[j]->rule], position);	
				updated_lookahead = get_updated_lookahead(lr_state,found_item->lookahead);
				update_flag += update_lookahead_of_kernel_item(next_state, j, updated_lookahead);				
				delete_lookahead(updated_lookahead);	
			}
		}	
	}

	for(int i = 0; i < lr_states[lr_state]->nonterminals_count; i++){	
		next_symbol = nonterminals[lr_states[lr_state]->nonterminals[i]->token];
		next_state = lr_states[lr_state]->nonterminals[i]->state;
		
		for(int j = 0; j < lr_states[next_state]->kernel_items_count; j++){
			position = lr_states[next_state]->kernel_items[j]->position - 1;
			if(rules[lr_states[next_state]->kernel_items[j]->rule]->body[position] == next_symbol){
				
				found_item = search_item(lr_state,rules[lr_states[next_state]->kernel_items[j]->rule], position);				
				updated_lookahead = get_updated_lookahead(lr_state,found_item->lookahead);
				update_flag += update_lookahead_of_kernel_item(next_state, j, updated_lookahead);
				delete_lookahead(updated_lookahead);			
			}
		}	
	}
	
	if(update_flag){
		for(int i = 0; i < lr_states[lr_state]->terminals_count; i++){	
			next_state = lr_states[lr_state]->terminals[i]->state;
			//printf("NEXT STATE %i\n", next_state); /////////////////////////////////////////////////////////////////	
			update_lookahead_of_kernel_items_in_next_states(next_state);		
		}	
		for(int i = 0; i < lr_states[lr_state]->nonterminals_count; i++){	
			next_state = lr_states[lr_state]->nonterminals[i]->state;
			update_lookahead_of_kernel_items_in_next_states(next_state);	
		}
	}	
}


void update_lookeahead_of_all_items(){
	LR_state* lr_state;
	Lookahead* new_lookahead;
	int lookahead_count;
	int* lookahead_symbol;
	
	for(int i = 0; i < lr_states_counter; i++){
		lr_state = lr_states[i];
		for(int j = 0; j < lr_state->kernel_items_count; j++){
			lookahead_symbol = lr_state->kernel_items[j]->lookahead->terminals;	
			lookahead_count = lr_state->kernel_items[j]->lookahead->terminals_count;	
			if(lookahead_symbol[lookahead_count-1] < 0){
				new_lookahead = get_updated_lookahead(i, lr_state->kernel_items[j]->lookahead);
				delete_lookahead(lr_state->kernel_items[j]->lookahead);
				lr_state->kernel_items[j]->lookahead = new_lookahead;
			}
		}
		
		for(int j = 0; j < lr_state->nonkernel_items_count; j++){
			lookahead_symbol = lr_state->nonkernel_items[j]->lookahead->terminals;	
			lookahead_count = lr_state->nonkernel_items[j]->lookahead->terminals_count;	
			if(lookahead_symbol[lookahead_count-1] < 0){
				new_lookahead = get_updated_lookahead(i, lr_state->nonkernel_items[j]->lookahead);
				delete_lookahead(lr_state->nonkernel_items[j]->lookahead);
				lr_state->nonkernel_items[j]->lookahead = new_lookahead;
			}
		}
		
	}
}

void set_lookaheads_2_kernel(){
	kernel_lookahead_table_size = lr_states_counter * max_kernel_items_count;
	kernel_lookahead_table = malloc(sizeof(Lookahead*) * kernel_lookahead_table_size);
	for(int i = 0; i < kernel_lookahead_table_size; i++) kernel_lookahead_table[i] = NULL;
	kernel_lookahead_table_register = malloc(sizeof(char) * kernel_lookahead_table_size);
	for(int i = 0; i < kernel_lookahead_table_size; i++) kernel_lookahead_table_register[i] = 0;
	
	terminals_register[0] = 1;
	Lookahead* first_lookahead = build_new_lookahead();
	update_lookahead_of_kernel_item(0, 0, first_lookahead);
	update_lookahead_of_kernel_items_in_next_states(0);	
	
	/*
	Lookahead* temp;
	for(int i = 0; i < lr_states_counter; i++){
		
		printf("STATE %i:\n", i);
		for(int j = 0; j < max_kernel_items_count; j++){
			temp = get_kernel_lookahead_table(i,j);
			if(temp != NULL){
				for(int k = 0; k < temp->terminals_count; k++){
					printf("\t%s\n",terminals[temp->terminals[k]]->lexeme);
				}
			}	
		}
	}
	*/	
	
	update_lookeahead_of_all_items();
	
	free(kernel_lookahead_table);
	free(kernel_lookahead_table_register);
	free(terminals_register);
	free(kernel_items_register);
}






