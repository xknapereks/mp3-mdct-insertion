/*
 * Author: Floris Creyf
 * Date: May 2015
 * Unpacks and decodes frames/headers.
 */

#include <string.h>
#include <stdio.h>
#include "mp3.h"
#include "util.h"

#define PI    3.141592653589793
#define SQRT2 1.
#define INT_MAX 0xFFFFFFFF

/**
 * Unpack the MP3 header.
 * @param buffer A pointer that points to the first byte of the frame header.
 * @param new_buffer A pointer that points to the first byte of the frame header of the output array
 */
void mp3::init_header_params(unsigned char *buffer, unsigned char *new_buffer)
{
	if (buffer[0] == 0xFF && buffer[1] >= 0xE0) {
		this->buffer = buffer;


		static bool init = true;
		if (init) {
			set_mpeg_version();
			set_layer(buffer[1]);
			set_crc();
			set_info();
			set_emphasis(buffer);
			set_sampling_rate();
			set_tables();

			mono = channel_mode == 3;
			init = false;
		}
        /* copy header, CRC and side info to output array */
        int constant = mono ? 21 : 36; //size of (header + side info)
        if (crc == 0)
            constant += 2;
        memcpy(new_buffer, buffer, constant);

		set_channel_mode(buffer);
		set_mode_extension(buffer);
		set_padding();
//		set_bit_rate(buffer, new_buffer, false);
        set_bit_rate(buffer, new_buffer);
		set_vbr_thresholds();
		set_frame_size();
	} else {
		valid = false;
//		printf("\nINVALID HEADER. Possible end of file.\n");
	}
}

void mp3::init_header_params_extract_data(unsigned char *buffer, unsigned char *new_buffer)
{
    if (buffer[0] == 0xFF && buffer[1] >= 0xE0) {
		this->buffer = buffer;

		static bool init = true;
		if (init) {
			set_mpeg_version();
			set_layer(buffer[1]);
			set_crc();
			set_info();
			set_emphasis(buffer);
			set_sampling_rate();
			set_tables();

			mono = channel_mode == 3;
			init = false;
		}

		set_channel_mode(buffer);
		set_mode_extension(buffer);
		set_padding();
		set_bit_rate(buffer, new_buffer);
		set_frame_size();
	} else {
		valid = false;
//		printf("\nINVALID HEADER. Possible end of file.\n");
	}
}

/**
 * Unpack and decode the MP3 frame.
 * @param buffer A pointer to the first byte of the frame header.
 * @requant if false, stop decoding before requantization, and copy samples into 'dct' buffer
 */
void mp3::init_frame_params(unsigned char *buffer, unsigned char *new_buffer, int frame_count, unsigned char *secret_bits, int* secret_cursor, unsigned secret_buffer_size, bool requant)
{
//	printf("\ninit_frame_params - Frame %d, w_main_data_beg = %i, secret_cursor = %i", frame_count, write_main_data_begin, *secret_cursor);
//	char a;
//	scanf("%c", &a);

	set_side_info(&buffer[4]);

	if (requant == false) {
		set_new_buffer(buffer, new_buffer, frame_count, secret_bits, secret_cursor, secret_buffer_size);
//		memset(dct, '\0', 576*sizeof(int)*4);
//		for (int gr = 0; gr < 2; gr++)
//			for (int ch = 0; ch < (mono ? 1 : 2); ch++)
//				for (int i=0; i<576; i++)
//					dct[1152*gr+576*ch+i] = (int) samples[gr][ch][i];
		return;
	}
//	set_main_data(buffer);
//	for (int gr = 0; gr < 2; gr++) {
//		for (int ch = 0; ch < (mono ? 1 : 2); ch++)
//			requantize(gr, ch);
//
//		if (channel_mode == 1 && mode_extension[0])
//			ms_stereo(gr);
//
//		for (int ch = 0; ch < (mono ? 1 : 2); ch++) {
//			if (block_type[gr][ch] == 2 || mixed_block_flag[gr][ch])
//				reorder(gr, ch);
//			else
//				alias_reduction(gr, ch);
//
//			imdct(gr, ch);
//			frequency_inversion(gr, ch);
//			synth_filterbank(gr, ch);
//		}
//	}
//	interleave();
}

void mp3::init_frame_params_extract_data(unsigned char *buffer, int frame_count, unsigned char* read_secret_bits, int* read_secret_cursor)
{
//    printf("\nifp-extract - Frame %d, secret_cursor = %i", frame_count, *read_secret_cursor);
    set_side_info(&buffer[4]);
    set_main_data(buffer, read_secret_bits, read_secret_cursor);
}

mp3::mp3(unsigned char *buffer, unsigned char *new_buffer, bool decodeOnly)
{
	if (buffer[0] == 0xFF && buffer[1] >= 0xE0) {
		valid = true;
		frame_size = 0;
		main_data_begin = 0;
		if (decodeOnly)
            init_header_params_extract_data(buffer, new_buffer);
        else
            init_header_params(buffer, new_buffer);
	}
}

/** Check validity of the header and frame. */
bool mp3::is_valid()
{
	return valid;
}

/** Determine MPEG version. */
void mp3::set_mpeg_version()
{
	if ((buffer[1] & 0x10) == 0x10 && (buffer[1] & 0x08) == 0x08)
		mpeg_version = 1;
	else if ((buffer[1] & 0x10) == 0x10 && (buffer[1] & 0x08) != 0x08)
		mpeg_version = 2;
	else if ((buffer[1] & 0x10) != 0x10 && (buffer[1] & 0x08) == 0x08)
		mpeg_version = 0;
	else if ((buffer[1] & 0x10) != 0x10 && (buffer[1] & 0x08) != 0x08)
		mpeg_version = 2.5;
}

float mp3::get_mpeg_version()
{
	return mpeg_version;
}

/** Determine layer. */
void mp3::set_layer(unsigned char byte)
{
	byte = byte << 5;
	byte = byte >> 6;
	layer = 4 - byte;
}

unsigned mp3::get_layer()
{
	return layer;
}

/**
 * Cyclic redundancy check. If set, two bytes after the header information are
 * used up by the CRC.
 */
void mp3::set_crc()
{
	crc = buffer[1] & 0x01;
}

bool mp3::get_crc()
{
	return crc;
}

/**
 * For variable bit rate (VBR) files, this data has to be gathered constantly.
 */
