#include <obs-module.h>
#include "dump_buffer.h"
#include "output.h"

static struct dump_buffer *u_dump_buffer = NULL;
void output_init(void)
{
	if (u_dump_buffer)
		return;
	u_dump_buffer = dump_buffer_create(5000000, 1024 * 1024 * 1024); // 5 seconds delay, 1 GB max size
	if (!u_dump_buffer) {
		blog(LOG_ERROR, "Failed to create dump buffer");
		return;
	}
	blog(LOG_INFO, "Output initialized with 5 sec delay and 1 GB max size");
}
void flush_buffer(void) {
	if (!u_dump_buffer)
		return;
	size_t discarded_packs;
	discarded_packs = dump_buffer_flush(u_dump_buffer);
	blog(LOG_INFO, "Buffer flushed with %zu packets", discarded_packs);
}
void output_shutdown(void)
{
	if (!u_dump_buffer)
		return;
	dump_buffer_destroy(u_dump_buffer);
	u_dump_buffer = NULL;
	blog(LOG_INFO, "Output shutdown");
}