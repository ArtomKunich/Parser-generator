#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "datastructs.h"
#include "lexer.h"
#include "parser.h"
#define MAX_STATE_STACK_SIZE 256
#define HASHTABLE_BASIS 17

/*------------Additional declaration----------*/

int sp;
Rule* expanded_rule;

/*------------Additional declaration ending----------*/

static int max_state_stack_size;
static int* state_stack;
static void** attrs_stack;
static int stack_pointer;
static Token* lookahead;
FILE* in_fp;
static HashtableElement** token_ids;

static void throw_exception(char*, ...);

/*------------Additional code----------*/

Rule* new_rule(int body_length){
	Rule* new_rule = malloc(sizeof(Rule));
	new_rule->body_length = body_length;
	new_rule->body = malloc(sizeof(Token*) * body_length);
	new_rule->action = NULL;
	return new_rule;
}
void delete_rule(Rule* r){
	free(r->body);
	free(r);
}
void add_token_2_rule(Rule* r, Token* t){
	r->body_length++;
	Token** body = r->body;

	r->body = malloc(sizeof(Token*) * r->body_length);
	for(int i = 0; i < r->body_length-1; i++) r->body[i] = body[i];

	r->body[r->body_length-1] = t;
}
void add_rule_2_rule(Rule* r1, Rule* r2){
	int old_length = r1->body_length;
	int new_length = r1->body_length + r2->body_length;
	Token** body = r1->body;

	r1->body = malloc(sizeof(Token*) * new_length);
	r1->body_length = new_length ;

	for(int i = 0; i < old_length; i++) r1->body[i] = body[i];
	for(int i = 0; i < r2->body_length; i++) r1->body[old_length + i] = r2->body[i];
	
	free(body);
}

static int rules_counter_temp = 1;
static int rules_max_count = RULES_MAX_COUNT;
void add_rule(Rule* new_rule){
	if(rules == NULL){ 
		rules = malloc(sizeof(Rule*) * rules_max_count);
		for(int i = 0; i < rules_max_count; i++) rules[i] = NULL;
		rules_counter = 1; 
	}
	if(rules_counter >= rules_max_count){
		rules_max_count *= 2;
		Rule** new_rules = malloc(sizeof(Rule*) * rules_max_count);
		for(int i = 0; i < rules_counter; i++) new_rules[i] = rules[i];
		for(int i = rules_counter; i < rules_max_count; i++) new_rules[i] = NULL;
		free(rules);
		rules = new_rules;
	}
	rules[rules_counter++] = new_rule;
}

void preparse(){
	Token* t;
	t = get_next_token();
	if(strcmp(t->token_id, "DECLARATION") == 0){
		declaration = t->lexeme;
		t = get_next_token();
		if(strcmp(t->token_id, "EOL") == 0) t = get_next_token();
		if(strcmp(t->token_id, "_EOSC_") == 0) return; else throw_exception("Expected token \"_EOSC_\", actual token is \"%s\"", t->token_id);
	}
	if(strcmp(t->token_id, "EOL") == 0) t = get_next_token();
	if(strcmp(t->token_id, "_EOSC_") == 0) return; else throw_exception("Expected token \"_EOSC_\", actual token is \"%s\"", t->token_id);
}
/********************** Rules printing *****************************/

void print_rule(int i){
	printf("<%s>",rules[i]->head->lexeme);
	printf(" ::=");
	for(int j = 0; j < rules[i]->body_length; j++){
		if(strcmp(rules[i]->body[j]->token_id, "TERMINAL") == 0)
			printf(" \"%s\"", rules[i]->body[j]->lexeme);
		else if(strcmp(rules[i]->body[j]->token_id, "EMPTY") == 0)
			printf(" empty");
		else if(strcmp(rules[i]->body[j]->token_id, "UNION") == 0)
			printf(" |");
		else if(strcmp(rules[i]->body[j]->token_id, "NONTERMINAL") == 0)
			printf(" <%s>", rules[i]->body[j]->lexeme);
		else printf(" <%s>", rules[i]->body[j]->token_id);
	}
	if(rules[i]->action) printf(" {%s}", rules[i]->action);
	printf("\n");
}

void print_rules(){
	printf("\n");
	for(int i = 1; i < rules_counter; i++){
		print_rule(i);
	}
}


/*------------Additional code ending----------*/

void push_state(int, void*);
static void throw_exception(char* msg, ...){
	va_list ptr;
	va_start(ptr, msg);
	printf("Parser error on line %i. ", line);
	vprintf(msg, ptr);
	va_end(ptr);
	getchar();
	exit(-1);
}

