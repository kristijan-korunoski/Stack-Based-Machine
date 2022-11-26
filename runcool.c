//  CITS2002 Project 1 2021
//  Name(s):             Luke Kirkby, Kristijan Korunoski
//  Student number(s):   22885101 (, 22966841)
//
//  NOTE:   This .c file runs the stack machine with a partially working cache implementation
//          The script for a 100% working non cache implementation is commented from line 450
//
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
AWORD  cache_memory[N_CACHE_WORDS][3];

int CP      = 0;                         // Cache Pointer starts at 0
int CPC     = 0; 
int PC, SP, FP, CFP;                    // Program Counter, Stack Pointer, Frame Pointer, Cache Frame Pointer
int CP_Holder;
int funcDepth = 0;

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

void write_cache_memory(int cache_address, AWORD value, AWORD main_address, int dirty)
{
    if (cache_address < 0) {
        cache_address = 32+cache_address;
    }
    if (cache_memory[cache_address][2] == 1){                                               // Address in cache is dirty
            write_memory(cache_memory[cache_address][1], cache_memory[cache_address][0]);   // Write value back to main memory
    }
    cache_memory[cache_address][0] = value;                 // Overwrite cache data
    cache_memory[cache_address][1] = main_address;          // Overwrite cache data
    cache_memory[cache_address][2] = dirty;                 // Overwrite cache data

    CP = cache_address;                                     // Set Cache Pointer to curernt address
    CP++;
    if (CP == 32) {
        CP = 0;
    }
}

AWORD read_cache_memory(int cache_adress, int SP_FP_PC, int ret)
{
    if (SP_FP_PC == 2){
        n_cache_memory_misses++;
        if (ret == 3) {                                         //pusha case
            CP_Holder = CP;
            write_cache_memory(CP, read_memory(cache_adress), PC, 1);
            CP = CP_Holder;
            return cache_memory[CP][0];
        } else {
            if (CPC == 32) {
                CPC = 0;
                funcDepth++;
            }
            CP_Holder = CP;
            write_cache_memory(CPC, read_memory(PC), PC, 2);    //PC fetch
            CPC++;
            
            CP = CP_Holder;
            return cache_memory[CPC-1][0];
        }
        
        
    } else if (cache_memory[cache_adress-ret][2] != 1) {   // Cache value is wiped
        n_cache_memory_misses++;
        if (SP_FP_PC == 1){  CP_Holder = CP;
            write_cache_memory(cache_adress-ret, read_memory(FP-ret), FP-ret, 1);   // FP fetch 
            CP = CP_Holder;
        } else {
            CP_Holder = CP;
            write_cache_memory(cache_adress-ret, read_memory(SP-ret), SP-ret, 1);       // TOS fetch
            CP = CP_Holder;
        }
        return cache_memory[cache_adress-ret][0];
    } else {    //in cache                  // Read cache memory
        n_cache_memory_hits++;
        return cache_memory[cache_adress-ret][0];
    } 
}

void dedirty(int cache_address) {               // Remove dirty tag from cache adress as value has been removed
    cache_memory[cache_address][2] = 0;
}

//  -------------------------------------------------------------------

