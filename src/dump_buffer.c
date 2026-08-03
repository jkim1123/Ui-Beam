/*
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
*/

#include "dump_buffer.h"

#include <obs-module.h>
#include <pthread.h>
#include <string.h>
#include <util/bmem.h>

struct dump_packet_node {
	struct encoder_packet packet;
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
//Free packet and node memory
static void release_packet_node(struct dump_packet_node *node)
{
	if (!node)
		return;

	obs_encoder_packet_release(&node->packet);
	bfree(node);
}
//Remove oldest packet from queue
static struct dump_packet_node *remove_head_locked(struct dump_buffer *buffer)
{
	struct dump_packet_node *node;

	if (!buffer || !buffer->head)
		return NULL;

	node = buffer->head;
	buffer->head = node->next;

	if (!buffer->head)
		buffer->tail = NULL;

	node->next = NULL;

	if (buffer->packet_count > 0)
		buffer->packet_count--;

	if (buffer->byte_count >= node->packet.size)
		buffer->byte_count -= node->packet.size;
	else
		buffer->byte_count = 0;

	if (buffer->head)
		buffer->oldest_dts_usec = buffer->head->packet.dts_usec;
	else
		buffer->oldest_dts_usec = 0;

	if (!buffer->tail)
		buffer->newest_dts_usec = 0;

	return node;
}

//Check if buffer exceeds memory limit
static void enforce_memory_limit_locked(struct dump_buffer *buffer)
{
	if (!buffer || buffer->max_bytes == 0)
		return;

	while (buffer->head && buffer->byte_count > buffer->max_bytes) {
		struct dump_packet_node *node = remove_head_locked(buffer);

		release_packet_node(node);
	}
}

//Initialize and allocate new dump buffer
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

//Add an encoded packet to the end of the queue
bool dump_buffer_push(struct dump_buffer *buffer, const struct encoder_packet *packet)
{
	struct dump_packet_node *node;

	if (!buffer || !packet || !packet->data || packet->size == 0)
		return false;

	node = bzalloc(sizeof(*node));
	if (!node)
		return false;

	/*
	 * Retain the encoded packet beyond the OBS callback.
	 *
	 * OBS 31 declares the second argument as non-const, so the cast is
	 * necessary even though this function does not logically modify the
	 * caller's packet.
	 */
	obs_encoder_packet_ref(&node->packet, (struct encoder_packet *)packet);

	pthread_mutex_lock(&buffer->mutex);

	if (buffer->tail)
		buffer->tail->next = node;
	else
		buffer->head = node;

	buffer->tail = node;

	buffer->packet_count++;
	buffer->byte_count += node->packet.size;

	if (buffer->packet_count == 1)
		buffer->oldest_dts_usec = node->packet.dts_usec;

	buffer->newest_dts_usec = node->packet.dts_usec;

	enforce_memory_limit_locked(buffer);

	pthread_mutex_unlock(&buffer->mutex);

	return true;
}


//Check if the oldest packet in the buffer is ready to be popped
bool dump_buffer_pop_ready(struct dump_buffer *buffer, int64_t newest_dts_usec, struct encoder_packet *out_packet)
{
	struct dump_packet_node *node;
	int64_t packet_age_usec;

	if (!buffer || !out_packet)
		return false;

	memset(out_packet, 0, sizeof(*out_packet));

	pthread_mutex_lock(&buffer->mutex);

	if (!buffer->head) {
		pthread_mutex_unlock(&buffer->mutex);
		return false;
	}

	packet_age_usec = newest_dts_usec - buffer->head->packet.dts_usec;

	if (packet_age_usec < buffer->delay_usec) {
		pthread_mutex_unlock(&buffer->mutex);
		return false;
	}

	node = remove_head_locked(buffer);

	/*
	 * Transfer ownership of the packet reference to the caller.
	 * The caller must call obs_encoder_packet_release().
	 */
	*out_packet = node->packet;
	memset(&node->packet, 0, sizeof(node->packet));

	bfree(node);

	pthread_mutex_unlock(&buffer->mutex);

	return true;
}


//Discard all content in the buffer
size_t dump_buffer_flush(struct dump_buffer *buffer)
{
	struct dump_packet_node *node;
	struct dump_packet_node *next;
	size_t discarded;

	if (!buffer)
		return 0;

	pthread_mutex_lock(&buffer->mutex);

	node = buffer->head;
	discarded = buffer->packet_count;

	buffer->head = NULL;
	buffer->tail = NULL;

	buffer->packet_count = 0;
	buffer->byte_count = 0;

	buffer->oldest_dts_usec = 0;
	buffer->newest_dts_usec = 0;

	pthread_mutex_unlock(&buffer->mutex);

	/*
	 * Release packet memory outside the mutex so the queue is not locked
	 * longer than necessary.
	 */
	while (node) {
		next = node->next;
		release_packet_node(node);
		node = next;
	}

	blog(LOG_WARNING, "[ui-beam] buffer flush discarded %zu packets", discarded);

	return discarded;
}

//Destroy the buffer and free all resources
void dump_buffer_destroy(struct dump_buffer *buffer)
{
	if (!buffer)
		return;

	dump_buffer_flush(buffer);
	pthread_mutex_destroy(&buffer->mutex);
	bfree(buffer);

	blog(LOG_INFO, "[ui-beam] dump buffer destroyed");
}