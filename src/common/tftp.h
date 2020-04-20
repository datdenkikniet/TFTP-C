/* 

    Functions and structs that can be used for TFTP
    Copyright (C) 2020 Johannes Draaijer

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 
 */

#ifndef TFTPSERVER_PACKET_H
#define TFTPSERVER_PACKET_H

#include <stdint.h>
#include <sys/socket.h>

#define TFTP_OPCODE_READ_REQUEST 1
#define TFTP_OPCODE_WRITE_REQUEST 2
#define TFTP_OPCODE_DATA 3
#define TFTP_OPCODE_ACKNOWLEDGEMENT 4
#define TFTP_OPCODE_ERROR 5
#define TFTP_OPCODE_OACK 6
#define TFTP_OPCODE_MIN TFTP_OPCODE_READ_REQUEST
#define TFTP_OPCODE_MAX TFTP_OPCODE_OACK

#define TFTP_OPTION_INVALID -2
#define TFTP_OPTION_UNKNOWN -1
#define TFTP_OPTION_BLOCKSIZE 0
#define TFTP_OPTION_TIMEOUT 1
#define TFTP_OPTION_WINDOW_SIZE 2

#define TFTP_SUCCESS 1
#define TFTP_TOO_LITTLE_DATA -1
#define TFTP_INVALID_OPCODE -2
#define TFTP_INVALID_NAME -3
#define TFTP_INVALID_MODE -4
#define TFTP_INVALID_OPTION -5
#define TFTP_INVALID_NUMBER -6


extern const char *TFTP_BLOCKSIZE_STRING;
extern const char *TFTP_TIMEOUT_STRING;
extern const char *TFTP_WINDOW_SIZE_STRING;


typedef struct {
    uint16_t opcode;
    char filename[256];
    char mode[256];

    int has_block_size;
    uint16_t block_size;

    int has_timeout;
    uint8_t timeout;

    int has_window_size;
    uint16_t window_size;
} tftp_packet_request;

typedef struct {
    uint16_t opcode;

    // The actual length of the data buffer (= block size)
    uint16_t data_length;

    // The used length of the data buffer
    uint16_t data_size;

    uint8_t *data;
} tftp_packet_data;

typedef struct {
    uint16_t opcode;

    uint16_t block_num;
} tftp_packet_ack;

typedef struct {
    uint16_t opcode;

    uint16_t error_code;

    uint16_t error_message_length;
    char message[512];
} tftp_packet_error;

typedef struct {
    uint16_t opcode;

    int has_block_size;
    uint16_t block_size;

    int has_timeout;
    uint8_t timeout;

    int has_window_size;
    uint16_t window_size;
} tftp_packet_optionack;

typedef struct {

    struct sockaddr *addr;
    int sockaddr_size;

    tftp_packet_request request;

} tftp_transmission;

char *tftp_test_string(char *possible_string_start, int max_length);

int tftp_parse_packet_request(tftp_packet_request *request, const uint8_t *data, uint16_t data_length);

int tftp_parse_option(char *possible_option, int max_length, char **option_end_ptr);

long tftp_parse_ascii_number(char *start, int max_length, char **value_end_ptr);

#endif //TFTPSERVER_PACKET_H
