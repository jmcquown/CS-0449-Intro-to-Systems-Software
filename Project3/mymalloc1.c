#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "mymalloc.h"

//gcc -o ./mallocdrv ./mallocdrv.c ./mymalloc.c -g

struct Node {
	//Previous node in list
	struct Node *previous;
	//Next node in list
	struct Node *next;
	//char in C is 1 byte long
	//First bit says if the node is free or not
	//Next 7 bits are the size of the chunk
	char header;
};

//Struct for the table that represent the free chunks in memory
//An array of struct pointers is used
struct entryTable {
	//table[0] = 2^5, 32 bytes
	//table[1] = 2^6, 64 bytes
	//up to table[25] = 2^30, 1GB 
	struct Node *table[26];
}entryTable;

void coalesce(struct Node *node0, struct Node *node1, int powerOfTwo);

//Global var that will represent the location of the chunk
static void *base = NULL;

void *my_buddy_malloc(int size) {
	if (base == NULL) {
		//Create intial space of 1GB
		base = mmap(NULL, 1<<30, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, 0, 0);
		//Fill each element in the free table to NULL
		int i = 0;
		for (i = 0; i < 26; i++)
			entryTable.table[i] = NULL;

		//Create a new struct Node that will hold the information of the chunk
		struct Node start;
		//Set the header to 0 so that we know that the chunk is unallocated
		start.header = 0;
		//Set the size, or the last 7 bits, to 30 so that we know it represents 2^30 memory
		start.header = start.header | 30;
		//Initialize the previous and next pointers to NULL
		start.previous = NULL;
		start.next = NULL;

		//Set base to the starting address of the table
		entryTable.table[25] = (struct Node*)base;
		//Place the start node at the front of our 1GB chunk
		*entryTable.table[25] = start;
	}

	//In table, table[0] = 2^5 or 32 bytes, table[1] = 64 bytes, and so on until table[25] = 2^30 or 1,073,741,824 bytes (1 GB)
	//In order to get the correct index to allocate memory for the size that that user wants we need to divide the input size by two
	//We divide the size by two until it is less than or equal to 32, which is the minimal size for allocation
	int numOfDivides = 0;	//b *0x400a87 in gdb to get here
	double divResult = (double) size;
	while (divResult >= 32) {
		numOfDivides = numOfDivides + 1;
		divResult = divResult / 2;
	}

	//If the location in the table is already occupied
	if (entryTable.table[numOfDivides] != NULL) {
		//Set a new pointer equal to occupied chunk
		struct Node *locToReturn = entryTable.table[numOfDivides];
		//Set the first bit to 1, so that the program knows it is now occupied
		locToReturn->header = locToReturn->header | 128;
		//Make table[numOfDivides] equal to the next Node
		entryTable.table[numOfDivides] = locToReturn->next;
		//Not sure why this is done....
		if (locToReturn->next != NULL)
			entryTable.table[numOfDivides]->previous = NULL;

		//Return everything except for the header information. Which is why I increase by 1.
		return ((char *)locToReturn + 1);
	}
	//Else there is nothing occupying the location in the free chunk table
	else {
		//Create an int index that will hold the value of the appropriate index that we want to allocate at in the table
		int index = numOfDivides;

		//In order to see which index in the table is actually occupied we need to check each index
		//starting from the value of "index" until we find a value that is not NULL
		while (entryTable.table[index] == NULL)
			index++;
		//This will set index to the appropriate place in the table in which we can start splitting in order to get the correct size

		//Now we need to split until we get to the correct index (numOfDivides)
		while (index != numOfDivides) {
			//Create a new pointer and set it equal to the chunk that needs to be split
			struct Node *chunk = entryTable.table[index];	//b *0x400b6c
			//Remove the pointer from the table
			//Set the right chunk to the head of the index
			entryTable.table[index] = chunk->next;

			//Makes the previous pointer of the new header equal to NULL
			if (chunk->next != NULL)
				entryTable.table[index]->previous = NULL;

			//Set a new char (1 byte) to the current chunks header
			//This is done in order to get the power of two that the current index is at
			char powerOfTwo = chunk->header;
			//powerOfTwo << 1 gives us the size of the index that we are currently at. 
			//Then dividing by two gives us the size of the chunks that we are about to create
			int newSize = (1 << powerOfTwo) / 2;

			//This is the formula that was given on the FAQ
			struct Node *buddy = (struct Node*) ( ((char *)chunk) + newSize);
			
			//I need to create new nodes in order to store them inside of the chunks of memory
			struct Node node0;
			struct Node node1;
			//Because index is initially set to numOfDivides, in order to reference table[0], index or numOfDivides would be equal to 1
			//For this reason I subtract 1 from index. I then add 5 in order to place the correct power of two in the header's 7 bits for the size
			node0.header = (index - 1) + 5;
			node1.header = (index - 1) + 5;

			//Set the next and previous pointers of each appropriately
			node0.next = buddy;
			node1.next = NULL;

			node0.previous = NULL;
			node1.previous = chunk;

			//Have the chunks in memory point to the nodes I just created
			*chunk = node0;
			*buddy = node1;

			//We then need to add these chunks to the table
			//Check if index - 1 in the table is NULL. If it is then set that location equal to chunk
			if (entryTable.table[index - 1] == NULL)
				entryTable.table[index - 1] = chunk;
			//Else the index is already occupied by something
			else {
				//Need to insert the two nodes that we just created into the list before the node that is currently at the index
				buddy->next = &(*entryTable.table[index - 1]);
				entryTable.table[index - 1]->previous = &(*buddy);
				entryTable.table[index - 1] = &(*chunk);
			}

			//Decrement the index and keep checking each element of the table
			//This will keep happening until index == numOfDivides
			index--;
		}
		//At this point we are at the correct index in the table and have split enough times
		//Remove the chunk from the table and return the location to the user
		struct Node *locToReturn = entryTable.table[numOfDivides];

		//Remove the current node from the table
		entryTable.table[numOfDivides] = locToReturn->next;
		if (locToReturn->next != NULL)
			entryTable.table[numOfDivides]->previous = NULL;
		//Set occupancy bit to 1
		locToReturn->header = locToReturn->header | 128;
		//Return without the header
		return ((char *) locToReturn + 1);
	}
	return NULL;

}

