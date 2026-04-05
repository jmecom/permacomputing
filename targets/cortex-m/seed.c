#include <stdint.h>

#define EMBER_RAM_BASE 0x20000000u
#define EMBER_RAM_TOP 0x20008000u
#define EMBER_RECOVERY_PAYLOAD_BASE 0x20001000u
#define EMBER_RECOVERY_PAYLOAD_LIMIT 0x00004000u
#define EMBER_RECOVERY_HANDOFF_ADDR 0x20000800u
#define SCB_VTOR_ADDR 0xe000ed08u

#define EMBER_HANDOFF_MAGIC 0x31424d45u

typedef struct {
  uint32_t magic;
  uint32_t payload_base;
  uint32_t payload_size;
  uint32_t payload_crc32;
} ember_recovery_handoff_t;

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

__attribute__((noreturn)) static void fail(void) {
  for (;;) {
    __asm__ volatile ("bkpt #0");
  }
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

static int address_in_sram(uint32_t address) {
  return address >= EMBER_RAM_BASE && address < EMBER_RAM_TOP;
}

__attribute__((noreturn)) void seed_main(void) {
  const ember_recovery_handoff_t *handoff =
      (const ember_recovery_handoff_t *)EMBER_RECOVERY_HANDOFF_ADDR;
  const uint32_t *vector = (const uint32_t *)handoff->payload_base;
  uint32_t next_msp = 0u;
  uint32_t next_pc = 0u;

  set_vtor((uint32_t)&__vector_table);
  barrier();

  if (handoff->magic != EMBER_HANDOFF_MAGIC) {
    fail();
  }

  if (handoff->payload_base != EMBER_RECOVERY_PAYLOAD_BASE) {
    fail();
  }

  if (handoff->payload_size < 8u || handoff->payload_size > EMBER_RECOVERY_PAYLOAD_LIMIT) {
    fail();
  }

  if (crc32((const uint8_t *)handoff->payload_base, handoff->payload_size) !=
      handoff->payload_crc32) {
    fail();
  }

  next_msp = vector[0];
  next_pc = vector[1];

  if (!address_in_sram(next_msp - 4u)) {
    fail();
  }

  if ((next_pc & 1u) == 0u || !address_in_sram(next_pc & ~1u)) {
    fail();
  }

  disable_irq();
  set_vtor(handoff->payload_base);
  barrier();
  set_msp(next_msp);
  ((void (*)(void))next_pc)();
  fail();
}
