#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Header {
	unsigned short start;			//JPEG start of file maker
	unsigned short APP1;			//JPEG APP1 marker
	unsigned short APP1_Length;		//Length of APP1 block
	char EXIF_String[4];			//Exif string
	unsigned short null_Term;		//NULL terminator or zero byte
	char endian[2];					//Endianness
	unsigned short version_number;	//is always 42
	unsigned int offset_start;		//Offset to start of Exif block from start of TIFF block (byte 12 of the file)
};

struct TIFF_Tag {
	unsigned short identifier;	//Tag identifier - number of items in the TIFF tag
	unsigned short data_type;	//Data type
	unsigned int num_data;		//Number of data items
	unsigned int offset_data;	//Value or offset of data items
};



int main(int argc, char **argv) {
	unsigned short count = 0;
	//Allocate space for the header struct
	struct Header *header = malloc(sizeof(struct Header));
	
	//Create new file
	FILE *file = fopen(argv[1], "r");

	//Read in the header data to the struct
	fread(header, 20, 1, file);

	//Testing purposes, prints each var in the header struct
	// printf("Start: 0x%x\n", header->start);
	// printf("APP1: 0x%x\n", header->APP1);
	// printf("APP1_Length: %hu\n", header->APP1_Length);
	// printf("EXIF_String: %s\n", header->EXIF_String);
	// printf("null_Term: 0x%x\n", header->null_Term);
	// printf("endian: %s\n", header->endian);
	// printf("version_number: %hu\n", header->version_number);
	// printf("offset_start: %d\n", header->offset_start);

	//If the endian string is equal to "MM" exit 
	if (strcmp(header->endian, "MM") == 0) {
		printf("Error. Big Endian.\n");
		exit(0);
	}
	if(strcmp(header->EXIF_String, "Exif") != 0) {
		printf("Error. Exif String is %s\n", header->EXIF_String);
		exit(0);
	}

	//Read in the number of TIFF tags
	fread(&count, 2, 1, file);
	//Allocate memory for the appropiate amount of TIFF tags (count * the size)
	struct TIFF_Tag *tiff = malloc(count * sizeof(struct TIFF_Tag));

	//Read in the TIFF tags count number of times?
	for (int i = 1; i < count + 1; i++) {
		fread(tiff, 12, 1, file);
		
		//Tests and prints out the values for each TIFF tag variable
		// printf("Tag: 0x%x\n", tiff->identifier);
		// printf("Data Type: %hu\n", tiff->data_type);
		// printf("Num Data: %d\n", tiff->num_data);
		// printf("Offset: %d\n\n", tiff->offset_data);

		//If tag identifier = 0x010F
		if ((tiff->identifier) == 0x010F) {
			//Create new char array with the number data as the length
			char str[tiff->num_data];
			//fseek to the appropriate place in memory 12+offset
			fseek(file, (tiff->offset_data) + 12, SEEK_SET);
			//Read the string from file
			fread(str, tiff->num_data, 1, file);
			
			printf("Manufacturer: %s\n", str);
			//fseek from the beginning to 22 (Where the TIFF tags are) + 12(Size of a TIFF tag) * i (the number of read TIFF tags)
			fseek(file, 22 + (12 * i), SEEK_SET);
			
		}
		//If tag identifier = 0x0110
		if ((tiff->identifier) == 0x0110) {
			char str[tiff->num_data];
			fseek(file, (tiff->offset_data) + 12, SEEK_SET);
			fread(str, tiff->num_data, 1, file);
			printf("Model:        %s\n", str);
			fseek(file, 22 + (12 * i), SEEK_SET);
		}
		//If tag identifier = 0x8769
		if ((tiff->identifier) == 0x8769) {
			//Seek to the offset + 12 then break out of the loop
			fseek(file, tiff->offset_data + 12, SEEK_SET);
			break;
		}
	}

	
	//Reset count
	count = 0;
	fread(&count, 2, 1, file);	//Get the new count
	struct TIFF_Tag *newTiff = malloc(count * sizeof(struct TIFF_Tag));
	//Save the current position in the file using ftell()
	int currPosition = ftell(file);

	for (int i = 1; i < count + 1; ++i) {
		fread(newTiff, 12, 1, file);

		//Tests and prints out the values for each TIFF tag variable
		// printf("Tag: %x\n", tiff->identifier);
		// printf("Data Type: %hu\n", tiff->data_type);
		// printf("Num Data: %d\n", tiff->num_data);
		// printf("Offset: %d\n\n", tiff->offset_data);

		//If the width tag is found
		if ((newTiff->identifier) == 0xA002)
			printf("Width:        %d pixels\n", newTiff->offset_data);
		//If the height tag is found
		if ((newTiff->identifier) == 0xA003)
			printf("Height:       %d pixels\n", newTiff->offset_data);
		//If the ISO tag is found
		if ((newTiff->identifier) == 0x8827)
			printf("ISO:          ISO %d\n", newTiff->offset_data);
		//If the Exposure speed tag is found
		if ((newTiff->identifier) == 0x829a) {
			unsigned int exposure1, exposure2;
			fseek(file, newTiff->offset_data + 12, SEEK_SET);
			fread(&exposure1, sizeof(unsigned int), 1, file);
			fread(&exposure2, sizeof(unsigned int), 1, file);

			printf("Exposure:     %d/%d second\n", exposure1, exposure2);
			//Need to get back to the correct position
			fseek(file, (12 * i) + currPosition, SEEK_SET);
		}
		//If the F-Stop tag is found
		if ((newTiff->identifier) == 0x829d) {
			unsigned int fstop1, fstop2;
			fseek(file, newTiff->offset_data + 12, SEEK_SET);
			fread(&fstop1, sizeof(unsigned int), 1, file);
			fread(&fstop2, sizeof(unsigned int), 1, file);
			//Need to fix format specifier
			printf("F-Stop:       f/%0.1f second\n", (double)fstop1/(double)fstop2);
			//Need to get back to the correct position
			fseek(file, (12 * i) + currPosition, SEEK_SET);
		}
		//If the Lens focal length tag is found
		if ((newTiff->identifier) == 0x920A) {
			unsigned int focal1, focal2;
			fseek(file, newTiff->offset_data + 12, SEEK_SET);
			fread(&focal1, sizeof(unsigned int), 1, file);
			fread(&focal2, sizeof(unsigned int), 1, file);
			printf("Focal Length: %d mm\n", focal1/focal2);
			//Need to get back to the correct position
			fseek(file, (12 * i) + currPosition, SEEK_SET);
		}
		//If the Date Taken tag is found
		if ((newTiff->identifier) == 0x9003) {
			char str[newTiff->num_data];
			fseek(file, newTiff->offset_data + 12, SEEK_SET);
			fread(str, newTiff->num_data, 1, file);
			printf("Date Taken:   %s\n", str);
			fseek(file, (12 * i) + currPosition, SEEK_SET);
		}
	}
}