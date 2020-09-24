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
#include <unistd.h>
#include <fcntl.h>
#include "tftp.h"

const char *TFTP_BLOCKSIZE_STRING = "blksize";
const char *TFTP_TIMEOUT_STRING = "timeout";
// const char *TFTP_WINDOW_SIZE_STRING = "windowsize";
const char *TFTP_TSIZE_STRING = "tsize";

const char *TFTP_ERROR_UNDEFINED_STRING = "Undefined error.";
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

tftp_transmission tftp_create_transmission(uint16_t block_size) {
    tftp_transmission transmission;
    transmission.socket = -1;
    transmission.original_socket = -1;
    transmission.file_descriptor = -1;
    transmission.client_addr_size = 0;
    transmission.client_addr = NULL;
    transmission.rx_size = 4 + block_size;
    transmission.rx_buffer = malloc(4 + block_size);
    transmission.tx_size = 4 + block_size;
    transmission.tx_buffer = malloc(4 + block_size);
    return transmission;
}

void tftp_stop_transmission(tftp_transmission *transmission) {
    if (transmission->socket != -1) {
        close(transmission->socket);
    }
    if (transmission->file_descriptor != -1) {
        close(transmission->file_descriptor);
    }
    free(transmission->client_addr);
    free(transmission->rx_buffer);
    free(transmission->tx_buffer);
}

tftp_packet_error tftp_create_packet_error() {
    tftp_packet_error error;
    error.opcode = TFTP_OPCODE_ERROR;
    error.error_code = TFTP_ERROR_UNDEF;
    error.error_message_length = 0;
    memset(error.message, 0, sizeof(error.message));
    tftp_set_error_message(&error, TFTP_ERROR_UNDEFINED_STRING);
    return error;
}

tftp_packet_optionack tftp_create_packet_oack() {
    tftp_packet_optionack oack;
    oack.has_window_size = 0;
    oack.has_timeout = 0;
    oack.has_block_size = 0;
    return oack;
}

tftp_packet_data tftp_create_packet_data() {
    tftp_packet_data data;
    data.block_num = 0;
    data.buffer_length = 0;
    data.data_size = 0;
    data.buffer = NULL;
    return data;
}

