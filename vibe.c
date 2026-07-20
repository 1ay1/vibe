/*
 * vibe.c — the traditional-build translation unit for libvibe.
 *
 * libvibe is a single-header library: the entire implementation lives in
 * vibe.h, guarded by VIBE_IMPLEMENTATION. This file exists only so the
 * Makefile (and anyone who prefers a compiled libvibe.a/.so) has one .c to
 * build. Header-only users can skip it entirely:
 *
 *     #define VIBE_IMPLEMENTATION
 *     #include "vibe.h"
 *
 * SPDX-License-Identifier: MIT
 */
#define VIBE_IMPLEMENTATION
#include "vibe.h"
