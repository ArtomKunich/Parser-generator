#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "datastructs.h"
#include "lexer.h"
#include "parser.h"

//HashtableElement** symbol_hashtable;

Rule* current_rule;
int body_length;

static void throw_exception(char* msg, ...){
	va_list ptr;
	va_start(ptr, msg);
	printf("Parser error on line %i. ", line);
	vprintf(msg, ptr);
	va_end(ptr);
	getchar();
	exit(-1);
}

static int rules_max_count = RULES_MAX_COUNT;
void add_rule(Rule* new_rule){
	if(rules_counter >= rules_max_count){
		rules_max_count *= 2;
		Rule** new_rules = malloc(sizeof(Rule*) * rules_max_count);
		for(int i = 0; i < rules_counter; i++) new_rules[i] = rules[i];
		for(int i = rules_counter; i < rules_max_count; i++) new_rules[i] = NULL;
		free(rules);
		rules = new_rules;
	}
	rules[rules_counter++] = new_rule;
	current_rule = new_rule;
}

/***********Rules initialization****************/

Rule* new_rule(Token* head, int body_length, char* action){
	Rule* rule = malloc(sizeof(Rule));
	rule->head = head;
	rule->body = malloc(sizeof(Token*) * body_length);
	rule->body_length = body_length;
	rule->action = action;
	return rule;
}

/***********Grammar productions****************/
Token* look;
FILE* source_code_file;

void move(){look = get_next_token(); }

void match(int tag){
	if(tag == look->tag){
		move();
		return;
	}
	throw_exception("Expected token %s, actual token is %s", token_tag_to_string(tag), look->lexeme);
}

void parser_initialize(FILE* fp){
	source_code_file = fp;
	rules = malloc(sizeof(Rule*) * rules_max_count);
	for(int i = 0; i < rules_max_count; i++) rules[i] = NULL;
	rules_counter = 1;
	current_rule = rules[rules_counter];
	lexer_initialize(fp);
	//symbol_hashtable = hashtableCreate(HASHTABLE_BASIS);//
}

void preparse(){
	move();
	if(look->tag == DECLARATION){
		declaration = look->lexeme;
		move();
		if(look->tag == EOL) move();
		match(DELIMETR);
		return;
	}
	if(look->tag == EOL) move();
	match(DELIMETR);
}

char* get_add_code(){
	if(look->tag == DELIMETR){
		move();	
		if(look->tag == ADD_CODE){
			move();
			return NULL;
		}
		return NULL;
	}
	throw_exception("Rule get_add_code - expected token %s, actual token is %s", token_tag_to_string(DELIMETR), token_tag_to_string(look->tag));	
	return NULL;
}

void rule();
void expression(Token*);
void nonempty_expression(Token*);
void expression_rest(Token*);
void nonempty_expression_rest(Token*);
Token* list(Token*);
Token* list_rest(Token*);
Token* symbol(Token*);

void read_rules(){
	do{
		move(); if(look->tag == EOL) move();
		if(/*look->tag == EOF ||*/ look->tag == DELIMETR) break;
		rule();
	} while(/*look->tag != EOF &&*/ look->tag != DELIMETR);
	
	Rule* expanded_rule = new_rule(nonterminals[0],1,NULL);
	expanded_rule->body[0] = rules[1]->head;
	rules[0] = expanded_rule;
}

void rule(){
	//printf("rule %s\n", token_tag_to_string(look->tag));//////////////////////////////////////////
	Token* rule_header;
	if(look->tag == NONTERMINAL){
		rule_header = look;
	}
	match(NONTERMINAL);
	match(DERIVATION);
	expression(rule_header);
}

void expression(Token* rule_header){
	//printf("expression %s\n", token_tag_to_string(look->tag));///////////////////////////////////////////////////////
	body_length = 0;
	char* rule_action = NULL;
	if(look->tag == EMPTY){
		Token* empty_symbol = look;
		move();
		//add new empty production with header rule_header;
		if(look->tag == ACTION){
			rule_action = look->lexeme;
			move();
		}
		add_rule(new_rule(rule_header,1,rule_action));
		(current_rule->body)[body_length] = empty_symbol;
		nonempty_expression_rest(rule_header);
		return;
	}
	list(rule_header);
	expression_rest(rule_header);
}

