#include "ps2_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_default_mode_is_digital(void) {
    ps2_protocol_state_t s;
    ps2_protocol_state_init(&s);
    assert(s.mode == PS2_MODE_DIGITAL);
    assert(s.config_mode_active == false);
    assert(ps2_identification_byte(&s) == 0x41);
    printf("test_default_mode_is_digital: PASS\n");
}

static void test_enter_config_mode_sets_escape_identification(void) {
    ps2_protocol_state_t s;
    ps2_protocol_state_init(&s);
    ps2_apply_command_effect(&s, PS2_CMD_CONFIG_MODE, 0x01);
    assert(s.config_mode_active == true);
    assert(ps2_identification_byte(&s) == 0xF3);
    printf("test_enter_config_mode_sets_escape_identification: PASS\n");
}

static void test_exit_config_mode_resumes_normal_identification(void) {
    ps2_protocol_state_t s;
    ps2_protocol_state_init(&s);
    ps2_apply_command_effect(&s, PS2_CMD_CONFIG_MODE, 0x01);
    assert(s.config_mode_active == true);
    ps2_apply_command_effect(&s, PS2_CMD_CONFIG_MODE, 0x00);
    assert(s.config_mode_active == false);
    assert(ps2_identification_byte(&s) == 0x41);
    printf("test_exit_config_mode_resumes_normal_identification: PASS\n");
}

static void test_poll_while_in_config_mode_uses_escape_response(void) {
    ps2_protocol_state_t s;
    ps2_protocol_state_init(&s);
    ps2_apply_command_effect(&s, PS2_CMD_CONFIG_MODE, 0x01);
    assert(ps2_should_use_dummy_response(&s, PS2_CMD_POLL) == true);
    assert(ps2_response_length(&s, PS2_CMD_POLL) == 9);

    uint8_t out[9];
    ps2_build_config_dummy_response(ps2_identification_byte(&s), out);
    uint8_t expected[9] = {0xFF, 0xF3, 0x5A, 0, 0, 0, 0, 0, 0};
    assert(memcmp(out, expected, 9) == 0);
    printf("test_poll_while_in_config_mode_uses_escape_response: PASS\n");
}

static void test_set_mode_to_analog(void) {
    ps2_protocol_state_t s;
    ps2_protocol_state_init(&s);
    ps2_apply_command_effect(&s, PS2_CMD_SET_MODE, 0x01);
    assert(s.mode == PS2_MODE_ANALOG);
    assert(ps2_should_use_dummy_response(&s, PS2_CMD_POLL) == false);
    assert(ps2_response_length(&s, PS2_CMD_POLL) == 9);
    assert(ps2_identification_byte(&s) == 0x73);
    printf("test_set_mode_to_analog: PASS\n");
}

static void test_set_mode_to_digital(void) {
    ps2_protocol_state_t s;
    ps2_protocol_state_init(&s);
    ps2_apply_command_effect(&s, PS2_CMD_SET_MODE, 0x01);
    assert(s.mode == PS2_MODE_ANALOG);
    ps2_apply_command_effect(&s, PS2_CMD_SET_MODE, 0x00);
    assert(s.mode == PS2_MODE_DIGITAL);
    assert(ps2_response_length(&s, PS2_CMD_POLL) == 5);
    assert(ps2_identification_byte(&s) == 0x41);
    printf("test_set_mode_to_digital: PASS\n");
}

static void test_digital_poll_response_length(void) {
    ps2_protocol_state_t s;
    ps2_protocol_state_init(&s);
    assert(ps2_response_length(&s, PS2_CMD_POLL) == 5);
    printf("test_digital_poll_response_length: PASS\n");
}

static void test_analog_poll_response_length(void) {
    ps2_protocol_state_t s;
    ps2_protocol_state_init(&s);
    ps2_apply_command_effect(&s, PS2_CMD_SET_MODE, 0x01);
    assert(ps2_response_length(&s, PS2_CMD_POLL) == 9);
    printf("test_analog_poll_response_length: PASS\n");
}

static void test_query_subcommands_get_dummy_response(void) {
    ps2_protocol_state_t s;
    ps2_protocol_state_init(&s);
    uint8_t ids[] = {PS2_CMD_QUERY_45, PS2_CMD_QUERY_46, PS2_CMD_QUERY_47, PS2_CMD_QUERY_4D};
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        assert(ps2_should_use_dummy_response(&s, ids[i]) == true);
        assert(ps2_response_length(&s, ids[i]) == 9);

        uint8_t out[9];
        ps2_build_config_dummy_response(ps2_identification_byte(&s), out);
        uint8_t expected[9] = {0xFF, 0x41, 0x5A, 0, 0, 0, 0, 0, 0};
        assert(memcmp(out, expected, 9) == 0);

        // No state should change from merely querying these.
        ps2_apply_command_effect(&s, ids[i], 0x01);
        assert(s.mode == PS2_MODE_DIGITAL);
        assert(s.config_mode_active == false);
    }
    printf("test_query_subcommands_get_dummy_response: PASS\n");
}

static void test_set_format_is_ignored(void) {
    ps2_protocol_state_t s;
    ps2_protocol_state_init(&s);
    ps2_apply_command_effect(&s, PS2_CMD_SET_MODE, 0x01); // analog mode
    ps2_apply_command_effect(&s, PS2_CMD_SET_FORMAT, 0xFF); // attempt custom format
    // Still responds with the full analog frame, unaffected by 0x4F.
    assert(ps2_should_use_dummy_response(&s, PS2_CMD_POLL) == false);
    assert(ps2_response_length(&s, PS2_CMD_POLL) == 9);
    printf("test_set_format_is_ignored: PASS\n");
}

int main(void) {
    test_default_mode_is_digital();
    test_enter_config_mode_sets_escape_identification();
    test_exit_config_mode_resumes_normal_identification();
    test_poll_while_in_config_mode_uses_escape_response();
    test_set_mode_to_analog();
    test_set_mode_to_digital();
    test_digital_poll_response_length();
    test_analog_poll_response_length();
    test_query_subcommands_get_dummy_response();
    test_set_format_is_ignored();
    printf("All tests passed.\n");
    return 0;
}
