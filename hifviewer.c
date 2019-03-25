#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include <lzma.h>

void hv_exit(char *str, int code) {
  perror(str);
  SDL_Quit();
  exit(1);
}

enum {
  ERROR = 1,
  LIBSDL_ERROR,
  LIBLZMA_ERROR,
};

int main(int argc, char *argv[]) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
    hv_exit("SDL_Init()", LIBSDL_ERROR);

  FILE *hif = fopen(argv[1], "rb");
  if (hif == NULL) hv_exit("fopen()", ERROR);

  fseek(hif, 0, SEEK_END);

  long siz_hifbuf = ftell(hif);
  size_t hif_size = siz_hifbuf - 12;
  rewind(hif);

  uint8_t *hifbuf = (uint8_t *) malloc(siz_hifbuf);
  uint8_t *zbuf = (uint8_t *) malloc(hif_size);
  uint8_t *unzbuf = (uint8_t *) malloc(hif_size);

  if (hifbuf == NULL) hv_exit("malloc()", ERROR);

  if (fread(hifbuf, siz_hifbuf, 1, hif) != 1) hv_exit("fread()", ERROR);

  for (size_t i = 0; i < 255; i++)
    printf("%x ", hifbuf[i]);
  puts("");

  int width = hifbuf[6] * 256 + hifbuf[7];
  int height = hifbuf[10] * 256 + hifbuf[11];

  printf("%d %d\n", width, height);

  memcpy(zbuf, hifbuf + 12, hif_size);
  puts("ZBUF");
  for (size_t i = 0; i < 255; i++)
    printf("%x ", zbuf[i]);

  lzma_stream defstream = LZMA_STREAM_INIT;
  uint32_t preset = 9 | 1;
  defstream.avail_in = (size_t) hif_size + 1;
  defstream.next_in = (uint8_t *) zbuf;
  defstream.avail_out = (size_t) hif_size + 1;
  defstream.next_out = (uint8_t *) unzbuf;

  lzma_ret ret = lzma_auto_decoder(&defstream, UINT64_MAX, 0);
  if (ret != LZMA_OK) hv_exit("lzma_easy_encoder()", LIBLZMA_ERROR);

  lzma_action action = LZMA_FINISH;
  ret = lzma_code(&defstream, action);
  printf("%d\n", ret);

  const char *msg;
  switch (ret) {
  case LZMA_MEM_ERROR:
    msg = "Memory allocation failed";
    break;

  case LZMA_FORMAT_ERROR:
    // .xz magic bytes weren't found.
    msg = "The input is not in the .xz format";
    break;

  case LZMA_OPTIONS_ERROR:
    // For example, the headers specify a filter
    // that isn't supported by this liblzma
    // version (or it hasn't been enabled when
    // building liblzma, but no-one sane does
    // that unless building liblzma for an
    // embedded system). Upgrading to a newer
    // liblzma might help.
    //
    // Note that it is unlikely that the file has
    // accidentally became corrupt if you get this
    // error. The integrity of the .xz headers is
    // always verified with a CRC32, so
    // unintentionally corrupt files can be
    // distinguished from unsupported files.
    msg = "Unsupported compression options";
    break;

  case LZMA_DATA_ERROR:
    msg = "Compressed file is corrupt";
    break;

  case LZMA_BUF_ERROR:
    // Typically this error means that a valid
    // file has got truncated, but it might also
    // be a damaged part in the file that makes
    // the decoder think the file is truncated.
    // If you prefer, you can use the same error
    // message for this as for LZMA_DATA_ERROR.
    msg = "Compressed file is truncated or "
        "otherwise corrupt";
    break;

  default:
    // This is most likely LZMA_PROG_ERROR.
    msg = "Unknown error, possibly a bug";
    break;
  }

  printf("%s\n", msg);

  if (ret != LZMA_OK && ret != LZMA_STREAM_END)
    hv_exit("lzma_code()", LIBLZMA_ERROR);

  // for (size_t i = 0; i < 255; i++)
  //   printf("%x ", unzbuf[i]);

  SDL_Window *win = SDL_CreateWindow(
    "Hello World!",
    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    width, height, SDL_WINDOW_SHOWN);
  
  if (win == NULL) hv_exit("SDL_CreateWindow()", LIBSDL_ERROR);

  SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (ren == NULL) hv_exit("SDL_CreateRenderer", LIBSDL_ERROR);

  bool quit = false;
  SDL_Event event;
  while (!quit) {
    SDL_RenderClear(ren);
    size_t l = 0;
    for (size_t i = 0; i < width; i++) {
      for (size_t j = 0; j < height; j++) {
        SDL_SetRenderDrawColor(ren, unzbuf[l++], unzbuf[l++], unzbuf[l++], 0xFF);
        SDL_RenderDrawPoint(ren, i, j);
      }
    }
    SDL_SetRenderDrawColor(ren, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderPresent(ren);
    
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        quit = true;
      }
    }
  }

  free(hifbuf);
  free(zbuf);
  free(unzbuf);

  SDL_DestroyWindow(win);
  SDL_Quit();
  return EXIT_SUCCESS;
}