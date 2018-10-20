//Lexer interface
#define TERMINALS_MAX_COUNT 128
#define NONTERMINALS_MAX_COUNT 128

typedef struct Token Token;

struct Token {
	char* token_id;
	char* lexeme;
	int offset; // offset of pointer's array terminals/nonterminals, is needed for faster searching
};

int line;

Token** terminals;
int terminals_counter;

Token** nonterminals;
int nonterminals_counter;

void lexer_initialize(FILE*);
Token* get_next_token();