#define LR_STATES_MAX_COUNTER 128

typedef struct LR_state LR_state;
typedef struct Item Item;
typedef struct LR_state_trans LR_state_trans;
typedef struct Lookahead Lookahead;

struct LR_state {
	Item** kernel_items;
	Item** nonkernel_items;
	int kernel_items_count;
	int nonkernel_items_count;
	
	LR_state_trans** terminals;
	LR_state_trans** nonterminals;
	int terminals_count;
	int nonterminals_count;
};

struct Item {
	int rule;
	int position;
	Lookahead* lookahead;	
};

struct LR_state_trans {
	int token;
	int state;
};

struct Lookahead {
	int* terminals;
	int terminals_count;
};

LR_state** lr_states;
int lr_states_counter;

int max_kernel_items_count;

void build_lr_automat();
int compare_lr_states(LR_state*, LR_state*);
void print_lr_automat();
void print_lr_state(int);

void print_lr_automat_items();
void print_lr_state_items(int);