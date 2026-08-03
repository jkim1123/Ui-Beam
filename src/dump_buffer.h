#pragma once

#include <obs-module.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct dump_buffer;

struct dump_buffer *dump_buffer_create(int64_t delay_usec, size_t max_bytes);

bool dump_buffer_push(struct dump_buffer *buffer, const struct encoder_packet *packet);

bool dump_buffer_pop_ready(struct dump_buffer *buffer, int64_t newest_dts_usec, struct encoder_packet *out_packet);

size_t dump_buffer_flush(struct dump_buffer *buffer);

void dump_buffer_destroy(struct dump_buffer *buffer);