//  EXECUTE THE INSTRUCTIONS IN main_memory[]
int execute_stackmachine(void)
{
//  THE 3 ON-CPU CONTROL REGISTERS:
    PC      = 1;                        // 1st instruction is at address=1
    SP      = N_MAIN_MEMORY_WORDS-1;    // initialised to top-of-stack          
    FP      = N_MAIN_MEMORY_WORDS;      // frame pointer                      
    CP      = 0;                        // cache pointer initailised to 0
    CFP     = 0;                        // cache frame pointer initailised to 0

    bool halted = false;

    while(!halted) {
        if (CFP == 32) {
            CFP = 0;
        }

        IWORD value1, value2;           // Variables to temporarily store values that have been read

        AWORD fp_read;

        IWORD   bytes[2];               // Variable used for prints instruction

        int i = 1;  //string reading count

//  FETCH THE NEXT INSTRUCTION TO BE EXECUTED
        IWORD instruction   = read_cache_memory(PC, 2, 0);
        ++PC;

//  SUPPORT INSTRUCTIONS HERE
//      ....
        switch(instruction){  
            case 0:    // halt
                halted = true;
                break;

            case 1:   // no operation
                break;

            case 2:   // addition
                value1 = read_cache_memory(CP, 0, 1);           //Reads value on top of cache
                SP++;
                value2 = read_cache_memory(CP, 0, 2);           //Reads value on second from top of cache
                dedirty(CP-1);                                  //Removes the dirty tag from cache adress of value1
                dedirty(CP-2);                                  //Removes the dirty tag from cache adress of value2
                write_cache_memory(CP-2, value1+value2, SP, 1); //Adds second extracted value to first and writes result in cache
                break;

            case 3:   // subtraction
                value1 = read_cache_memory(CP, 0, 1);           //Reads value on top of cache
                SP++;
                value2 = read_cache_memory(CP, 0, 2);           //Reads value on second from top of cache
                dedirty(CP-1);                                  //Removes the dirty tag from cache adress of value1
                dedirty(CP-2);                                  //Removes the dirty tag from cache adress of value2
                write_cache_memory(CP-2, value2-value1, SP, 1); //Subtracts second extracted value from first and writes result in cache
                break;

            case 4:   // multiplication
                value1 = read_cache_memory(CP, 0, 1);           //Reads value on top of cache
                SP++;
                value2 = read_cache_memory(CP, 0, 2);           //Reads value on second from top of cache
                dedirty(CP-1);                                  //Removes the dirty tag from cache adress of value1
                dedirty(CP-2);                                  //Removes the dirty tag from cache adress of value2
                write_cache_memory(CP-2, value1*value2, SP, 1); //Multiplies second extracted value by first and writes result in cache
                break;

            case 5:   // division 
                value1 = read_cache_memory(CP, 0, 1);           //Reads value on top of cache
                SP++;
                value2 = read_cache_memory(CP, 0, 2);           //Reads value on second from top of cache
                dedirty(CP-1);                                  //Removes the dirty tag from cache adress of value1
                dedirty(CP-2);                                  //Removes the dirty tag from cache adress of value2
                write_cache_memory(CP-2, value2/value1, SP, 1); //Divides the second extracted value by the first and writes result in cache
                break;

            case 6:   // function call
                SP--;
                value1 = FP;
                FP = SP;
                CFP = (65536-FP)%32;
                if (CFP == 0){
                    CFP = 32;
                }
                write_cache_memory(CP, PC+1, SP, 1);
                SP--;
                write_cache_memory(CP, value1, SP, 1);
                PC = read_cache_memory(PC, 2, 0)+1;
                break;

            case 7:   // function return
                value1 = read_cache_memory(PC, 2, 0);
                PC = read_cache_memory(CFP, 1, 2);
                value2 = read_cache_memory(CP, 0, 1);
                fp_read = read_cache_memory(CFP, 1, 1);   
                while (CP!=CFP-value1-1) {
                    dedirty(CP-1);
                    CP--;
                    if (CP == 0){
                        CP = 32;
                    }
                }
                CP = CFP-value1-1;
                write_cache_memory(CP, value2, FP-value1-1, 1);
                SP = FP+value1-1;
                FP = fp_read;
                CFP = (65536-FP)%32;
                if (CFP == 0){
                    CFP = 32;
                }
                break;

            case 8:   // unconditional jump
                PC = read_cache_memory(PC, 2, 0)+1;
                break;

            case 9:   // conditional jump
                value1 = read_cache_memory(CP, 0, 1);
                SP++;
                CP--;
                dedirty(CP);
                if (value1 == 0) {
                    PC = read_cache_memory(PC, 2, 0)+1;
                } else {
                    PC++;
                }
                break;

            case 10:   // print integer
                printf("%i", read_cache_memory(CP, 0, 1));
                CP--;
                dedirty(CP);
                SP++;
                break;

            case 11:   // print string
                value1 = read_cache_memory(PC, 2, 0);
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
                value1 = read_cache_memory(PC, 2, 0);
                PC++;
                SP--;
                write_cache_memory(CP, value1, SP, 1);
                break;

            case 13:   // push absolute
                value1 = read_cache_memory(PC, 2, 0)+1;
                value2 = read_cache_memory(value1, 2, 3);
                PC++;
                SP--;
                write_cache_memory(CP, value2, SP, 1);
                break;

            case 14:   // push relative
                value1 = read_cache_memory(PC, 2, 0);
                PC++;
                SP--;
                write_cache_memory(CP, read_cache_memory(CFP, 1, value1+1), SP, 1);
                break;

            case 15:   // pop absolute
                value1 = read_cache_memory(PC, 2, 0)+1;
                value2 = read_cache_memory(CP, 0, 1);
                SP++;
                PC++;
                dedirty(CP-1);
                CP--;
                write_memory(value1, value2);
                break;

            case 16:   // pop relative
                value2 = read_cache_memory(CP, 0, 1);
                value1 = read_cache_memory(PC, 2, 0);
                SP++;
                PC++;
                CP_Holder = CP;
                dedirty(CP-1);
                dedirty(CFP-value1-1);
                write_cache_memory(CFP-value1-1, value2, FP-value1-1, 1);
                CP = CP_Holder-1;
                break;
        }
    }

//  THE RESULT OF EXECUTING THE INSTRUCTIONS IS FOUND ON THE TOP-OF-STACK
    return read_cache_memory(1, 0, 0);
}

//  -------------------------------------------------------------------

//  READ THE PROVIDED coolexe FILE INTO main_memory[]
void read_coolexe_file(char filename[])
{

    for (int i = 0; i < 32; i++) {      //initialise cache
        cache_memory[i][0] = 0;
        cache_memory[i][1] = 0;
        cache_memory[i][2] = 1;
    }

    memset(main_memory, 0, sizeof main_memory);   //  clear all memory

//  READ CONTENTS OF coolexe FILE
    FILE *fp = fopen(filename, "rb");   //  opens file

    if(fp == NULL){   //  checks if file is valid
        exit(EXIT_FAILURE);
    }

    int i = 1;   //  integer for cycling through all 16-bit words in the file
    AWORD temp;   // pointer to temporarily store 16-bit word

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






//  -------------------------------------------------------------------
/*
//  No cache Implementaion
//
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
               printf("%i", read_memory(SP++)); // remove “Print:
               break;
 
           case 11:   // print string
               //printf("Print: "); // remove “Print:”
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
*/