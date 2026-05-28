#ifndef PROTOCOL_H
#define PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ============================================================
 * Buzz-In UART Protocol
 * ------------------------------------------------------------
 * Frame: [0xAA][src_id][msg_type][len][payload 0..4][checksum]
 * Checksum: XOR of src_id, msg_type, len, and all payload bytes
 * ============================================================ */

#define PROTO_START_BYTE   0xAA
#define PROTO_MAX_PAYLOAD  4
#define PROTO_MAX_FRAME    (5 + PROTO_MAX_PAYLOAD)

/* Device IDs */
#define ID_MASTER  0x00
#define ID_POD1    0x01
#define ID_POD2    0x02
#define ID_PC      0xFF

/* Message types */
#define MSG_BUZZ            0x01  /* pod -> master       */
#define MSG_BUZZ_ACK        0x02  /* master -> pod       */
#define MSG_BUZZ_REJECT     0x03  /* master -> pod       */
#define MSG_SET_SCORE       0x04  /* master -> pod  1B   */
#define MSG_ARM             0x05  /* master -> pod       */
#define MSG_DISARM          0x06  /* master -> pod       */
#define MSG_ROULETTE_START  0x07  /* master -> PC        */
#define MSG_ROULETTE_STOP   0x08  /* master -> PC        */
#define MSG_PLAY_BUZZ       0x10  /* master -> PC   1B   */
#define MSG_UPDATE_STATE    0x11  /* master -> PC   4B   */

/* Game states (used in UPDATE_STATE payload byte 0) */
#define STATE_IDLE          0x00
#define STATE_ARMED         0x01
#define STATE_LOCKED        0x02
#define STATE_ROULETTE_SPIN 0x03

/* Parser states */
typedef enum {
    PARSE_WAIT_START = 0,
    PARSE_SRC,
    PARSE_TYPE,
    PARSE_LEN,
    PARSE_PAYLOAD,
    PARSE_CHECKSUM
} parse_state_t;

/* A received frame */
typedef struct {
    uint8_t src;
    uint8_t type;
    uint8_t len;
    uint8_t payload[PROTO_MAX_PAYLOAD];
} frame_t;

/* Parser context - one per UART channel */
typedef struct {
    parse_state_t state;
    frame_t       frame;
    uint8_t       payload_idx;
    uint8_t       checksum_accum;
} parser_t;

/* Initialise parser to idle state */
void parser_init(parser_t *p);

/* Feed one byte. Returns 1 if a valid frame is ready in p->frame */
int parser_feed(parser_t *p, uint8_t byte);

/* Build a frame into out_buf. Returns total byte count (5..9) */
uint8_t frame_build(uint8_t *out_buf,
                    uint8_t  src,
                    uint8_t  type,
                    const uint8_t *payload,
                    uint8_t  len);

#ifdef __cplusplus
}
#endif

#define STATE_IDLE           0x00
#define STATE_ARMED          0x01
#define STATE_LOCKED         0x02
#define STATE_ROULETTE_SPIN  0x03

#endif /* PROTOCOL_H */
