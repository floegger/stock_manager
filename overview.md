# <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 576 512"><!--!Font Awesome Free v7.2.0 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license/free Copyright 2026 Fonticons, Inc.--><path d="M384 160c-17.7 0-32-14.3-32-32s14.3-32 32-32l160 0c17.7 0 32 14.3 32 32l0 160c0 17.7-14.3 32-32 32s-32-14.3-32-32l0-82.7-169.4 169.4c-12.5 12.5-32.8 12.5-45.3 0L192 269.3 54.6 406.6c-12.5 12.5-32.8 12.5-45.3 0s-12.5-32.8 0-45.3l160-160c12.5-12.5 32.8-12.5 45.3 0L320 306.7 466.7 160 384 160z"/>Stock Manager</svg> Stock Manager

## Related Notes

[[Elite Hash Table Design — Complete Pipeline]] | [[Exact Algorithm Used by Modern High-Performance Hash Tables]] | [[FH/SS2026/ALGOS/Hashtabellen_Zusatzaufgabe]] | [[FH/SS2026/ALGOS/PA1_Hashtabelle]] | [[Combine Robin Hood + SIMD]] | [[Dynamische Datenstrukturen]] | [[FH/SS2026/ALGOS/UE1_Uebungsblatt]] | [[FH/SS2026/ALGOS/UE2_Uebungsblatt]] | [[FH/SS2026/ALGOS/UE3_Uebungsblatt]] | [[FH/SS2026/ALGOS/PA2_TreeCheck]]

## Model:

- Swiss Table Style with SIMD probing and Robin Hood insert algorithm

### Swiss Table

#### Complexity:

    - O(|key|) → key < 30 chars: O(1)

#### Description:

- split an 8-byte-hash into one byte for metadata and 7 byte hash
    - metadata byte stores:
        - state ( EMPTY, DELETED, OCCUPIED )
            - EMPTY 0x80 ( 1000 0000)
            - DELETED 0xFE ( 1111 1110 )
        - fingerprint : hash & 0x7F
- advantage:
    - performance gain by comparing fingerprints first and only comparing keys ( CPU heavier ) once fp match has been found.

## Robin-Hood-like Tombstone Reuse

#### Complexity:

    O(1)

'reuse tombstones'

- insert uses first tombstone in preference to an EMPTY slot
- avoids tombstone accumulation - clustering

## Chosen Hash-Algorthm

#### FNV-1a ( Foller-Noll-Vo variant 1a)

    FNV-1a offset basis: 14695981039346656037ULL
    FNV-1a prime:        1099511628211ULL

- non cryptographic hash
- converts data ( string, integers, .. ) into fixed-size integer hash
    - XOR hash with next byte
    - multiply by special prime number
        - avalanche effect:
            - lower bits affect higher bits
            - higher bits feed back into lower bits
            - carries create nonlinear mixing - small changes flip many bits
            - → **one bit change in input affects many bits in state**

```cpp
uint64_t HashTable::hashString ( const std::string &key ) noexcept {
    uint64_t hash = 14695981039346656037ULL;  // FNV-1a offset basis
    for ( char c : key ) {
        hash ^= static_cast<uint8_t> ( c );
        hash *= 1099511628211ULL;  // FNV-1a prime
    }
    return hash;
}
```

#### How FNV primes are chosen:

- large Hamming weight ( number of bits set to 1 ) - high Hamming weight → better mixing
- structured to spread bits efficiently
- multiplication is fast on CPUs
- avoid clustering

#### Why FNV-1a:

- fast and lightweight
- produces a well-distributed integer hash
-

### SIMD:

"Single Instruction Multiple Data"

- one cache line = 32 bytes → load 32 values to cache and apply the same operation to each simultaneously
- does not improve or change the algorithm but improves performance by reducing the number of CPU cycles needed for comparing a number of keys ( SIMD - 32 comparisons in one CPU cycle, without: 32 CPU cycles! )
- benefit from the fact, that CPU always loads a full cache line → use the data already in cache instead of constantly loading from memory to cache ( slow )

### AVX2

AVX = **A**dvanced **V**ector **E**xtension

CPU instruction-set extension for doing the same operation on multiple pieces of data at once.

**parallel batch processing of numbers**

### Intrinsics

```cpp
#include <immintrin.h>

__m256i // 256 raw bits

__epi8 // 32 lanes x 8 bit
__epi16 // 16 lanes x 16 bit
__epi32 // 8 lanes x 32 bit
__epi64 // 4 lanes x 64 bit
```

#### intrinsic functions:

```cpp
#include <immintrin.h>

// used in probe
// used for setting all slots to empty + for fingerprint

_mm256_set1_epi8 // broadcast a single byte into all 32 lanes

// used to load croup into vecrot
_mm256_loadu_si256 // loads 32 bytes from memory (unaligned ) into resgister

// used to compare vectors

_m256i_cmpeq_epi8 // compares all 32  byte lane pairs simultaneously -> equal: 0xFF, unequal: 0x00

// used to create matchMask
_mm256_movemask_epi8 (a __m2561a ) // collapses 256-bit comparison result into 32-bit bitmask - extract bit 7 of each lane

// used to find forst fingerprint match
__builtin_ctz ( unsigned x ) // (= count trailing zeros ) returns the index of the lowest set bit

```
