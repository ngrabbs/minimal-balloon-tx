#include "wspr_encoder.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// ========================= Spec constants =========================
// Convolutional encoder polynomials (K=32, r=1/2), MSB-first taps:
#define POLY0 0xF2D05351u
#define POLY1 0xE4613C47u
// 162-bit sync vector (per G4JNT / WSJT-X notes)
static const uint8_t SYNC[WSPR_SYMS] = {
  1,1,0,0,0,0,0,0,1,0,0,0,1,1,1,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,0,0,0,0,1,0,0,1,0,1,0,0,
  0,0,0,0,1,0,1,1,0,0,1,1,0,1,0,0,0,1,1,0,1,0,0,0,0,1,1,0,1,0,1,0,1,0,1,0,0,1,0,0,1,0,
  1,1,0,0,0,1,1,0,1,0,1,0,0,0,1,0,0,0,0,0,1,0,0,1,0,0,1,1,1,0,1,1,0,0,1,1,0,1,0,0,0,1,
  1,1,0,0,0,0,0,1,0,1,0,0,1,1,0,0,0,0,0,0,0,1,1,0,1,0,1,1,0,0,0,1,1,0,0,0
};

// Run-time selectable minute windows (bitmask over even minutes 0..58)
// Bit n corresponds to minute (2*n). Default {0,2,4,6,8} -> bits 0..4 => 0b1_1111 = 0x1F
static uint32_t g_minute_mask = 0x1F;

// ========================= Helpers =========================
static inline int is_even_minute(int m) { return (m & 1) == 0; }

uint32_t wspr_minutes_mask_get(void){ return g_minute_mask; }
void wspr_minutes_mask_set(uint32_t mask){ g_minute_mask = mask; }

bool wspr_should_tx_in_minute(int even_minute){
  if (!is_even_minute(even_minute)) return false;
  int idx = even_minute / 2; // 0..29
  if (idx < 0 || idx > 29) return false;
  return (g_minute_mask >> idx) & 1u;
}

// get bit MSB-first from packed byte array
static inline int get_bit(const uint8_t *buf, int bit_idx){
  // bit_idx 0 = MSB of buf[0]
  int byte = bit_idx >> 3;
  int bit  = 7 - (bit_idx & 7);
  return (buf[byte] >> bit) & 1;
}
static inline void set_bit(uint8_t *buf, int bit_idx, int v){
  int byte = bit_idx >> 3;
  int bit  = 7 - (bit_idx & 7);
  if (v) buf[byte] |= (1u << bit);
  else   buf[byte] &= ~(1u << bit);
}

// map callsign char to value (0-9 => 0..9, A-Z => 10..35, space => 36)
static int chval(int c){
  if (c==' ') return 36;
  if (c>='0' && c<='9') return c - '0';
  if (c>='A' && c<='Z') return 10 + (c - 'A');
  return -1;
}

// Pack callsign per G4JNT rules into 28-bit N
static bool pack_callsign_28(const char *in_callsign, uint32_t *outN){
  char cs[7]={0};
  // Uppercase and copy (6 max)
  int n = 0;
  for (const char *p=in_callsign; *p && n<6; ++p){
    cs[n++] = (char)toupper((unsigned char)*p);
  }
  cs[n]=0;

  // Ensure 3rd character is a digit: prepend space if needed
  if (n < 3 || !(cs[2]>='0' && cs[2]<='9')){
    // shift right, prepend space
    if (n==6) n=5;
    for (int i=n; i>0; --i) cs[i]=cs[i-1];
    cs[0]=' ';
    ++n;
    cs[6]=0;
  }

  // pad to 6 with spaces
  while (n<6) cs[n++]=' ';
  cs[6]=0;

  // Values with constraints: c3 must be digit; last 3 must be A..Z or space
  int c1=chval(cs[0]); if (c1<0) return false;
  int c2=chval(cs[1]); if (c2<0 || cs[1]==' ') return false; // second cannot be space
  if (!(cs[2]>='0' && cs[2]<='9')) return false;
  int d3 = cs[2]-'0';
  // last 3
  int a = chval(cs[3]); int b = chval(cs[4]); int c = chval(cs[5]);
  if (a<10 || b<10 || c<10) return false; // must be 10..36

  // Build N using mixed radices
  // N1 = c1 (37 values inc space)
  // N2 = N1*36 + c2 (second cannot be space)
  // N3 = N2*10 + d3 (digit)
  // N4 = 27*N3 + (a-10)
  // N5 = 27*N4 + (b-10)
  // N6 = 27*N5 + (c-10)
  uint32_t N = (uint32_t)c1;
  N = N*36u + (uint32_t)c2;
  N = N*10u + (uint32_t)d3;
  N = 27u*N + (uint32_t)(a-10);
  N = 27u*N + (uint32_t)(b-10);
  N = 27u*N + (uint32_t)(c-10);

  *outN = N; // fits 28 bits
  return true;
}

