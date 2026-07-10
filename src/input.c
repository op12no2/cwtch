#include <string.h>
#include <stdio.h>
#include "input.h"
#include "timecontrol.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#define INPUT_BUF_SIZE 8192

static char line_buf[INPUT_BUF_SIZE];
static volatile int line_ready = 0;
static volatile int searching = 0;

#ifdef _WIN32
static DWORD WINAPI input_loop(LPVOID arg) {
  (void)arg;
#else
static void *input_loop(void *arg) {
  (void)arg;
#endif

  char buf[INPUT_BUF_SIZE];

  while (fgets(buf, sizeof(buf), stdin)) {

    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
      buf[len - 1] = '\0';

    if (searching) {
      if (strcmp(buf, "stop") == 0) {
        time_control.finished = 1;
        continue;
      }
      if (strcmp(buf, "quit") == 0 || strcmp(buf, "q") == 0) {
        time_control.finished = 1;
      }
    }

    while (line_ready) {}

    strcpy(line_buf, buf);
    line_ready = 1;
  }

  // stdin closed
  while (line_ready) {}
  strcpy(line_buf, "quit");
  line_ready = 1;

#ifdef _WIN32
  return 0;
#else
  return NULL;
#endif
}

void input_init(void) {
#ifdef _WIN32
  CreateThread(NULL, 0, input_loop, NULL, 0, NULL);
#else
  pthread_t thread;
  pthread_create(&thread, NULL, input_loop, NULL);
  pthread_detach(thread);
#endif
}

int input_get(char *buf, int size) {

  while (!line_ready) {}

  strncpy(buf, line_buf, size);
  buf[size - 1] = '\0';
  line_ready = 0;

  return 1;
}

void input_set_searching(int s) {
  searching = s;
}
