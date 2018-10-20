#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "lexer.h"
#include "parser.h"
#include "slr_automat_generator.h"

int final_state;

char str_temp[512];

static void throw_exception(char* msg, ...){
	va_list ptr;
	va_start(ptr, msg);
	printf("Parser generator error");
	vprintf(msg, ptr);
	va_end(ptr);
	getchar();
	exit(-1);
}

char* get_rule_action(Rule* rule){
	
	char num[5];
	
	int i = 0;
	int j = 0;
	int c = 0;
	
	while(rule->action[i] != '\0'){
		
		if(rule->action[i] == '$'){
			if(rule->action[i + 1] == '$'){
				i += 2; 			
				strcpy(str_temp + j, "new_attr");
				j += 8;	
				continue;
			}
			if(rule->action[i + 1] >= '0' && rule->action[i + 1] <= '9'){
				i++;
				c = 0;
				while(rule->action[i] >= '0' && rule->action[i] <= '9'){
					if(c > 3) throw_exception("Too big identifier %s", num);
					num[c++] = rule->action[i++];
					if(c == 4) num[c] = '\0';
				}
				num[c] = '\0';
				sprintf(str_temp + j, "attrs_stack[stack_pointer - %i + %s]", rule->body_length, num);				
				j = strlen(str_temp);
				continue;
			}			
		}
		
		
		str_temp[j++] = rule->action[i++];
	}
	str_temp[j] = '\0';
	return str_temp;
}

void print_goto_state(FILE* fp, int state){
	
	if(lr_states[state]->nonterminals_count == 0) return;
	
	fprintf(fp,"\tif(state == %i){\n", state);
	fprintf(fp,"\t\tswitch(nonterminal){\n");
	
	for(int i = 0; i < lr_states[state]->nonterminals_count; i++) fprintf(fp, "\t\t\tcase %i: return %i;\n", lr_states[state]->nonterminals[i]->token, lr_states[state]->nonterminals[i]->state);
	
	//fprintf(fp, "\t\t\tdefault: throw_exception(\"There is no transfers for state %%i , and nonterminal %%s\", state_stack[stack_pointer], lookahead->token_id);\n");
	fprintf(fp, "\t\t}\n");
	fprintf(fp, "\t}\n");
}

void print_action_state(FILE* fp, int state){
	
	fprintf(fp,"\tif(state == %i){\n", state);
	fprintf(fp,"\t\tswitch(token_id){\n");
	
	for(int i = 0; i < lr_states[state]->terminals_count; i++){
		fprintf(fp, "\t\t\tcase %i: push_state(%i, lookahead); lookahead = get_next_token(in_fp); return state_stack[stack_pointer];\n", lr_states[state]->terminals[i]->token, lr_states[state]->terminals[i]->state);
	}	

	for(int i = 0; i < lr_states[state]->kernel_items_count; i++){
		if(lr_states[state]->kernel_items[i]->position == rules[lr_states[state]->kernel_items[i]->rule]->body_length){	
			for(int j = 0; j < lr_states[state]->kernel_items[i]->lookahead->terminals_count; j++){
				fprintf(fp, "\t\t\tcase %i:\n", lr_states[state]->kernel_items[i]->lookahead->terminals[j]);
				if(rules[lr_states[state]->kernel_items[i]->rule]->action) fprintf(fp, "\t\t\t\t%s\n", get_rule_action(rules[lr_states[state]->kernel_items[i]->rule]));
				fprintf(fp, "\t\t\t\tpop_state(%i); push_state(do_goto(%i), new_attr); return state_stack[stack_pointer];\n", lr_states[state]->kernel_items[i]->position, rules[lr_states[state]->kernel_items[i]->rule]->head->offset);
			}
		}
	}

	for(int i = 0; i < lr_states[state]->nonkernel_items_count; i++){
		if(/*(lr_states[state]->nonkernel_items[i]->position == rules[lr_states[state]->nonkernel_items[i]->rule]->body_length) ||*/ (rules[lr_states[state]->nonkernel_items[i]->rule]->body[0]->tag == EMPTY)){	
			for(int j = 0; j < lr_states[state]->nonkernel_items[i]->lookahead->terminals_count; j++){
				fprintf(fp, "\t\t\tcase %i:\n", lr_states[state]->nonkernel_items[i]->lookahead->terminals[j]);
				if(rules[lr_states[state]->nonkernel_items[i]->rule]->action) fprintf(fp, "\t\t\t\t%s\n", get_rule_action(rules[lr_states[state]->nonkernel_items[i]->rule]));
				fprintf(fp, "\t\t\t\tpush_state(do_goto(%i), new_attr); return state_stack[stack_pointer];\n", rules[lr_states[state]->nonkernel_items[i]->rule]->head->offset);
			}
		}
	}
	
	//fprintf(fp, "\t\t\tdefault: throw_exception(\"There is no transfers for state %%i , and nonterminal %%s\", state_stack[stack_pointer], lookahead->token_id);\n");
	fprintf(fp,"\t\t}\n");
	fprintf(fp,"\t}\n");
}

