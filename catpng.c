#include <arpa/inet.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "./catpng.h"
//#include "./main_write_header_cb.h"
#include "./crc.h"
#include "./zutil.h"


bool check_PNG_header(char* buf, long len);
bool write_PNG_header(char* buf, long len);
bool write_PNG_data_IHDR(char* buf, long len);
bool write_PNG_width(char* buf, unsigned width, long len);
bool write_PNG_height(char* buf, unsigned height, long len);
int get_PNG_width(char* buf, long len);
int get_PNG_height(char* buf, long len);

bool check_CRC_error(char* buf, long len);
unsigned get_chunk_data_length(char* buf, char* chunk, long len);
char* get_chunk_pointer(char*buf, long len, unsigned chunk_type);
unsigned get_CRC(char* buf, char* chunk, long len);


int catpng(RECV_BUF** recv_data_arr, unsigned n) {
	if (!recv_data_arr) { return -1; }

	unsigned PNG_count = 0;
	char* data_arr[n];
	U64 data_len_arr[n];
	for (int i = 0; i < n; i++) {
			data_arr[i] = NULL;
			data_len_arr[i] = 0;
	}

	unsigned all_height = 0;
	unsigned all_width = 0;
    U64 all_len = 0;
    char* all_buf;

	U64 decompressed_len = 0;
	char* decompressed_data = NULL;

	U64 compressed_len = 0;
	char* compressed_data = NULL;

	for (int i = 0; i < n; i++) {
		if (!recv_data_arr[i]) {
			data_arr[i] = NULL;	
			continue; 
		}

		char* buffer;
		long file_length = recv_data_arr[i]->size;

		bool is_PNG;
		unsigned width_PNG;
		unsigned height_PNG;

		buffer = (char*) malloc((recv_data_arr[i]->size + 1) * sizeof(char));
		if (buffer == NULL) { printf("Malloc Error\n"); return -1; }
 		buffer[file_length] = '\0';
    	
		memcpy(buffer, recv_data_arr[i]->buf, file_length);

		is_PNG = check_PNG_header(buffer, file_length);
    	if (!is_PNG) {
			data_arr[i] = NULL;
        	free(buffer);
			continue;
    	}
		
    	width_PNG = get_PNG_width(buffer, file_length);
    	height_PNG = get_PNG_height(buffer, file_length);
	
		if (!all_width) {
			all_width = width_PNG;
		} else if (width_PNG != all_width) {
			data_arr[i] = NULL;
			free(buffer);
			continue;
		}
		all_height += height_PNG;

		char* p_IDAT = get_chunk_pointer(buffer, file_length, 1);
		if(!p_IDAT) {
			data_arr[i] = NULL;
			free(buffer);
		   	continue;
		}
		
		U64 IDAT_data_len = get_chunk_data_length(buffer, p_IDAT, file_length);
		data_arr[i] = (char*)malloc((height_PNG*(width_PNG*4 + 1))*sizeof(char));
		
		int decompress_res = mem_inf((U8*)data_arr[i], &data_len_arr[i],
									 (U8*)(p_IDAT+8), IDAT_data_len);
		
		if (decompress_res != Z_OK) {
			printf("Decompression failed");
			free(data_arr[i]);
			data_arr[i] = NULL;
			data_len_arr[i] = 0;
			free(buffer);
			continue;
		}

		free(buffer);
		++PNG_count;
	}

	if (!PNG_count) {
		for (int i = 0; i < n; i++)
        	if (data_arr[i])
            	free(data_arr[i]);
		printf("No PNG file found\n");
		return 0;
	}


	for (int i = 0; i < n; i++)
		if (data_len_arr[i] && data_arr[i]){
			decompressed_len += data_len_arr[i];
			printf("PNG i: %d, len: %d", i, data_len_arr[i]);
		}
	printf("Decompressed Lenght *** %u\n", decompressed_len);

	decompressed_data = (char*)malloc(decompressed_len*sizeof(char) + 1);
	if (!decompressed_data) { printf("Malloc Err\n"); return -1; }
	decompressed_data[decompressed_len] = '\0';
	
	unsigned index = 0;
	for (int i = 0; i < n; i++) {
		if (data_arr[i] == NULL)
			continue;
		for (U64 j = 0; j < data_len_arr[i]; j++) {
			decompressed_data[index] = data_arr[i][j];
			++index;
		}	
	}
	
	compressed_data = (char*)malloc(decompressed_len * sizeof(char));
	int compressed_status = mem_def((U8*)compressed_data, &compressed_len,
									(U8*)decompressed_data, decompressed_len, 6);
	if (compressed_status != Z_OK) {
		printf("Compression Failed: %d\n", compressed_status);
		return -1;
	}
	
	all_len = 8 + 3*4 + 13 + 3*4 + compressed_len + 3*4;
	all_buf = (char*)malloc(all_len*sizeof(char) + 1);
	if (!all_buf) { printf("Malloc Err\n"); return -1; }
	all_buf[all_len] = '\0';
	
	char* p_IHDR = (char*)all_buf + 8;
	char* p_IDAT = (char*)all_buf + 33;
	char* p_IEND = (char*)all_buf + 37 + compressed_len + 4 + 4;
	char PNG_type_IHDR[5] = "IHDR";
	char PNG_type_IDAT[5] = "IDAT";
	char PNG_type_IEND[5] = "IEND";
	unsigned PNG_length_IHDR = 13; 
	unsigned PNG_length_IDAT = compressed_len;
	unsigned PNG_length_IEND = 0;
	unsigned PNG_CRC_IHDR;
	unsigned PNG_CRC_IDAT;
	unsigned PNG_CRC_IEND;
	
	write_PNG_header(all_buf, all_len);
	write_PNG_width(all_buf, all_width, all_len);
	write_PNG_height(all_buf, all_height, all_len);
	write_PNG_data_IHDR(all_buf, all_len);

	*(unsigned*)((char*)all_buf + 12) = *(unsigned*)PNG_type_IHDR;
	*(unsigned*)((char*)all_buf + 37) = *(unsigned*)PNG_type_IDAT;
	*(unsigned*)((char*)all_buf + all_len - 8)
				= *(unsigned*)PNG_type_IEND;

	for (U64 i = 0; i < compressed_len; i++)
			all_buf[i + 41] = compressed_data[i];

	*(unsigned*)((char*)all_buf + 8) = htonl(PNG_length_IHDR);
	*(unsigned*)((char*)all_buf + 33) = htonl(PNG_length_IDAT);
	*(unsigned*)((char*)all_buf + all_len - 12)
				= htonl(PNG_length_IEND);

	PNG_CRC_IHDR = htonl(crc((unsigned char*)(p_IHDR + 4), PNG_length_IHDR + 4));
	PNG_CRC_IDAT = htonl(crc((unsigned char*)(p_IDAT + 4), PNG_length_IDAT + 4));
	PNG_CRC_IEND = htonl(crc((unsigned char*)(p_IEND + 4), PNG_length_IEND + 4));

	*(unsigned*)((char*)all_buf + 29) = PNG_CRC_IHDR;
    *(unsigned*)((char*)all_buf + 41 + compressed_len) = PNG_CRC_IDAT;
    *(unsigned*)((char*)all_buf + all_len - 4) = PNG_CRC_IEND;

	FILE *new_file = fopen("all.png", "wb");
	if (!new_file) { printf("New file creation Err\n"); return -1; }

	for (unsigned i = 0; i < all_len; i++)
			fputc(all_buf[i], new_file);
	fclose(new_file);


	// clean up
	free(decompressed_data);
	free(compressed_data);
	free(all_buf);

	for (int i = 0; i < n; i++)
		if (data_arr[i])
			free(data_arr[i]);

	return 0;
}


