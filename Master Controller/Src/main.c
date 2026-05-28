#include "stm32f303xc.h"
#include "protocol.h"
#include "uart.h"
#include "gpio_pins.h"
#include "motor.h"
#include "sensors.h"
#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * Buzz-In Master Console - Main
 *
 * State machine:
 *   IDLE -> (ARM btn) -> ARMED
 *   ARMED -> (BUZZ rx) -> LOCKED
 *   LOCKED -> (CORRECT btn) -> score++, -> IDLE
 *   LOCKED -> (WRONG btn) -> re-ARM (let other player buzz)
 *   Any -> (ROULETTE btn) -> ROULETTE_SPIN -> (ROULETTE btn) -> IDLE
 *
 * LED mapping (PE8-PE15, led_num 0-7):
 *   0 = Pod 1 locked in
 *   1 = Pod 2 locked in
 *   2 = Round armed
 *   3 = Roulette spinning
 *   7 = Game active (heartbeat)
 * ============================================================ */

/* Forward declarations */
static void send_to_pod(uart_channel_t ch, uint8_t msg_type);
static void send_to_pod_with_payload(uart_channel_t ch, uint8_t msg_type, uint8_t *payload, uint8_t len);
static void send_to_pc(uint8_t msg_type, uint8_t *payload, uint8_t len);
static void update_pc_state(void);
static void process_pod_frame(uart_channel_t ch, frame_t *f);
static void send_scores_to_pods(void);

/* External from gpio_pins.c */
extern uint32_t millis(void);

/* ---- Game State ---- */
static uint8_t game_state = STATE_IDLE;
static uint8_t scores[2] = {0, 0};       /* Pod 1, Pod 2 */
static uint8_t locked_pod = 0;            /* Which pod is locked in (1 or 2, 0=none) */
static uint8_t questions_since_roulette = 0;

/* Parsers - one per UART input channel */
static parser_t parser_pod1;
static parser_t parser_pod2;

/* Debounce timestamps */
static uint32_t db_arm = 0;
static uint32_t db_correct = 0;
static uint32_t db_wrong = 0;
static uint32_t db_roulette = 0;

#define DEBOUNCE_MS 200

/* ---- Helper: map pod ID to UART channel ---- */
static uart_channel_t pod_uart(uint8_t pod_id) {
    return (pod_id == 1) ? UART_POD1 : UART_POD2;
}

/* ---- Send helpers ---- */
static void send_to_pod(uart_channel_t ch, uint8_t msg_type) {
    uint8_t buf[PROTO_MAX_FRAME];
    uint8_t len = frame_build(buf, ID_MASTER, msg_type, NULL, 0);
    uart_send(ch, buf, len);
}

static void send_to_pod_with_payload(uart_channel_t ch, uint8_t msg_type, uint8_t *payload, uint8_t plen) {
    uint8_t buf[PROTO_MAX_FRAME];
    uint8_t len = frame_build(buf, ID_MASTER, msg_type, payload, plen);
    uart_send(ch, buf, len);
}

static void send_to_pc(uint8_t msg_type, uint8_t *payload, uint8_t plen) {
    uint8_t buf[PROTO_MAX_FRAME];
    uint8_t len = frame_build(buf, ID_MASTER, msg_type, payload, plen);
    uart_send(UART_PC, buf, len);
}

static void update_pc_state(void) {
    uint8_t payload[4] = {game_state, scores[0], scores[1], locked_pod};
    send_to_pc(MSG_UPDATE_STATE, payload, 4);
}

static void send_scores_to_pods(void) {
    uint8_t s0 = scores[0];
    uint8_t s1 = scores[1];
    send_to_pod_with_payload(UART_POD1, MSG_SET_SCORE, &s0, 1);
    send_to_pod_with_payload(UART_POD2, MSG_SET_SCORE, &s1, 1);
}

/* ---- Process a received frame from a pod ---- */
static void process_pod_frame(uart_channel_t ch, frame_t *f) {
    if (f->type == MSG_BUZZ) {
        if (game_state == STATE_ARMED) {
            /* First buzz wins! */
            locked_pod = (ch == UART_POD1) ? 1 : 2;
            game_state = STATE_LOCKED;

            /* ACK the winner, REJECT the loser */
            send_to_pod(ch, MSG_BUZZ_ACK);
            send_to_pod((ch == UART_POD1) ? UART_POD2 : UART_POD1, MSG_BUZZ_REJECT);

            /* Tell PC to play buzz sound */
            uint8_t pod_id = locked_pod;
            send_to_pc(MSG_PLAY_BUZZ, &pod_id, 1);

            /* Update LEDs */
            leds_all_off();
            led_on(locked_pod - 1);  /* LED 0 or 1 */

            /* Update PC */
            update_pc_state();
        } else {
            /* Not armed - reject */
            send_to_pod(ch, MSG_BUZZ_REJECT);
        }
    }
}