void generate_parser(FILE* fp_in, FILE* fp_out){
		
char* tmpStr = "#include <stdio.h>\n\
#include <stdlib.h>\n\
#include <string.h>\n\
#include <stdarg.h>\n\
#include \"datastructs.h\"\n\
#include \"lexer.h\"\n\
#include \"parser.h\"\n\
#define MAX_STATE_STACK_SIZE 256\n\
#define HASHTABLE_BASIS 17\n\n\
/*------------Additional declaration----------*/\n";
fprintf(fp_out,tmpStr);

if(declaration) fprintf(fp_out,declaration);

tmpStr = "\n\
/*------------Additional declaration ending----------*/\n\n\
static int max_state_stack_size;\n\
static int* state_stack;\n\
static void** attrs_stack;\n\
static int stack_pointer;\n\
static Token* lookahead;\n\
FILE* in_fp;\n\
static HashtableElement** token_ids;\n\n\
static void throw_exception(char*, ...);\n\n\
/*------------Additional code----------*/";
fprintf(fp_out,tmpStr);

char peek;

while(1){	
	peek = fgetc(fp_in);
	if(peek == EOF) break;
	fputc(peek, fp_out);
}

tmpStr =  "\n\n/*------------Additional code ending----------*/\n\nvoid push_state(int, void*);\n\
static void throw_exception(char* msg, ...){\n\
	va_list ptr;\n\
	va_start(ptr, msg);\n\
	printf(\"Parser error on line %%i. \", line);\n\
	vprintf(msg, ptr);\n\
	va_end(ptr);\n\
	getchar();\n\
	exit(-1);\n\
}\n\
\n";
fprintf(fp_out,tmpStr);

tmpStr = "void parser_initialize(FILE* fp){\n\
	lexer_initialize(fp);\n\
	max_state_stack_size = MAX_STATE_STACK_SIZE;\n\
	state_stack = malloc(sizeof(int) * max_state_stack_size);\n\
	attrs_stack = malloc(sizeof(void*) * max_state_stack_size);\n\
	stack_pointer = -1;\n\
	in_fp = fp;\n\
	token_ids = hashtableCreate(HASHTABLE_BASIS);\n\
	push_state(0, NULL);\n";
fprintf(fp_out,tmpStr);

for(int i = 0; i < terminals_counter; i++) fprintf(fp_out,"\thashtableAdd(token_ids, \"%s\", (void*)%i);\n", terminals[i]->lexeme, terminals[i]->offset);

fprintf(fp_out,"}\n\n");

tmpStr = "void resize_stack(){\n\
	max_state_stack_size += MAX_STATE_STACK_SIZE;\n\
	int* new_state_stack = malloc(sizeof(int) * max_state_stack_size);\n\
	void** new_attrs_stack = malloc(sizeof(void*) * max_state_stack_size);\n\
	for(int i = 0; i < stack_pointer; i++){\n\
		new_state_stack[i] = state_stack[i];\n\
		new_attrs_stack[i] = attrs_stack[i];\n\
	}\n\
	free(state_stack);\n\
	free(attrs_stack);\n\
	state_stack = new_state_stack;\n\
	attrs_stack = new_attrs_stack;\n\
}\n\
\n\
void push_state(int state, void* attrs){\n\
	if(stack_pointer + 1 > max_state_stack_size){\n\
		resize_stack();\n\
	}\n\
	state_stack[++stack_pointer] = state;\n\
	attrs_stack[stack_pointer] = attrs;\n\
}\n\
\n\
int pop_state(int count){\n\
	stack_pointer -= (count-1);\n\
	return state_stack[stack_pointer--];\n\
}\n\
\n";
fprintf(fp_out,tmpStr);

//do goto
tmpStr = "int do_goto(int nonterminal){\n\n\
	int state = state_stack[stack_pointer];\n\n\
	printf(\"do_goto state: %%i nonterminal: %%i\\n\", state, nonterminal);\n\n";
fprintf(fp_out,tmpStr);
for(int i = 0; i < lr_states_counter; i++) print_goto_state(fp_out, i);
fprintf(fp_out, "\tthrow_exception(\"There is no transfers for state %%i , and nonterminal %%s\", state_stack[stack_pointer], lookahead->token_id);\n");
fprintf(fp_out,	"\treturn -1;\n");
fprintf(fp_out,"}\n\n");

for(int state = 0; state < lr_states_counter; state++)
	for(int i = 0; i < lr_states[state]->kernel_items_count; i++)
		if(lr_states[state]->kernel_items[i]->position == rules[lr_states[state]->kernel_items[i]->rule]->body_length)
			if(lr_states[state]->kernel_items[i]->rule == 0) final_state = state;

//do action
tmpStr = "int do_action(){\n\n\
	int state = state_stack[stack_pointer];\n\
	int token_id = (int)hashtableGetValue(token_ids, lookahead->token_id);\n\
	void* new_attr = NULL;\n\
	if(state == %i && token_id == 0) return -1;\n\n\
	printf(\"do_action state: %%i terminal: %%i\\n\", state, token_id);\n\n";
fprintf(fp_out, tmpStr, final_state);
for(int i = 0; i < lr_states_counter; i++) print_action_state(fp_out, i);
fprintf(fp_out, "\tthrow_exception(\"There is no transfers for state %%i , and terminal %%s\", state_stack[stack_pointer], lookahead->token_id);\n");
fprintf(fp_out,"\treturn state_stack[stack_pointer];\n");
fprintf(fp_out,"}\n");

tmpStr = "int parse(){\n\
	lookahead = get_next_token();\n\
	while(do_action() > -1);\n\
	printf(\"Parse OK\");\n\
	return 0;\n\
}\n";
fprintf(fp_out, tmpStr);		
}