bool check_PNG_header(char* buf, long int len) {
    if (len < 8) { return false; }
    const char png_check[] = "PNG";
    for (int i = 0; i < 3; i++) {
        if (buf[i+1] != png_check[i])
            return false;
    }
    return true;
}

unsigned get_chunk_data_length(char* buf, char* chunk,  long len) {
    if (chunk - buf + 4 >= len || chunk == NULL || buf == NULL) {
        return -1;
    }
    return htonl(*(unsigned*)chunk);
}

bool write_PNG_header(char* buf, long len) {
	if (len < 8) { return false; }
	unsigned char PNG_header[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
	for ( int i = 0; i < 8; i++)
		buf[i] = PNG_header[i];
	return true;
}

bool write_PNG_width(char* buf, unsigned width, long len) {
	if (len < 20) { return false; }
	char* pointer = buf + 16;
	*(unsigned*)pointer = htonl(width);
	return true;
}

bool write_PNG_height(char* buf, unsigned height, long len) {
	if (len < 24) { return false; }
	char* pointer = buf + 20;
	*(unsigned*)pointer = htonl(height);
	return true;
}

bool write_PNG_data_IHDR(char* buf, long len) {
	if (len < 29) { return false; }
	char PNG_data_IHDR[13] = { 0,0,0,0, 0,0,0,0, 8, 6, 0, 0, 0 };
	for (int i = 0; i < 5; i++)
		buf[i + 24] = PNG_data_IHDR[i + 8];
	return true;
}

int get_PNG_height(char* buf, long len) {
    int offset = 20;
    if (len < 24 || buf == NULL) { return -1; }
    return htonl(*(unsigned*)&buf[0 + offset]);
}

int get_PNG_width(char* buf, long len) {
    int offset = 16;
    if (len < 20 || buf == NULL) { return -1; }
    return htonl(*(unsigned*)&buf[0 + offset]);
}

char* get_chunk_pointer(char*buf, long len, unsigned chunk_type){
    if (chunk_type >= 3) { printf("Chunk type 0, 1, 2\n"); return NULL; }

    long int current_pos = 0;
    int png_data_len = 0;

    current_pos += 8;

    if (chunk_type == 0) {   // reach IHDR
        if (current_pos >= len) { printf("IHDR Exceeds len\n"); return NULL; }
        return &buf[current_pos];
    }
    current_pos += 4 + 4 + 13 + 4;

    if (chunk_type == 1) {  //reach IDAT
        if (current_pos >= len) { printf("IDAT Exceeds len\n"); return NULL; }
        return (char*)buf + current_pos;
    }

    png_data_len = get_chunk_data_length(buf, (char*)(buf + current_pos), len);
    if (png_data_len < 0) { printf("Chunk Exceeds len\n"); return NULL; }

    current_pos += 4 + 4 + png_data_len + 4;

    if (current_pos >= len) { printf("IEND Exceeds len\n"); return NULL; }
    return &buf[current_pos]; //reach IEND
}

unsigned get_CRC(char* buf, char* chunk, long len) {
    long offset = (long)( (char*)chunk - (char*)buf );
    if (len < offset || chunk < buf || buf == NULL || chunk == NULL)
        return -1;

    unsigned length = get_chunk_data_length(buf, chunk, len);
    char* crc_pointer = &chunk[4 + 4 + length];
    if ((crc_pointer + 3) > &buf[len-1]) { return -1; }

    return htonl(*(unsigned*)crc_pointer);
}



