#include <stdint.h>

#define EMBER_RAM_BASE 0x20000000u
#define EMBER_RAM_TOP 0x20008000u
#define EMBER_CONTROL_BASE 0x20007000u
#define EMBER_MAGIC 0x52424d45u
#define EMBER_VERSION 1u
#define SCB_VTOR_ADDR 0xe000ed08u

#define STACK_DEPTH 8u
#define INPUT_BUFFER_SIZE 128u
#define OUTPUT_BUFFER_SIZE 384u

#define CHANNEL_IDLE 0u
#define CHANNEL_BUSY 1u
#define CHANNEL_READY 2u

#define STATUS_OK 0u
#define STATUS_INPUT_TRUNCATED 1u
#define STATUS_UNKNOWN 2u
#define STATUS_STACK_UNDERFLOW 3u
#define STATUS_STACK_OVERFLOW 4u
#define STATUS_ALIGNMENT 5u
#define STATUS_RANGE 6u
#define STATUS_OUTPUT_TRUNCATED 7u

typedef struct {
  volatile uint32_t magic;
  volatile uint32_t version;
  volatile uint32_t request_seq;
  volatile uint32_t response_seq;
  volatile uint32_t state;
  volatile uint32_t status;
  volatile uint32_t input_len;
  volatile uint32_t output_len;
  volatile char input[INPUT_BUFFER_SIZE];
  volatile char output[OUTPUT_BUFFER_SIZE];
} ember_channel_t;

static volatile ember_channel_t *const channel = (volatile ember_channel_t *)EMBER_CONTROL_BASE;
static uint32_t stack[STACK_DEPTH];
static uint32_t stack_depth;
static char line_buffer[INPUT_BUFFER_SIZE];
static uint32_t output_length;
static uint32_t current_status;
static uint32_t output_truncated;

extern uint32_t __vector_table;

static inline void disable_irq(void) {
  __asm__ volatile ("cpsid i" : : : "memory");
}

static inline void set_msp(uint32_t value) {
  __asm__ volatile ("msr msp, %0" : : "r" (value) : "memory");
}

static inline void set_vtor(uint32_t value) {
  *(volatile uint32_t *)SCB_VTOR_ADDR = value;
}

static inline void barrier(void) {
  __asm__ volatile ("dsb 0xf" : : : "memory");
  __asm__ volatile ("isb 0xf" : : : "memory");
}

static int address_in_sram(uint32_t address) {
  return address >= EMBER_RAM_BASE && address < EMBER_RAM_TOP;
}

static void note_status(uint32_t status) {
  if (current_status == STATUS_OK) {
    current_status = status;
  }
}

static void output_reset(void) {
  output_length = 0u;
  output_truncated = 0u;
  channel->output_len = 0u;
  channel->output[0] = '\0';
}

static void out_char(char ch) {
  if (output_length + 1u >= OUTPUT_BUFFER_SIZE) {
    output_truncated = 1u;
    return;
  }

  channel->output[output_length++] = ch;
  channel->output[output_length] = '\0';
}

static void out_string(const char *text) {
  while (*text != '\0') {
    out_char(*text++);
  }
}

static void out_hex_digit(uint32_t value) {
  value &= 0xfu;
  out_char((char)(value < 10u ? ('0' + value) : ('A' + value - 10u)));
}

static void out_hex8(uint32_t value) {
  out_hex_digit(value >> 4);
  out_hex_digit(value);
}

static void out_hex32(uint32_t value) {
  for (int shift = 28; shift >= 0; shift -= 4) {
    out_hex_digit(value >> shift);
  }
}

static void out_u32_line(uint32_t value) {
  out_hex32(value);
  out_char('\n');
}

static void out_addr_value(const char *label, uint32_t value) {
  out_string(label);
  out_char(' ');
  out_hex32(value);
  out_char('\n');
}

static int stack_push(uint32_t value) {
  if (stack_depth >= STACK_DEPTH) {
    note_status(STATUS_STACK_OVERFLOW);
    return 0;
  }

  stack[stack_depth++] = value;
  return 1;
}