void nonempty_expression(Token* rule_header){
	//printf("nonempty_expression %s\n", token_tag_to_string(look->tag));/////////////////////////////////
	body_length = 0;
	if(look->tag == EMPTY){
		throw_exception("Rule nonempty_expression - there is not allowed one more empty production");	
	}
	list(rule_header);
	nonempty_expression_rest(rule_header);
}

void expression_rest(Token* rule_header){
	//printf("expression_rest %s\n", token_tag_to_string(look->tag));/////////////////////////////////////
	if(look->tag == UNION){
		move();
		expression(rule_header);
	} else if (look->tag == EOL /*|| look->tag == EOF*/){
		//EMPTY RULE
	} else {
		throw_exception("Rule list_rest - expected tokens %s, %s, %s, %s, %s, actual token is %s", token_tag_to_string(UNION), token_tag_to_string(EOL), token_tag_to_string(EOF), look->lexeme);			
	}
}

void nonempty_expression_rest(Token* rule_header){
	//printf("nonempty_expression_rest %s\n", token_tag_to_string(look->tag));//////////////////////////
	if(look->tag == UNION){
		move();
		nonempty_expression(rule_header);
	} else if (look->tag == EOL /*|| look->tag == EOF*/){
		//EMPTY RULE
	} else {
		throw_exception("Rule list_rest - expected tokens %s, %s, %s, %s, %s, actual token is %s", token_tag_to_string(UNION), token_tag_to_string(EOL), token_tag_to_string(EOF), look->lexeme);			
	}
}

Token* list(Token* rule_header){
	//printf("list %s\n", token_tag_to_string(look->tag));//////////////////////////////
	Token* local_symbol = symbol(rule_header);
	char* rule_action = NULL;
	move();
	body_length++;
	if(list_rest(rule_header) == NULL){
		//create new rule with head - rule_header and body length = body_length
		if(look->tag == ACTION){
			rule_action = look->lexeme;
			move();
		}
		add_rule(new_rule(rule_header,body_length, rule_action));//
	}
	body_length--;
	(current_rule->body)[body_length] = local_symbol;
	return local_symbol;
}

Token* list_rest(Token* rule_header){
	//printf("list_rest %s\n", token_tag_to_string(look->tag));////////////////////////////
	if(look->tag == TERMINAL || look->tag == NONTERMINAL){
		return list(rule_header);
	} else if (look->tag == UNION || look->tag == EOL || look->tag == ACTION /*|| look->tag == EOF*/){
		//EMPTY RULE
		return NULL;
	} else {
		throw_exception("Rule list_rest - expected tokens %s, %s, %s, %s, actual token is %s", token_tag_to_string(TERMINAL), token_tag_to_string(NONTERMINAL), token_tag_to_string(UNION), token_tag_to_string(EOL), token_tag_to_string(look->tag));		
	}
	return NULL;
}

Token* symbol(Token* rule_header){
	//printf("symbol %s %s\n", token_tag_to_string(look->tag), look->lexeme);////////////////////////////
	if(look->tag == TERMINAL || look->tag == NONTERMINAL){
		return look;
	} else {
		throw_exception("Rule symbol - expected tokens %s, %s, actual token is %s", token_tag_to_string(TERMINAL), token_tag_to_string(NONTERMINAL), token_tag_to_string(look->tag));
	}
	return NULL;
}

/********************** Rules printing *****************************/

void print_rule(int i){
	printf("<%s>",rules[i]->head->lexeme);
	printf(" ::=");
	for(int j = 0; j < rules[i]->body_length; j++){
		if(rules[i]->body[j]->tag == TERMINAL)
			printf(" \"%s\"", rules[i]->body[j]->lexeme);
	else if(rules[i]->body[j]->tag == EMPTY)
			printf(" empty");
		else if(rules[i]->body[j]->tag == NONTERMINAL)
		printf(" <%s>", rules[i]->body[j]->lexeme);
	}
	printf("\n");
}

void print_rules(){
	for(int i = 0; i < rules_counter; i++){
		print_rule(i);
	}
}