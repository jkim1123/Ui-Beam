#pragma once

void dump_buffer_init(void);

void dump_buffer_push(void *packet);

void dump_buffer_flush(void);

void dump_buffer_destroy(void);