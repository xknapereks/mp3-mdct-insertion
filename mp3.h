
#include <math.h>
#include <stdlib.h>
#include "tables.h"

#ifndef MP3_H
#define MP3_H

#define MP3_INFO_PRIVATE   0
#define MP3_INFO_COPYRIGHT 1
#define MP3_INFO_ORIGINAL  2

class mp3 {
public:
	mp3(unsigned char *buffer, unsigned char *new_buffer, bool decodeOnly=false);
 	void init_header_params(unsigned char *buffer, unsigned char *new_buffer);
 	void init_header_params_extract_data(unsigned char *buffer, unsigned char *new_buffer);
	void init_frame_params(unsigned char *buffer, unsigned char *new_buffer, int frame_count, unsigned char *secret_bits, int* secret_cursor, unsigned secret_buffer_size);
	void init_frame_params_extract_data(unsigned char *buffer, int frame_count, unsigned char* read_secret_bits, int* read_secret_cursor);

private:
	unsigned char *buffer;
	bool valid;

public:
	bool is_valid();
	bool mono; //was private
	int threshold1 = 150;
	int threshold2 = 50;
	int threshold3 = 500;
	int bitsInsertedNormally;
	int bitsInsertedToLinbits;

private: /* Header */
	float mpeg_version;
	unsigned layer;
	bool crc;
	unsigned bit_rate;
	unsigned write_bit_rate;
	unsigned sampling_rate;
	bool padding;
	unsigned channel_mode;

	unsigned mode_extension[2];
	unsigned emphasis;
	bool info[3];
	struct {
		const unsigned *long_win;
		const unsigned *short_win;
	} band_index;
	struct {
		const unsigned *long_win;
		const unsigned *short_win;
	} band_width;

	void set_mpeg_version();
	void set_layer(unsigned char byte);
	void set_crc();
	void set_bit_rate(unsigned char *buffer, unsigned char *new_buffer);
	void change_write_bitrate(unsigned char *buffer, unsigned char *new_buffer, int change_level);
	void set_vbr_thresholds();
	void set_sampling_rate();
	void set_padding();
	void set_channel_mode(unsigned char *buffer);
	void set_mode_extension(unsigned char *buffer);
	void set_emphasis(unsigned char *buffer);
	void set_info();
	void set_tables();


public:
	float get_mpeg_version();
	unsigned get_layer();
	bool get_crc();
	unsigned get_bit_rate();
	unsigned get_write_bit_rate();
	unsigned get_sampling_rate();
	bool get_padding();

	/**
	 * Values:
	 * 0 -> Stereo
	 * 1 -> Joint stereo (this option requires use of mode_extension)
	 * 2 -> Dual channel
	 * 3 -> Single channel
	 */
	unsigned get_channel_mode();
	unsigned *get_mode_extension();

	/**
	 * Values:
	 * 0 -> None
	 * 1 -> 50/15 MS
	 * 2 -> Reserved
	 * 3 -> CCIT J.17
	 */
	unsigned get_emphasis();
	bool *get_info();

    //following 5 were private
	int prev_frame_size;
	int write_prev_frame_size =  mono ? 21 : 36; //initial value to pass first if conditions in set_new_buffer correctly
	int frame_size;
	int write_frame_size;

	int main_data_begin;
	int write_main_data_begin = 0;
private: /* Frame */

	bool scfsi[2][4];

	/* Allocate space for two granules and two channels. */
	int part2_3_length[2][2];
	int part2_length[2][2];
	int big_value[2][2];
	int global_gain[2][2];
	int scalefac_compress[2][2];
	int slen1[2][2];
	int slen2[2][2];
	bool window_switching[2][2];
	int block_type[2][2];
	bool mixed_block_flag[2][2];
	int switch_point_l[2][2];
	int switch_point_s[2][2];
	int table_select[2][2][3];
	int subblock_gain[2][2][3];
	int region0_count[2][2];
	int region1_count[2][2];
	int preflag[2][2];
	int scalefac_scale[2][2];
	int count1table_select[2][2];

	int scalefactor[2][2][39];
	int scalefac_l[2][2][22];
	int scalefac_s[2][2][3][13];

//	float samples[2][2][576];
    int samples[2][2][576];
	float pcm[576 * 4];
	int dct[576 * 4];

	void set_frame_size(bool frame_init = true);
	void set_side_info(unsigned char *buffer);
	void set_main_data(unsigned char *buffer, unsigned char* read_secret_bits, int* read_secret_cursor);
	void set_new_buffer(unsigned char *buffer, unsigned char *new_buffer, int frame_count, unsigned char *secret_bits, int* secret_cursor, unsigned secret_buffer_size);
	void unpack_scalefac(unsigned char *bit_stream, int gr, int ch, int &bit);
	void unpack_samples(unsigned char *bit_stream, int gr, int ch, int bit, int max_bit);
	void retrieve_secret_bits(unsigned char *main_data, int gr, int ch, int bit, int max_bit, unsigned char* read_secret_bits, int* read_secret_cursor);
	void pack_samples(unsigned char *new_main_data, int gr, int ch, int* bit_cursor, unsigned char *secret_bits, int* secret_cursor, unsigned secret_buffer_size);
	void interleave();


public:
    void interleave_dct();
	float *get_samples();
	int *get_samples_dct();
	unsigned get_frame_size();
	unsigned get_write_frame_size();
};

#endif	/* MP3_H */

