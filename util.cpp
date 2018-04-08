/*
 * Author: Floris Creyf
 * Date: May 2015
 */

#include "util.h"
#include <stdio.h>
#include <stdlib.h>

void retrieve_LSB(int* sample, unsigned char* read_secret_bits, int* read_secret_cursor, bool huffTab_over15)
{
    int sam = abs(*sample);
    if ((sam > 14) && huffTab_over15)
        sam -= 15;
    push_bits_inc(read_secret_bits, sam << 31, 1, read_secret_cursor);
}

void embed_LSB(int* sample, unsigned char *secret_bits, int* secret_cursor, bool huffTab_over15)
{
    bool sign = (*sample > 0) ? true : false;
    bool linbitcase = false;
    int sam = abs(*sample);
    if ((sam > 14) && huffTab_over15) {
        sam -= 15;
        linbitcase = true;
    }
    if (get_bits_inc(secret_bits, secret_cursor, 1)) //ak je bit na zapisanie rovny jednej
        sam |= 1;
    else
        sam &= 0xFFFFFFFE;
    if (linbitcase)
        sam += 15;
    if (!sign)
        sam = -sam;
    *sample = sam;
}

void compare_buffers(unsigned char *orig_buffer, unsigned char *new_buffer, unsigned length, int frame_count, char what)
{
    for (int i = 0; i < length; i++)
        if (new_buffer[i] != orig_buffer[i]) {
            char a;
            printf("\nIncorrect %c reconstruction. Frame_count = %d, orig_buffer[%d] = %X, new_buffer[%d] = %X", what, frame_count, i, orig_buffer[i], i, new_buffer[i]);
            scanf("%c", &a);
        }
}

/*
 * Function for setting specific bits in side info (or wherever)
 * @bits have to be shifted towards MSB
 * @startbit position from MSB where buffer should be overwritten
*/
//void write_side_info_part (unsigned char *buffer, int bits, int bits_length, unsigned start_bit) {
void write_side_info_part (unsigned char *buffer, unsigned bits, int bits_length, unsigned start_bit) {
    int byte_cursor = 0;

	while (start_bit >= 8) {
		start_bit -= 8;
		byte_cursor++;
	}

	// loop to write bits to a single byte. Parameters to hand over: byte_cursor, start_bit, bits, bits_length
	while (bits_length > 0) {
//        int write_mask_len = bits_length > (8 - start_bit) ? 8 : bits_length;
        int write_mask_len = (bits_length > (8 - start_bit)) ? (8 - start_bit) : bits_length;
        unsigned char write_mask = ~(((1 << write_mask_len) - 1) << (8 - start_bit - write_mask_len));

        //apply write_mask
        buffer[byte_cursor] &= write_mask;
		//bits to write - change type to char
		buffer[byte_cursor] = buffer[byte_cursor] | ((char) (bits >> (32 - (8 - start_bit))));
		bits_length -= 8 - start_bit;

		//following is necessary for the case of another run of the loop
		bits <<= 8 - start_bit; //erase written bits by shifting 'bits' variable
		start_bit = 0;
		byte_cursor++;
	}

}

void push_bits_modified(unsigned char *dest_buffer, int* writing_bit_cursor, unsigned char *src_buffer, int start_bit, int end_bit) {
	int byte_cursor = 0;
	int length = end_bit - start_bit;
	while (start_bit >= 8) {
		start_bit -= 8;
		byte_cursor++;
	}

    int restOfByte;
    int lengthToWrite;
    while (length > 0) { //in each run max 8 bits are being pushed to dest_buffer
        restOfByte = 8 - start_bit;
        if (length < restOfByte)
            lengthToWrite = length;
        else
            lengthToWrite = restOfByte;

        push_bits_inc(dest_buffer, ((unsigned) ((src_buffer[byte_cursor] << start_bit) >> (8-lengthToWrite))) << (32-lengthToWrite), lengthToWrite, writing_bit_cursor);
        //updating loop parameters
        length -= lengthToWrite;
        byte_cursor++;
        start_bit = 0;
    }
}

/*
Example of usage: if three bits, 011 are to be inserted to *buffer, then:
bits = 0110 0000 0000 0000 0000 0000 0000 0000 (shifted towards MSB) and
bits_length = 3
Important is, that all following bits are 0
*/
void push_bits_inc(unsigned char *buffer, unsigned bits, int bits_length, int *bit_cursor)
{
	int byte_cursor = 0;
	int start_bit = *bit_cursor;

	while (start_bit >= 8) {
		start_bit -= 8;
		byte_cursor++;
	}
	*bit_cursor += bits_length;

	// loop to write bits to a single byte. Parameters to hand over: byte_cursor, start_bit, bits, bits_length
	while (bits_length > 0) {
		if (start_bit == 0) //write all zeros to byte
			buffer[byte_cursor] = 0x00;

		//bits to write - change type to char
		buffer[byte_cursor] = buffer[byte_cursor] | ((char) (bits >> (32 - (8 - start_bit))));
		bits_length -= 8 - start_bit;

		//following is necessary for the case of another run of the loop
		bits <<= 8 - start_bit; //erase written bits by shifting 'bits' variable
		start_bit = 0;
		byte_cursor++;
	}
}

unsigned get_bits(unsigned char *buffer, int start_bit, int end_bit)
{
	/* Decrement end_bit so that i < end_bit. */
	end_bit--;

	int start_byte = 0;
	int end_byte = 0;

	while (start_bit >= 8) {
		end_bit -= 8;
		start_bit -= 8;
		start_byte++;
		end_byte++;
	}
	while (end_bit >= 8) {
		end_bit -= 8;
		end_byte++;
	}

	/* Get the bits. */
	unsigned result = ((unsigned)buffer[start_byte] << (32 - (8 - start_bit))) >> (32 - (8 - start_bit));

	if (start_byte != end_byte) {
		while (++start_byte != end_byte) {
			result <<= 8;
			result += buffer[start_byte];
		}
		result <<= end_bit + 1;
		result += buffer[end_byte] >> (7 - end_bit);
	} else if (end_bit != 7)
		result >>= (7 - end_bit);

	return result;
}

unsigned get_bits_inc(unsigned char *buffer, int *offset, int count)
{
	unsigned result = get_bits(buffer, *offset, *offset + count);
	*offset += count;
	return result;
}

int char_to_int(unsigned char *buffer)
{
	unsigned num = 0x00;
	for (int i = 0; i < 4; i++)
		num = (num << 7) + buffer[i];
	return num;
}