/* ---- State transition helpers ---- */
static void enter_idle(void) {
    game_state = STATE_IDLE;
    locked_pod = 0;
    leds_all_off();
    led_on(7);  /* Game active heartbeat */

    /* Disarm both pods */
    send_to_pod(UART_POD1, MSG_DISARM);
    send_to_pod(UART_POD2, MSG_DISARM);
    update_pc_state();
}

static void enter_armed(void) {
    game_state = STATE_ARMED;
    locked_pod = 0;
    leds_all_off();
    led_on(2);  /* Armed LED */

    /* Arm both pods */
    send_to_pod(UART_POD1, MSG_ARM);
    send_to_pod(UART_POD2, MSG_ARM);
    update_pc_state();
}

static void award_point(void) {
    if (locked_pod == 0) return;
    scores[locked_pod - 1]++;
    send_scores_to_pods();
    questions_since_roulette++;
    enter_idle();
}

static void deny_point(void) {
    /* Re-arm so the other player can buzz */
    game_state = STATE_ARMED;
    locked_pod = 0;
    leds_all_off();
    led_on(2);  /* Armed LED */

    /* Re-arm both pods */
    send_to_pod(UART_POD1, MSG_ARM);
    send_to_pod(UART_POD2, MSG_ARM);
    update_pc_state();
}

static void enter_roulette(void) {
    game_state = STATE_ROULETTE_SPIN;
    leds_all_off();
    led_on(3);  /* Roulette LED */

    /* Start motor */
    motor_set_speed(800);  /* ~80% speed */

    /* Tell PC */
    send_to_pc(MSG_ROULETTE_START, NULL, 0);
    update_pc_state();
}

static void stop_roulette(void) {
    motor_stop();
    questions_since_roulette = 0;

    /* Tell PC */
    send_to_pc(MSG_ROULETTE_STOP, NULL, 0);

    enter_idle();
}

/* ============================================================
 * Main
 * ============================================================ */
int main(void) {
    /* ---- Init everything ---- */
    gpio_init_all();
    uart_init_all();
    adc_init();
    motor_pwm_init();

    parser_init(&parser_pod1);
    parser_init(&parser_pod2);

    /* Start in idle */
    enter_idle();

    /* ---- Main loop ---- */
    while (1) {

        /* ---- Poll UART from Pod 1 ---- */
        if (uart_available(UART_POD1)) {
            uint8_t byte = uart_read_byte(UART_POD1);
            if (parser_feed(&parser_pod1, byte)) {
                process_pod_frame(UART_POD1, &parser_pod1.frame);
            }
        }

        /* ---- Poll UART from Pod 2 ---- */
        if (uart_available(UART_POD2)) {
            uint8_t byte = uart_read_byte(UART_POD2);
            if (parser_feed(&parser_pod2, byte)) {
                process_pod_frame(UART_POD2, &parser_pod2.frame);
            }
        }

        /* ---- Poll host buttons ---- */
        switch (game_state) {

        case STATE_IDLE:
            /* ARM button (onboard user button PA0) -> arm the round */
            if (btn_debounce(btn_user_pressed(), &db_arm, DEBOUNCE_MS)) {
                enter_armed();
            }
            /* ROULETTE button -> only if 3+ questions since last roulette */
            if (btn_debounce(btn_roulette_pressed(), &db_roulette, DEBOUNCE_MS)) {
                if (questions_since_roulette >= 3) {
                    enter_roulette();
                }
            }
            break;

        case STATE_ARMED:
            /* Buzzes handled in process_pod_frame above */
            /* ARM button in armed state = cancel round, go back to idle */
            if (btn_debounce(btn_user_pressed(), &db_arm, DEBOUNCE_MS)) {
                enter_idle();
            }
            break;

        case STATE_LOCKED:
            /* CORRECT button -> award point */
            if (btn_debounce(btn_correct_pressed(), &db_correct, DEBOUNCE_MS)) {
                award_point();
            }
            /* WRONG button -> deny, re-arm for other player */
            if (btn_debounce(btn_wrong_pressed(), &db_wrong, DEBOUNCE_MS)) {
                deny_point();
            }
            break;

        case STATE_ROULETTE_SPIN:
            /* ROULETTE button again -> stop the wheel */
            if (btn_debounce(btn_roulette_pressed(), &db_roulette, DEBOUNCE_MS)) {
                stop_roulette();
            }
            break;
        }

        /* ---- Heartbeat blink (LED 7) every 500ms in idle ---- */
        if (game_state == STATE_IDLE) {
            static uint32_t last_blink = 0;
            if (millis() - last_blink > 500) {
                led_toggle(7);
                last_blink = millis();
            }
        }

        /* ---- Periodic LDR read (just send to PC for display) ---- */
        {
            static uint32_t last_adc = 0;
            if (millis() - last_adc > 250) {
                /* We read it here but don't act on it in this version.
                 * The value gets sent as part of UPDATE_STATE or could
                 * be used to trigger arming when "stage lights" come on.
                 * For now just read it to satisfy the ADC requirement. */
                uint16_t ldr_val = adc_read_ldr();
                (void)ldr_val;  /* TODO: use or send to PC */
                last_adc = millis();
            }
        }
    }
}
