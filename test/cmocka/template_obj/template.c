#include <json-c/json.h>
#include <libubox/blobmsg_json.h>
#include <libubox/blobmsg.h>
#include <libubus.h>

struct ubus_context *ctx;
static void send_restricted_event(struct uloop_timeout *t);
static void send_event(struct uloop_timeout *t);
static void toggle_object(struct uloop_timeout *t);

struct uloop_timeout toggling_event = {
	.cb = toggle_object
};

struct uloop_timeout restricted_event = {
	.cb = send_restricted_event
};

struct uloop_timeout event = {
	.cb = send_event
};

static void send_event(struct uloop_timeout *t)
{
	struct blob_buf b = {0};

	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "test", "success");
	ubus_send_event(ctx, "template", b.head);
	blob_buf_free(&b);

	uloop_timeout_set(&event, 2 * 1000);
}

static void send_restricted_event(struct uloop_timeout *t)
{
	struct blob_buf b = {0};

	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "test", "success");
	ubus_send_event(ctx, "restricted_template", b.head);
	blob_buf_free(&b);

	uloop_timeout_set(&restricted_event, 2 * 1000);
}

int test(struct ubus_context *ctx, struct ubus_object *obj,
		  struct ubus_request_data *req, const char *method,
		  struct blob_attr *msg)
{
	struct blob_buf b = {0};

	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "test", "success");
	ubus_send_reply(ctx, req, b.head);
	blob_buf_free(&b);
	return 0;
}

struct ubus_method template_object_methods[] = {
	UBUS_METHOD_NOARG("test", test),
};

struct ubus_object_type template_object_type =
	UBUS_OBJECT_TYPE("template", template_object_methods);

struct ubus_object template_object = {
	.name = "template",
	.type = &template_object_type,
	.methods = template_object_methods,
	.n_methods = ARRAY_SIZE(template_object_methods),
};

struct ubus_object invalid_object = {
	.name = "invalid_template",
	.type = &template_object_type,
	.methods = template_object_methods,
	.n_methods = ARRAY_SIZE(template_object_methods),
};

struct ubus_object restricted_object = {
	.name = "restricted_template",
	.type = &template_object_type,
	.methods = template_object_methods,
	.n_methods = ARRAY_SIZE(template_object_methods),
};

struct ubus_object toggling_object = {
	.name = "toggling_template",
	.type = &template_object_type,
	.methods = template_object_methods,
	.n_methods = ARRAY_SIZE(template_object_methods),
};

/* ubus_remove_object(struct ubus_context *ctx, struct ubus_object *obj); */

bool toggle;

static void toggle_object(struct uloop_timeout *t)
{
	toggle = !toggle;
	if (toggle)
		ubus_add_object(ctx, &toggling_object);
	else
		ubus_remove_object(ctx, &toggling_object);

	uloop_timeout_set(&toggling_event, 3 * 1000);
}

int main(int argc, char **argv)
{
	int ret;

	uloop_init();

	ctx = ubus_connect(NULL);
	if (!ctx)
			return -EIO;

	printf("connected as %08x\n", ctx->local_id);
	ubus_add_uloop(ctx);
	ret = ubus_add_object(ctx, &template_object);
	if (ret != 0)
			fprintf(stderr, "Failed to publish object '%s': %s\n", template_object.name, ubus_strerror(ret));

	ret = ubus_add_object(ctx, &restricted_object);
	if (ret != 0)
			fprintf(stderr, "Failed to publish object '%s': %s\n", restricted_object.name, ubus_strerror(ret));

	ret = ubus_add_object(ctx, &invalid_object);
	if (ret != 0)
			fprintf(stderr, "Failed to publish object '%s': %s\n", restricted_object.name, ubus_strerror(ret));

	uloop_timeout_set(&event, 2 * 1000);
	uloop_timeout_set(&restricted_event, 2 * 1000);
	uloop_timeout_set(&toggling_event, 3 * 1000);
	uloop_run();
	ubus_free(ctx);

	return 0;
}