void mp3::set_bit_rate(unsigned char *buffer, unsigned char *new_buffer)
{
	if (mpeg_version == 1) {
		if (layer == 1) {
			bit_rate = buffer[2] * 32;
			printf("\nmpeg_version = 1, layer = 1. Setting write_bit_rate incorrectly.");
		} else if (layer == 2) {
			const int rates[14] {32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384};
			bit_rate = rates[(buffer[2] >> 4) - 1] * 1000;
		} else if (layer == 3) {
			const int rates[14] {32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
			bit_rate = rates[(buffer[2] >> 4) - 1] * 1000;
		} else
			valid = false;
	} else {
		if (layer == 1) {
			const int rates[14] {32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256};
			bit_rate = rates[(buffer[2] >> 4) - 1] * 1000;
		} else if (layer < 4) {
			const int rates[14] {8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160};
			bit_rate = rates[(buffer[2] >> 4) - 1] * 1000;
		} else
			valid = false;
	}
    write_bit_rate = bit_rate;
}

void mp3::change_write_bitrate(unsigned char *buffer, unsigned char *new_buffer, int change_level)
{
    const int rates[14] {32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320}; //for MPEG1 layer3
    int index = (new_buffer[2] >> 4) -1 + change_level;
    while (index > 13) {//safety condition
//        printf("\nDecreasing bitrates index. Index = %i", index);
        index--;
    }
    write_bit_rate = rates[index] * 1000;
//    printf("\nChanged bitrate: %i. Change level: %i", rates[index], change_level);
    write_side_info_part(&new_buffer[2], (index + 1) << 28, 4, 0); //writing bitrate into header of current frame
    set_frame_size(false);
}

void mp3::set_vbr_thresholds()
{
    if (bit_rate == 256000) {
        threshold1 = 200; //to prevent the lack of space for new_main_data. Changed from 250 to 200
        threshold2 = 0; //not usable
    }
    else if (bit_rate == 320000) {
//        printf("\nFile type not supported. Bitrate = 320 kbps.");
        threshold1 = 0;
        threshold2 = 0;
    }
    else { //default values. Most of MP3 cases
        threshold1 = 150;
        threshold2 = 50;
    }
}

unsigned mp3::get_bit_rate()
{
	return bit_rate;
}

unsigned mp3::get_write_bit_rate()
{
	return write_bit_rate;
}

/** Sampling rate. */
void mp3::set_sampling_rate()
{
	int rates[3][3] {44100, 48000, 32000, 22050, 24000, 16000, 11025, 12000, 8000};

	for (int version = 1; version <= 3; version++)
		if (mpeg_version == version) {
			if ((buffer[2] & 0x08) != 0x08 && (buffer[2] & 0x04) != 0x04) {
				sampling_rate = rates[version - 1][0];
				break;
			} else if ((buffer[2] & 0x08) != 0x08 && (buffer[2] & 0x04) == 0x04) {
				sampling_rate = rates[version - 1][1];
				break;
			} else if ((buffer[2] & 0x08) == 0x08 && (buffer[2] & 0x04) != 0x04) {
				sampling_rate = rates[version - 1][2];
				break;
			}
		}
}

unsigned mp3::get_sampling_rate()
{
	return sampling_rate;
}

/**
 * During the decoding process different tables are used depending on the
 * sampling rate.
 */
void mp3::set_tables()
{
	switch (sampling_rate) {
		case 32000:
			band_index.short_win = band_index_table.short_32;
			band_width.short_win = band_width_table.short_32;
			band_index.long_win = band_index_table.long_32;
			band_width.long_win = band_width_table.long_32;
			break;
		case 44100:
			band_index.short_win = band_index_table.short_44;
			band_width.short_win = band_width_table.short_44;
			band_index.long_win = band_index_table.long_44;
			band_width.long_win = band_width_table.long_44;
			break;
		case 48000:
			band_index.short_win = band_index_table.short_48;
			band_width.short_win = band_width_table.short_48;
			band_index.long_win = band_index_table.long_48;
			band_width.long_win = band_width_table.long_48;
			break;
	}
}

/** If set, the frame size is 1 byte larger. */
void mp3::set_padding()
{
	padding = buffer[2] & 0x02;
}

bool mp3::get_padding()
{
	return padding;
}

/**
 * 0 -> Stereo
 * 1 -> Joint stereo (this option requires use of mode_extension)
 * 2 -> Dual channel
 * 3 -> Single channel
 */
void mp3::set_channel_mode(unsigned char *buffer)
{
	channel_mode = buffer[3] >> 6;
}

unsigned mp3::get_channel_mode()
{
	return channel_mode;
}

/** Applies only to joint stereo. */
void mp3::set_mode_extension(unsigned char *buffer)
{
	if (layer == 3) {
		mode_extension[0] = buffer[3] & 0x20;
		mode_extension[1] = buffer[3] & 0x10;
	}
}

unsigned *mp3::get_mode_extension()
{
	return mode_extension;
}

/** Although rarely used, there is no method for emphasis. */
void mp3::set_emphasis(unsigned char *buffer)
{
	emphasis = buffer[3] << 6;
	emphasis = emphasis >> 6;
}

unsigned mp3::get_emphasis()
{
	return emphasis;
}

/** Additional information (not important). */
void mp3::set_info()
{
	info[0] = buffer[2] & 0x01;
	info[1] = buffer[3] & 0x08;
	info[2] = buffer[3] & 0x04;
}

bool *mp3::get_info()
{
	return info;
}

/** Determine the frame size. */
void mp3::set_frame_size(bool frame_init)
{
	unsigned int samples_per_frame;
	switch (layer) {
		case 3:
			if (mpeg_version == 1)
				samples_per_frame = 1152;
			else
				samples_per_frame = 576;
			break;
		case 2:
			samples_per_frame = 1152;
			break;
		case 1:
			samples_per_frame = 384;
			break;
	}
	if (frame_init) {
        prev_frame_size = frame_size;
        write_prev_frame_size = write_frame_size;
	}
	frame_size = (samples_per_frame / 8 * bit_rate / sampling_rate);
	write_frame_size = (samples_per_frame / 8 * write_bit_rate / sampling_rate);
	if (padding == 1) {
		frame_size += 1;
		write_frame_size += 1;
	}
}

unsigned mp3::get_frame_size()
{
	return frame_size;
}

unsigned mp3::get_write_frame_size()
{
	return write_frame_size;
}

/**
 * The side information contains information on how to decode the main_data.
 * @param buffer A pointer to the first byte of the side info.
 */
void mp3::set_side_info(unsigned char *buffer)
{
	int count = 0;

	/* Number of bytes the main data ends before the next frame header. */
	main_data_begin = (int)get_bits_inc(buffer, &count, 9);

	/* Skip private bits. Not necessary. */
	count += mono ? 5 : 3;

	for (int ch = 0; ch < (mono ? 1 : 2); ch++)
		for (int scfsi_band = 0; scfsi_band < 4; scfsi_band++)
			/* - Scale factor selection information.
			 * - If scfsi[scfsi_band] == 1, then scale factors for the first
			 *   granule are reused in the second granule.
			 * - If scfsi[scfsi_band] == 0, then each granule has its own scaling factors.
			 * - scfsi_band indicates what group of scaling factors are reused. */
			scfsi[ch][scfsi_band] = get_bits_inc(buffer, &count, 1) != 0;

	for (int gr = 0; gr < 2; gr++)
		for (int ch = 0; ch < (mono ? 1 : 2); ch++) {
			/* Length of the scaling factors and main data in bits. */
			part2_3_length[gr][ch] = (int)get_bits_inc(buffer, &count, 12);
			/* Number of values in each big_region. */
			big_value[gr][ch] = (int)get_bits_inc(buffer, &count, 9);
			/* Quantizer step size. */
			global_gain[gr][ch] = (int)get_bits_inc(buffer, &count, 8);
			/* Used to determine the values of slen1 and slen2. */
			scalefac_compress[gr][ch] = (int)get_bits_inc(buffer, &count, 4);
			/* Number of bits given to a range of scale factors.
			 * - Normal blocks: slen1 0 - 10, slen2 11 - 20
			 * - Short blocks && mixed_block_flag == 1: slen1 0 - 5, slen2 6-11
			 * - Short blocks && mixed_block_flag == 0: */
			slen1[gr][ch] = slen[scalefac_compress[gr][ch]][0];
			slen2[gr][ch] = slen[scalefac_compress[gr][ch]][1];
			/* If set, a not normal window is used. */
			window_switching[gr][ch] = get_bits_inc(buffer, &count, 1) == 1;

			if (window_switching[gr][ch]) {
				/* The window type for the granule.
				 * 0: reserved
				 * 1: start block
				 * 2: 3 short windows
				 * 3: end block */
				block_type[gr][ch] = (int)get_bits_inc(buffer, &count, 2);
				/* Number of scale factor bands before window switching. */
				mixed_block_flag[gr][ch] = get_bits_inc(buffer, &count, 1) == 1;
				if (mixed_block_flag[gr][ch]) {
					switch_point_l[gr][ch] = 8;
					switch_point_s[gr][ch] = 3;
				} else {
					switch_point_l[gr][ch] = 0;
					switch_point_s[gr][ch] = 0;
				}

				/* These are set by default if window_switching. */
				region0_count[gr][ch] = block_type[gr][ch] == 2 ? 8 : 7;
				/* No third region. */
				region1_count[gr][ch] = 20 - region0_count[gr][ch];

				for (int region = 0; region < 2; region++) {
					/* Huffman table number for a big region. */
					table_select[gr][ch][region] = (int)get_bits_inc(buffer, &count, 5);
//					printf("\nregion = %i, table_num = %i", region, table_select[gr][ch][region]); //monitoring
				}
				for (int window = 0; window < 3; window++)
					subblock_gain[gr][ch][window] = (int)get_bits_inc(buffer, &count, 3);
			} else {
				/* Set by default if !window_switching. */
				block_type[gr][ch] = 0;
				mixed_block_flag[gr][ch] = false;

				for (int region = 0; region < 3; region++) {
					table_select[gr][ch][region] = (int)get_bits_inc(buffer, &count, 5);
//					printf("\nregion = %i, table_num = %i", region, table_select[gr][ch][region]);
				}

				/* Number of scale factor bands in the first big value region. */
				region0_count[gr][ch] = (int)get_bits_inc(buffer, &count, 4);
				/* Number of scale factor bands in the third big value region. */
				region1_count[gr][ch] = (int)get_bits_inc(buffer, &count, 3);
				/* # scale factor bands is 12*3 = 36 */
			}

			/* If set, add values from a table to the scaling factors. */
			preflag[gr][ch] = (int)get_bits_inc(buffer, &count, 1);
			/* Determines the step size. */
			scalefac_scale[gr][ch] = (int)get_bits_inc(buffer, &count, 1);
			/* Table that determines which count1 table is used. */
			count1table_select[gr][ch] = (int)get_bits_inc(buffer, &count, 1);
		}
}

/**
 * Due to the Huffman bits' varying length the main_data isn't aligned with the
 * frames. Unpacks the scaling factors and quantized samples.
 * @param buffer A buffer that points to the the first byte of the frame header.
 */
void mp3::set_main_data(unsigned char *buffer, unsigned char* read_secret_bits, int* read_secret_cursor)
{
	/* header + side_information */
	int constant = mono ? 21 : 36; //size of (header + side info)
	if (crc == 0)
		constant += 2;

	/* Let's put the main data in a separate buffer so that side info and header
	 * don't interfere. The main_data_begin may be larger than the previous frame
	 * and doesn't include the size of side info and headers. */
	unsigned char *main_data;
//	if (main_data_begin > prev_frame_size) { //system error
    if (main_data_begin > prev_frame_size - constant) {
		int part2_len = prev_frame_size - constant;
		int part1_len = main_data_begin - part2_len;
		int part12_len = part1_len + part2_len;

		int buffer_length = part12_len + frame_size;
	 	int buffer_offset = main_data_begin + constant;

		main_data = new unsigned char[buffer_length];
		memcpy(main_data, buffer - buffer_offset, part1_len);
		memcpy(&main_data[part1_len], buffer - part2_len, part2_len);
		memcpy(&main_data[part12_len], buffer + constant, frame_size - constant);
	} else {
		int buffer_length = main_data_begin + frame_size;

		main_data = new unsigned char[buffer_length];
		memcpy(main_data, buffer - main_data_begin, main_data_begin);
		memcpy(&main_data[main_data_begin], buffer + constant, frame_size - constant);
	}

	int bit = 0;
	for (int gr = 0; gr < 2; gr++)
		for (int ch = 0; ch < (mono ? 1 : 2); ch++) {
			int max_bit = bit + part2_3_length[gr][ch];
			unpack_scalefac(main_data, gr, ch, bit);
//			unpack_samples(main_data, gr, ch, bit, max_bit);
            retrieve_secret_bits(main_data, gr, ch, bit, max_bit, read_secret_bits, read_secret_cursor);
			bit = max_bit;
		}

	delete[] main_data;
}

/**
 * Modified version of mp3::set_main_data.
 * Performed tasks:
 * - assembly main data for a specific frame (at adress 'buffer')
 * - start unpacking scalefactors and huffman code bits for gr0, left channel,
 * - track current position in main_data bitstream
 * - unpack scalefactors and huffman code bits for all granules and channels, ending with field samples[2][2][576] set
 * NEW IN THIS FUNCTION:
 * - (change 'samples')
 * - pack samples for each granule and channel (perform huffman coding) using unchanged parameters (huffman tables)
 * - interlace scalefactors and huffman code bits for each gr. and ch. yielding new_main_data bitstream
 */
void mp3::set_new_buffer(unsigned char *buffer, unsigned char *new_buffer, int frame_count, unsigned char *secret_bits, int* secret_cursor, unsigned secret_buffer_size)
{
//	printf("\nset_new_buffer - Frame %d", frame_count);
//	char a;
//	scanf("%c", &a);

	/* header + side_information */
	int constant = mono ? 21 : 36; //size of (header + side info)
	if (crc == 0)
		constant += 2;

	int buffer_length;

	/* Let's put the main data in a separate buffer so that side info and header
	 * don't interfere. The main_data_begin may be larger than the previous frame
	 * and doesn't include the size of side info and headers. */
	unsigned char *main_data;
	if (main_data_begin > prev_frame_size - constant) { //if main data starts two frames before
		int part2_len = prev_frame_size - constant;
		int part1_len = main_data_begin - part2_len;
		int part12_len = part1_len + part2_len;

		buffer_length = part12_len + frame_size;
	 	int buffer_offset = main_data_begin + constant;

		main_data = new unsigned char[buffer_length];
		memcpy(main_data, buffer - buffer_offset, part1_len); //copying part1
		memcpy(&main_data[part1_len], buffer - part2_len, part2_len); //copying part2
		memcpy(&main_data[part12_len], buffer + constant, frame_size - constant); //copying main data volume of current frame
	} else { //if main data doesn't start before previous frame
		buffer_length = main_data_begin + frame_size;

		main_data = new unsigned char[buffer_length];
		memcpy(main_data, buffer - main_data_begin, main_data_begin); //copying main data from previous frame volume
		memcpy(&main_data[main_data_begin], buffer + constant, frame_size - constant); //copying main data volume of current frame
	}

	int bit = 0;
	unsigned char new_main_data[buffer_length + 64]; // with reserve
	int writing_bit_cursor = 0;
	int previous_wbc = 0;
	int start_bit = 0;
	int si_cursor = mono ? 18 : 20; //points where to write part2_3_length for actual granule and channel. This is an initial offset
	int si_cursor_tableSel = si_cursor;
	for (int gr = 0; gr < 2; gr++)
		for (int ch = 0; ch < (mono ? 1 : 2); ch++) {
            int max_bit = bit + part2_3_length[gr][ch];
			unpack_scalefac(main_data, gr, ch, bit);
			push_bits_modified(new_main_data, &writing_bit_cursor, main_data, start_bit, bit); //push scalefac bits to new_main_data
			unpack_samples(main_data, gr, ch, bit, max_bit);
			//now 576 samples (one channel of one granule) are set

			//inverse process for 576 samples
			pack_samples(new_main_data, gr, ch, &writing_bit_cursor, secret_bits, secret_cursor, secret_buffer_size); //embed secret data, encode samples and push to new_main_data
			//set new part2_3_length
            write_side_info_part(&new_buffer[4], (writing_bit_cursor - previous_wbc) << (32 - 12), 12, si_cursor);

			si_cursor += 59; //this is the length of SI for one channel of a granule

            start_bit = max_bit; //where scalefactors of next channel start
			bit = max_bit;
			previous_wbc = writing_bit_cursor;
		}
    delete[] main_data;
	//at this place new_main_data is given. Bits are saved in interval <0; writing_bit_cursor)

    int new_main_data_length = (writing_bit_cursor / 8) + (writing_bit_cursor % 8 ? 1 : 0);


    /* adjustments */
	//get write_main_data_begin and write it to side info (first 9 bits of side info)
    //if write_main_data_begin > 511 (largest possible value), clip it
    if (write_main_data_begin > ((1 << 9) - 1))
        write_main_data_begin = (1 << 9) - 1;
    write_side_info_part(&new_buffer[4], write_main_data_begin << (32 - 9), 9, 0);

	/* Adjust write_bitrate and write_frame size -> avoid lack or excess of space */
    int available_maindata_space = write_main_data_begin + write_frame_size - constant;
    if (available_maindata_space - new_main_data_length < threshold2)
        change_write_bitrate(buffer, new_buffer, 2);
    else if (available_maindata_space - new_main_data_length < threshold1)
        change_write_bitrate(buffer, new_buffer, 1);
    else if (available_maindata_space - new_main_data_length > threshold3)
        change_write_bitrate(buffer, new_buffer, -1);

	/* BIT RESERVOIR APPROACH: INTERLEAVING MAIN DATA with FRAME HEADERS & SI */
	// inputs: write_main_data_begin computed after processing of the preceding frame
    // outputs: write_main_data_begin for the consecutive frame
    int part2_len = write_prev_frame_size - constant;
	if (write_main_data_begin > part2_len) { //if main data starts two frames before
		int part1_len = write_main_data_begin - part2_len;

		memcpy(&new_buffer[- write_main_data_begin - constant], new_main_data, part1_len);
		int bytes_in_part2 = new_main_data_length - part1_len;
		if (bytes_in_part2 > part2_len) { //main data: part1 Y, part2 Y, part3 Y
            bytes_in_part2 = part2_len;
            int bytes_in_part3 = new_main_data_length - bytes_in_part2 - part1_len;
            if (bytes_in_part3 > write_frame_size - constant) {
                printf("\nNot enough space for new_main_data. Increasing bitrate!");
                change_write_bitrate(buffer, new_buffer, 2);
//                bytes_in_part3 = write_frame_size - constant;
            }
            memcpy(&new_buffer[-part2_len], &new_main_data[part1_len], bytes_in_part2);
            memcpy(&new_buffer[constant], &new_main_data[part1_len + part2_len], bytes_in_part3);
            write_main_data_begin = write_frame_size - constant - bytes_in_part3;
        } else if (bytes_in_part2 > 0) { //main data: part1 Y, part2 Y, part3 N
            memcpy(&new_buffer[-part2_len], &new_main_data[part1_len], bytes_in_part2);
            write_main_data_begin = write_frame_size - constant + part2_len - bytes_in_part2;
        } else { //main data: part1 Y, part2 N, part3 N
             //bytes_in part2 = 0;
            write_main_data_begin = write_frame_size - constant + part2_len;
		}
	} else { //if main data doesn't start before previous frame
        if (new_main_data_length >= write_main_data_begin) { //main data: part1 N, part2 Y/N, part3 Y
            memcpy(&new_buffer[-write_main_data_begin], new_main_data, write_main_data_begin);
            int bytes_in_part3 = new_main_data_length - write_main_data_begin;
            if (bytes_in_part3 > write_frame_size - constant) {
                printf("\nNot enough space for new_main_data. Increasing bitrate!");
                change_write_bitrate(buffer, new_buffer, 2);
//                bytes_in_part3 = write_frame_size - constant;
            }
            if (bytes_in_part3 > 0) {
                memcpy(&new_buffer[constant], &new_main_data[write_main_data_begin], bytes_in_part3);
                write_main_data_begin = write_frame_size - constant - bytes_in_part3;
            } else {
                write_main_data_begin = write_frame_size - constant + write_main_data_begin - new_main_data_length;
            }
        } else { //main data: part1 N, part2 Y, part3 N
            memcpy(&new_buffer[-write_main_data_begin], new_main_data, new_main_data_length);
            write_main_data_begin = write_frame_size - constant + write_main_data_begin - new_main_data_length;

        }
	}

}


/**
 * This will get the scale factor indices from the main data. slen1 and slen2
 * represent the size in bits of each scaling factor. There are a total of 21 scaling
 * factors for long windows and 12 for each short window.
 * @param main_data Buffer solely containing the main_data - excluding the frame header and side info.
 * @param gr
 * @param ch
 */
void mp3::unpack_scalefac(unsigned char *main_data, int gr, int ch, int &bit)
{
	int sfb = 0;
	int window = 0;
	int scalefactor_length[2] {
		slen[scalefac_compress[gr][ch]][0],
		slen[scalefac_compress[gr][ch]][1]};

	int scalefac[39] = {0};
	int i = 0;

	/* No scale factor transmission for short blocks. */
	if (block_type[gr][ch] == 2 && window_switching[gr][ch]) {
		if (mixed_block_flag[gr][ch] == 1) { /* Mixed blocks. */
			for (sfb = 0; sfb < 8; sfb++) {
				scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);
				scalefac[i++] = scalefac_l[gr][ch][sfb];
			}
			for (sfb = 3; sfb < 6; sfb++)
				for (window = 0; window < 3; window++) {
					scalefac_s[gr][ch][window][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);
					scalefac[i++] = scalefac_s[gr][ch][window][sfb];
				}
		} else { /* Short blocks. */
			for (sfb = 0; sfb < 6; sfb++)
				for (window = 0; window < 3; window++) {
					scalefac_s[gr][ch][window][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);
					scalefac[i++] = scalefac_s[gr][ch][window][sfb];
				}
		}
		for (sfb = 6; sfb < 12; sfb++) {
			for (window = 0; window < 3; window++) {
				scalefac_s[gr][ch][window][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[1]);
				scalefac[i++] = scalefac_s[gr][ch][window][sfb];
			}
		}
		for (window = 0; window < 3; window++) {
			scalefac_s[gr][ch][window][12] = 0;
			scalefac[i++] = scalefac_s[gr][ch][window][sfb];
		}
	}

	/* Scale factors for long blocks. */
	else {
		if (gr == 0) {
			for (sfb = 0; sfb < 11; sfb++)
				scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);
			for (; sfb < 21; sfb++)
				scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[1]);
		} else {

			/* Scale factors might be reused in the second granule. */
			const int sb[5] = {6, 11, 16, 21};
			for (int i = 0; i < 2; i++)
				for (; sfb < sb[i]; sfb++) {
					if (scfsi[ch][i])
						scalefac_l[gr][ch][sfb] = scalefac_l[0][ch][sfb];
					else
						scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[0]);

				}
			for (int i = 2; i < 4; i++)
				for (; sfb < sb[i]; sfb++) {
					if (scfsi[ch][i])
						scalefac_l[gr][ch][sfb] = scalefac_l[0][ch][sfb];
					else
						scalefac_l[gr][ch][sfb] = (int)get_bits_inc(main_data, &bit, scalefactor_length[1]);
				}
		}
		scalefac_l[gr][ch][21] = 0;
	}
}

