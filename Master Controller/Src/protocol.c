#include "protocol.h"

void parser_init(parser_t *p) {
    p->state = PARSE_WAIT_START;
    p->payload_idx = 0;
    p->checksum_accum = 0;
}

int parser_feed(parser_t *p, uint8_t byte) {
    switch (p->state) {

    case PARSE_WAIT_START:
        if (byte == PROTO_START_BYTE) {
            p->state = PARSE_SRC;
            p->checksum_accum = 0;
        }
        /* else: discard garbage, stay in WAIT_START */
        break;

    case PARSE_SRC:
        p->frame.src = byte;
        p->checksum_accum ^= byte;
        p->state = PARSE_TYPE;
        break;

    case PARSE_TYPE:
        p->frame.type = byte;
        p->checksum_accum ^= byte;
        p->state = PARSE_LEN;
        break;

    case PARSE_LEN:
        p->frame.len = byte;
        p->checksum_accum ^= byte;
        if (byte > PROTO_MAX_PAYLOAD) {
            /* Invalid length - reset */
            p->state = PARSE_WAIT_START;
        } else if (byte == 0) {
            /* No payload, go straight to checksum */
            p->state = PARSE_CHECKSUM;
        } else {
            p->payload_idx = 0;
            p->state = PARSE_PAYLOAD;
        }
        break;

    case PARSE_PAYLOAD:
        p->frame.payload[p->payload_idx] = byte;
        p->checksum_accum ^= byte;
        p->payload_idx++;
        if (p->payload_idx >= p->frame.len) {
            p->state = PARSE_CHECKSUM;
        }
        break;

    case PARSE_CHECKSUM:
        p->state = PARSE_WAIT_START;
        if (byte == p->checksum_accum) {
            return 1;  /* Valid frame ready in p->frame */
        }
        /* Bad checksum - frame discarded */
        break;
    }

    return 0;
}

uint8_t frame_build(uint8_t *out_buf,
                    uint8_t  src,
                    uint8_t  type,
                    const uint8_t *payload,
                    uint8_t  len) {
    uint8_t checksum = 0;
    uint8_t idx = 0;

    out_buf[idx++] = PROTO_START_BYTE;
    out_buf[idx++] = src;      checksum ^= src;
    out_buf[idx++] = type;     checksum ^= type;
    out_buf[idx++] = len;      checksum ^= len;

    for (uint8_t i = 0; i < len; i++) {
        out_buf[idx++] = payload[i];
        checksum ^= payload[i];
    }

    out_buf[idx++] = checksum;
    return idx;  /* Total frame size */
}