void parser_initialize(FILE* fp){
	lexer_initialize(fp);
	max_state_stack_size = MAX_STATE_STACK_SIZE;
	state_stack = malloc(sizeof(int) * max_state_stack_size);
	attrs_stack = malloc(sizeof(void*) * max_state_stack_size);
	stack_pointer = -1;
	in_fp = fp;
	token_ids = hashtableCreate(HASHTABLE_BASIS);
	push_state(0, NULL);
	hashtableAdd(token_ids, "_EOSC_", (void*)0);
	hashtableAdd(token_ids, "EOL", (void*)1);
	hashtableAdd(token_ids, "DECLARATION", (void*)2);
	hashtableAdd(token_ids, "NONTERMINAL", (void*)3);
	hashtableAdd(token_ids, "::=", (void*)4);
	hashtableAdd(token_ids, "EMPTY", (void*)5);
	hashtableAdd(token_ids, "UNION", (void*)6);
	hashtableAdd(token_ids, "O_BRACKET", (void*)7);
	hashtableAdd(token_ids, "C_BRACKET", (void*)8);
	hashtableAdd(token_ids, "SEMANTIC_ACTION_TEXT", (void*)9);
	hashtableAdd(token_ids, "TERMINAL", (void*)10);
}

void resize_stack(){
	max_state_stack_size += MAX_STATE_STACK_SIZE;
	int* new_state_stack = malloc(sizeof(int) * max_state_stack_size);
	void** new_attrs_stack = malloc(sizeof(void*) * max_state_stack_size);
	for(int i = 0; i < stack_pointer; i++){
		new_state_stack[i] = state_stack[i];
		new_attrs_stack[i] = attrs_stack[i];
	}
	free(state_stack);
	free(attrs_stack);
	state_stack = new_state_stack;
	attrs_stack = new_attrs_stack;
}

void push_state(int state, void* attrs){
	if(stack_pointer + 1 > max_state_stack_size){
		resize_stack();
	}
	state_stack[++stack_pointer] = state;
	attrs_stack[stack_pointer] = attrs;
}

int pop_state(int count){
	stack_pointer -= (count-1);
	return state_stack[stack_pointer--];
}

int do_goto(int nonterminal){

	int state = state_stack[stack_pointer];

	printf("do_goto state: %i nonterminal: %i\n", state, nonterminal);

	if(state == 0){
		switch(nonterminal){
			case 1: return 4;
			case 2: return 5;
		}
	}
	if(state == 5){
		switch(nonterminal){
			case 3: return 11;
			case 4: return 12;
		}
	}
	if(state == 12){
		switch(nonterminal){
			case 3: return 17;
			case 4: return 12;
			case 5: return 18;
		}
	}
	if(state == 16){
		switch(nonterminal){
			case 6: return 24;
			case 7: return 25;
			case 12: return 26;
		}
	}
	if(state == 22){
		switch(nonterminal){
			case 8: return 29;
		}
	}
	if(state == 25){
		switch(nonterminal){
			case 8: return 31;
		}
	}
	if(state == 26){
		switch(nonterminal){
			case 7: return 32;
			case 12: return 26;
			case 13: return 33;
		}
	}
	if(state == 28){
		switch(nonterminal){
			case 14: return 35;
			case 15: return 36;
		}
	}
	if(state == 29){
		switch(nonterminal){
			case 10: return 38;
		}
	}
	if(state == 31){
		switch(nonterminal){
			case 9: return 40;
		}
	}
	if(state == 37){
		switch(nonterminal){
			case 7: return 43;
			case 11: return 44;
			case 12: return 26;
		}
	}
	if(state == 39){
		switch(nonterminal){
			case 6: return 45;
			case 7: return 25;
			case 12: return 26;
		}
	}
	if(state == 42){
		switch(nonterminal){
			case 14: return 46;
			case 15: return 36;
		}
	}
	if(state == 43){
		switch(nonterminal){
			case 8: return 47;
		}
	}
	if(state == 47){
		switch(nonterminal){
			case 10: return 49;
		}
	}
	if(state == 48){
		switch(nonterminal){
			case 14: return 50;
			case 15: return 36;
		}
	}
	throw_exception("There is no transfers for state %i , and nonterminal %s", state_stack[stack_pointer], lookahead->token_id);
	return -1;
}

