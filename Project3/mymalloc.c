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

void my_free(void *ptr) {
	//1. Free the block of memory
	//2. Look at the neighboring block. Check if it's free
	//3. If it is free, combine the two. Go back to step 2

	char *ptrToNode = ptr;
	//*ptr is passed in without the header information
	//To get the header info we have to subtract the pointer by 1
	ptrToNode = ptrToNode - 1;
	//Then we need to set the occupancy bit to 0 to indicate that it is free
	ptrToNode[0] = ptrToNode[0] & 127;	//127 = 01111111
	char chunkSize = ptrToNode[0];
	//Get the index of the table from the size in the header
	//ptrToNode holds the power of 2, therefore to get the appropriate index I subtract by 5 (table[0] = 2^5)
	int index = (int) ptrToNode[0] - 5;
	//Calculate address of ptr 
	int addressOfPointer = ptrToNode - (char *)base;
	//Calculate the size of ptr. Shift 1 by the power of two that is in the pointer header
	int sizeOfPointer = 1 << ptrToNode[0];
	//The FAQ says that I "gotta re-add the base to the buddy address"
	//I just set the address and size equal to variables in order to clean up the the formula
	char *buddy = (addressOfPointer ^ sizeOfPointer) + (char *)base;

	//Set everything in the current node to NULL
	struct Node *ptrChunk = (struct Node*) ptrToNode;

	//So I've found the neighboring (buddy) block and now I need to check if it is free
	//Shift the header of the buddy to the right by 7 bits, then and it with 1
	int occupancyBit = (buddy[0] >> 7) & 1;

	

	//If the buddy is free, then coalesce
	if (occupancyBit == 0) {
		struct Node *ptrToBuddy = (struct Node*) buddy;
		coalesce(ptrChunk, ptrToBuddy, chunkSize);
	}
	
	//If the occupancy bit is 1, then we need to just add the ptr to the table
	//If the index is null, then just add the chunk to the table
	if (entryTable.table[index] == NULL)
		entryTable.table[index] = ptrChunk;
	//Else the index holds something
	else {
		entryTable.table[index] = ptrChunk->next;
		entryTable.table[index] = ptrChunk;
		
	} 


}

//Coalesce by calculating the buddy address then calling a method with those two nodes and their order of magnitude
	//Inside method check:
		//1) Is the memory at the max (2^30) - would the buddy be out of bounds
		//2) Find which node is left by subtracting their addresses - Set a new node* equal to the respective node that was passed in
		//3) Find the buddy, check if it's free
			//If it is remove from the list and coalesce
			//Else free the current node
void coalesce(struct Node *node0, struct Node *node1, int powerOfTwo) {
	if (powerOfTwo == 30) {
		return;
	}
	//Calculate the difference of the two node's addresses
	struct Node *leftNode;
	//If the first node's address is larger than the second node's address
	//Then node0 is the left chunk and node1 is the right chunk
	if (node1 - node0 > 0) {
		//Left chunk is node0
		leftNode = node0;
		//If the first element in the linked list at the index is the left buddy
		if (entryTable.table[powerOfTwo - 5] == node0)
			//Set the first element to the next of the left chunk
			entryTable.table[powerOfTwo - 5] = node0->next;
		//Else remove the chunk from the list this way
		else 
			(node0->previous)->next = node0->next;
		//Else if the first element in the linked list at the index is the right buddy
		if (entryTable.table[powerOfTwo - 5] == node1)
			//Set the first element to the next of the right chunk
			entryTable.table[powerOfTwo - 5] = node1->next;
		else
			(node1->previous)->next = node1->next;
	}
	//If node1's address is larger than node0's address
	//Node1 is the left chunk and node0 is the right chunk
	else {
		//Left chunk is node1
		leftNode = node1;
		//First element is the left chunk
		if (entryTable.table[powerOfTwo - 5] == node1)
			//Set the first element to the next of the left chunk
			entryTable.table[powerOfTwo - 5] = node1->next;
		else
			(node1->previous)->next = node1->next;
		//If the right chunk is the first element in the list
		if (entryTable.table[powerOfTwo - 5] == node0)
			//Set the first element to the next of the right chunk
			entryTable.table[powerOfTwo - 5] = node0->next;
		else
			(node0->previous)->next = node0->next;
	}
	//Reset the next and previous for the two nodes
	// node0->next = NULL;
	// node0->previous = NULL;
	// node1->next = NULL;
	// node1->previous = NULL;


	//Set the leftNode to the appropriate size and index in the table
	//Increase the size
	leftNode->header = powerOfTwo + 1;
	//If the left chunk is the first element in the table at that index
	if (leftNode->previous == NULL)
		//Set its next equal to the first element
		leftNode->next = entryTable.table[powerOfTwo - 5];
	//Else leftNode is somewhere else in the list
	else 
		//Set the previous' next of the left chunk equal to the next of the left chunk
		(leftNode->previous)->next = leftNode->next;	//Seg fault here...
		

	//Set the current first element of the appropriate index to the leftNode's next
	entryTable.table[leftNode->header] = leftNode->next;
	//Set the first element equal to the leftNode
	entryTable.table[leftNode->header] = leftNode;

	//Need to find he address of the buddy and check if it's free
	//Use the same process as before the get the occupancy bit of the buddy and calculate its address
	char *headerPointer = leftNode;
	int size = 1 << headerPointer[0];
	int address = headerPointer - (char *)base;
	char *buddy = (address ^ size) + (char *)base;

	char buddyHeader = buddy[0];
	int occupancyBit = buddyHeader >> 7 & 1;
	struct Node *buddyPointer = (struct Node*) buddy;

	//If the buddy is free, call coalesce
	if (occupancyBit == 0)
		coalesce(leftNode, buddyPointer, leftNode->header);
	else 
		return;

}
