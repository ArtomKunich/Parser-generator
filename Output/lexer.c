#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "datastructs.h"
#include "lexer.h"

#define TERMINAL_MAX_LENGTH 200
#define NONTERMINAL_MAX_LENGTH 100
#define HASHTABLE_BASIS 17

static char str_temp[4596];
static char peek;
static FILE* fp_in;

//typedef struct HashtableElement HashtableElement;
HashtableElement** terminals_hashtable;
HashtableElement** nonterminals_hashtable;
HashtableElement** other_symbols_hashtable;

static void throw_exception(char* msg, ...){
	va_list ptr;	
	va_start(ptr, msg);
	printf("Lexer error on line %i. ", line);
	vprintf(msg, ptr);
	va_end(ptr);
	getchar();
	exit(-1);
}

/*
void print_terminals(){
	printf("---TERMINALS---\n");
	for(int i = 0; i < terminals_counter; i++) printf("\"%s\"\n",terminals[i]->lexeme);
}

void print_nonterminals(){
	printf("---NONTERMINALS---\n");
	for(int i = 0; i < nonterminals_counter; i++) printf("<%s>\n",nonterminals[i]->lexeme);
}


char* token_to_string(Token* t){
	if(t->tag < 256){
		sprintf(str_temp, "%c", (char)t->tag);
		return str_temp;
	}	
	return t->lexeme;	
}

char* token_tag_to_string(int tag){
	if(tag < 256){
		sprintf(str_temp, "%c", (char)tag);
		return str_temp;
	}
	switch(tag){
		case EMPTY: return "empty";
		case UNION: return "|";
		case TERMINAL:	return "terminal";
		case NONTERMINAL:	return "nonterminal";
		case DELIMETR:	return "%%";
		case DERIVATION:	return "::=";			
		case ACTION:	return "action";	
		case EOL:	return "EOL";		
		case EOF:	return "EOF";			
	}	
	sprintf(str_temp, "%c", (char)tag);
	return str_temp;
}
*/

static int terminals_max_count = TERMINALS_MAX_COUNT;
void add_terminal(Token* new_terminal){
	if(terminals_counter >= terminals_max_count){
		terminals_max_count *= 2;
		Token** new_terminals = malloc(sizeof(Token*) * terminals_max_count);
		for(int i = 0; i < terminals_counter; i++) new_terminals[i] = terminals[i];
		for(int i = terminals_counter; i < terminals_max_count; i++) new_terminals[i] = NULL;
		free(terminals);
		terminals = new_terminals;
	}
	new_terminal->offset = terminals_counter;
	terminals[terminals_counter++] = new_terminal;
}

static int nonterminals_max_count = NONTERMINALS_MAX_COUNT;
void add_nonterminal(Token* new_nonterminal){
	if(nonterminals_counter >= nonterminals_max_count){
		nonterminals_max_count *= 2;
		Token** new_nonterminals = malloc(sizeof(Token*) * nonterminals_max_count);
		for(int i = 0; i < nonterminals_counter; i++) new_nonterminals[i] = nonterminals[i];
		for(int i = nonterminals_counter; i < nonterminals_max_count; i++) new_nonterminals[i] = NULL;		
		free(nonterminals);
		nonterminals = new_nonterminals;
	}
	new_nonterminal->offset = nonterminals_counter;
	nonterminals[nonterminals_counter++] = new_nonterminal;
}

Token* token(char* token_id){
	Token* token = malloc(sizeof(Token));
	token->token_id = token_id;
	token->lexeme = NULL;
	return token; 
}

Token* token_symbol(char* token_id, char* lexeme){
	Token* token = malloc(sizeof(Token));
	token->token_id = token_id;
	token->lexeme = malloc(strlen(lexeme) + 1);
	strcpy(token->lexeme,lexeme);
	return token;
}

void reserve(HashtableElement** hashtable, Token* t){
	hashtableAdd(hashtable, t->lexeme, t);
}

void lexer_initialize(FILE* fp){
	
	line = 1;
	
	fp_in = fp;
	
	terminals_hashtable = hashtableCreate(HASHTABLE_BASIS);
	nonterminals_hashtable = hashtableCreate(HASHTABLE_BASIS);
	
	terminals = malloc(sizeof(Token*) * terminals_max_count);
	for(int i = 1; i < terminals_max_count; i++) terminals[i] = NULL;
	add_terminal(token_symbol("TERMINAL", "_EOSC_"));
	hashtableAdd(terminals_hashtable, "_EOSC_", terminals[0]);
	
	nonterminals = malloc(sizeof(Token*) * nonterminals_max_count);
	for(int i = 1; i < nonterminals_max_count; i++) nonterminals[i] = NULL;
	add_nonterminal(token_symbol("NONTERMINAL","first-symbol"));
	
	other_symbols_hashtable = hashtableCreate(HASHTABLE_BASIS);	
	hashtableAdd(other_symbols_hashtable, "EOL", token("EOL"));
	hashtableAdd(other_symbols_hashtable, "::=", token("::="));
	hashtableAdd(other_symbols_hashtable, "|", token("UNION"));
	hashtableAdd(other_symbols_hashtable, "_EOSC_", token("_EOSC_"));
	hashtableAdd(other_symbols_hashtable, "EMPTY", token("EMPTY"));
	hashtableAdd(other_symbols_hashtable, "{", token("O_BRACKET"));
	hashtableAdd(other_symbols_hashtable, "}", token("C_BRACKET"));	
}

char readch(){
	return (char)fgetc(fp_in);	
}

int read_and_equalch(char e){
	char old_peek = peek;	
	peek = readch();
	if(peek == e) return 1;
	ungetc(peek, fp_in);
	peek = old_peek;
	return 0;	
}

