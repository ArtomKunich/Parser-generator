#define DERIVATION 256
#define TERMINAL 257
#define NONTERMINAL 258
#define EMPTY 259
#define UNION 260
#define EOL 261
#define DELIMETR 262
#define ACTION 263
#define ADD_CODE 264
#define DECLARATION 265
#define TERMINALS_MAX_COUNT 128
#define NONTERMINALS_MAX_COUNT 128

typedef struct Token Token;
typedef struct HashtableElement HashtableElement;

struct Token{
	int tag;
	char* lexeme;
	int offset; // offset of pointer's array terminals/nonterminals, is needed for faster searching
};


//Variables
int line;

Token** terminals;
int terminals_counter;

Token** nonterminals;
int nonterminals_counter;


//Methods
void lexer_initialize(FILE*);
Token* get_next_token();

void print_terminals();
void print_nonterminals();
char* token_to_string(Token*);
char* token_tag_to_string(int);