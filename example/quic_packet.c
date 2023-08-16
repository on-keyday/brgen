
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
    uint8_t fixed_bit_long_packet_type_reserved_packet_number_length;
    uint32_t version;
    ConnectionID dst_conn_id;
    ConnectionID src_conn_id;
} LongPacket;

typedef struct {
    uint8_t fixed_bit_;
} ShortPacket;

typedef struct {
    uint8_t form;
    union {
        LongPacket long_packet__;
        struct {
            ConnectionID dst_conn_id;
        };
    };
} QUICPacket;

int decode_LongPacket(Input* input) {
}

int decode_QUICPacket(Input* input, QUICPacket* output) {
    uint8_t form = (input->buffer[input->bit_index >> 3] & (uint8_t)(0x1 << (7 - (input->bit_index & 0x7)))) >> 7;
    output->form = form;
    if (!input_seek_bit(input, 1)) {
        return 0;
    }
    if (form != 0) {
        if (!decode_LongPacket(input)) {
            return form;
        }
    }
    else {
    }
}