Token* get_terminal(HashtableElement** hashtable){
	int i = 0;	
	peek = readch();	
	if(peek == '"') return hashtableGetValue(other_symbols_hashtable, "EMPTY");
	
	do {	
		if(peek == '\\') peek = readch();
		str_temp[i++] = peek;
		if(i == (TERMINAL_MAX_LENGTH + 1)){
			str_temp[i] = '\0';	
			throw_exception("Too long terminal %s. Max length of id is %i", str_temp, TERMINAL_MAX_LENGTH);
		}
		peek = readch();
	} while(peek != '"');

	str_temp[i] = '\0';		
		
	Token* terminal = hashtableGetValue(hashtable, str_temp);

	if(terminal == NULL){
		terminal = token_symbol("TERMINAL", str_temp);
		hashtableAdd(hashtable, str_temp, terminal);
		add_terminal(terminal);
	}
	
	return terminal;		
}

Token* get_nonterminal(HashtableElement** hashtable){
	int i = 0;	
	peek = readch();
	Token* nonterminal;	
	if(peek == '>'){ 
		throw_exception("There is empty nonterminal");
		return NULL;
	}

	if((peek >= 'A' && peek <= 'Z') || (peek >= 'a' && peek <= 'z')){			
		do {	
			str_temp[i++] = peek;
			if(i == (NONTERMINAL_MAX_LENGTH + 1)){
				str_temp[i] = '\0';	
				throw_exception("Too long nonterminal %s. Max length of nonterminal is %i", str_temp, NONTERMINAL_MAX_LENGTH);
			}
			peek = readch();		
		} while((peek >= '0' && peek <= '9') || (peek >= 'A' && peek <= 'Z') || (peek >= 'a' && peek <= 'z') || (peek == '-'));
			
		if(peek != '>') throw_exception("Identifier format is not correct: %c", peek);

		str_temp[i] = '\0';			
		nonterminal = hashtableGetValue(hashtable, str_temp);
		
		if(nonterminal == NULL){
			nonterminal = token_symbol("NONTERMINAL", str_temp);
			hashtableAdd(hashtable, str_temp, nonterminal);
			add_nonterminal(nonterminal);
		}
		
	} else {
		throw_exception("Identifier format is not correct: %c", peek);
	}
	return nonterminal;	
}

Token* get_semantic_action_text(){
	Token* semantic_action_text;
	int i = 0;
	while(!(peek == '{' || peek == '}')){
		if(peek == '\\') peek = readch();
		str_temp[i++] = peek;	
		peek = readch();
	}
	ungetc(peek, fp_in);
	str_temp[i] = '\0';	
	semantic_action_text = token_symbol("SEMANTIC_ACTION_TEXT", str_temp);	
	return semantic_action_text;
}

Token* get_declaration(){
	Token* declaration;
	int i = 0;
	while(1){
		peek = readch();
		if(peek == EOF) throw_exception("There is no close bracket %%} in declaration");;
		if(peek == '%' && read_and_equalch('}')) break;
		str_temp[i++] = peek;
	}
	str_temp[i] = '\0';
	//printf("%s\n",str_temp);
	declaration = token_symbol("DECLARATION", str_temp);
	return declaration;	
}
/*Token* get_action(FILE* fp_in, HashtableElement** hashtable){
	Token* action;
	if(peek == '{'){	
		int i = 0;
		int bracket_counter = 1;
		peek = readch();	
		if(peek == '}'){
			action = hashtableGetValue(hashtable, "");			
			if (action == NULL){
				action = token_symbol(ACTION, "");
				hashtableAdd(hashtable, "", action);
			}			
			return action;							
		} else {			
			if(peek == '{') bracket_counter++;			
			do {	
				str_temp[i++] = peek;
				peek = readch();				
				if(peek == '{'){bracket_counter++;}
				else if(peek == '}'){bracket_counter--;}
				if(bracket_counter < 0) throw_exception("There is more \'}\' brackets then \'{\' brackets in action");						
			} while(bracket_counter != 0);
			str_temp[i] = '\0';	
			action = hashtableGetValue(hashtable, str_temp);				
			if (action == NULL){
				action = token_symbol(ACTION, str_temp);
				hashtableAdd(hashtable, str_temp, action);
			}						
		}	
	} else {
		throw_exception("Not allowed symbol \'%c\' in action", peek);
	}
	return action;	
}*/

Token* get_next_token(){	
	
	peek = readch();
	int local_line = line;
	
	for(int i = 0; 1; i++){
		if(peek == ' ' || peek == '\t' || peek == '\n'){
			if(peek == '\n') {line++;}
			peek = readch();
			continue;
		}
		else {
			break;
		}
	}
	
	if(line > local_line){
		ungetc(peek, fp_in);
		return hashtableGetValue(other_symbols_hashtable, "EOL");
	}

	//if(peek == EOF) return token(EOF);

	if(peek == ':' && read_and_equalch(':') && read_and_equalch('=')) return hashtableGetValue(other_symbols_hashtable, "::=");

	if(peek == '|') return hashtableGetValue(other_symbols_hashtable, "|");	

	if(peek == '<') return get_nonterminal(nonterminals_hashtable);

	if(peek == '"') return get_terminal(terminals_hashtable);

	if(peek == '%' && read_and_equalch('%')) return hashtableGetValue(other_symbols_hashtable, "_EOSC_");

	if(peek == '%' && read_and_equalch('{')) return get_declaration();
	
	if(peek == '{') return hashtableGetValue(other_symbols_hashtable, "{");

	if(peek == '}') return hashtableGetValue(other_symbols_hashtable, "}");
	
	return get_semantic_action_text();
	
	throw_exception("Was not found any token");
	return NULL;
}