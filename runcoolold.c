//  CITS2002 Project 1 2021
//  Name(s):             Luke Kirkby, Kristijan Korunoski
//  Student number(s):   22885101 (, 22966841)
 
//  compile with:  cc -std=c11 -Wall -Werror -o runcool runcool.c
 
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
 
//  THE STACK-BASED MACHINE HAS 2^16 (= 65,536) WORDS OF MAIN MEMORY
#define N_MAIN_MEMORY_WORDS (1<<16)
 
//  EACH WORD OF MEMORY CAN STORE A 16-bit UNSIGNED ADDRESS (0 to 65535)
#define AWORD               uint16_t
//  OR STORE A 16-bit SIGNED INTEGER (-32,768 to 32,767)
#define IWORD               int16_t
 
//  THE ARRAY OF 65,536 WORDS OF MAIN MEMORY
AWORD                       main_memory[N_MAIN_MEMORY_WORDS];
 
//  THE SMALL-BUT-FAST CACHE HAS 32 WORDS OF MEMORY
#define N_CACHE_WORDS       32
 
 
//  see:  http://teaching.csse.uwa.edu.au/units/CITS2002/projects/coolinstructions.php
enum INSTRUCTION {
   I_HALT      = 0,
   I_NOP       = 1,
   I_ADD       = 2,
   I_SUB       = 3,
   I_MULT      = 4,
   I_DIV       = 5,
   I_CALL      = 6,
   I_RETURN    = 7,
   I_JMP       = 8,
   I_JEQ       = 9,
   I_PRINTI    = 10,
   I_PRINTS    = 11,
   I_PUSHC     = 12,
   I_PUSHA     = 13,
   I_PUSHR     = 14,
   I_POPA      = 15,
   I_POPR      = 16
};
 
//  USE VALUES OF enum INSTRUCTION TO INDEX THE INSTRUCTION_name[] ARRAY
const char *INSTRUCTION_name[] = {
   "halt",
   "nop",
   "add",
   "sub",
   "mult",
   "div",
   "call",
   "return",
   "jmp",
   "jeq",
   "printi",
   "prints",
   "pushc",
   "pusha",
   "pushr",
   "popa",
   "popr"
};
 
//  ----  IT IS SAFE TO MODIFY ANYTHING BELOW THIS LINE  --------------
 
//  THE STATISTICS TO BE ACCUMULATED AND REPORTED
int n_main_memory_reads     = 0;
int n_main_memory_writes    = 0;
int n_cache_memory_hits     = 0;
int n_cache_memory_misses   = 0;
 
void report_statistics(void)
{
   printf("@number-of-main-memory-reads\t%i\n",    n_main_memory_reads);
   printf("@number-of-main-memory-writes\t%i\n",   n_main_memory_writes);
   printf("@number-of-cache-memory-hits\t%i\n",    n_cache_memory_hits);
   printf("@number-of-cache-memory-misses\t%i\n",  n_cache_memory_misses);
}
 
//  -------------------------------------------------------------------
 
//  EVEN THOUGH main_memory[] IS AN ARRAY OF WORDS, IT SHOULD NOT BE ACCESSED DIRECTLY.
//  INSTEAD, USE THESE FUNCTIONS read_memory() and write_memory()
//
//  THIS WILL MAKE THINGS EASIER WHEN WHEN EXTENDING THE CODE TO
//  SUPPORT CACHE MEMORY
//WORD  cache_memory[N_CACHE_WORDS][2];
 
AWORD read_memory(int address)
{
   n_main_memory_reads++;
   return main_memory[address];
}
 
void write_memory(AWORD address, AWORD value)
{
   n_main_memory_writes++;
   main_memory[address] = value;
}

//  -------------------------------------------------------------------
 
