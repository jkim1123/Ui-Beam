#include <obs-module.h>

void output_init(void)
{
	blog(LOG_INFO, "Output initialized");
}

void output_shutdown(void)
{
	blog(LOG_INFO, "Output shutdown");
}