/**
 * The Huffman bits (part3) will be unpacked. Four bytes are retrieved from the
 * bit stream, and are consecutively evaluated against values of the selected Huffman
 * tables.
 * | big_value | big_value | big_value | quadruple | zero |
 * Each hit gives two samples.
 * @param main_data Buffer solely containing the main_data excluding the frame header and side info.
 * @param gr
 * @param ch
 */
void mp3::unpack_samples(unsigned char *main_data, int gr, int ch, int bit, int max_bit)
{
	int sample = 0;
	int table_num;
	const unsigned *table;

	/* Get the big value region boundaries. */
	int region0;
	int region1;
	if (window_switching[gr][ch] && block_type[gr][ch] == 2) {
		region0 = 36;
		region1 = 576;
	} else {
		region0 = band_index.long_win[region0_count[gr][ch] + 1];
		region1 = band_index.long_win[region0_count[gr][ch] + 1 + region1_count[gr][ch] + 1];
	}

	/* Get the samples in the big value region. Each entry in the Huffman tables
	 * yields two samples. */
	for (; sample < big_value[gr][ch] * 2; sample += 2) {
		if (sample < region0) {
			table_num = table_select[gr][ch][0];
			table = big_value_table[table_num];
		} else if (sample < region1) {
			table_num = table_select[gr][ch][1];
			table = big_value_table[table_num];
		} else {
			table_num = table_select[gr][ch][2];
			table = big_value_table[table_num];
		}

		if (table_num == 0) {
			samples[gr][ch][sample] = 0;
			samples[gr][ch][sample+1] = 0;
			continue;
		}

		bool repeat = true;
		unsigned bit_sample = get_bits(main_data, bit, bit + 32);

		/* Cycle through the Huffman table and find a matching bit pattern. */
		for (int row = 0; row < big_value_max[table_num] && repeat; row++)
			for (int col = 0; col < big_value_max[table_num]; col++) {
				int i = 2 * big_value_max[table_num] * row + 2 * col; //linear index of table element
				unsigned value = table[i];
				unsigned size = table[i + 1];

				/* TODO Need to update tables so that we can simply write:
				 *      value == bit_sample >> (32 - size) */
				if (value >> (32 - size) == bit_sample >> (32 - size)) { //if value from huff table matches corresp. # of bits of the bitstream
					bit += size;

					int values[2] = {row, col};
					for (int j = 0; j < 2; j++) {

						/* linbits extends the sample's size if needed. */
						int linbit = 0;
						if (big_value_linbit[table_num] != 0 && values[j] == big_value_max[table_num] - 1)
							linbit = (int)get_bits_inc(main_data, &bit, big_value_linbit[table_num]);

						/* If the sample is negative or positive. */
						int sign = 1;
						if (values[j] > 0)
							sign = get_bits_inc(main_data, &bit, 1) ? -1 : 1;

//						samples[gr][ch][sample + j] = (float)(sign * (values[j] + linbit));
                        samples[gr][ch][sample + j] = (sign * (values[j] + linbit));
					}

					repeat = false;
					break;
				}
			}
	}

	/* Quadruples region. */
	for (; bit < max_bit && sample + 4 < 576; sample += 4) {
		int values[4];

		/* Flip bits. */
		if (count1table_select[gr][ch] == 1) {
			unsigned bit_sample = get_bits_inc(main_data, &bit, 4);
			values[0] = (bit_sample & 0x08) > 0 ? 0 : 1; //condition ? result_if_true : result_if_false
			values[1] = (bit_sample & 0x04) > 0 ? 0 : 1;
			values[2] = (bit_sample & 0x02) > 0 ? 0 : 1;
			values[3] = (bit_sample & 0x01) > 0 ? 0 : 1;
		} else {
			unsigned bit_sample = get_bits(main_data, bit, bit + 32);
			for (int entry = 0; entry < 16; entry++) {
				unsigned value = quad_table_1.hcod[entry];
				unsigned size = quad_table_1.hlen[entry];
				if (value >> (32 - size) == bit_sample >> (32 - size)) {
					bit += size;
					for (int i = 0; i < 4; i++)
						values[i] = (int)quad_table_1.value[entry][i];
				}
			}
		}

		/* Get the sign bit. */
		for (int i = 0; i < 4; i++)
			if (values[i] > 0 && get_bits_inc(main_data, &bit, 1) == 1)
				values[i] = -values[i];

		for (int i = 0; i < 4; i++)
			samples[gr][ch][sample + i] = values[i];
	}

	/* Fill remaining samples with zero. */
	for (; sample < 576; sample++)
		samples[gr][ch][sample] = 0;
}

