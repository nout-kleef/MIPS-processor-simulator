/*************************************************************************************|
|   1. YOU ARE NOT ALLOWED TO SHARE/PUBLISH YOUR CODE (e.g., post on piazza or
online)| |   2. Fill main.c and memory_hierarchy.c files | |   3. Do not use any
other .c files neither alter main.h or parser.h                 | |   4. Do not
include any other library files                                         |
|*************************************************************************************/
#include "mipssim.h"

#define ADDR_SIZE 32     // size of any address in bits
#define BYTES_IN_ADDR 4  // --> 4
#define BLOCK_SIZE 16    // size of a cache block in bytes

typedef struct {
  uint32_t tag;
  uint32_t index;
  uint32_t byte_offset;
} addr_decomposition;

typedef struct {
  uint8_t valid;                               // (size: 1b)
  int tag;                                     // (size: [tag_bits]b)
  uint32_t words[BLOCK_SIZE / BYTES_IN_ADDR];  // (size: [BLOCK_SIZE]B)
} cache_entry;

uint8_t byte_offset_bits = 0;  // initialised in memory_state_init()
uint8_t index_bits = 0;        // initialised in memory_state_init()
uint8_t tag_bits = 0;          // initialised in memory_state_init()
cache_entry* cache;            // initialised in allocate_cache()

void allocate_cache(const int NUM_BLOCKS) {
  cache = malloc(sizeof(cache_entry) * NUM_BLOCKS);
  // invalidate all entries
  for (int i = 0; i < NUM_BLOCKS; ++i) {
    cache[i].valid = 0;
  }
}

addr_decomposition* decompose(int address, addr_decomposition* decomp) {
  const uint8_t INDEX_START = byte_offset_bits;
  const uint8_t TAG_START = INDEX_START + index_bits;
  assert(ADDR_SIZE - TAG_START == tag_bits);  // just to make sure; TODO remove
  decomp->tag = (uint32_t)get_piece_of_a_word(address, TAG_START, tag_bits);
  decomp->index =
      (uint32_t)get_piece_of_a_word(address, INDEX_START, index_bits);
  decomp->byte_offset =
      (uint32_t)get_piece_of_a_word(address, 0, byte_offset_bits);
  return decomp;
}

void memory_state_init(struct architectural_state* arch_state_ptr) {
  arch_state_ptr->memory =
      (uint32_t*)malloc(sizeof(uint32_t) * MEMORY_WORD_NUM);
  memset(arch_state_ptr->memory, 0, sizeof(uint32_t) * MEMORY_WORD_NUM);
  if (cache_size == 0) {
    // CACHE DISABLED
    memory_stats_init(arch_state_ptr, 0);  // WARNING: no cache --> 0
  } else {
    // CACHE ENABLED
    assert(cache_size % BLOCK_SIZE == 0 &&
           "cache_size is not a multiple of BLOCK_SIZE");  // TODO remove
    // calculate distribution of the word into: tag, index, byte offset
    const int NUM_BLOCKS = cache_size / BLOCK_SIZE;
    int num_blocks = NUM_BLOCKS;
    int block_size = BLOCK_SIZE;
    while (block_size >>= 1) {
      ++byte_offset_bits;
    }
    while (num_blocks >>= 1) {
      ++index_bits;
    }
    tag_bits = ADDR_SIZE - index_bits - byte_offset_bits;
    printf("byte_offset_bits: %u, index_bits: %u, tag_bits: %u\n",
           byte_offset_bits, index_bits, tag_bits);
    assert(tag_bits + index_bits + byte_offset_bits == ADDR_SIZE);
    memory_stats_init(arch_state_ptr, tag_bits);  // <-- fill number of tag bits
    allocate_cache(NUM_BLOCKS);
  }
}

void fetch_block_into_cache(const int address, addr_decomposition* decomp) {
  // get block address (in bytes, in main memory) by "flushing" the byte offset
  const uint32_t MEMORY_INDEX =
      ((address >> byte_offset_bits) << byte_offset_bits) /
      BYTES_IN_ADDR;  // NB: memory holds words, not individual bytes!
  const uint32_t* BLOCK_MEM_START = &arch_state.memory[MEMORY_INDEX];
  // iteratively move the words into the corresponding cache entry
  for (int byte = 0; byte < BLOCK_SIZE; byte += BYTES_IN_ADDR /* 4 */) {
    // move block[byte (-) byte + 3] --> block[byte / BYTES_IN_ADDR]
    const int word = byte / BYTES_IN_ADDR;
    cache[decomp->index].words[word] = *(BLOCK_MEM_START + word);
  }
}

// returns data on memory[address / BYTES_IN_ADDR]
int memory_read(const int address) {
  arch_state.mem_stats.lw_total++;
  check_address_is_word_aligned(address);

  if (cache_size == 0) {
    // CACHE DISABLED
    return (int)arch_state.memory[address / BYTES_IN_ADDR];
  } else {
    // CACHE ENABLED
    // decompose the memory address into tag, index and byte offset
    addr_decomposition decomp;
    decomp = *(decompose(address, &decomp));  // pass decomp to avoid local var
    cache_entry* block = &cache[decomp.index];

    if (block->valid && block->tag == decomp.tag) {
      // HIT!
      arch_state.mem_stats.lw_cache_hits++;
    } else {
      // MISS!
      fetch_block_into_cache(address, &decomp);
      // flag that this block is now valid for future memory accesses
      block->valid = 1;
    }
    // regardless of hit/miss, this cache block now contains our data.
    return block->words[decomp.byte_offset / BYTES_IN_ADDR];
  }
}

// writes data on memory[address / BYTES_IN_ADDR]
void memory_write(int address, int write_data) {
  arch_state.mem_stats.sw_total++;
  check_address_is_word_aligned(address);

  if (cache_size == 0) {
    // CACHE DISABLED
    arch_state.memory[address / BYTES_IN_ADDR] = (uint32_t)write_data;
  } else {
    // CACHE ENABLED
    addr_decomposition decomp;
    decomp = *(decompose(address, &decomp));
    cache_entry* block = &cache[decomp.index];

    if (block->valid && block->tag == decomp.tag) {
      // HIT! write to cache
      arch_state.mem_stats.sw_cache_hits++;
      block->words[decomp.byte_offset / BYTES_IN_ADDR] = (uint32_t)write_data;
    }  // else: do nothing (write-no-allocate)
    // write to main memory; we ALWAYS do this (write-through)
    arch_state.memory[address / BYTES_IN_ADDR] = (uint32_t)write_data;
  }
}
