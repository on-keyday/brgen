
#include <stdint.h>

typedef struct {
    const uint8_t* buffer;
    size_t buffer_size;
    size_t index;
    uint8_t top;
    uint8_t top_enable : 1;
    uint8_t bit_index : 7;
} Input;

int input_peek_top(Input* input) {
    if (!input->top_enable) {
        if (input->buffer_size >= input->index) {
            return 0;
        }
        input->top = input->buffer[0];
    }
    return 1;
}

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
} LongPacket;

typedef struct {
    uint8_t form;
    union {
        struct {
            uint8_t fixed_bit_long_packet_type_reserved_packet_number_length;
            uint32_t version;
            ConnectionID dst_conn_id;
            ConnectionID src_conn_id;
        };
        struct {
            ConnectionID dst_conn_id;
        };
    };
} QUICPacket;

int decode_LongPacket(Input* input) {
}

int decode_QUICPacket(Input* input, QUICPacket* output) {
    if (!input_peek_top(input)) {
        return 0;
    }
    uint8_t form = (input->top & 0x80) >> 7;
    output->form = form;
    if (!input_seek_bit(input, 1)) {
        return 0;
    }
    if (form != 0) {
    }
}