/**
* Customized version of unpack_samples. Needs extra arguments read_secret_bits and read_secret_cursor.
* Performs reading of LSBs, while specific table_num for particular samples is known.
*/
void mp3::retrieve_secret_bits(unsigned char *main_data, int gr, int ch, int bit, int max_bit, unsigned char* read_secret_bits, int* read_secret_cursor)
{
	int sample = 0;
	int table_num;
	const unsigned *table;

	/* Get the big value region boundaries. */
	int region0;
	int region1;
	if (window_switching[gr][ch] && block_type[gr][ch] == 2) {
		region0 = 36;
		region1 = 576;
	} else {
		region0 = band_index.long_win[region0_count[gr][ch] + 1];
		region1 = band_index.long_win[region0_count[gr][ch] + 1 + region1_count[gr][ch] + 1];
	}

	/* Get the samples in the big value region. Each entry in the Huffman tables
	 * yields two samples. */
	for (; sample < big_value[gr][ch] * 2; sample += 2) {
		if (sample < region0) {
			table_num = table_select[gr][ch][0];
			table = big_value_table[table_num];
		} else if (sample < region1) {
			table_num = table_select[gr][ch][1];
			table = big_value_table[table_num];
		} else {
			table_num = table_select[gr][ch][2];
			table = big_value_table[table_num];
		}

		if (table_num == 0) {
			samples[gr][ch][sample] = 0;
			samples[gr][ch][sample+1] = 0;
			continue;
		}

		bool repeat = true;
		unsigned bit_sample = get_bits(main_data, bit, bit + 32);

		/* Cycle through the Huffman table and find a matching bit pattern. */
		for (int row = 0; row < big_value_max[table_num] && repeat; row++)
			for (int col = 0; col < big_value_max[table_num]; col++) {
				int i = 2 * big_value_max[table_num] * row + 2 * col; //linear index of table element
				unsigned value = table[i];
				unsigned size = table[i + 1];

				/* TODO Need to update tables so that we can simply write:
				 *      value == bit_sample >> (32 - size) */
				if (value >> (32 - size) == bit_sample >> (32 - size)) { //if value from huff table matches corresp. # of bits of the bitstream
					bit += size;

					int values[2] = {row, col};
					for (int j = 0; j < 2; j++) {

						/* linbits extends the sample's size if needed. */
						int linbit = 0;
						if (big_value_linbit[table_num] != 0 && values[j] == big_value_max[table_num] - 1)
							linbit = (int)get_bits_inc(main_data, &bit, big_value_linbit[table_num]);

						/* If the sample is negative or positive. */
						int sign = 1;
						if (values[j] > 0)
							sign = get_bits_inc(main_data, &bit, 1) ? -1 : 1;

//						samples[gr][ch][sample + j] = (float)(sign * (values[j] + linbit));
                        samples[gr][ch][sample + j] = (sign * (values[j] + linbit));
					}

					repeat = false;
					break;
				}
			}

        /* Steganographic data retrieval block */
        if (table_num > 3) {
            if (table_num < 16) { //huffman tables without linbits
                for (int i = 0; i < 2; i++) {
                    if (abs(samples[gr][ch][sample+i]) > 1) {
                        retrieve_LSB(&samples[gr][ch][sample+i], read_secret_bits, read_secret_cursor, false);
                    }
                }
            } else { //huffman tables with linbits supported
                for (int i = 0; i < 2; i++) {
                    if (abs(samples[gr][ch][sample+i]) > 1) { //else do not embed
                        if (abs(samples[gr][ch][sample+i]) != 14) { //there should be no information embedded in samples with value 14
                            retrieve_LSB(&samples[gr][ch][sample+i], read_secret_bits, read_secret_cursor, true);
                        }
                    }
                }
            }
        }


        /* End of steganographic data retrieval block */
	}

	/* Quadruples region. */
	for (; bit < max_bit && sample + 4 < 576; sample += 4) {
		int values[4];

		/* Flip bits. */
		if (count1table_select[gr][ch] == 1) {
			unsigned bit_sample = get_bits_inc(main_data, &bit, 4);
			values[0] = (bit_sample & 0x08) > 0 ? 0 : 1; //condition ? result_if_true : result_if_false
			values[1] = (bit_sample & 0x04) > 0 ? 0 : 1;
			values[2] = (bit_sample & 0x02) > 0 ? 0 : 1;
			values[3] = (bit_sample & 0x01) > 0 ? 0 : 1;
		} else {
			unsigned bit_sample = get_bits(main_data, bit, bit + 32);
			for (int entry = 0; entry < 16; entry++) {
				unsigned value = quad_table_1.hcod[entry];
				unsigned size = quad_table_1.hlen[entry];
				if (value >> (32 - size) == bit_sample >> (32 - size)) {
					bit += size;
					for (int i = 0; i < 4; i++)
						values[i] = (int)quad_table_1.value[entry][i];
				}
			}
		}

		/* Get the sign bit. */
		for (int i = 0; i < 4; i++)
			if (values[i] > 0 && get_bits_inc(main_data, &bit, 1) == 1)
				values[i] = -values[i];

		for (int i = 0; i < 4; i++)
			samples[gr][ch][sample + i] = values[i];
	}

	/* Fill remaining samples with zero. */
	for (; sample < 576; sample++)
		samples[gr][ch][sample] = 0;
}