// Pack locator+power into 22 bits M (15+7)
static bool pack_loc_pow_22(const char *grid, int pwr_dbm, uint32_t *outM){
  if (!grid || strlen(grid)!=4) return false;
  char L1=toupper(grid[0]), L2=toupper(grid[1]);
  char L3=grid[2], L4=grid[3];
  if (L1<'A'||L1>'R'||L2<'A'||L2>'R'||L3<'0'||L3>'9'||L4<'0'||L4>'9') return false;
  int loc1=L1-'A', loc2=L2-'A', loc3=L3-'0', loc4=L4-'0';

  // M1 = (179 - 10*Loc1 - Loc3)*180 + 10*Loc2 + Loc4
  uint32_t M1 = (uint32_t)((179 - 10*loc1 - loc3)*180 + 10*loc2 + loc4);

  if (pwr_dbm < 0) pwr_dbm = 0;
  if (pwr_dbm > 60) pwr_dbm = 60;
  // M = M1*128 + pwr + 64
  uint32_t M = M1*128u + (uint32_t)(pwr_dbm + 64);
  *outM = M; // fits 22 bits
  return true;
}

// Build 50-bit payload packed MSB-first in payload50[0..6] (top 50 bits used)
static void pack_payload50(uint32_t N28, uint32_t M22, uint8_t out[7]){
  // Layout: [N:28 bits][M:22 bits] => total 50
  // We'll put them into a 56-bit buffer and just ignore the last 6 bits.
  uint64_t v = (((uint64_t)N28) << 22) | ((uint64_t)M22);
  // place v at top (bits 55..6) so our first 50 bits are MSB-first
  v <<= 6; // now 50 MSB bits occupy bits 55..6
  for (int i=0;i<7;i++){
    out[i] = (uint8_t)((v >> (56-8*(i+1))) & 0xFF);
  }
}

// Convolutional encoder: read 81 bits (50 + 31 zeros) MSB-first from payload+padding
static void conv_encode_162(const uint8_t payload50[7], uint8_t out162[162]){
  uint32_t r0 = 0, r1 = 0; // shift registers
  int outp = 0;
  // We need 81 input bits: 50 payload bits then 31 zeros
  for (int i=0;i<81;i++){
    int inb;
    if (i < 50) inb = get_bit(payload50, i);
    else        inb = 0;

    // shift left, inject at LSB
    r0 = (r0 << 1) | (uint32_t)inb;
    r1 = (r1 << 1) | (uint32_t)inb;

    // parity of (r & POLY)
    uint32_t x = r0 & POLY0;
    x ^= x >> 16; x ^= x >> 8; x ^= x >> 4; x ^= x >> 2; x ^= x >> 1;
    out162[outp++] = (uint8_t)(x & 1u);

    x = r1 & POLY1;
    x ^= x >> 16; x ^= x >> 8; x ^= x >> 4; x ^= x >> 2; x ^= x >> 1;
    out162[outp++] = (uint8_t)(x & 1u);
  }
}

