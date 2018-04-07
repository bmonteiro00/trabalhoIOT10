/********************************************************************************

Máquina virtual IOT010

	Este codigo implementa uma máquina virtual (interpretador) capaz de buscar,
decodificar e executar um set de instrucão criado exclusivamente para demostrações 
durante as aulas de IOT010.   

***********************************************************************************

Detalhes do set de instrução

	Tamanho das instruções: 16 bits
	
	Código das intruções:
	
	    LOAD   0001 -> 1
	    STORE  0011 -> 3
	    ADD    0111 -> 7
	    SUB    1111 -> 15
	    PRINTREG 1000 -> 8
	    PRINTMEMO 1100 -> 8

	Instruções Tipo 1: 
	
		- Utilizado para operaçções aritméticas (soma, subtração, ...)
	     
             MSB                                      LSB
		   
		(Tipo instr.) (End. Reg 1) (End. Reg 2) (End Reg Dest.)
          
           4 bits        4 bits        4 bits       4 bits
           
		   
         - Exemplo: 0b0001000000010010 >>> |0001|0000|0001|0010
         
         	 	Realiza a soma (0001 >> tipo da instrução) do registro 0 (0000 
 	 	 	 >> end. Reg 1) com o registro 1 (0001 >> end. Reg 2) e salva o resultado
 	 	 	 em registro 2 (0010 >> end. Reg Dest.)
 	 	 	 
 	 	 	 
    Instruções Tipo 2:
    
     	 - Uitlizado para operações de LOAD e STORE
     	 
     	       MSB                        LSB
     	 
     	 (Tipo instr.) (End Reg) (End Memória de dados)

		    4 bits       4 bits        8 bits
		    
   	   - Exemplo: 0b1000000000010010 >>> |1000|0000|00001010
         
         	 	Realiza o LOAD (1000 >> tipo da instrução) do endereço de 
			memória 10 (00001010 >> end. Memória) para o registro 0 
			(0000 >> end. Reg )
			
     Instruções Tipo 3:
    
     	 - Uitlizado para operações PRINTREG
     	 
     	       MSB                     
     	 
     	 (Tipo instr.) (End Reg)

		    4 bits       4 bits 
		    
   	   - Exemplo: 0b1000000100000000 >>> |1000|0000|00000000
         
         	 Realiza o PRINT do registrador.
         	 
      Instruções Tipo 4:
    
     	 - Uitlizado para operações PRINTMEMO
     	 
     	       MSB                     
     	 
     	 (Tipo instr.) (End memoria)

		    4 bits        12 bits
		    
   	   - Exemplo: 0b1000000000000001 >>> |1000|0000|00000001
         
         	 Realiza o PRINT da memoria.
 	 	 	 
********************************************************************************/

#include <iostream>
#include <cstring>
#include <locale>
#include <iomanip>
#include <stdio.h>
#include <stdlib.h>


using namespace std;

/* Cache Sizes (in bytes) */
#define CACHE_SIZE 16384
#define BLOCK_SIZE 4
#define DEBUG 1

/* Block
 *
 * Holds an integer that states the validity of the bit (0 = invalid,
 * 1 = valid), the tag being held, and another integer that states if
 * the bit is dirty or not (0 = clean, 1 = dirty).
 */

struct Block_ {
    int valid;
    char* tag;
    int dirty;
    int data;
};

typedef struct Block_* Block;


/* Cache
 *
 * Cache object that holds all the data about cache access as well as 
 * the write policy, sizes, and an array of blocks.
 *
 * @param   hits            # of cache accesses that hit valid data
 * @param   misses          # of cache accesses that missed valid data
 * @param   reads           # of reads from main memory
 * @param   writes          # of writes from main memory
 * @param   cache_size      Total size of the cache in bytes
 * @param   block_size      How big each block of data should be
 * @param   numLines        Total number of blocks 
 */

struct Cache_ {
    int hits;
    int misses;
    int reads;
    int writes;
    int cache_size;
    int block_size;
    int numLines;
	Block* blocks;  
	   
};

typedef struct Cache_* Cache;

// Memoria de programa            
unsigned int ProgMemory[] = {	0b0001000000000000, // load
								0b0001000100000001, // load
								0b0111000000010010, // add
								0b0011001000000010, // store
								0b1000000000000000, // print registradores
								0b1000000100000000, // print registradores
								0b1100000000000010, // print conteudo na memoria (DataMemory)
								0b0001000000000011, // quando executar a instrução dara cache miss e colocara na memoria cache
								0b0001000000000011}; // quando executar nao dara mais cache miss 
// Memoria de dados
unsigned int DataMemory[] = { 1, 3, 0, 0, 0, 0, 0, 0};

									
// Registradores
unsigned int PC;
unsigned int Instr;
unsigned int InstrType;
unsigned int RegSourceA;
unsigned int RegSourceB;
unsigned int RegDest;
unsigned int RegAddrMemory;
unsigned int Reg[10];

// Prototipos
void decode(void);
void execute(Cache);
Cache createCache(int, int );

