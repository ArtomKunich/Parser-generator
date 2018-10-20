#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "lexer.h"
#include "parser.h"
#include "slr_automat_generator.h"
#include "lalr_automat_generator.h"
#include "parser_generator.h"

void throw_exception(char* msg, ...){
	va_list ptr;	
	va_start(ptr, msg);
	printf("Main error. ");
	vprintf(msg, ptr);
	va_end(ptr);	
	getchar();
	exit(-1);
}

int main(int argc, char** argv){
	if(argc < 2){
		throw_exception("Incorrect number of arguments. Expected %i, actual %i", 2, argc);	
	}
	char* fileNamePt = argv[1];
	FILE* fp_in = fopen( fileNamePt, "r" );

	parser_initialize(fp_in);
	parse();	
	
	//print_rules();
	//print_terminals();
	//print_nonterminals();
	
	build_lr_automat();
	set_lookaheads_2_nonkernel();
	set_lookaheads_2_kernel();
	
	print_lr_automat();
	print_lr_automat_items();	

	if(argc > 2){
		fileNamePt = argv[2];
		FILE* fp_out = fopen( fileNamePt, "w" );
		generate_parser(fp_in, fp_out);
		fclose(fp_out);
	}	
	fclose(fp_in);
	return 0;
}