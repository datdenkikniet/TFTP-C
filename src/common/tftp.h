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

// Return value definitions
#define TFTP_SUCCESS 1
#define TFTP_TOO_LITTLE_DATA -1
#define TFTP_INVALID_OPCODE -2
#define TFTP_INVALID_NAME -3
#define TFTP_INVALID_MODE -4
#define TFTP_INVALID_OPTION -5
#define TFTP_INVALID_NUMBER -6
#define TFTP_STRING_TOO_LONG -7
#define TFTP_SEND_FAILED -8
#define TFTP_RECV_FAILED -9
#define TFTP_OP_ERROR -10
#define TFTP_ERROR -11;

// Specification definitions
#define TFTP_OPCODE_READ_REQUEST 1u
#define TFTP_OPCODE_WRITE_REQUEST 2u
#define TFTP_OPCODE_DATA 3u
#define TFTP_OPCODE_ACKNOWLEDGEMENT 4u
#define TFTP_OPCODE_ERROR 5u
#define TFTP_OPCODE_OACK 6u

#define TFTP_OPTION_INVALID -2
#define TFTP_OPTION_UNKNOWN -1
#define TFTP_OPTION_BLOCKSIZE 0
#define TFTP_OPTION_TIMEOUT 1
#define TFTP_OPTION_WINDOW_SIZE 2

#define TFTP_ERROR_UNDEF 0
#define TFTP_ERROR_ENOENT 1
#define TFTP_ERROR_ACCESS_VIOLATION 2
#define TFTP_ERROR_DISK_FULL 3
#define TFTP_ERROR_ILLEGAL_OP 4
#define TFTP_ERROR_UNKNOWN_TID 5
#define TFTP_ERROR_FILE_EXISTS 6
#define TFTP_ERROR_NO_SUCH_USER 7



extern const char *TFTP_BLOCKSIZE_STRING;
extern const char *TFTP_TIMEOUT_STRING;
// extern const char *TFTP_WINDOW_SIZE_STRING;

extern const char *TFTP_ERROR_UNDEFINED_STRING;
extern const char *TFTP_ERROR_ENOENT_STRING;
extern const char *TFTP_ERROR_ACCESS_VIOLATION_STRING;
extern const char *TFTP_ERROR_DISK_FULL_STRING;
extern const char *TFTP_ERROR_ILLEGAL_OP_STRING;
extern const char * TFTP_ERROR_UNKNOWN_TID_STRING;
extern const char * TFTP_ERROR_FILE_EXISTS_STRING;
extern const char *TFTP_ERROR_NO_SUCH_USER_STRING;


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
    uint16_t block_num;

    // The actual length of the buffer (= block size)
    uint16_t buffer_length;

    // The length of the data in the buffer
    uint16_t data_size;

    uint8_t *buffer;
} tftp_packet_data;

typedef struct {
    uint16_t block_num;
} tftp_packet_ack;

typedef struct {
    uint16_t opcode;

    uint16_t error_code;

    char message[512];

    uint16_t error_message_length;
} tftp_packet_error;

typedef struct {
    int has_block_size;
    uint16_t block_size;

    int has_timeout;
    uint8_t timeout;

    int has_window_size;
    uint16_t window_size;
} tftp_packet_optionack;

typedef struct {

    int original_socket;
    int socket;

    struct sockaddr *client_addr;
    unsigned int client_addr_size;

    tftp_packet_request request;

    int rx_size;
    uint8_t *rx_buffer;

    int tx_size;
    uint8_t *tx_buffer;
} tftp_transmission;


char *tftp_test_string(char *possible_string_start, int max_length);

tftp_transmission tftp_create_transmission(uint16_t block_size);

void tftp_stop_transmission(tftp_transmission *transmission);

tftp_packet_optionack tftp_create_packet_oack();

tftp_packet_error tftp_create_packet_error();

tftp_packet_data  tftp_create_packet_data();

tftp_packet_ack tftp_create_packet_ack();

int tftp_parse_packet_request(tftp_packet_request *request, const uint8_t *data, uint16_t data_length);

int tftp_parse_option(char *possible_option, int max_length, char **option_end_ptr);

long tftp_parse_ascii_number(char *start, int max_length, char **value_end_ptr);

long tftp_write_number_option(uint8_t *start_ptr, const char *option_name, long value);

int tftp_request_has_options(tftp_packet_request request);

int tftp_set_error_message(tftp_packet_error *error, const char *message);

int tftp_set_error(tftp_packet_error *error, int error_number);

int tftp_send_error(tftp_transmission *transmission, tftp_packet_error *error, int from_original_socket);

int tftp_send_oack(tftp_transmission *transmission, tftp_packet_optionack optionack);

int tftp_send_data(tftp_transmission *transmission, tftp_packet_data *data, int copy_buffer);

int tftp_receive_ack(tftp_transmission *transmission, tftp_packet_ack *ack, tftp_packet_error *error);
#endif //TFTPSERVER_PACKET_H
