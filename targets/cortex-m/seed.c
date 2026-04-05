#include <stdint.h>

#define EMBER_RAM_BASE 0x20000000u
#define EMBER_RAM_TOP 0x20008000u
#define SCB_VTOR_ADDR 0xe000ed08u
#define IO_PATCH_TABLE_MAGIC 0x51545442u
#define IO_PATCH_TABLE_VERSION 1u
#define IO_PATCH_TABLE_OFFSET 0x40u

#define IO_FLAG_UART_ENABLED (1u << 0)
#define IO_FLAG_UART_RX (1u << 1)
#define IO_FLAG_UART_TX (1u << 2)
#define IO_FLAG_DEBUGGER_ONLY (1u << 3)
#define IO_FLAG_SHARED_STATUS (1u << 4)

#define STACK_DEPTH 8u
#define LINE_BUFFER_SIZE 80u

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t flags;
  uint32_t uart_base;
  uint32_t uart_tx_off;
  uint32_t uart_rx_off;
  uint32_t uart_stat_off;
  uint32_t uart_tx_ready_mask;
  uint32_t uart_rx_ready_mask;
  uint32_t uart_tx_ready_polarity;
  uint32_t uart_rx_ready_polarity;
} io_patch_table_t;

enum {
  SEED_STATUS_BOOT = 0x454d4230u,
  SEED_STATUS_PATCH_INVALID = 0x454d4231u,
  SEED_STATUS_DEBUGGER_ONLY = 0x454d4232u,
  SEED_STATUS_UART_DISABLED = 0x454d4233u,
  SEED_STATUS_MONITOR = 0x454d4234u,
};

static uint32_t stack[STACK_DEPTH];
static uint32_t stack_depth;
static char line_buffer[LINE_BUFFER_SIZE];
extern uint32_t __vector_table;
static volatile uint32_t seed_status;

__attribute__((section(".patch_table"), used)) const io_patch_table_t default_io_patch_table = {
    .magic = IO_PATCH_TABLE_MAGIC,
    .version = IO_PATCH_TABLE_VERSION,
    .flags = IO_FLAG_UART_ENABLED | IO_FLAG_UART_RX | IO_FLAG_UART_TX,
    .uart_base = 0x4000c000u,
    .uart_tx_off = 0x00000000u,
    .uart_rx_off = 0x00000000u,
    .uart_stat_off = 0x00000018u,
    .uart_tx_ready_mask = 0x00000020u,
    .uart_rx_ready_mask = 0x00000010u,
    .uart_tx_ready_polarity = 0u,
    .uart_rx_ready_polarity = 0u,
};

static const io_patch_table_t *active_patch_table(void) {
  return (const io_patch_table_t *)((uintptr_t)&__vector_table + IO_PATCH_TABLE_OFFSET);
}

static inline volatile uint32_t *patch_reg(const io_patch_table_t *table, uint32_t offset) {
  return (volatile uint32_t *)(table->uart_base + offset);
}

static inline void set_vtor(uint32_t value) {
  *(volatile uint32_t *)SCB_VTOR_ADDR = value;
}

static inline void disable_irq(void) {
  __asm__ volatile ("cpsid i" : : : "memory");
}

static inline void set_msp(uint32_t value) {
  __asm__ volatile ("msr msp, %0" : : "r" (value) : "memory");
}

static inline void barrier(void) {
  __asm__ volatile ("dsb 0xf" : : : "memory");
  __asm__ volatile ("isb 0xf" : : : "memory");
}

static uint32_t patch_status_bits(const io_patch_table_t *table) {
  uint32_t status_off = table->uart_stat_off;

  if ((table->flags & IO_FLAG_SHARED_STATUS) != 0u) {
    status_off = table->uart_tx_off;
  }

  return *patch_reg(table, status_off);
}

static int patch_ready(uint32_t status_bits, uint32_t mask, uint32_t polarity) {
  uint32_t ready = (status_bits & mask) != 0u ? 1u : 0u;
  return ready == (polarity != 0u ? 1u : 0u);
}

static int patch_table_valid(const io_patch_table_t *table) {
  if (table->magic != IO_PATCH_TABLE_MAGIC || table->version != IO_PATCH_TABLE_VERSION) {
    return 0;
  }

  if ((table->flags & IO_FLAG_UART_ENABLED) == 0u) {
    return 1;
  }

  if (table->uart_base == 0u) {
    return 0;
  }

  if ((table->flags & IO_FLAG_UART_TX) != 0u && table->uart_tx_ready_mask == 0u) {
    return 0;
  }

  if ((table->flags & IO_FLAG_UART_RX) != 0u && table->uart_rx_ready_mask == 0u) {
    return 0;
  }

  return 1;
}