void mp3::pack_samples(unsigned char *new_main_data, int gr, int ch, int* bit_cursor, unsigned char *secret_bits, int* secret_cursor, unsigned secret_buffer_size)
{
	int sample = 0;
	int table_num;

	const unsigned *table;

	/* Get the big value region boundaries. */
	int region0;
	int region1;
	if (window_switching[gr][ch] && block_type[gr][ch] == 2) {
		region0 = 36;
		region1 = 576;
	} else {
		region0 = band_index.long_win[region0_count[gr][ch] + 1];
		region1 = band_index.long_win[region0_count[gr][ch] + 1 + region1_count[gr][ch] + 1];
	}

//	printf("\nHuffmann tables gr%d ch%d: %d %d %d", gr, ch, table_select[gr][ch][0], table_select[gr][ch][1], table_select[gr][ch][2]);

	/* BIG VALUE REGION HUFFMAN ENCODING */
	for (; sample < big_value[gr][ch] * 2; sample += 2) {
		if (sample < region0) {
			table_num = table_select[gr][ch][0];
			table = big_value_table[table_num];
		} else if (sample < region1) {
			table_num = table_select[gr][ch][1];
			table = big_value_table[table_num];
		} else {
			table_num = table_select[gr][ch][2];
			table = big_value_table[table_num];
		}

		if (table_num == 0) {
		 	continue; //nothing to write
		}

		/* embedding to samples according to the rules */
        if ((table_num > 3) && ((*secret_cursor / 8) < secret_buffer_size)) {
            if (table_num < 16) { //huffman tables without linbits
                for (int i = 0; i < 2; i++) {
                    if (abs(samples[gr][ch][sample+i]) > 1) {
                        embed_LSB(&samples[gr][ch][sample+i], secret_bits, secret_cursor, false);
                    }
                }
            } else { //huffman tables with linbits supported
                for (int i = 0; i < 2; i++) {
                    if (abs(samples[gr][ch][sample+i]) > 1) { //else do not embed
                        if (abs(samples[gr][ch][sample+i]) != 14) {
                            //no linbits: avoid increasing samples value to 15
                            embed_LSB(&samples[gr][ch][sample+i], secret_bits, secret_cursor, true);

                        }
                    }
                }
            }
        }

		/* end of embedding */


		unsigned int linbit[2] = {INT_MAX, INT_MAX}; // saving all ones in 4 bytes
		int entryIndices[2];
		for (int i = 0; i < 2; i++) {//	printf("\nHuffmann tables gr%d ch%d: %d %d %d", gr, ch, table_select[gr][ch][0], table_select[gr][ch][1], table_select[gr][ch][2]);
			if (abs(samples[gr][ch][sample+i]) < big_value_max[table_num] - 1)
				entryIndices[i] = abs(samples[gr][ch][sample+i]);
			else {
				entryIndices[i] = big_value_max[table_num] - 1;
				linbit[i] = abs(samples[gr][ch][sample+i]) - entryIndices[i];
				if (linbit[i] > (0x1 << big_value_linbit[table_num]) - 1)
				//check if linbit capacity is not exceeded when using the same Huffman table after manipulation with DCT coefficients
					printf("\nERROR. table_num: %d linbit number to write: %d", table_num, linbit[i]);

			}
		}
		int i = 2 * big_value_max[table_num] * entryIndices[0] + 2 * entryIndices[1]; //linear index of table element
		unsigned value = table[i];
		unsigned size = table[i + 1];

		//push huffman code
		push_bits_inc(new_main_data, value, size, bit_cursor);

		//push linbits and sign bits
		for (int j = 0; j < 2; j++) {
			if (linbit[j] < INT_MAX)
				push_bits_inc(new_main_data, linbit[j] << (32 - big_value_linbit[table_num]), big_value_linbit[table_num], bit_cursor);
            if (samples[gr][ch][sample + j] < 0)
				push_bits_inc(new_main_data, 1 << 31, 1, bit_cursor);
			else if (samples[gr][ch][sample + j] > 0)
				push_bits_inc(new_main_data, 0, 1, bit_cursor);
		}
	}

	/* Quadruples region Huffman encoding */
	//seek first non-zero value from the end of array samples
	int k = 576;
	while (samples[gr][ch][--k] == 0);
	//k is now index of last sample from quadruples region (or index of a sample from last quadruple)

	for (; sample < k + 1; sample += 4) {
		/* Flip bits. */
		if (count1table_select[gr][ch] == 1) {
			for (int i = 0; i < 4; i++)
				push_bits_inc(new_main_data, (abs(samples[gr][ch][sample+i]) > 0 ? 0 : 1) << 31, 1, bit_cursor);
		} else {
			int index = 0;
			int grade = 1;
			for (int i = 3; i > -1; i--) {
				index += abs(samples[gr][ch][sample+i]) * grade;
				grade *= 2;
			}
			push_bits_inc(new_main_data, quad_table_1.hcod[index], quad_table_1.hlen[index], bit_cursor);
		}

		/* Push the sign bits */
		for (int i = 0; i < 4; i++)
			if (samples[gr][ch][sample+i] < 0)
				push_bits_inc(new_main_data, 1 << 31, 1, bit_cursor);
			else if (samples[gr][ch][sample+i] > 0)
				push_bits_inc(new_main_data, 0, 1, bit_cursor);
	}
}

