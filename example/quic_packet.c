
#include <stdint.h>

typedef struct {
    const uint8_t* buffer;
    size_t buffer_size;  // byte
    size_t bit_index;    // bit
} Input;

int input_seek_bit(Input* input, int bit) {
    if (!input_peek_top(input)) {
        return 0;
    }
    if (input->bit_index == 8) {
    }
}

typedef struct {
    size_t size;
    union {
        const uint8_t* write_buf;
        uint8_t* read_buf;
    };
} ConnectionID;

typedef struct {
    uint8_t long_packet_type : 2;
    uint8_t reserved : 2;
    uint8_t packet_number_length : 2;
    uint32_t version;
    ConnectionID dst_conn_id;
    ConnectionID src_conn_id;
} LongPacket;

typedef struct {
    uint8_t spin : 1;
    uint8_t reserved : 2;
    uint8_t key_phase : 1;
    uint8_t packet_number_length : 2;
    ConnectionID dst_conn_id;
} OneRTTPacket;

typedef struct {
    uint8_t form : 1;
    uint8_t fixed : 1;
    union {
        LongPacket long_packet__;
        OneRTTPacket one_rtt_packet__;
    };
} QUICPacket;

int decode_QUICPacket(Input* input, QUICPacket* output) {
    output->form = (input->buffer[input->bit_index >> 3] & 0x80);
    output->fixed = (input->buffer[input->bit_index >> 3] & 0x40);
    input->bit_index += 2;
    if (output->form == 1) {
        if (!decode_LongPacket(input, &output->long_packet__)) {
            return 0;
        }
    }
    else {
        if (!decode_OneRTTPacket(input, &output->one_rtt_packet__)) {
            return 0;
        }
    }
}
