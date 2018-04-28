#ifndef UTIL_H
#define	UTIL_H

void retrieve_LSB(int* sample, unsigned char* read_secret_bits, int* read_secret_cursor, bool huffTab_over15);

void embed_LSB(int* sample, unsigned char *secret_bits, int* secret_cursor, bool huffTab_over15);

void compare_buffers(unsigned char *orig_buffer, unsigned char *new_buffer, unsigned length, int frame_count, char what);

void write_side_info_part (unsigned char *buffer, unsigned bits, int bits_length, unsigned start_bit);

/**
 * Copies data FROM src_buffer <start_bit; end_bit)
 * TO dest_buffer starting from bit number *writing_bit_cursor
 * works for arbitrary buffer lengths
*/
void push_bits_modified(unsigned char *dest_buffer, int* writing_bit_cursor, unsigned char *src_buffer, int start_bit, int end_bit);

/**
 * Copies bits FROM "bits" of length "bits_length" shifted towards MSB of a 4-byte integer
 * TO buffer starting from bit number *bit_cursor
 * WARNING: works for "bits" of length max. 32 bits!
 * For copying longer buffers use push_bits_modified
*/
void push_bits_inc(unsigned char *buffer, unsigned bits, int bits_length, int *bit_cursor);


/**
 * Assumes that end_bit is greater than start_bit and that the result is less than
 * 32 bits, length of an unsigned type.
 * TODO Could function a bit more efficiently.
 * @param buffer
 * @param start_bit
 * @param end_bit
 * @return {unsigned}
 */
unsigned get_bits(unsigned char *buffer, int start_bit, int end_bit);

/**
 * Uses get_bits() but mutates offset so that offset == offset + count.
 * @param buffer
 * @param offset
 * @param count
 */
unsigned get_bits_inc(unsigned char *buffer, int *offset, int count);

/** Puts four bytes into a single four byte integer type. */
int char_to_int(unsigned char *buffer);

#endif	/* UTIL_H */