//  EXECUTE THE INSTRUCTIONS IN main_memory[]
int execute_stackmachine(void)
{
//  THE 3 ON-CPU CONTROL REGISTERS:
   int PC      = 0;                    // 1st instruction is at address=0
   int SP      = N_MAIN_MEMORY_WORDS-1;  // initialised to top-of-stack        for some reason i had to do this for parameters.coolexe to work
   int FP      = N_MAIN_MEMORY_WORDS;  // frame pointer                        FP should be at top of stack
  
   bool halted = false;
 
   while(!halted) {
 
       IWORD value1, value2;
	
        IWORD   bytes[2];
 
        int i = 0;
 
 
//  FETCH THE NEXT INSTRUCTION TO BE EXECUTED
       IWORD instruction   = read_memory(PC);
       ++PC;
 
//  SUPPORT INSTRUCTIONS HERE
//      ....
       switch(instruction){ 
           case 0:    // halt
               halted = true;	//DOUBLE CHECK THIS, I THINK I DID IT WRONG
 
               break;
 
           case 1:   // no operation
               break;
 
           case 2:   // addition
               value1 = read_memory(SP++);
               value2 = read_memory(SP);
               //SP++;
               //SP--;
               write_memory(SP, value1+value2);
               break;
 
           case 3:   // subtraction
               value1 = read_memory(SP);
               SP++;
               value2 = read_memory(SP);
               //SP++;
               //SP--;
               write_memory(SP, value2-value1);
               break;
 
           case 4:   // multiplication
               value1 = read_memory(SP++);
               value2 = read_memory(SP);
               //SP++;
               //SP--;
               write_memory(SP, value1*value2);
               break;
 
           case 5:   // division
               value1 = read_memory(SP++);
               value2 = read_memory(SP);
               //SP++;
               //SP--;
               write_memory(SP, value2/value1);
               break;
 
           case 6:   // function call
               SP--;
               write_memory(SP--, PC+1);
               write_memory(SP, FP);
               FP = SP;
               PC = read_memory(PC);
               break;
 
           case 7:   // function return
               value1 = read_memory(PC);
               PC = read_memory(FP+1);
               write_memory(FP+value1, read_memory(SP));
               SP = FP+value1;
               FP = read_memory(FP);
               break;
 
           case 8:   // unconditional jump
               PC = read_memory(PC);
               break;
 
           case 9:   // conditional jump
               value1 = read_memory(SP++);
               if (value1 == 0) {
                   PC = read_memory(PC);
               } else {
                   PC++;
               }
               break;
 
           case 10:   // print integer
               printf("%i", read_memory(SP++)); // remove ???Print:
               break;
 
           case 11:   // print string
               //printf("Print: "); // remove ???Print:???
               value1 = read_memory(PC);
               while(true){
                   value2 = read_memory(value1+i);
                   bytes[0] = value2 >> 8;
                   bytes[1] = value2 & 0xFF;
                   if (bytes[1] == 0) {
                       break;
                   } else if (bytes[0] == 0) {
                       printf("%c", bytes[1]);
                       break;
                   } else {
                       printf("%c", bytes[1]);
                       printf("%c", bytes[0]);
                   }
                   i++;
                 }
                 PC++;
               break;
 
           case 12:   // push integer constant
               value1 = read_memory(PC++);
               write_memory(--SP, value1);
               break;
 
           case 13:   // push absolute
               value1 = read_memory(read_memory(PC++));
               write_memory(--SP, value1);
               break;
 
           case 14:   // push relative
               value1 = read_memory(PC++);
               write_memory(--SP, read_memory(FP+value1));
               break;
 
           case 15:   // pop absolute
               value1 = read_memory(PC++);
               value2 = read_memory(SP++);
               write_memory(value1, value2);
               break;
 
           case 16:   // pop relative
               value1 = read_memory(PC++);
               value2 = read_memory(SP++);
               write_memory(FP+value1, value2);
               break;
       }
   }
 
//  THE RESULT OF EXECUTING THE INSTRUCTIONS IS FOUND ON THE TOP-OF-STACK
   return read_memory(SP);
}
 
//  -------------------------------------------------------------------
 
//  READ THE PROVIDED coolexe FILE INTO main_memory[]
void read_coolexe_file(char filename[])
{
   memset(main_memory, 0, sizeof main_memory);   //  clear all memory
 
//  READ CONTENTS OF coolexe FILE
   FILE *fp = fopen(filename, "rb");   //  opens file
 
   if(fp == NULL){   //  checks if file is valid
       exit(EXIT_FAILURE);
   }
 
   int i = 0;   //  integer for cycling through all 16-bit words in the file
   int temp;   // pointer to temporarily store 16-bit word
 
   while (true) {
       if (fread(&temp, 2, 1, fp) == 0) {   //  checks if end of file has beeen reached, also assigns 16-bit word to value
           break;   //  ends loop if end of file has been reached
       }
       write_memory(i, temp);   //  assigning a 16 bit word to an address in main_memory
       n_main_memory_writes--;
       i++;   //  go to next word
      
   }
  
}
 
//  -------------------------------------------------------------------
 
int main(int argc, char *argv[])
{
//  CHECK THE NUMBER OF ARGUMENTS
   if(argc != 2) {
       fprintf(stderr, "Usage: %s program.coolexe\n", argv[0]);
       exit(EXIT_FAILURE);
   }
 
//  READ THE PROVIDED coolexe FILE INTO THE EMULATED MEMORY
   read_coolexe_file(argv[1]);
 
//  EXECUTE THE INSTRUCTIONS FOUND IN main_memory[]
   int result = execute_stackmachine();
 
   report_statistics();
   printf("@\n@exit(%i)\n", result);
 
   return result;          // or  exit(result);
}