int do_action(){

	int state = state_stack[stack_pointer];
	int token_id = (int)hashtableGetValue(token_ids, lookahead->token_id);
	void* new_attr = NULL;
	if(state == 4 && token_id == 0) return -1;

	printf("do_action state: %i terminal: %i\n", state, token_id);

	if(state == 0){
		switch(token_id){
			case 0: push_state(1, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 1: push_state(2, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 2: push_state(3, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 3:
				push_state(do_goto(2), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 1){
		switch(token_id){
			case 1: push_state(6, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
		}
	}
	if(state == 2){
		switch(token_id){
			case 0: push_state(7, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 2: push_state(8, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
		}
	}
	if(state == 3){
		switch(token_id){
			case 1: push_state(9, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
		}
	}
	if(state == 4){
		switch(token_id){
			case 0:
				pop_state(1); push_state(do_goto(0), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 5){
		switch(token_id){
			case 3: push_state(10, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
		}
	}
	if(state == 6){
		switch(token_id){
			case 3:
				pop_state(2); push_state(do_goto(2), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 7){
		switch(token_id){
			case 1: push_state(13, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
		}
	}
	if(state == 8){
		switch(token_id){
			case 1: push_state(14, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
		}
	}
	if(state == 9){
		switch(token_id){
			case 0: push_state(15, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
		}
	}
	if(state == 10){
		switch(token_id){
			case 4: push_state(16, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
		}
	}
	if(state == 11){
		switch(token_id){
			case 0:
				expanded_rule = new_rule(1); expanded_rule->head = nonterminals[0]; expanded_rule->body[0] = rules[1]->head; rules[0] = expanded_rule;
				pop_state(2); push_state(do_goto(1), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 12){
		switch(token_id){
			case 3: push_state(10, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 0:
				push_state(do_goto(5), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 13){
		switch(token_id){
			case 3:
				pop_state(3); push_state(do_goto(2), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 14){
		switch(token_id){
			case 0: push_state(19, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
		}
	}
	if(state == 15){
		switch(token_id){
			case 1: push_state(20, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
		}
	}
	if(state == 16){
		switch(token_id){
			case 3: push_state(21, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 5: push_state(22, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 10: push_state(23, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
		}
	}
	if(state == 17){
		switch(token_id){
			case 0:
				new_attr = attrs_stack[stack_pointer - 1 + 1];
				pop_state(1); push_state(do_goto(5), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 18){
		switch(token_id){
			case 0:
				pop_state(2); push_state(do_goto(3), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 19){
		switch(token_id){
			case 1: push_state(27, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
		}
	}
	if(state == 20){
		switch(token_id){
			case 3:
				declaration = ((Token*)attrs_stack[stack_pointer - 4 + 1])->lexeme;
				pop_state(4); push_state(do_goto(2), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 21){
		switch(token_id){
			case 1:
				new_attr = new_rule(1); ((Rule*)new_attr)->body[0] = attrs_stack[stack_pointer - 1 + 1];
				pop_state(1); push_state(do_goto(12), new_attr); return state_stack[stack_pointer];
			case 3:
				new_attr = new_rule(1); ((Rule*)new_attr)->body[0] = attrs_stack[stack_pointer - 1 + 1];
				pop_state(1); push_state(do_goto(12), new_attr); return state_stack[stack_pointer];
			case 6:
				new_attr = new_rule(1); ((Rule*)new_attr)->body[0] = attrs_stack[stack_pointer - 1 + 1];
				pop_state(1); push_state(do_goto(12), new_attr); return state_stack[stack_pointer];
			case 7:
				new_attr = new_rule(1); ((Rule*)new_attr)->body[0] = attrs_stack[stack_pointer - 1 + 1];
				pop_state(1); push_state(do_goto(12), new_attr); return state_stack[stack_pointer];
			case 10:
				new_attr = new_rule(1); ((Rule*)new_attr)->body[0] = attrs_stack[stack_pointer - 1 + 1];
				pop_state(1); push_state(do_goto(12), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 22){
		switch(token_id){
			case 7: push_state(28, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 1:
				push_state(do_goto(8), new_attr); return state_stack[stack_pointer];
			case 6:
				push_state(do_goto(8), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 23){
		switch(token_id){
			case 1:
				new_attr = new_rule(1); ((Rule*)new_attr)->body[0] = attrs_stack[stack_pointer - 1 + 1];
				pop_state(1); push_state(do_goto(12), new_attr); return state_stack[stack_pointer];
			case 3:
				new_attr = new_rule(1); ((Rule*)new_attr)->body[0] = attrs_stack[stack_pointer - 1 + 1];
				pop_state(1); push_state(do_goto(12), new_attr); return state_stack[stack_pointer];
			case 6:
				new_attr = new_rule(1); ((Rule*)new_attr)->body[0] = attrs_stack[stack_pointer - 1 + 1];
				pop_state(1); push_state(do_goto(12), new_attr); return state_stack[stack_pointer];
			case 7:
				new_attr = new_rule(1); ((Rule*)new_attr)->body[0] = attrs_stack[stack_pointer - 1 + 1];
				pop_state(1); push_state(do_goto(12), new_attr); return state_stack[stack_pointer];
			case 10:
				new_attr = new_rule(1); ((Rule*)new_attr)->body[0] = attrs_stack[stack_pointer - 1 + 1];
				pop_state(1); push_state(do_goto(12), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 24){
		switch(token_id){
			case 1: push_state(30, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
		}
	}
	if(state == 25){
		switch(token_id){
			case 7: push_state(28, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 1:
				push_state(do_goto(8), new_attr); return state_stack[stack_pointer];
			case 6:
				push_state(do_goto(8), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 26){
		switch(token_id){
			case 3: push_state(21, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 10: push_state(23, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 1:
				new_attr = new_rule(0);
				push_state(do_goto(13), new_attr); return state_stack[stack_pointer];
			case 6:
				new_attr = new_rule(0);
				push_state(do_goto(13), new_attr); return state_stack[stack_pointer];
			case 7:
				new_attr = new_rule(0);
				push_state(do_goto(13), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 27){
		switch(token_id){
			case 3:
				declaration = ((Token*)attrs_stack[stack_pointer - 5 + 2])->lexeme;
				pop_state(5); push_state(do_goto(2), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 28){
		switch(token_id){
			case 9: push_state(34, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 8:
				new_attr = "";
				push_state(do_goto(14), new_attr); return state_stack[stack_pointer];
			case 7:
				new_attr = "";
				push_state(do_goto(15), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 29){
		switch(token_id){
			case 6: push_state(37, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 1:
				new_attr = new_rule(0);
				push_state(do_goto(10), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 30){
		switch(token_id){
			case 0:
				for(int i = rules_counter_temp; i < rules_counter; i++){rules[i]->head = attrs_stack[stack_pointer - 4 + 1];}rules_counter_temp = rules_counter;
				pop_state(4); push_state(do_goto(4), new_attr); return state_stack[stack_pointer];
			case 3:
				for(int i = rules_counter_temp; i < rules_counter; i++){rules[i]->head = attrs_stack[stack_pointer - 4 + 1];}rules_counter_temp = rules_counter;
				pop_state(4); push_state(do_goto(4), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 31){
		switch(token_id){
			case 6: push_state(39, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 1:
				new_attr = new_rule(0);
				push_state(do_goto(9), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 32){
		switch(token_id){
			case 1:
				new_attr = attrs_stack[stack_pointer - 1 + 1];
				pop_state(1); push_state(do_goto(13), new_attr); return state_stack[stack_pointer];
			case 6:
				new_attr = attrs_stack[stack_pointer - 1 + 1];
				pop_state(1); push_state(do_goto(13), new_attr); return state_stack[stack_pointer];
			case 7:
				new_attr = attrs_stack[stack_pointer - 1 + 1];
				pop_state(1); push_state(do_goto(13), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 33){
		switch(token_id){
			case 1:
				new_attr = new_rule(0); add_rule_2_rule(new_attr, attrs_stack[stack_pointer - 2 + 1]); add_rule_2_rule(new_attr, attrs_stack[stack_pointer - 2 + 2]); delete_rule(attrs_stack[stack_pointer - 2 + 1]); delete_rule(attrs_stack[stack_pointer - 2 + 2]);
				pop_state(2); push_state(do_goto(7), new_attr); return state_stack[stack_pointer];
			case 6:
				new_attr = new_rule(0); add_rule_2_rule(new_attr, attrs_stack[stack_pointer - 2 + 1]); add_rule_2_rule(new_attr, attrs_stack[stack_pointer - 2 + 2]); delete_rule(attrs_stack[stack_pointer - 2 + 1]); delete_rule(attrs_stack[stack_pointer - 2 + 2]);
				pop_state(2); push_state(do_goto(7), new_attr); return state_stack[stack_pointer];
			case 7:
				new_attr = new_rule(0); add_rule_2_rule(new_attr, attrs_stack[stack_pointer - 2 + 1]); add_rule_2_rule(new_attr, attrs_stack[stack_pointer - 2 + 2]); delete_rule(attrs_stack[stack_pointer - 2 + 1]); delete_rule(attrs_stack[stack_pointer - 2 + 2]);
				pop_state(2); push_state(do_goto(7), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 34){
		switch(token_id){
			case 8:
				new_attr = ((Token*)attrs_stack[stack_pointer - 1 + 1])->lexeme;
				pop_state(1); push_state(do_goto(14), new_attr); return state_stack[stack_pointer];
			case 7:
				new_attr = ((Token*)attrs_stack[stack_pointer - 1 + 1])->lexeme;
				pop_state(1); push_state(do_goto(15), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 35){
		switch(token_id){
			case 8: push_state(41, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
		}
	}
	if(state == 36){
		switch(token_id){
			case 7: push_state(42, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
		}
	}
	if(state == 37){
		switch(token_id){
			case 3: push_state(21, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 10: push_state(23, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
		}
	}
	if(state == 38){
		switch(token_id){
			case 1:
				new_attr = new_rule(1); ((Rule*)new_attr)->body[0] = attrs_stack[stack_pointer - 3 + 1]; ((Rule*)new_attr)->action = attrs_stack[stack_pointer - 3 + 2]; add_rule(new_attr);
				pop_state(3); push_state(do_goto(6), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 39){
		switch(token_id){
			case 3: push_state(21, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 5: push_state(22, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 10: push_state(23, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
		}
	}
	if(state == 40){
		switch(token_id){
			case 1:
				new_attr = attrs_stack[stack_pointer - 3 + 1]; ((Rule*)attrs_stack[stack_pointer - 3 + 1])->action = attrs_stack[stack_pointer - 3 + 2]; add_rule(attrs_stack[stack_pointer - 3 + 1]);
				pop_state(3); push_state(do_goto(6), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 41){
		switch(token_id){
			case 1:
				new_attr = attrs_stack[stack_pointer - 3 + 2];
				pop_state(3); push_state(do_goto(8), new_attr); return state_stack[stack_pointer];
			case 6:
				new_attr = attrs_stack[stack_pointer - 3 + 2];
				pop_state(3); push_state(do_goto(8), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 42){
		switch(token_id){
			case 9: push_state(34, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 8:
				new_attr = "";
				push_state(do_goto(14), new_attr); return state_stack[stack_pointer];
			case 7:
				new_attr = "";
				push_state(do_goto(15), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 43){
		switch(token_id){
			case 7: push_state(28, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 1:
				push_state(do_goto(8), new_attr); return state_stack[stack_pointer];
			case 6:
				push_state(do_goto(8), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 44){
		switch(token_id){
			case 1:
				pop_state(2); push_state(do_goto(10), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 45){
		switch(token_id){
			case 1:
				pop_state(2); push_state(do_goto(9), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 46){
		switch(token_id){
			case 8: push_state(48, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
		}
	}
	if(state == 47){
		switch(token_id){
			case 6: push_state(37, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 1:
				new_attr = new_rule(0);
				push_state(do_goto(10), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 48){
		switch(token_id){
			case 9: push_state(34, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];
			case 8:
				new_attr = "";
				push_state(do_goto(14), new_attr); return state_stack[stack_pointer];
			case 7:
				new_attr = "";
				push_state(do_goto(15), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 49){
		switch(token_id){
			case 1:
				new_attr = attrs_stack[stack_pointer - 3 + 1]; ((Rule*)attrs_stack[stack_pointer - 3 + 1])->action = attrs_stack[stack_pointer - 3 + 2]; add_rule(attrs_stack[stack_pointer - 3 + 1]); 
				pop_state(3); push_state(do_goto(11), new_attr); return state_stack[stack_pointer];
		}
	}
	if(state == 50){
		switch(token_id){
			case 8:
				sp = 0; new_attr = malloc(sizeof(char) * (strlen(attrs_stack[stack_pointer - 5 + 1]) + strlen(attrs_stack[stack_pointer - 5 + 3]) + strlen(attrs_stack[stack_pointer - 5 + 5]) + 3)); strcpy(new_attr + sp, attrs_stack[stack_pointer - 5 + 1]); sp += strlen(attrs_stack[stack_pointer - 5 + 1]); strcpy(new_attr + sp, "{"); sp++; strcpy(new_attr + sp, attrs_stack[stack_pointer - 5 + 3]); sp += strlen(attrs_stack[stack_pointer - 5 + 3]); strcpy(new_attr + sp, "}"); sp++; strcpy(new_attr + sp, attrs_stack[stack_pointer - 5 + 5]); sp += strlen(attrs_stack[stack_pointer - 5 + 5]); ((char*)new_attr)[sp] = '\0';
				pop_state(5); push_state(do_goto(14), new_attr); return state_stack[stack_pointer];
		}
	}
	throw_exception("There is no transfers for state %i , and terminal %s", state_stack[stack_pointer], lookahead->token_id);
	return state_stack[stack_pointer];
}
int parse(){
	lookahead = get_next_token();
	while(do_action() > -1);
	printf("Parse OK");
	return 0;
}
