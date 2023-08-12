#include "lib/api.h"
#include "lib/crypto_aead.h"

#include <string.h>
#include <stdio.h>
#include <sodium.h>
#include <time.h>
#include <x86intrin.h>

#define CHUNK_SIZE 4096

#define cpucycles(cycles) cycles = __rdtsc()
#define cpucycles_reset() cpucycles_sum = 0
#define cpucycles_start() cpucycles(cpucycles_before)
#define cpucycles_stop()                                 \
  do                                                     \
  {                                                      \
    cpucycles(cpucycles_after);                          \
    cpucycles_sum += cpucycles_after - cpucycles_before; \
  } while (0)

#define cpucycles_result() cpucycles_sum

unsigned long long cpucycles_before, cpucycles_after, cpucycles_sum;

size_t rlen_total;
double total_cpucycles;
struct timespec begin_cpu, end_cpu, begin_wall, end_wall;

static int decrypt(const char *target_file, const char *source_file) {
  #define ADDITIONAL_DATA (const unsigned char *)"123456"
  #define ADDITIONAL_DATA_LEN 6

  unsigned char buf_in[CHUNK_SIZE + CRYPTO_ABYTES];
  unsigned char buf_out[CHUNK_SIZE];
  unsigned char nonce[CRYPTO_NPUBBYTES];
  unsigned char key[CRYPTO_KEYBYTES];

  FILE *fp_t, *fp_s, *pmk_key;
  unsigned long long out_len;
  size_t  rlen;

  pmk_key = fopen("PMK.key", "rb");
  fp_s = fopen(source_file, "rb");
  fp_t = fopen(target_file, "wb");

  fread(nonce, sizeof(unsigned char), sizeof(nonce), fp_s); // Writing nonce into file
  fread(key, 1, CRYPTO_KEYBYTES, pmk_key); // Reading PMK key file

  clock_gettime(CLOCK_REALTIME, &begin_wall);
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &begin_cpu);

  while(rlen = fread(buf_in, 1, sizeof buf_in, fp_s)) {
    cpucycles_reset();
    cpucycles_start();
    crypto_aead_decrypt(buf_out, &out_len, (void*)0, buf_in, rlen, ADDITIONAL_DATA, ADDITIONAL_DATA_LEN, nonce, key);
    cpucycles_stop();

    fwrite(buf_out, 1, (size_t)out_len, fp_t);

    total_cpucycles += cpucycles_result();
    rlen_total += rlen;
  }

  clock_gettime(CLOCK_REALTIME, &end_wall);
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_cpu);

  fclose(fp_t);
  fclose(fp_s);
  fclose(pmk_key);
  return 0;
}

int main(int argc, char *argv[]) {
  printf("\n[*] Attempting to decrypt key\n");

  if (strcmp(argv[1], "secret") == 0){
    if (decrypt("secret.key", "secret.key.hacklab") != 0) {
      return 1;
    }
  }

  if (strcmp(argv[1], "pub") == 0){
    if (decrypt("decrypt.key", "public.key.hacklab") != 0) {
      return 1;
    }
  }

  if (strcmp(argv[1], "nbit") == 0){
    if (decrypt("nbit.key", "nbit.key.hacklab") != 0) {
      return 1;
    }
  }

  printf("\n[+] Key decrypted\n");

  double total_time_cpu = (end_cpu.tv_sec - begin_cpu.tv_sec) + (end_cpu.tv_nsec - begin_cpu.tv_nsec) / 1000000000.0;
  double total_time_wall = (end_wall.tv_sec - begin_wall.tv_sec) + (end_wall.tv_nsec - begin_wall.tv_nsec) / 1000000000.0;

  printf("\nWALL time: %f seconds\n", total_time_wall);
  printf("CPU time: %f seconds\n", total_time_cpu);

  printf("\nTotal CPU Cycles: %.0f\n", total_cpucycles);
  printf("CPU Cycles/Bytes: %f\n", total_cpucycles / rlen_total);

  return 0;
}