static int patch_uart_enabled(const io_patch_table_t *table) {
  return (table->flags & IO_FLAG_UART_ENABLED) != 0u &&
         (table->flags & IO_FLAG_DEBUGGER_ONLY) == 0u;
}

static void idle_until_patchable(void) {
  for (;;) {
    const io_patch_table_t *table = active_patch_table();

    if (patch_table_valid(table)) {
      if ((table->flags & IO_FLAG_DEBUGGER_ONLY) != 0u) {
        seed_status = SEED_STATUS_DEBUGGER_ONLY;
      } else if (patch_uart_enabled(table)) {
        return;
      } else {
        seed_status = SEED_STATUS_UART_DISABLED;
      }
    } else {
      seed_status = SEED_STATUS_PATCH_INVALID;
    }

    __asm__ volatile ("nop");
  }
}

static void emit(const io_patch_table_t *table, char ch) {
  if ((table->flags & IO_FLAG_UART_TX) == 0u) {
    return;
  }

  while (!patch_ready(patch_status_bits(table), table->uart_tx_ready_mask,
                      table->uart_tx_ready_polarity)) {
  }

  *patch_reg(table, table->uart_tx_off) = (uint32_t)(uint8_t)ch;
}

static void uart_puts(const io_patch_table_t *table, const char *text) {
  while (*text != '\0') {
    if (*text == '\n') {
      emit(table, '\r');
    }
    emit(table, *text++);
  }
}

static int has_key(const io_patch_table_t *table) {
  if ((table->flags & IO_FLAG_UART_RX) == 0u) {
    return 0;
  }

  return patch_ready(patch_status_bits(table), table->uart_rx_ready_mask,
                     table->uart_rx_ready_polarity);
}

static char key(const io_patch_table_t *table) {
  while (!has_key(table)) {
  }

  return (char)(*patch_reg(table, table->uart_rx_off) & 0xffu);
}

static void put_hex_digit(const io_patch_table_t *table, uint32_t value) {
  value &= 0xfu;
  emit(table, (char)(value < 10u ? ('0' + value) : ('A' + value - 10u)));
}

static void put_hex8(const io_patch_table_t *table, uint8_t value) {
  put_hex_digit(table, (uint32_t)(value >> 4));
  put_hex_digit(table, (uint32_t)value);
}

static void put_hex32(const io_patch_table_t *table, uint32_t value) {
  for (int shift = 28; shift >= 0; shift -= 4) {
    put_hex_digit(table, value >> shift);
  }
}

static void put_u32_line(const io_patch_table_t *table, uint32_t value) {
  put_hex32(table, value);
  uart_puts(table, "\n");
}

static void print_prompt(const io_patch_table_t *table) {
  uart_puts(table, "ok> ");
}

static uint32_t crc32(const uint8_t *data, uint32_t size) {
  uint32_t crc = 0xffffffffu;

  for (uint32_t i = 0; i < size; ++i) {
    crc ^= (uint32_t)data[i];

    for (uint32_t bit = 0; bit < 8; ++bit) {
      uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
      crc = (crc >> 1) ^ (0xedb88320u & mask);
    }
  }

  return ~crc;
}

static int stack_push(uint32_t value) {
  if (stack_depth >= STACK_DEPTH) {
    uart_puts(active_patch_table(), "ERR stack-overflow\n");
    return 0;
  }

  stack[stack_depth++] = value;
  return 1;
}

static int stack_pop(uint32_t *value) {
  if (stack_depth == 0u) {
    uart_puts(active_patch_table(), "ERR stack-underflow\n");
    return 0;
  }

  *value = stack[--stack_depth];
  return 1;
}