// Interleave via bit-reversed addresses
static void interleave_162(const uint8_t in[162], uint8_t out[162]){
  int p = 0;
  for (int i=0;i<256 && p<162;i++){
    // bit-reverse 8-bit i
    uint8_t j = (uint8_t)i;
    j = (uint8_t)(((j * 0x0802u & 0x22110u) | (j * 0x8020u & 0x88440u)) * 0x10101u >> 16);
    if (j < 162){
      out[j] = in[p++];
    }
  }
}

// Merge with sync to produce tones
static void merge_sync_to_symbols(const uint8_t data_inter[162],
                                  uint8_t symbols[162], uint8_t sync_out[162]){
  for (int i=0;i<162;i++){
    uint8_t s = SYNC[i] & 1u;
    uint8_t d = data_inter[i] & 1u;
    sync_out[i] = s;
    symbols[i] = (uint8_t)(s + 2*d); // 0..3
  }
}

bool wspr_build_frame(const wspr_cfg_t *cfg, wspr_frame_t *out){
  if (!cfg || !out) return false;
  wspr_frame_t z = {0};
  uint32_t N28=0, M22=0;
  if (!pack_callsign_28(cfg->callsign, &N28)) return false;
  if (!pack_loc_pow_22(cfg->grid, cfg->power_dbm, &M22)) return false;

  pack_payload50(N28, M22, z.payload50);
  conv_encode_162(z.payload50, z.conv162_bits);
  interleave_162(z.conv162_bits, z.interleaved_bits);
  merge_sync_to_symbols(z.interleaved_bits, z.symbols, z.sync_bits);

  *out = z;
  return true;
}

// Pretty-printers
static void print_bits_u8_array(const uint8_t *bits, int n){
  for (int i=0;i<n;i++){
    putchar(bits[i] ? '1':'0');
    if ((i%8)==7) putchar(' ');
  }
  putchar('\n');
}
static void print_u8_csv(const uint8_t *a, int n){
  for (int i=0;i<n;i++){
    printf("%d%s", (int)a[i], (i==n-1)?"\n":",");
  }
}

void wspr_print_frame(const wspr_cfg_t *cfg, const wspr_frame_t *f){
  printf("[WSPR] call=%s grid=%s pwr=%d dBm\n", cfg->callsign, cfg->grid, cfg->power_dbm);

  // 50-bit payload (show as 50 bits)
  printf("[WSPR] 50b payload (MSB->LSB): ");
  for (int i=0;i<50;i++){
    int b = get_bit(f->payload50, i);
    putchar(b?'1':'0');
    if ((i%8)==7) putchar(' ');
  }
  putchar('\n');

  printf("[WSPR] 162b after convolution (pre-interleave) bits:\n");
  print_bits_u8_array(f->conv162_bits, 162);

  printf("[WSPR] 162b after interleave (data bits):\n");
  print_bits_u8_array(f->interleaved_bits, 162);

  printf("[WSPR] 162 symbols (0..3):\n");
  print_u8_csv(f->symbols, 162);
}


static void maidenhead_4_from_latlon(double lat, double lon, char out[5]){
  // Clamp ranges, convert to field/square
  if (lat >  90) lat =  90; if (lat < -90) lat = -90;
  if (lon > 180) lon = 180; if (lon < -180) lon = -180;
  double adj_lon = lon + 180.0;
  double adj_lat = lat +  90.0;

  int A = (int)(adj_lon / 20.0);
  int B = (int)(adj_lat / 10.0);
  int C = (int)fmod(adj_lon, 20.0) / 2;  // 0..9
  int D = (int)fmod(adj_lat, 10.0) / 1;  // 0..9

  out[0] = 'A' + A;
  out[1] = 'A' + B;
  out[2] = '0' + C;
  out[3] = '0' + D;
  out[4] = 0;
}

void wspr_update_grid_from_latlon(double lat_deg, double lon_deg){
  char g[5]; maidenhead_4_from_latlon(lat_deg, lon_deg, g);
  wspr_set_grid(g);
}