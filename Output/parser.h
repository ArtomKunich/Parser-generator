#define HASHTABLE_BASIS 17
#define RULES_MAX_COUNT 128

typedef struct Rule Rule;

struct Rule {
	Token* head;
	Token** body;
	int body_length;
	
	char* action;
};

Rule** rules;
int rules_counter;
char* declaration;

void print_rule(int);
void print_rules();

//Parser interface

void parser_initialize(FILE*);
void preparse();
int parse();