static int address_in_sram(uint32_t address) {
  return address >= EMBER_RAM_BASE && address < EMBER_RAM_TOP;
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

static void print_words(const io_patch_table_t *table) {
  uart_puts(table, "WORDS PING INFO PATCH . .S DROP DUP SWAP @ ! C@ C! DUMP LOAD CRC32 BOOT JUMP\n");
}

static void print_patch_table(const io_patch_table_t *table) {
  uart_puts(table, "PATCH+00 MAGIC ");
  put_u32_line(table, table->magic);
  uart_puts(table, "PATCH+04 VERSION ");
  put_u32_line(table, table->version);
  uart_puts(table, "PATCH+08 FLAGS ");
  put_u32_line(table, table->flags);
  uart_puts(table, "PATCH+0C UART_BASE ");
  put_u32_line(table, table->uart_base);
  uart_puts(table, "PATCH+10 UART_TX_OFF ");
  put_u32_line(table, table->uart_tx_off);
  uart_puts(table, "PATCH+14 UART_RX_OFF ");
  put_u32_line(table, table->uart_rx_off);
  uart_puts(table, "PATCH+18 UART_STAT_OFF ");
  put_u32_line(table, table->uart_stat_off);
  uart_puts(table, "PATCH+1C UART_TX_READY_MASK ");
  put_u32_line(table, table->uart_tx_ready_mask);
  uart_puts(table, "PATCH+20 UART_RX_READY_MASK ");
  put_u32_line(table, table->uart_rx_ready_mask);
  uart_puts(table, "PATCH+24 UART_TX_READY_POLARITY ");
  put_u32_line(table, table->uart_tx_ready_polarity);
  uart_puts(table, "PATCH+28 UART_RX_READY_POLARITY ");
  put_u32_line(table, table->uart_rx_ready_polarity);
}

static void print_info(const io_patch_table_t *table) {
  uart_puts(table, "ARCH ARMV7-M\n");
  uart_puts(table, "PROFILE GENERIC-CORTEX-M-UART-PATCH-TABLE\n");
  uart_puts(table, "LOAD ");
  put_u32_line(table, (uint32_t)(uintptr_t)&__vector_table);
  uart_puts(table, "RAM 20000000 20008000\n");
  uart_puts(table, "PATCH_TABLE_OFFSET 00000040\n");
  print_patch_table(table);
}

static void print_stack(const io_patch_table_t *table) {
  uart_puts(table, "<");
  put_hex32(table, stack_depth);
  uart_puts(table, ">");

  for (uint32_t i = 0; i < stack_depth; ++i) {
    emit(table, ' ');
    put_hex32(table, stack[i]);
  }

  uart_puts(table, "\n");
}

static void dump_memory(const io_patch_table_t *table, uint32_t base, uint32_t length) {
  const uint8_t *ptr = (const uint8_t *)base;

  for (uint32_t offset = 0; offset < length; offset += 16u) {
    put_hex32(table, base + offset);
    uart_puts(table, ": ");

    uint32_t row_length = length - offset;
    if (row_length > 16u) {
      row_length = 16u;
    }

    for (uint32_t i = 0; i < row_length; ++i) {
      put_hex8(table, ptr[offset + i]);
      if (i + 1u != row_length) {
        emit(table, ' ');
      }
    }
    uart_puts(table, "\n");
  }
}

static int read_hex_stream_byte(const io_patch_table_t *table, uint8_t *value) {
  uint32_t hi = 0u;
  uint32_t lo = 0u;
  char ch = '\0';

  do {
    ch = key(table);
    if (ch == '\r') {
      emit(table, '\n');
    } else {
      emit(table, ch);
    }
  } while (char_is_space(ch));

  if (!hex_digit_value(ch, &hi)) {
    return 0;
  }

  do {
    ch = key(table);
    if (ch == '\r') {
      emit(table, '\n');
    } else {
      emit(table, ch);
    }
  } while (char_is_space(ch));

  if (!hex_digit_value(ch, &lo)) {
    return 0;
  }

  *value = (uint8_t)((hi << 4) | lo);
  return 1;
}

static void load_hex_stream(const io_patch_table_t *table, uint32_t base, uint32_t length) {
  uint8_t *ptr = (uint8_t *)base;

  uart_puts(table, "DATA ");
  put_hex32(table, length);
  uart_puts(table, " bytes as hex\n");

  for (uint32_t i = 0; i < length; ++i) {
    uint8_t byte = 0u;

    if (!read_hex_stream_byte(table, &byte)) {
      uart_puts(table, "\nERR load\n");
      return;
    }

    ptr[i] = byte;
  }

  uart_puts(table, "\nOK load\n");
}

__attribute__((noreturn)) static void boot_image(const io_patch_table_t *table, uint32_t vector_base) {
  const uint32_t *vector = (const uint32_t *)vector_base;
  uint32_t next_msp = vector[0];
  uint32_t next_pc = vector[1];

  if (!address_in_sram(vector_base) || !address_in_sram(vector_base + 7u)) {
    uart_puts(table, "ERR boot-base\n");
    for (;;) {
    }
  }

  if (!address_in_sram(next_msp - 4u)) {
    uart_puts(table, "ERR boot-msp\n");
    for (;;) {
    }
  }

  if ((next_pc & 1u) == 0u || !address_in_sram(next_pc & ~1u)) {
    uart_puts(table, "ERR boot-pc\n");
    for (;;) {
    }
  }

  uart_puts(table, "BOOT ");
  put_hex32(table, vector_base);
  uart_puts(table, "\n");

  disable_irq();
  set_vtor(vector_base);
  barrier();
  set_msp(next_msp);
  ((void (*)(void))next_pc)();

  for (;;) {
  }
}

static void execute_token(const io_patch_table_t *table, char *token) {
  uint32_t value = 0u;
  uint32_t value_b = 0u;

  if (token[0] == '\0') {
    return;
  }

  if (parse_hex_u32(token, &value)) {
    (void)stack_push(value);
    return;
  }

  if (token_equals(token, "PING")) {
    uart_puts(table, "PONG\n");
    return;
  }

  if (token_equals(token, "WORDS") || token_equals(token, "HELP")) {
    print_words(table);
    return;
  }

  if (token_equals(token, "INFO")) {
    print_info(table);
    return;
  }

  if (token_equals(token, "PATCH")) {
    print_patch_table(table);
    return;
  }

  if (token_equals(token, ".")) {
    if (stack_pop(&value)) {
      put_u32_line(table, value);
    }
    return;
  }

  if (token_equals(token, ".S")) {
    print_stack(table);
    return;
  }

  if (token_equals(token, "DROP")) {
    (void)stack_pop(&value);
    return;
  }

  if (token_equals(token, "DUP")) {
    if (stack_pop(&value)) {
      (void)stack_push(value);
      (void)stack_push(value);
    }
    return;
  }

  if (token_equals(token, "SWAP")) {
    if (stack_pop(&value) && stack_pop(&value_b)) {
      (void)stack_push(value);
      (void)stack_push(value_b);
    }
    return;
  }

  if (token_equals(token, "@")) {
    if (stack_pop(&value)) {
      (void)stack_push(*(volatile uint32_t *)value);
    }
    return;
  }

  if (token_equals(token, "!")) {
    uint32_t address = 0u;
    uint32_t data = 0u;

    if (stack_pop(&address) && stack_pop(&data)) {
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

  if (token_equals(token, "DUMP")) {
    uint32_t length = 0u;
    uint32_t address = 0u;

    if (stack_pop(&length) && stack_pop(&address)) {
      dump_memory(table, address, length);
    }
    return;
  }

  if (token_equals(token, "LOAD")) {
    uint32_t length = 0u;
    uint32_t address = 0u;

    if (stack_pop(&length) && stack_pop(&address)) {
      load_hex_stream(table, address, length);
    }
    return;
  }

  if (token_equals(token, "CRC32")) {
    uint32_t length = 0u;
    uint32_t address = 0u;

    if (stack_pop(&length) && stack_pop(&address)) {
      (void)stack_push(crc32((const uint8_t *)address, length));
    }
    return;
  }

  if (token_equals(token, "BOOT") || token_equals(token, "JUMP")) {
    if (stack_pop(&value)) {
      boot_image(table, value);
    }
    return;
  }

  uart_puts(table, "ERR token ");
  uart_puts(table, token);
  uart_puts(table, "\n");
}

static void execute_line(const io_patch_table_t *table, char *line) {
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
    execute_token(table, token);
    *end = saved;
    token = end;
  }
}

__attribute__((noreturn)) static void repl(const io_patch_table_t *table) {
  uint32_t line_length = 0u;

  seed_status = SEED_STATUS_MONITOR;
  uart_puts(table, "\nEMBER seed monitor\n");
  print_info(table);
  print_words(table);
  print_prompt(table);

  for (;;) {
    char ch = key(table);

    if (ch == '\r' || ch == '\n') {
      uart_puts(table, "\n");
      line_buffer[line_length] = '\0';
      execute_line(table, line_buffer);
      line_length = 0u;
      print_prompt(table);
      continue;
    }

    if (ch == '\b' || ch == 0x7fu) {
      if (line_length != 0u) {
        --line_length;
        uart_puts(table, "\b \b");
      }
      continue;
    }

    if (line_length + 1u >= LINE_BUFFER_SIZE) {
      uart_puts(table, "\nERR line-too-long\n");
      line_length = 0u;
      print_prompt(table);
      continue;
    }

    line_buffer[line_length++] = ch;
    emit(table, ch);
  }
}

__attribute__((noreturn)) void seed_main(void) {
  const io_patch_table_t *table = active_patch_table();

  seed_status = SEED_STATUS_BOOT;
  set_vtor((uint32_t)&__vector_table);
  barrier();
  if (!patch_table_valid(table) || !patch_uart_enabled(table)) {
    idle_until_patchable();
    table = active_patch_table();
  }
  repl(table);
}