int main()
{
	unsigned char i;
	Cache cache;
	
	// Inicializacao dos registros
	PC = 0;
	for(i = 0; i < 10; i++)
	{
		Reg[i] = 0;
	}
	
	cache = createCache(CACHE_SIZE, BLOCK_SIZE);
	
	while(PC < 9)
	{
		Instr = ProgMemory[PC]; // busca da instrução
		PC = PC + 1;
		decode();    // decodificação
		execute(cache);
	}
	
	if(DEBUG)
	{
       printf("Cache hits : %i \n", cache->hits);
       printf("Cache miss: %i \n", cache->misses);
	}

    return 0;       
}

/* Inicializa a memória cache de 4096 linhas
*/
Cache createCache(int cache_size, int block_size)
{
    Cache cache;
    int i;
    
    if(cache_size <= 0)
    {
        printf("Cache size must be greater than 0 bytes...\n");
        return NULL;
    }
    
    if(block_size <= 0)
    {
        printf("Block size must be greater than 0 bytes...\n");
        return NULL;
    }
    
    cache = (Cache) malloc( sizeof( struct Cache_ ) );
    if(cache == NULL)
    {
        printf("Could not allocate memory for cache.\n");
        return NULL;
    }
    
    cache->hits = 0;
    cache->misses = 0;
    cache->reads = 0;
    cache->writes = 0;
    
    cache->cache_size = CACHE_SIZE;
    cache->block_size = BLOCK_SIZE;
    
     /* calcula quantidade de linhas */
    cache->numLines = (int)(CACHE_SIZE / BLOCK_SIZE);
    
    cache->blocks = (Block*) malloc( sizeof(Block) * cache->numLines );
        
    /* iniciando as posições( linhas) da memória */
    for(i = 0; i < cache->numLines; i++)
    {
    	//preenche a memoria cache com o valor da memoria de programa
    	if( i == 0 || i == 1)
		{
			cache->blocks[i] = (Block) malloc( sizeof( struct Block_ ) );
            cache->blocks[i]->valid = 1;
            cache->blocks[i]->dirty = 0;
            cache->blocks[i]->data = DataMemory[i];
		}
		else
		{
			cache->blocks[i] = (Block) malloc( sizeof( struct Block_ ) );
            cache->blocks[i]->valid = 0;
            cache->blocks[i]->dirty = 0;
            cache->blocks[i]->tag = NULL;
		}
   }
        
    return cache;
}

void decode(void)
{
	InstrType = Instr >> 12;
	
	if(InstrType == 1 )
	{
        /* Load */
		RegDest = Instr >> 8;
		RegDest = RegDest & 0b0000000000001111; 
		RegAddrMemory = Instr & 0b0000000011111111;
		
	}
	else if(InstrType == 7 || InstrType == 15)
	{
	   // Soma, Subtracao
		RegSourceA = Instr >> 8;
		RegSourceA = RegSourceA & 0b0000000000001111;
		RegSourceB = Instr >> 4;
		RegSourceB = RegSourceB & 0b0000000000001111;
		RegDest = Instr & 0b0000000000001111;	
	}
	else if(InstrType == 3)
	{
		/* Store */
		RegSourceA = Instr >> 8;
		RegSourceA = RegSourceA & 0b0000000000001111; 
		RegAddrMemory = Instr & 0b0000000011111111;
		
	}
	else if(InstrType == 8)
	{
		RegDest = Instr >> 8;
		RegDest = RegDest & 0b0000000000001111; 
	}
	else if(InstrType == 12)
	{
		RegAddrMemory = Instr & 0b0000000011111111;
	}

}

unsigned int getValueByCacheAddr(Cache cache, unsigned int MemAddr) 
{
	if(cache->blocks[MemAddr]->valid == 1)
	{
		cache->hits++;
		
		if(DEBUG)
		{
           printf("Value of cache memory: %i \n",cache->blocks[MemAddr]->data);
		}

		return cache->blocks[MemAddr]->data;
	}
	else
	{
		cache->misses++;
		
		cache->blocks[MemAddr]->valid = 1;
		cache->blocks[MemAddr]->dirty = 0;
        cache->blocks[MemAddr]->data = DataMemory[MemAddr];

		return NULL;
	}	
}

void execute(Cache cache)
{
	if(InstrType == 1)
	{
        // Load
		
		unsigned int addrMemoryValue = getValueByCacheAddr(cache, RegAddrMemory); 
		if( NULL != addrMemoryValue)
		{
			if(DEBUG)
			{
				printf("Using value from cache memory \n");
			}

			Reg[RegDest] = addrMemoryValue;
		}
		else
		{
			if(DEBUG)
			{
				printf("Using value from data memory \n");
			}

			Reg[RegDest] = DataMemory[RegAddrMemory];
		}
		
		
	}
	else if(InstrType == 3)
	{
		// Store
		DataMemory[RegAddrMemory] = Reg[RegSourceA];
		
	}
	else if(InstrType == 7)
	{
	     // Soma
		Reg[RegDest] = Reg[RegSourceA] + Reg[RegSourceB];
		
	}
	else if(InstrType == 15)
	{
		// Subtracao
		Reg[RegDest] = Reg[RegSourceA] - Reg[RegSourceB];
	
	}
	else if(InstrType == 8)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          
	{
		if(DEBUG)
		{
           printf( "Instrucao  PRINTREG : %i \n",Reg[RegDest]);
		}

	}
	else if(InstrType == 12)
	{
		if(DEBUG)
		{
           printf( "Instrucao  PRINTMEMO : %i \n",DataMemory[RegAddrMemory]);
		}

	}
}