/**
 * The reduced samples are rescaled to their original scales and precisions.
 * @param gr
 * @param ch
 */
void mp3::requantize(int gr, int ch)
{
	float exp1, exp2;
	int window = 0;
	int sfb = 0;
	const float scalefac_mult = scalefac_scale[gr][ch] == 0 ? 0.5 : 1;

	for (int sample = 0, i = 0; sample < 576; sample++, i++) {
		if (block_type[gr][ch] == 2 || (mixed_block_flag[gr][ch] && sfb >= 8)) {
			if (i == band_width.short_win[sfb]) {
				i = 0;
				if (window == 2) {
					window = 0;
					sfb++;
				} else
					window++;
			}

			exp1 = global_gain[gr][ch] - 210.0 - 8.0 * subblock_gain[gr][ch][window];
			exp2 = scalefac_mult * scalefac_s[gr][ch][window][sfb];
		} else {
			if (sample == band_index.long_win[sfb + 1])
				/* Don't increment sfb at the zeroth sample. */
				sfb++;

			exp1 = global_gain[gr][ch] - 210.0;
			exp2 = scalefac_mult * scalefac_l[gr][ch][sfb] + preflag[gr][ch] * pretab[sfb];
		}

		float sign = samples[gr][ch][sample] < 0 ? -1.0f : 1.0f;
		float a = pow(abs(samples[gr][ch][sample]), 4.0 / 3.0);
		float b = pow(2.0, exp1 / 4.0);
		float c = pow(2.0, -exp2);

		samples[gr][ch][sample] = sign * a * b * c;
	}
}