void my_free(void *ptr)
{
	char *pointer = ptr;

	// The pointer given to us points to the byte after the header, so we must decrement 
	// our pointer 1 byte to have the correct pointer
	pointer  = pointer - 1;

	// We first want to turn off the first bit;
	pointer[0] = pointer[0] & 127;

	// First we grab the first byte in the memory that was given to us by the user.
	// This will contain the header information that we need for the merging algorithm.
	char blockSize = pointer[0];

	// We then convert the size we extract from the header into an index for our freeTable.
	// We do this by subtracting 5 from our size. We subtract five since we start at 2^5.
	int index = ((int)blockSize) - 5;

	// We must now perform the buddy algorithm to see if the buddies are available to be merged.
	// While the buddy is free to be merged, we merge the memory. At the end we will add the memory
	// back to the freeTable. 

	// Get the offset of our pointer so we can treat our first address as 0
	int addr = (pointer - (char *)base);

	// We now get the size of our block
	int size = 1<<blockSize;

	// Now we calculate the address offset of our buddy
	int buddyOffset = (addr ^ size);

	// We'll add this offset to base to get the location of our buddy
	char *buddy = ((char *)base) + buddyOffset; 

	// Now that we have the address of our buddy, we have to check the header bit to see if it is
	// a 0 or 1. If it is a 1, then the buddy is not free, so we just add our chunk back to the free table
	// If it is a 0, then this means that our buddy is free, and we can merge our chunks and check for
	// another open buddy.
	char buddyHeader = buddy[0];
	int bit7 = buddyHeader>>7 & 1;

	struct Node *block = (struct Node*)pointer;
	struct Node front;
	front.header = blockSize;
	front.next = NULL;
	front.previous = NULL;
	*block = front;


	// This is where we will merge buddies until the occupancy bit is a 1
	while(bit7 == 0)
	{
		// First make a new node to be the head of the new chunk
		struct Node head;
		head.header = blockSize + 1;
		head.previous = NULL;
		head.next = NULL;

		// We then add our node to the beginning of the memory
		*block = head;

		// We know where our buddy is and we know it is free. We now check for two things.
		// If the buddy.previous is NULL, it means that it is the first chunk in the free list,
		// so we should make freeList at that index point to buddy.next. If buddy.previous is
		// not NULL, then this means that it is in the middle of our freelist somwhere, and we should
		// make the node before the buddy point to the node after the buddy.
		struct Node *node_buddy = (struct Node*)buddy;

		if( (*node_buddy).previous == NULL ) 
		{
			entryTable.table[index] = (*node_buddy).next;
		}
		else
		{
			(*(*node_buddy).previous).next = (*node_buddy).next;
		}	

		// If the address of the buddy is less than the address of the other buddy,
		// we update our pointer to point to the first buddy
		if(buddy < pointer)
		{
			pointer = buddy;
			block = (struct Node*)pointer;
			*block = head;
		}

		// Now we've found and removed the buddy from our free table, thereby merging the buddies together.
		// We must now increment blockSize by 1 and start the process of finding the 7th bit of the buddy
		// header again.
		blockSize = blockSize + 1;
		index = ((int)blockSize) - 5;

		// if we reach the top of the table, we will be done
		if(index == 25)
		{
			break;
		}

		size = 1<<blockSize;
		buddyOffset = (addr ^ size);
		buddy = ((char *)base) + buddyOffset; 
		buddyHeader = buddy[0];
		bit7 = buddyHeader>>7 & 1;
		
		// This will keep going until the occupancy bit is 1, which means the buddy isn't free. At that point
		// we will add our chunk to the correct position in the free table
	}

	// Add block to correct place in the freeTable and we are done
	int indexToPlace = ((int)pointer[0]) - 5;

	if( entryTable.table[indexToPlace] != NULL )
	{
		(*entryTable.table[indexToPlace]).previous = block;
		entryTable.table[indexToPlace] = block;
	}
	else
	{
		entryTable.table[indexToPlace] = block;
	}
}