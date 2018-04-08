/* Author of MP3 Decoder: Floris Creyf
 * Author of MDCT insertion and retrieval: Stefan Knaperek
 */

#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h> /* dnf install alsa-lib-devel */
#include <vector>
#include "id3.h"
#include "mp3.h"
#include "xing.h"
#include "util.h"

#include <iostream>
#include <iomanip>
using namespace std;

#define ALSA_PCM_NEW_HW_PARAMS_API

/**
 * Start decoding the MP3 and let ALSA hand the PCM stream over to a driver.
 * @param mp3_decode
 * @param buffer A buffer containing the MP3 bit stream.
 * @param offset An offset to a MP3 frame header.
 */
inline void stream(mp3 &decoder, unsigned char *buffer, unsigned offset)
{
	unsigned sampeling_rate = decoder.get_sampling_rate();
	unsigned channels = decoder.get_channel_mode() == 3 ? 1 : 2;
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *hw = NULL;
	snd_pcm_uframes_t frames = 128;

	if (snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0)
		exit(1);

	snd_pcm_hw_params_alloca(&hw);
	snd_pcm_hw_params_any(handle, hw);

	if (snd_pcm_hw_params_set_access(handle, hw, SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
		exit(1);
	if (snd_pcm_hw_params_set_format(handle, hw, SND_PCM_FORMAT_FLOAT_LE) < 0)
		exit(1);
	if (snd_pcm_hw_params_set_channels(handle, hw, channels) < 0)
		exit(1);
	if (snd_pcm_hw_params_set_rate_near(handle, hw, &sampeling_rate, NULL) < 0)
		exit(1);
	if (snd_pcm_hw_params_set_period_size_near(handle, hw, &frames, NULL) < 0)
		exit(1);
	if (snd_pcm_hw_params(handle, hw) < 0)
		exit(1);
	if (snd_pcm_hw_params_get_period_size(hw, &frames, NULL) < 0)
		exit(1);
	if (snd_pcm_hw_params_get_period_time(hw, &sampeling_rate, NULL) < 0)
		exit(1);

    int frame_count = 0;
    unsigned char *dummy;
	/* Start decoding. */
	while (decoder.is_valid()) {
		decoder.init_header_params(&buffer[offset], dummy);
		if (decoder.is_valid()) {

			decoder.init_frame_params(&buffer[offset], dummy, frame_count++, dummy, 0, 0);
			offset += decoder.get_frame_size();
		}

		int e = snd_pcm_writei(handle, decoder.get_samples(), 1152);
		if (e == -EPIPE)
			snd_pcm_recover(handle, e, 0);
	}

	snd_pcm_drain(handle);
	snd_pcm_close(handle);
}

inline void write_secret_data(mp3 &deco_enco, unsigned char *buffer, unsigned offset, unsigned char **new_buffer, unsigned char *secret_bits, int* secret_cursor, int pf, unsigned* buffer_length, unsigned secret_buffer_size) {
//	int *dct1;
	int frame_count = 0;
	unsigned write_offset = offset;

    while (deco_enco.is_valid() && (frame_count < pf)) { // new run === new frame
		deco_enco.init_header_params(&buffer[offset], *new_buffer + write_offset);
		int constant = deco_enco.mono ? 21 : 36;
		if (deco_enco.is_valid()) {
			deco_enco.init_frame_params(&buffer[offset], *new_buffer + write_offset, frame_count, secret_bits, secret_cursor, secret_buffer_size, false);

			offset += deco_enco.get_frame_size();
			write_offset += deco_enco.get_write_frame_size();

			if (*buffer_length - write_offset < deco_enco.write_frame_size + 500) {
                *buffer_length += 50 * deco_enco.write_frame_size;
                *new_buffer = (unsigned char*) realloc(*new_buffer, *buffer_length); //extend new_buffer to carry 50 more frames
                if (*new_buffer == NULL) {
                    free(*new_buffer);
                    puts ("Error (re)allocating memory");
                    exit (1);
                }
//                else
//                    printf("\nSuccessfull reallocation!");
			}

//			deco_enco.interleave_dct();
//			dct1 = deco_enco.get_samples_dct();
//          memcpy(&dct[frame_count*2304], dct1, 2304 * sizeof(int));
			frame_count++;

		}
	}
	//crop new_buffer to desired size
	*buffer_length = write_offset;
	*new_buffer = (unsigned char*) realloc(*new_buffer, *buffer_length);
    if (*new_buffer == NULL) {
         free(*new_buffer);
         puts ("Error (re)allocating memory");
         exit (1);
    }
//    else
//        printf("\nSuccessfull reallocation!");

    printf("\nAverage bitrate of file: %d kbps", (unsigned) (*buffer_length * 8 / (1024 *frame_count * 0.026)));

//	printf("\nwritten samples:\n");
//    for (int i = 0; i < pf*2304; i++) {
//        printf("%i ", dct[i]);
//    }
}

//inline void read_secret_data(mp3 &decoder, unsigned char *buffer, unsigned offset, int* written_dct, int pf, unsigned char* read_secret_bits, int* read_secret_cursor)
inline void read_secret_data(mp3 &decoder, unsigned char *buffer, unsigned offset, int pf, unsigned char** read_secret_bits, int* read_secret_cursor, unsigned* read_secret_buffer_size)
{
	int frame_count = 0;
//	int dct[pf*2304];
//	int *dct1;
    unsigned char *dummy;
	/* Start decoding. */
	while (decoder.is_valid() && (frame_count < pf)) {
		decoder.init_header_params_extract_data(&buffer[offset], dummy);
		if (decoder.is_valid()) {

			decoder.init_frame_params_extract_data(&buffer[offset], frame_count, *read_secret_bits, read_secret_cursor);
			offset += decoder.get_frame_size();

			if ((*read_secret_buffer_size - (*read_secret_cursor / 8)) < (decoder.get_frame_size() / 5)) { //if space left in secret buffer is less than 20% of a frame size
                *read_secret_buffer_size += 5 * decoder.get_frame_size();
                *read_secret_bits = (unsigned char*) realloc(*read_secret_bits, *read_secret_buffer_size); //extend *read_secret_bits
                if (*read_secret_bits == NULL) {
                    free(*read_secret_bits);
                    puts ("Error (re)allocating memory");
                    exit (1);
                }
//                else
//                    printf("\nSuccessfull reallocation!");

			}

//			decoder.interleave_dct();
//			dct1 = decoder.get_samples_dct();
//			memcpy(&dct[frame_count*2304], dct1, 2304 * sizeof(int));
			frame_count++;
		}
	}
    //crop read_secret_bits
	while ((*read_secret_cursor % 8) != 0) //lossy operation: flooring *read_secret_cursor to align to bytes
            *read_secret_cursor = *read_secret_cursor - 1;
    *read_secret_buffer_size = *read_secret_cursor / 8;
	*read_secret_bits = (unsigned char*) realloc(*read_secret_bits, *read_secret_buffer_size);
    if (*read_secret_bits == NULL) {
         free(*read_secret_bits);
         puts ("Error (re)allocating memory");
         exit (1);
    }
//    else
//        printf("\nSuccessfull reallocation!");

//	printf("\nincorrectly decoded samples:\n");
//    for (int i = 0; i < pf*2304; i++) {
//        if (dct[i] != written_dct[i]) {
//            int f = i / 2304;
//            int g = (i % 2304) / 1152;
//            int s = ((i % 2304) % 1152) / 2;
//            int c = ((i % 2304) % 1152) % 2;
//            printf("\ni = %i, frame %d gr%d ch%d sam %d: dct[i] = %i, written_dct[i] = %i", i, f, g, c, s, dct[i], written_dct[i]);
//        }
//
//    }

}



unsigned char *get_file(const char *dir, unsigned *buffer_length)
{
	std::ifstream file(dir, std::ios::in | std::ios::binary);
	file.seekg(0, file.end);
	*buffer_length = file.tellg();
	file.seekg(0, file.beg);
	char *buffer = new char[*buffer_length];
	file.read(buffer, *buffer_length);
	file.close();
	return reinterpret_cast<unsigned char *>(buffer);
}

void set_stego_file(char *dir, unsigned char *buffer, unsigned buffer_length)
{
//    dir[0] = '_';
    char array1[strlen(dir)];
    memset(array1, '\0', strlen(dir));
    strncpy(array1, dir, strlen(dir) - 4);
    char array2[] = "_stego.mp3";
    char * newDir = new char[strlen(dir)+5+1];
    strcpy(newDir,array1);
    strcat(newDir,array2);
//    printf("\n%s", newDir);
    std::ofstream file(newDir, std::ios::out | std::ios::binary);
    delete [] newDir;
    file.write(reinterpret_cast<const char *>(buffer), buffer_length);
    file.close();
}

void set_retrieved_file(char *dir, unsigned char *buffer, unsigned buffer_length)
{
//    dir[0] = '_';
    char array1[strlen(dir)];
    memset(array1, '\0', strlen(dir));
    strncpy(array1, dir, strlen(dir) - 4);
    char array2[] = "_retrieved";
    char array3[8];
    memset(array3, '\0', 8);
    strncpy(array3, dir + strlen(dir) - 4, 4);
//    char* newDir = new char[strlen(dir)+9+1];
    char* newDir = new char[strlen(dir)+8];
    memset(newDir, '\0', strlen(dir) + 8);
    strcpy(newDir, array1);
    strcat(newDir, array2);
    strcat(newDir, array3);
//    printf("\n%s", newDir);
    std::ofstream file(newDir, std::ios::out | std::ios::binary);
    delete [] newDir;
    file.write(reinterpret_cast<const char *>(buffer), buffer_length);
    file.close();
}

std::vector<id3> get_id3_tags(unsigned char *buffer, unsigned &offset)
{
	std::vector<id3> tags;
	int i = 0;
	bool valid = true;

	while (valid) {
		id3 tag(&buffer[offset]);
		if (valid = tag.is_valid()) {
			tags.push_back(tag);
			offset += tags[i++].get_id3_offset() + 10;
		}
	}

	return tags;
}

unsigned get_id3_length(unsigned char *buffer) {

    unsigned offset = 0;

    if (buffer[0] == 'I' && buffer[1] == 'D' && buffer[2] == '3') {
        for (int i = 0; i < 4; i++) {
            offset += get_bits(&buffer[9 - i], 1, 8) << (7 * i);
        }

        if (buffer[5] & 0x10 > 0)
            offset += 20;
        else
            offset += 10;
    }

    while (buffer[offset] != 0xFF || buffer[offset + 1] < 0xE0) //make sure buffer[offset] is the beginning of a frame
        offset++;
    return offset;
}

int main(int argc, char **argv)
{
	if (argc > 3) {
		printf("Unexpected number of arguments.\n");
		return -1;
	} else if (argc == 1) {
		printf("No directory specified.\n");
		return -1;
	}

	try {
	    unsigned buffer_length = 0;
		unsigned char *buffer = get_file(argv[1], &buffer_length);
//		unsigned char new_buffer[buffer_length]; //temporary definition. Need for dynamic allocation
        unsigned char *new_buffer;
        new_buffer = (unsigned char*) malloc (buffer_length);
        if (new_buffer == NULL) exit (1);


        int process_frames = 9000; //maximal number of frames to process
//        int secret_buffer_size = 600000;
        unsigned secret_buffer_size;
        unsigned char *secret_bits = get_file(argv[2], &secret_buffer_size); //loading content to hide in mp3 file
//		unsigned char secret_bits[secret_buffer_size];
//        memset(secret_bits, 0xA6, secret_buffer_size);
		int secret_cursor = 0;

		unsigned offset = get_id3_length(buffer);

        if (offset > 0)
            memcpy(new_buffer, buffer, offset); //copy id3 tags to new_buffer without parsing

//		std::vector<id3> tags = get_id3_tags(buffer, offset); //questionable. New offset is not returned. Files with ID3 tags are not decoded.
		// printf("%d\n", offset);

		mp3 deco_enco(&buffer[offset], &new_buffer[offset]);
//		 stream(decoder, buffer, offset);
//        int *written_dct;
//		written_dct = new int[2304*process_frames];
//		write_secret_data(deco_enco, buffer, offset, new_buffer, secret_bits, &secret_cursor, written_dct, process_frames, buffer_length);
        write_secret_data(deco_enco, buffer, offset, &new_buffer, secret_bits, &secret_cursor, process_frames, &buffer_length, secret_buffer_size);

        set_stego_file(argv[1], new_buffer, buffer_length); //new_buffer could have changed its address when reallocationg in write secret data!! return of new pointer necessary!
        printf("\nSuccessfully written to file.\n%.3f kB of secret data embedded.\nFile size = %.2f kB", (float) secret_cursor/(8 * 1024), (float) buffer_length / 1024);
        printf("\nSecret data occupy %.2f %% of file size.", (float) 100 * secret_cursor / (8 * buffer_length));



        unsigned char *dummy;
        mp3 decoder(&new_buffer[offset], dummy, true);
//        unsigned char read_secret_bits[secret_buffer_size];
        unsigned read_secret_buffer_size = buffer_length / 10; //after splitting the code into embedding and retrieval part, buffer_length will be directly given
        unsigned char *read_secret_bits;
        read_secret_bits = (unsigned char*) malloc (read_secret_buffer_size);
        if (new_buffer == NULL) exit (1);
        int read_secret_cursor = 0;
//        read_secret_data(decoder, new_buffer, offset, written_dct, process_frames, read_secret_bits, &read_secret_cursor);
        read_secret_data(decoder, new_buffer, offset, process_frames, &read_secret_bits, &read_secret_cursor, &read_secret_buffer_size);

        set_retrieved_file(argv[2], read_secret_bits, read_secret_buffer_size);
        printf("\nSuccessfully retrieved %.3f kB of secret data.", (float)read_secret_cursor/(8*1024));

	} catch (std::bad_alloc) {
		printf("File does not exist.\n");
		return -1;
	}

	return 0;
}

