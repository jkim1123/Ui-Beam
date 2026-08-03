#include <obs-module.h>
#include <dump_buffer.h>
#include <pthread.h>


struct dump_packet_node {
	void *packet;
	struct dump_packet_node *next;
};

struct dump_buffer {
	pthread_mutex_t mutex;
	struct dump_packet_node *head;
	struct dump_packet_node *tail;
	size_t packet_count;
	size_t byte_count;

	int64_t oldest_dts_usec;
	int64_t newest_dts_usec;
	int64_t delay_usec;
	size_t max_bytes;

};

static void release_packet_node(struct dump_packet_node *node)
{
	if (!node)
		return;
	obs_encoder_packet_release(&node->packet);
	bfree(node);
}

struct dump_buffer *dump_buffer_create(int64_t delay_usec, size_t max_bytes)
{
	struct dump_buffer *buffer;

	if (delay_usec < 0)
		delay_usec = 0;

	buffer = bzalloc(sizeof(*buffer));
	if (!buffer)
		return NULL;

	if (pthread_mutex_init(&buffer->mutex, NULL) != 0) {
		bfree(buffer);
		return NULL;
	}

	buffer->delay_usec = delay_usec;
	buffer->max_bytes = max_bytes;

	blog(LOG_INFO, "[ui-beam] dump buffer created: delay=%lld ms, max=%zu bytes", (long long)(delay_usec / 1000),
	     max_bytes);

	return buffer;
}
void dump_buffer_init(void)
{
	blog(LOG_INFO, "Buffer initialized");
}

void dump_buffer_push(void *packet)
{
	// TODO
}

void dump_buffer_flush(void)
{
	blog(LOG_INFO, "Buffer flushed");
}

void dump_buffer_destroy(void)
{
	blog(LOG_INFO, "Buffer destroyed");
}