/**
 * Reorder short blocks, mapping from scalefactor subbands (for short windows) to 18 sample blocks.
 * @param gr
 * @param ch
 */
void mp3::reorder(int gr, int ch)
{
	int total = 0;
	int start = 0;
	int block = 0;
	float samples[576] = {0};

	for (int sb = 0; sb < 12; sb++) {
		const int SB_WIDTH = band_width.short_win[sb];

		for (int ss = 0; ss < SB_WIDTH; ss++) {
			samples[start + block + 0] = this->samples[gr][ch][total + ss + SB_WIDTH * 0];
			samples[start + block + 6] = this->samples[gr][ch][total + ss + SB_WIDTH * 1];
			samples[start + block + 12] = this->samples[gr][ch][total + ss + SB_WIDTH * 2];

			if (block != 0 && block % 5 == 0) { /* 6 * 3 = 18 */
				start += 18;
				block = 0;
			} else
				block++;
		}

		total += SB_WIDTH * 3;
	}

	for (int i = 0; i < 576; i++)
		this->samples[gr][ch][i] = samples[i];
}

/**
 * The left and right channels are added together to form the middle channel. The
 * difference between each channel is stored in the side channel.
 * @param gr
 */
void mp3::ms_stereo(int gr)
{
	for (int sample = 0; sample < 576; sample++) {
		float middle = samples[gr][0][sample];
		float side = samples[gr][1][sample];
		samples[gr][0][sample] = (middle + side) / SQRT2;
		samples[gr][1][sample] = (middle - side) / SQRT2;
	}
}