static int stack_pop(uint32_t *value) {
  if (stack_depth == 0u) {
    note_status(STATUS_STACK_UNDERFLOW);
    return 0;
  }

  *value = stack[--stack_depth];
  return 1;
}

static int char_is_space(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static char upper_char(char ch) {
  if (ch >= 'a' && ch <= 'z') {
    return (char)(ch - ('a' - 'A'));
  }
  return ch;
}

static int token_equals(const char *token, const char *word) {
  while (*token != '\0' && *word != '\0') {
    if (upper_char(*token) != *word) {
      return 0;
    }
    ++token;
    ++word;
  }

  return *token == '\0' && *word == '\0';
}

static int hex_digit_value(char ch, uint32_t *value) {
  if (ch >= '0' && ch <= '9') {
    *value = (uint32_t)(ch - '0');
    return 1;
  }
  if (ch >= 'a' && ch <= 'f') {
    *value = (uint32_t)(ch - 'a' + 10);
    return 1;
  }
  if (ch >= 'A' && ch <= 'F') {
    *value = (uint32_t)(ch - 'A' + 10);
    return 1;
  }
  return 0;
}

static int parse_hex_u32(const char *token, uint32_t *value) {
  uint32_t acc = 0u;
  uint32_t digit = 0u;
  int saw_digit = 0;

  if (token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
    token += 2;
  }

  while (*token != '\0') {
    if (!hex_digit_value(*token, &digit)) {
      return 0;
    }
    saw_digit = 1;
    acc = (acc << 4) | digit;
    ++token;
  }

  if (!saw_digit) {
    return 0;
  }

  *value = acc;
  return 1;
}

static int pop_aligned_u32(uint32_t *value) {
  if (!stack_pop(value)) {
    return 0;
  }
  if ((*value & 3u) != 0u) {
    note_status(STATUS_ALIGNMENT);
    return 0;
  }
  return 1;
}

static void print_words(void) {
  out_string("? I . @ ! C@ C! D J B\n");
}

static void print_info(void) {
  out_addr_value("SEED", (uint32_t)(uintptr_t)&__vector_table);
  out_addr_value("CTRL", EMBER_CONTROL_BASE);
}

static void dump_bytes(uint32_t base, uint32_t length) {
  for (uint32_t offset = 0; offset < length; offset += 16u) {
    uint32_t row_len = length - offset;
    if (row_len > 16u) {
      row_len = 16u;
    }

    out_hex32(base + offset);
    out_string(": ");
    for (uint32_t i = 0; i < row_len; ++i) {
      out_hex8(*(volatile uint8_t *)(base + offset + i));
      if (i + 1u != row_len) {
        out_char(' ');
      }
    }
    out_char('\n');
  }
}

__attribute__((noreturn)) static void jump_thumb(uint32_t entry) {
  disable_irq();
  barrier();
  ((void (*)(void))entry)();
  for (;;) {
  }
}

static int can_boot_image(uint32_t vector_base) {
  const uint32_t *vector = 0;
  uint32_t next_msp = 0u;
  uint32_t next_pc = 0u;

  if ((vector_base & 3u) != 0u) {
    return 0;
  }
  if (!address_in_sram(vector_base) || !address_in_sram(vector_base + 7u)) {
    return 0;
  }
  vector = (const uint32_t *)vector_base;
  next_msp = vector[0];
  next_pc = vector[1];
  if (!address_in_sram(next_msp - 4u)) {
    return 0;
  }
  if ((next_pc & 1u) == 0u || !address_in_sram(next_pc & ~1u)) {
    return 0;
  }
  return 1;
}

__attribute__((noreturn)) static void boot_image(uint32_t vector_base) {
  const uint32_t *vector = (const uint32_t *)vector_base;
  uint32_t next_msp = vector[0];
  uint32_t next_pc = vector[1];

  disable_irq();
  set_vtor(vector_base);
  barrier();
  set_msp(next_msp);
  ((void (*)(void))next_pc)();

  for (;;) {
  }
}

static void execute_token(char *token) {
  uint32_t value = 0u;

  if (token[0] == '\0') {
    return;
  }

  if (parse_hex_u32(token, &value)) {
    (void)stack_push(value);
    return;
  }

  if (token_equals(token, "?")) {
    print_words();
    return;
  }
  if (token_equals(token, "I")) {
    print_info();
    return;
  }
  if (token_equals(token, ".")) {
    if (stack_pop(&value)) {
      out_u32_line(value);
    }
    return;
  }
  if (token_equals(token, "@")) {
    if (pop_aligned_u32(&value)) {
      (void)stack_push(*(volatile uint32_t *)value);
    }
    return;
  }
  if (token_equals(token, "!")) {
    uint32_t address = 0u;
    uint32_t data = 0u;

    if (pop_aligned_u32(&address) && stack_pop(&data)) {
      *(volatile uint32_t *)address = data;
    }
    return;
  }
  if (token_equals(token, "C@")) {
    if (stack_pop(&value)) {
      (void)stack_push(*(volatile uint8_t *)value);
    }
    return;
  }
  if (token_equals(token, "C!")) {
    uint32_t address = 0u;
    uint32_t data = 0u;

    if (stack_pop(&address) && stack_pop(&data)) {
      *(volatile uint8_t *)address = (uint8_t)data;
    }
    return;
  }
  if (token_equals(token, "D")) {
    uint32_t length = 0u;
    uint32_t address = 0u;

    if (stack_pop(&length) && stack_pop(&address)) {
      dump_bytes(address, length);
    }
    return;
  }
  if (token_equals(token, "J")) {
    if (stack_pop(&value)) {
      if ((value & 1u) == 0u || !address_in_sram(value & ~1u)) {
        note_status(STATUS_RANGE);
        return;
      }
      jump_thumb(value);
    }
    return;
  }
  if (token_equals(token, "B")) {
    if (stack_pop(&value)) {
      if (!can_boot_image(value)) {
        note_status(STATUS_RANGE);
        return;
      }
      boot_image(value);
    }
    return;
  }

  note_status(STATUS_UNKNOWN);
  out_string("? ");
  out_string(token);
  out_char('\n');
}

static void execute_line(char *line) {
  char *token = line;

  while (*token != '\0') {
    while (char_is_space(*token)) {
      ++token;
    }
    if (*token == '\0') {
      break;
    }

    char *end = token;
    while (*end != '\0' && !char_is_space(*end)) {
      ++end;
    }

    char saved = *end;
    *end = '\0';
    execute_token(token);
    *end = saved;

    if (current_status != STATUS_OK) {
      return;
    }
    token = end;
  }
}

static void initialize_channel(void) {
  channel->magic = EMBER_MAGIC;
  channel->version = EMBER_VERSION;
  channel->request_seq = 0u;
  channel->response_seq = 0u;
  channel->state = CHANNEL_IDLE;
  channel->status = STATUS_OK;
  channel->input_len = 0u;
  output_reset();
  out_string("EMBER\n");
  print_words();
  channel->output_len = output_length;
  channel->state = CHANNEL_READY;
}

static void service_request(void) {
  uint32_t request = channel->request_seq;
  uint32_t input_len = channel->input_len;

  channel->state = CHANNEL_BUSY;
  current_status = STATUS_OK;
  output_reset();

  if (input_len >= INPUT_BUFFER_SIZE) {
    input_len = INPUT_BUFFER_SIZE - 1u;
    note_status(STATUS_INPUT_TRUNCATED);
  }

  for (uint32_t i = 0; i < input_len; ++i) {
    line_buffer[i] = channel->input[i];
  }
  line_buffer[input_len] = '\0';

  execute_line(line_buffer);
  if (output_length == 0u) {
    out_string("ok\n");
  }
  if (output_truncated) {
    note_status(STATUS_OUTPUT_TRUNCATED);
  }

  channel->status = current_status;
  channel->output_len = output_length;
  channel->response_seq = request;
  channel->state = CHANNEL_READY;
}

__attribute__((noreturn)) void seed_main(void) {
  set_vtor((uint32_t)&__vector_table);
  barrier();
  initialize_channel();

  for (;;) {
    if (channel->request_seq != channel->response_seq) {
      service_request();
    }
  }
}
