// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2018 Drew Richardson <drewrichardson@gmail.com>

#include "dr.h"

#if defined(_WIN32)

#include <windows.h>

dr_handle_t dr_stdin;
dr_handle_t dr_stdout;
dr_handle_t dr_stderr;

struct dr_result_void dr_console_startup(void) {
  HANDLE handle;

  handle = GetStdHandle(STD_INPUT_HANDLE);
  if (dr_unlikely(handle == INVALID_HANDLE_VALUE)) {
    return DR_RESULT_GETLASTERROR_VOID();
  }
  dr_stdin = (dr_handle_t)handle;

  handle = GetStdHandle(STD_OUTPUT_HANDLE);
  if (dr_unlikely(handle == INVALID_HANDLE_VALUE)) {
    return DR_RESULT_GETLASTERROR_VOID();
  }
  dr_stdout = (dr_handle_t)handle;

  handle = GetStdHandle(STD_ERROR_HANDLE);
  if (dr_unlikely(handle == INVALID_HANDLE_VALUE)) {
    return DR_RESULT_GETLASTERROR_VOID();
  }
  dr_stderr = (dr_handle_t)handle;

  return DR_RESULT_OK_VOID();
}

#else

#include <unistd.h>

dr_handle_t dr_stdin = STDIN_FILENO;
dr_handle_t dr_stdout = STDOUT_FILENO;
dr_handle_t dr_stderr = STDERR_FILENO;

struct dr_result_void dr_console_startup(void) {
  return DR_RESULT_OK_VOID();
}

#endif