/**
 * @param gr
 * @param ch
 */
void mp3::alias_reduction(int gr, int ch)
{
	static const float cs[8] {
			.8574929257, .8817419973, .9496286491, .9833145925,
			.9955178161, .9991605582, .9998991952, .9999931551
	};
	static const float ca[8] {
			-.5144957554, -.4717319686, -.3133774542, -.1819131996,
			-.0945741925, -.0409655829, -.0141985686, -.0036999747
	};

	int sb_max = mixed_block_flag[gr][ch] ? 2 : 32;

	for (int sb = 1; sb < sb_max; sb++)
		for (int sample = 0; sample < 8; sample++) {
			int offset1 = 18 * sb - sample - 1;
			int offset2 = 18 * sb + sample;
			float s1 = samples[gr][ch][offset1];
			float s2 = samples[gr][ch][offset2];
			samples[gr][ch][offset1] = s1 * cs[sample] - s2 * ca[sample];
			samples[gr][ch][offset2] = s2 * cs[sample] + s1 * ca[sample];
		}
}

/**
 * Inverted modified discrete cosine transformations (IMDCT) are applied to each
 * sample and are afterwards windowed to fit their window shape. As an addition, the
 * samples are overlapped.
 * @param gr
 * @param ch
 */
void mp3::imdct(int gr, int ch)
{
	static bool init = true;
	static float sine_block[4][36];
	static float prev_samples[2][32][18];
	float sample_block[36];

	if (init) {
		int i;
		for (i = 0; i < 36; i++)
			sine_block[0][i] = sin(PI / 36.0 * (i + 0.5));
		for (i = 0; i < 18; i++)
			sine_block[1][i] = sin(PI / 36.0 * (i + 0.5));
		for (; i < 24; i++)
			sine_block[1][i] = 1.0;
		for (; i < 30; i++)
			sine_block[1][i] = sin(PI / 12.0 * (i - 18.0 + 0.5));
		for (; i < 36; i++)
			sine_block[1][i] = 0.0;
		for (i = 0; i < 12; i++)
			sine_block[2][i] = sin(PI / 12.0 * (i + 0.5));
		for (i = 0; i < 6; i++)
			sine_block[3][i] = 0.0;
		for (; i < 12; i++)
			sine_block[3][i] = sin(PI / 12.0 * (i - 6.0 + 0.5));
		for (; i < 18; i++)
			sine_block[3][i] = 1.0;
		for (; i < 36; i++)
			sine_block[3][i] = sin(PI / 36.0 * (i + 0.5));
	}

	const int N = block_type[gr][ch] == 2 ? 12 : 36;
	const int HALF_N = N / 2;
	int sample = 0;

	for (int block = 0; block < 32; block++) {
		for (int win = 0; win < (block_type[gr][ch] == 2 ? 3 : 1); win++) {
			for (int i = 0; i < N; i++) {
				float xi = 0.0;
				for (int k = 0; k < HALF_N; k++) {
					float s = samples[gr][ch][18 * block + HALF_N * win + k];
					xi += s * cos(PI / (2 * N) * (2 * i + 1 + HALF_N) * (2 * k + 1));
				}

				/* Windowing samples. */
				sample_block[win * N + i] = xi * sine_block[block_type[gr][ch]][i];
			}
		}

		if (block_type[gr][ch] == 2) {
			float temp_block[36];
			memcpy(temp_block, sample_block, 36 * 4);

			int i = 0;
			for (; i < 6; i++)
				sample_block[i] = 0;
			for (; i < 12; i++)
				sample_block[i] = temp_block[0 + i - 6];
			for (; i < 18; i++)
				sample_block[i] = temp_block[0 + i - 6] + temp_block[12 + i - 12];
			for (; i < 24; i++)
				sample_block[i] = temp_block[12 + i - 12] + temp_block[24 + i - 18];
			for (; i < 30; i++)
				sample_block[i] = temp_block[24 + i - 18];
			for (; i < 36; i++)
				sample_block[i] = 0;
		}

		/* Overlap. */
		for (int i = 0; i < 18; i++) {
			samples[gr][ch][sample + i] = sample_block[i] + prev_samples[ch][block][i];
			prev_samples[ch][block][i] = sample_block[18 + i];
		}
		sample += 18;
	}
}

/**
 * @param gr
 * @param ch
 */
void mp3::frequency_inversion(int gr, int ch)
{
	for (int sb = 1; sb < 18; sb += 2)
		for (int i = 1; i < 32; i += 2)
			samples[gr][ch][i * 18 + sb] *= -1;
}

/**
 * @param gr
 * @param ch
 */
void mp3::synth_filterbank(int gr, int ch)
{
	static float fifo[2][1024];
	static float N[64][32];
	static bool init = true;

	if (init) {
		init = false;
		for (int i = 0; i < 64; i++)
			for (int j = 0; j < 32; j++)
				N[i][j] = cos((16.0 + i) * (2.0 * j + 1.0) * (PI / 64.0));
	}

	float S[32], U[512], W[512];
	float pcm[576];

	for (int sb = 0; sb < 18; sb++) {
		for (int i = 0; i < 32; i++)
			S[i] = samples[gr][ch][i * 18 + sb];

		for (int i = 1023; i > 63; i--)
			fifo[ch][i] = fifo[ch][i - 64];

		for (int i = 0; i < 64; i++) {
			fifo[ch][i] = 0.0;
			for (int j = 0; j < 32; j++)
				fifo[ch][i] += S[j] * N[i][j];
		}

		for (int i = 0; i < 8; i++)
			for (int j = 0; j < 32; j++) {
				U[i * 64 + j] = fifo[ch][i * 128 + j];
				U[i * 64 + j + 32] = fifo[ch][i * 128 + j + 96];
			}

		for (int i = 0; i < 512; i++)
			W[i] = U[i] * synth_window[i];

		for (int i = 0; i < 32; i++) {
			float sum = 0;
			for (int j = 0; j < 16; j++)
				sum += W[j * 32 + i];
			pcm[32 * sb + i] = sum;
		}
	}

	memcpy(samples[gr][ch], pcm, 576 * 4);
}

void mp3::interleave()
{
	int i = 0;
	static const int CHANNELS = mono ? 1 : 2;
	for (int gr = 0; gr < 2; gr++)
		for (int sample = 0; sample < 576; sample++)
			for (int ch = 0; ch < CHANNELS; ch++)
				pcm[i++] = samples[gr][ch][sample];

}

void mp3::interleave_dct()
{
    int i = 0;
	static const int CHANNELS = mono ? 1 : 2;
	for (int gr = 0; gr < 2; gr++)
		for (int sample = 0; sample < 576; sample++)
			for (int ch = 0; ch < CHANNELS; ch++)
				dct[i++] = samples[gr][ch][sample];
}

float *mp3::get_samples()
{
	return pcm;
}

int *mp3::get_samples_dct()
{
	return dct;
}