tftp_packet_ack tftp_create_packet_ack() {
    tftp_packet_ack ack;
    ack.block_num = 0;
    return ack;
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
    char *end_ptr = NULL;
    while (data_length_left > 2) {
        int option = tftp_parse_option(start_ptr, data_length_left, &end_ptr);
        data_length_left -= end_ptr - start_ptr + 1;

        /*
         * Parse an option.
         * If the value provided for this option has no valid ending, ignore it and the options following it
         * If the value provided for this option is invalid or out of range, set it to the default value specified
         * If the
         * opt      The option to check for
         * bool     The boolean field that indicates if this option is set or not in the tftp_packet_request struct
         * val      The value field that indicates the value of this option in the tftp_packet_request struct
         * min      The minimum allowed value for this option
         * max      The maximum allowed value for this option
         * defvalue The default value of this option
         */
#define DO_PARSE(opt, bool, val, min, max, defvalue)                                                \
        if (option == opt) {                                                                        \
            char *value_end_ptr;                                                                    \
            long value = tftp_parse_ascii_number(end_ptr + 1, data_length_left, &value_end_ptr);    \
            data_length_left -= value_end_ptr - end_ptr - 1;                                        \
            if (value_end_ptr != NULL){                                                             \
                start_ptr = value_end_ptr + 1;                                                      \
            } else {                                                                                \
                break;                                                                              \
            }                                                                                       \
            if (value != TFTP_INVALID_NUMBER && value >= min && (value <= max || max == -1)){       \
              request->bool = 1;                                                                    \
              request->val = value;                                                                 \
            } else {                                                                                \
                request->bool = 1;                                                                  \
                request-> val = defvalue;                                                           \
            }                                                                                       \
        }

        DO_PARSE(TFTP_OPTION_TIMEOUT, has_timeout, timeout, 1, 255, 5)
        else DO_PARSE(TFTP_OPTION_BLOCKSIZE, has_block_size, block_size, 8, 65464, 512)
        else DO_PARSE(TFTP_OPTION_WINDOW_SIZE, has_window_size, window_size, 1, 65535, 4)
        else DO_PARSE(TFTP_OPTION_TSIZE, has_transfer_size, transfer_size, 0, 1, 0)
        else if (option == TFTP_OPTION_INVALID) {
            return TFTP_INVALID_OPTION;
        } else {
            char *value_end = tftp_test_string(end_ptr + 1, data_length_left);
            data_length_left -= value_end - end_ptr - 1;
            if (value_end != NULL) {
                start_ptr = value_end + 1;
            } else {
                break;
            }
        }
    }

    if (!request->has_block_size) {
        request->block_size = 512;
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
        } else if (strcmp(option_start, TFTP_TSIZE_STRING) == 0) {
            return TFTP_OPTION_TSIZE;
        } /* else if (strcmp(option_start, TFTP_WINDOW_SIZE_STRING) == 0) {
                return TFTP_OPTION_WINDOW_SIZE;
            } */
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

long tftp_write_number_option(uint8_t *start_ptr, const char *option_name, long value) {
    uint8_t *end_ptr = start_ptr;
    strcpy(end_ptr, option_name);
    end_ptr += strlen(option_name) + 1;
    int printed = sprintf(end_ptr, "%li", value);
    end_ptr += printed + 1;
    return end_ptr - start_ptr;
}

int tftp_set_error_message(tftp_packet_error *error, const char *message) {
    unsigned long length = strlen(message);
    if (length < sizeof(error->message)) {
        error->error_message_length = length + 1;
        strcpy(error->message, message);
        return TFTP_SUCCESS;
    } else {
        return TFTP_STRING_TOO_LONG;
    }
}

int tftp_set_error(tftp_packet_error *error, int error_number) {
#define HANDLE_ERROR(errnum) if (error_number == errnum) { tftp_set_error_message(error, errnum##_STRING); error->error_code = error_number; return TFTP_SUCCESS;}

    HANDLE_ERROR(TFTP_ERROR_ENOENT)
    HANDLE_ERROR(TFTP_ERROR_ACCESS_VIOLATION)
    HANDLE_ERROR(TFTP_ERROR_ILLEGAL_OP)
    HANDLE_ERROR(TFTP_ERROR_DISK_FULL)
    HANDLE_ERROR(TFTP_ERROR_FILE_EXISTS)
    HANDLE_ERROR(TFTP_ERROR_NO_SUCH_USER)
    return TFTP_ERROR;
}

int tftp_send_error(tftp_transmission *transmission, tftp_packet_error *error, int from_original_socket) {
    int socket;
    if (from_original_socket) {
        socket = transmission->original_socket;
    } else {
        socket = transmission->socket;
    }

    int error_message_length = error->error_message_length == 0 ? 1 : error->error_message_length;
    error->opcode = htons(error->opcode);
    int sent = sendto(socket, error, 4 + error_message_length, 0, transmission->client_addr,
                      transmission->client_addr_size);

    if (sent < 0 && !from_original_socket) {
        tftp_send_error(transmission, error, 1);
    }

    error->opcode = ntohs(error->opcode);
    return 0;
}

int tftp_request_has_options(const tftp_packet_request *request) {
    return request->has_block_size || request->has_timeout || request->has_window_size || request->has_transfer_size;
}

int tftp_send_oack(tftp_transmission *transmission, tftp_packet_optionack optionack) {

    uint8_t *start_ptr = transmission->tx_buffer;

    *(start_ptr++) = TFTP_OPCODE_OACK >> 8u;
    *(start_ptr++) = TFTP_OPCODE_OACK & 0xffu;

    if (optionack.has_block_size) {
        start_ptr += tftp_write_number_option(start_ptr, TFTP_BLOCKSIZE_STRING, optionack.block_size);
    }
    /* if (req.has_window_size) {
        start_ptr += tftp_write_number_option(start_ptr, TFTP_WINDOW_SIZE_STRING, optionack.window_size);

    } */
    if (optionack.has_timeout) {
        start_ptr += tftp_write_number_option(start_ptr, TFTP_TIMEOUT_STRING, optionack.timeout);
    }

    if (optionack.has_transfer_size) {
        start_ptr += tftp_write_number_option(start_ptr, TFTP_TSIZE_STRING, optionack.transfer_size);
    }

    long length = start_ptr - transmission->tx_buffer;
    int sent = sendto(transmission->socket, transmission->tx_buffer, length, 0, transmission->client_addr,
                      transmission->client_addr_size);
    if (sent < 0) {
        return TFTP_SEND_FAILED;
    }
    return TFTP_SUCCESS;
}


int tftp_send_data(tftp_transmission *transmission, tftp_packet_data *data, int copy_buffer) {

    uint8_t *start_ptr = transmission->tx_buffer;
    uint16_t data_size = data->data_size;

    *(start_ptr++) = TFTP_OPCODE_DATA >> 8u;
    *(start_ptr++) = TFTP_OPCODE_DATA & 0xffu;

    *(start_ptr++) = data->block_num >> 8u & 0xFFu;
    *(start_ptr++) = data->block_num & 0xFFu;

    if (copy_buffer) {
        memcpy(start_ptr, data->buffer, data_size);
    }

    int sent = sendto(transmission->socket, transmission->tx_buffer, 4 + data_size, 0, transmission->client_addr,
                      transmission->client_addr_size);
    if (sent < 0) {
        return TFTP_SEND_FAILED;
    }
    return TFTP_SUCCESS;
}

int tftp_receive_ack(tftp_transmission *transmission, tftp_packet_ack *ack, tftp_packet_error *error) {
    int received = recvfrom(transmission->socket, transmission->rx_buffer, transmission->rx_size, 0,
                            transmission->client_addr,
                            &transmission->client_addr_size);
    if (received < 4) {
        return TFTP_RECV_FAILED;
    }
    uint16_t opcode = (transmission->rx_buffer[0] << 8u) + (transmission->rx_buffer[1]);
    uint16_t block_num = (transmission->rx_buffer[2] << 8u) + (transmission->rx_buffer[3]);

    if (opcode == TFTP_OPCODE_ERROR) {
        int offset = 4;
        char *start_ptr = (char *) transmission->rx_buffer + offset;
        int max_size = transmission->rx_size - offset;
        error->error_code = block_num;
        uint8_t *end_ptr = (uint8_t *) tftp_test_string(start_ptr, max_size);
        long error_length = (end_ptr - (transmission->rx_buffer + offset));
        if (end_ptr != NULL && error_length < sizeof(error->message)) {
            strcpy(error->message, (char *) transmission->rx_buffer + offset);
            error->error_message_length = strlen(error->message) + 1;
        } else {
            *error->message = 0;
            error->error_message_length = 0;
        }
        return TFTP_OP_ERROR;
    }

    if (opcode != TFTP_OPCODE_ACKNOWLEDGEMENT) {
        return TFTP_INVALID_OPCODE;
    }
    ack->block_num = block_num;
    return TFTP_SUCCESS;
}

