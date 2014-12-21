
typedef void (*destroy_data_t)(void* data);

typedef struct _process_plane_context
{
	void* data;
	destroy_data_t destroy;
} process_plane_context;

void destroy_context(process_plane_context* context);

void init_context(process_plane_context* context);