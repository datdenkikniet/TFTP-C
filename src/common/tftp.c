/* 

    Provide an implementation for tftp.h
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
#define DEBUG
#ifdef DEBUG

#include <stdio.h>

#endif

#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <errno.h>
#include "tftp.h"

const char *TFTP_BLOCKSIZE_STRING = "blksize";
const char *TFTP_TIMEOUT_STRING = "timeout";
const char *TFTP_WINDOW_SIZE_STRING = "windowsize";

const char *TFTP_ERROR_ENOENT_STRING = "No such file.";
const char *TFTP_ERROR_ACCESS_VIOLATION_STRING = "Access violation";
const char *TFTP_ERROR_DISK_FULL_STRING = "Disk full or allocation exceeded.";
const char *TFTP_ERROR_ILLEGAL_OP_STRING = "Illegal operation";
const char *TFTP_ERROR_UNKNOWN_TID_STRING = "Unknown TID.";
const char *TFTP_ERROR_FILE_EXISTS_STRING = "File exists";
const char *TFTP_ERROR_NO_SUCH_USER_STRING = "No such user";

char *tftp_test_string(char *possible_string_start, int max) {
    char *string_end_index = possible_string_start;
    for (int i = 0; i < max; i++) {
        char current = *(string_end_index++);
        if (current == 0) {
            return string_end_index - 1;
        }
    }
    return NULL;
}

int tftp_parse_packet_request(tftp_packet_request *request, const uint8_t *data, const uint16_t data_length) {
    request->opcode = -1;
    memset(request->filename, 0, sizeof(request->filename));
    memset(request->mode, 0, sizeof(request->mode));
    request->has_window_size = 0;
    request->has_timeout = 0;
    request->has_block_size = 0;

    if (data_length < 6) {
        return TFTP_TOO_LITTLE_DATA;
    }

    uint16_t data_length_left = data_length;

    uint16_t opcode = (data[0] << 8u) + (data[1]);

    request->opcode = opcode;

    data_length_left -= 2;

    if (opcode != TFTP_OPCODE_READ_REQUEST && opcode != TFTP_OPCODE_WRITE_REQUEST) {
        return TFTP_INVALID_OPCODE;
    }

    uint16_t max_length_filename =
            data_length_left <= sizeof(request->filename) ? data_length_left : sizeof(request->filename);

    char *file_name_start = (char *) data + 2;
    char *file_name_end = tftp_test_string(file_name_start, max_length_filename);

    if (file_name_end == NULL) {
        return TFTP_INVALID_NAME;
    }

    strcpy(request->filename, file_name_start);

    data_length_left -= (file_name_end - file_name_start + 1);

    uint16_t max_length_mode = data_length_left <= sizeof(request->mode) ? data_length_left : sizeof(request->mode);

    char *mode_name_start = (char *) file_name_end + 1;
    char *mode_name_end = tftp_test_string(mode_name_start, max_length_mode);

    if (mode_name_end == NULL || strcmp(mode_name_end, "octet") == 0) {
        return TFTP_INVALID_MODE;
    }

    strcpy(request->mode, mode_name_start);

    data_length_left -= (mode_name_end - mode_name_start + 1);
    char *start_ptr = mode_name_end + 1;
    char *end_ptr;
    while (data_length_left > 2) {
        int option = tftp_parse_option(start_ptr, data_length_left, &end_ptr);
        data_length_left -= end_ptr - start_ptr + 1;
        if (option == TFTP_OPTION_TIMEOUT) {
            char *value_end_ptr;
            long timeout = tftp_parse_ascii_number(end_ptr + 1, data_length_left, &value_end_ptr);

            data_length_left -= value_end_ptr - end_ptr - 1;
            start_ptr = value_end_ptr + 1;

            if (timeout >= 1 && timeout <= 255) {
                request->has_timeout = 1;
                request->timeout = timeout;
#ifdef DEBUG
                printf("Found timeout %li\n", timeout);
#endif
            }
        } else if (option == TFTP_OPTION_BLOCKSIZE) {
            char *value_end_ptr;
            long blocksize = tftp_parse_ascii_number(end_ptr + 1, data_length_left, &value_end_ptr);

            data_length_left -= value_end_ptr - end_ptr - 1;
            start_ptr = value_end_ptr + 1;

            if (blocksize >= 8 && blocksize <= 65464) {
                request->has_block_size = 1;
                request->block_size = blocksize;
#ifdef DEBUG
                printf("Found blocksize %li\n", blocksize);
#endif
            }
        } else if (option == TFTP_OPTION_WINDOW_SIZE) {
            char *value_end_ptr;
            long window_size = tftp_parse_ascii_number(end_ptr + 1, data_length_left, &value_end_ptr);

            data_length_left -= value_end_ptr - end_ptr - 1;
            start_ptr = value_end_ptr + 1;

            if (window_size >= 1 && window_size <= 65535) {
                request->has_block_size = 1;
                request->block_size = window_size;
#ifdef DEBUG
                printf("Found window size %li\n", window_size);
#endif
            }
        } else if (option == TFTP_OPTION_INVALID) {
            return TFTP_INVALID_OPTION;
        } else {
#ifdef DEBUG
            printf("Found unknown option %s\n", start_ptr);
#endif
            char *value_end = tftp_test_string(end_ptr + 1, data_length_left);
            data_length_left -= value_end - end_ptr - 1;
            start_ptr = value_end + 1;
        }
    }

    return TFTP_SUCCESS;
}

int tftp_parse_option(char *possible_option, int max_length, char **option_end_ptr) {
    char *option_start = possible_option;
    char *option_end = tftp_test_string(option_start, max_length);

    if (option_end == NULL) {
        return TFTP_OPTION_INVALID;
    } else {
        *option_end_ptr = option_end;
        if (strcmp(option_start, TFTP_TIMEOUT_STRING) == 0) {
            return TFTP_OPTION_TIMEOUT;
        } else if (strcmp(option_start, TFTP_BLOCKSIZE_STRING) == 0) {
            return TFTP_OPTION_BLOCKSIZE;
        } else if (strcmp(option_start, TFTP_WINDOW_SIZE_STRING) == 0) {
            return TFTP_OPTION_WINDOW_SIZE;
        }
    }
    return TFTP_OPTION_UNKNOWN;
}

long tftp_parse_ascii_number(char *data, int max_length, char **value_end_ptr) {

    char *ascii_nr_start = data;
    char *ascii_nr_end = tftp_test_string(ascii_nr_start, max_length);

    if (ascii_nr_end == NULL) {
        return TFTP_INVALID_OPTION;
    }

    long value = strtol(ascii_nr_start, value_end_ptr, 10);
    if (value == 0 && errno != 0) {
        return TFTP_INVALID_NUMBER;
    }
    return value;
}

void tftp_init_transmission(tftp_transmission *transmission, uint16_t block_size) {
    transmission->socket = -1;
    transmission->original_socket = -1;
    transmission->client_addr_size = 0;
    transmission->client_addr = NULL;
    transmission->rx_size = 4 + block_size;
    transmission->rx_buffer = malloc(4 + block_size);
    transmission->tx_size = 4 + block_size;
    transmission->tx_buffer = malloc(4 + block_size);
}

void tftp_init_error(tftp_packet_error *packet) {
    packet->opcode = TFTP_OPCODE_ERROR;
    packet->error_code = TFTP_ERROR_UNDEF;
    packet->error_message_length = 0;
    memset(packet->message, 0, sizeof(packet->message));
}

int tftp_set_error_message(tftp_packet_error *error, const char *message) {
    unsigned long length = strlen(message);
    if (length < sizeof(error->message)) {
        error->error_message_length = length + 1;
        strcpy(error->message, message);
        printf("%s\n%s\n", message, error->message);
        return TFTP_SUCCESS;
    } else {
        return TFTP_STRING_TOO_LONG;
    }
}

int tftp_send_error(tftp_transmission transmission, tftp_packet_error *error, int from_original_socket) {
    int socket;
    if (from_original_socket) {
        socket = transmission.original_socket;
    } else {
        socket = transmission.socket;
    }
    int error_message_length = error->error_message_length == 0 ? 1 : error->error_message_length;
    error->opcode = htons(error->opcode);
    int sent = sendto(socket, error, 4 + error_message_length, 0, transmission.client_addr,
                      transmission.client_addr_size);

    if (sent < 0 && !from_original_socket) {
        tftp_send_error(transmission, error, 1);
    }

    error->opcode = ntohs(error->opcode);
    return 0;
}

int tftp_request_has_options(tftp_packet_request request) {
    return request.has_block_size || request.has_timeout || request.has_window_size;
}

void tftp_init_oack(tftp_packet_optionack *optionack) {
    optionack->opcode = TFTP_OPCODE_OACK;
    optionack->has_window_size = 0;
    optionack->has_timeout = 0;
    optionack->has_block_size = 0;
}

int tftp_send_oack(tftp_transmission transmission, tftp_packet_optionack optionack) {

    uint8_t *start_ptr = transmission.tx_buffer;
    tftp_packet_request req = transmission.request;

    *(start_ptr++) = optionack.opcode >> 8u & 0xFF;
    *(start_ptr++) = optionack.opcode & 0xff;

    if (req.has_block_size) {
        start_ptr += tftp_write_number_option(start_ptr, TFTP_BLOCKSIZE_STRING, optionack.block_size);
    }
    if (req.has_window_size) {
        start_ptr += tftp_write_number_option(start_ptr, TFTP_WINDOW_SIZE_STRING, optionack.window_size);

    }
    if (req.has_timeout) {
        start_ptr += tftp_write_number_option(start_ptr, TFTP_TIMEOUT_STRING, optionack.timeout);
    }

    long length = start_ptr - transmission.tx_buffer;
    int sent = sendto(transmission.socket, transmission.tx_buffer, length, 0, transmission.client_addr,
                      transmission.client_addr_size);
    if (sent < 0){
        printf("Errno: %d, length: %li\n", errno, length);
        return TFTP_SEND_FAILED;
    }
    printf("Sent oack\n");
    return TFTP_SUCCESS;
}

long tftp_write_number_option(uint8_t *start_ptr, const char *option_name, long value){
    uint8_t *end_ptr = start_ptr;
    strcpy(end_ptr, option_name);
    end_ptr += strlen(option_name) + 2;
    int printed = sprintf(end_ptr, "%d", value);
    end_ptr += printed + 2;
    return end_ptr - start_ptr;
}
