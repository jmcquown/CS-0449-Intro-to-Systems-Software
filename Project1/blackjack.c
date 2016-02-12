#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

int randomCard();

int main () {
	time_t t;
	int player1, player2, dealer1, dealer2, playerSum, dealerSum;
	char string[25];	//Completely random number I picked
	int playerStand = 0, dealerStand = 0;
	srand((unsigned) time(&t));	//I guess I have to do this before using rand()?

	//Generate two cards for the the player
	player1 = randomCard();
	player2 = randomCard();

	//Generate two cards for the dealer
	dealer1 = randomCard();
	dealer2 = randomCard();

	

	while (1) {
		if (playerStand + dealerStand == 2) {
			//If the player's total is greater than the dealers, the player wins
			if ((dealer1 + dealer2) < (player1 + player2))
				printf("You win!\n\n");
			//The dealer's total is greater than the players
			if ((dealer1 + dealer2) > (player1 + player2))
				printf("Dealer wins\n\n");
			//Print the results
			printf("The Dealer: \n%d + %d = %d\n", dealer1, dealer2, dealer1 + dealer2);
			printf("You: \n%d + %d = %d\n", player1, player2, (player1 + player2));
			exit(0);
		}
		//Print the hands of both players appropriately
		printf("The Dealer: \n%d + ?\n", dealer1);
		printf("You: \n%d + %d = %d\n", player1, player2, (player1 + player2));

		//If the player sum is equal to 21, he wins
		if ((player1 + player2) == 21) {
			printf("You win!\n");
			exit(0);
		}
		//If the dealer sum is equal to 21, he wins
		if ((dealer1 + dealer2) == 21) {
			printf("Dealer wins.\n");
			exit(0);
		}
		//Ask user if they want to hit or stand
		printf("Would you like to \"hit\" or \"stand\"? ");
		fgets(string, sizeof(string), stdin);	//Get string input

		//If the user inputs "hit"
		if (strncmp(string, "hit", 3) == 0) {
			player1 = player1 + player2;	//Set player1 equal to the sum in order to display properly
			player2 = randomCard();			//Generate new random card
			//If the new card is an ace
			if ((player2 == 11) || (player2 == 1)) {
				//If the sum if greater than 21, set the ace equal to 1
				if ((player1 + player2) > 21)
					player2 = 1;
				//If the current sum is less than 21, check if adding 10 would make it go over 21
				//If so, set the ace equal to 11
				else if ((player1 + player2 + 10) < 21)
					player2 = 11;
			}

			//If the new card + the sum is greater than or equal to 21
			if ( (player1 + player2) > 21) {
				printf("%d + %d = %d BUSTED!\n", player1, player2, (player1 + player2));
				printf("You busted, dealer wins\n");
				exit(0);	//Exit program
			}
		}
		
		//If the user inputs "stand" set the int to 1;
		else if (strncmp(string, "stand", 5) == 0)
			playerStand = 1;

		//If the sum of the dealer's cards are less than 17, he must hit
		if (dealer1 + dealer2 < 17) {
			dealer1 = dealer1 + dealer2;
			dealer2 = randomCard();

			//If the dealer sum is greater than 21, the player wins
			if ((dealer1 + dealer2) > 21) {
				printf("You Win, Dealer busted.\n");
				printf("Dealer: %d + %d = %d\n", dealer1, dealer2, dealer1 + dealer2);
				exit(0);
			}
		}
		//Else set the dealer stand to 1
		else
			dealerStand = 1;
		
	}


	return 0;
}

//Creates a random number and returns the value of a card
int randomCard() {
	int cardValues [] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 10, 10, 11};
	int randomNum = (rand() % 14);

	return cardValues[randomNum];
}

