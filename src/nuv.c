#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <uv.h>
#include <node_api.h>
#include <napi-macros.h>

typedef struct {
  napi_env env; // The JS environment.
  napi_ref this; // Event handlers, read buffer, and `this` for callbacks.
} nuv_context_t;

#define NUV_CALLBACK_START(handle, name) \
  nuv_context_t* context = handle->data; \
  napi_env env = context->env; \
  napi_handle_scope scope; \
  napi_open_handle_scope(env, &scope); \
  napi_value this; \
  napi_get_reference_value(env, context->this, &this); \
  napi_value handler; \
  napi_get_named_property(env, this, name, &handler);

#define NUV_CALLBACK_ARGS(count) \
  int argc = count; \
  napi_value argv[count];

#define NUV_CALLBACK_CALL() \
  napi_value result; \
  napi_make_callback(env, NULL, this, handler, argc, argv, &result); \

#define NUV_CALLBACK_END() \
  napi_close_handle_scope(env, scope);

#define NAPI_BOOL(name, val) \
  bool name; \
  napi_get_value_bool(env, val, &name);

static napi_status nuv_create_status(napi_env env, int status, napi_value* target) {
  if (status) {
    napi_value code;
    napi_value message;
    napi_create_string_utf8(env, uv_err_name(status), NAPI_AUTO_LENGTH, &code);
    napi_create_string_utf8(env, uv_strerror(status), NAPI_AUTO_LENGTH, &message);
    return napi_create_error(env, code, message, target);
  }
  return napi_get_undefined(env, target);
}

static napi_value parse_sockaddr(napi_env env, struct sockaddr_storage* address) {
  char ip[INET6_ADDRSTRLEN];
  int port = 0;
  napi_value obj;
  napi_create_object(env, &obj);
  if (address->ss_family == AF_INET) {
    struct sockaddr_in* addrin = (struct sockaddr_in*)address;
    uv_inet_ntop(AF_INET, &(addrin->sin_addr), ip, INET6_ADDRSTRLEN);
    port = ntohs(addrin->sin_port);
  } else if (address->ss_family == AF_INET6) {
    struct sockaddr_in6* addrin6 = (struct sockaddr_in6*)address;
    uv_inet_ntop(AF_INET6, &(addrin6->sin6_addr), ip, INET6_ADDRSTRLEN);
    port = ntohs(addrin6->sin6_port);
  }
  napi_value port_value;
  napi_create_uint32(env, port, &port_value);
  napi_set_named_property(env, obj, "port", port_value);
  napi_value ip_value;
  napi_create_string_utf8(env, ip, NAPI_AUTO_LENGTH, &ip_value);
  napi_set_named_property(env, obj, "ip", ip_value);
  return obj;
}

static napi_status nuv_create_addr(napi_env env, struct sockaddr_storage* address, napi_value* res) {
  *res = parse_sockaddr(env, address);
  return napi_ok;
}

static void on_uv_close (uv_handle_t *handle) {
  // printf("on_uv_close handle=%p\n", handle);
  NUV_CALLBACK_START(handle, "onClose")
  NUV_CALLBACK_ARGS(0)
  NUV_CALLBACK_CALL()
  NUV_CALLBACK_END()

  // Clean up memory and release references now that the handle is closed.
  napi_delete_reference(env, context->this);
  free(handle->data);
  handle->data = NULL;
}

static void on_uv_connection(uv_stream_t* handle, int status) {
  // printf("on_uv_connection handle=%p status=%d\n", handle, status);
  if (status) {
    NUV_CALLBACK_START(handle, "onError")
    NUV_CALLBACK_ARGS(1)
    nuv_create_status(env, status, &argv[0]);
    NUV_CALLBACK_CALL()
    NUV_CALLBACK_END()
  } else {
    NUV_CALLBACK_START(handle, "onConnection")
    NUV_CALLBACK_ARGS(0)
    NUV_CALLBACK_CALL()
    NUV_CALLBACK_END()
  }
}

static void on_uv_alloc (uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  // printf("on_uv_alloc handle=%p suggested_size=%zu buf=%p\n", handle, suggested_size, buf);
  NUV_CALLBACK_START(handle, "readBuffer")
  NAPI_BUFFER_CAST(char *, data, handler)
  buf->base = data;
  buf->len = data_len;
  NUV_CALLBACK_END()
}

static void on_uv_read (uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
  // printf("on_uv_read handle=%p nread=%zd buf=%p\n", handle, nread, buf);
  if (!nread) return;
  NUV_CALLBACK_START(handle, "onRead")
  NUV_CALLBACK_ARGS(2)
  if (nread < 0) nuv_create_status(env, nread, &argv[0]);
  else napi_get_undefined(env, &argv[0]);
  if (nread > 0) napi_create_uint32(env, nread, &argv[1]);
  else napi_get_undefined(env, &argv[1]);
  NUV_CALLBACK_CALL()
  NUV_CALLBACK_END()
}

static void on_uv_connect (uv_connect_t* req, int status) {
  // printf("on_uv_connect req=%p status=%d\n", req, status);
  uv_stream_t* handle = req->handle;
  NUV_CALLBACK_START(handle, "onConnect")
  NUV_CALLBACK_ARGS(1)
  nuv_create_status(env, status, &argv[0]);
  NUV_CALLBACK_CALL()
  napi_delete_reference(env, req->data);
  req->data = NULL;
  NUV_CALLBACK_END()
}

static void on_uv_shutdown(uv_shutdown_t* req, int status) {
  // printf("on_uv_shutdown req=%p status=%d\n", req, status);
  uv_stream_t* handle = req->handle;
  NUV_CALLBACK_START(handle, "onShutdown")
  NUV_CALLBACK_ARGS(1)
  nuv_create_status(env, status, &argv[0]);
  NUV_CALLBACK_CALL()
  napi_delete_reference(env, req->data);
  req->data = NULL;
  NUV_CALLBACK_END()
}

static void on_uv_write(uv_write_t* req, int status) {
  // printf("on_uv_write req=%p status=%d\n", req, status);
  uv_stream_t* handle = req->handle;
  NUV_CALLBACK_START(handle, "onWrite")
  NUV_CALLBACK_ARGS(1)
  nuv_create_status(env, status, &argv[0]);
  NUV_CALLBACK_CALL()
  napi_delete_reference(env, req->data);
  req->data = NULL;
  NUV_CALLBACK_END()
}

static void on_uv_send(uv_udp_send_t* req, int status) {
  uv_udp_t* handle = req->handle;
  NUV_CALLBACK_START(handle, "onSend")
  NUV_CALLBACK_ARGS(1)
  nuv_create_status(env, status, &argv[0]);
  NUV_CALLBACK_CALL()
  napi_delete_reference(env, req->data);
  req->data = NULL;
  NUV_CALLBACK_END()
}

static void on_uv_recv(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags) {
  // printf("on_uv_recv handle=%p nread=%zd buf=%p addr=%p flags=%u\n", handle, nread, buf, addr, flags);
  if (!nread) return;
  NUV_CALLBACK_START(handle, "onRecv")
  NUV_CALLBACK_ARGS(3)
  if (nread < 0) nuv_create_status(env, nread, &argv[0]);
  else napi_get_undefined(env, &argv[0]);
  if (nread > 0) {
    napi_create_uint32(env, nread, &argv[1]);
    nuv_create_addr(env, (struct sockaddr_storage*)addr, &argv[2]);
  }
  else {
    napi_get_undefined(env, &argv[1]);
    napi_get_undefined(env, &argv[2]);
  }
  NUV_CALLBACK_CALL()
  NUV_CALLBACK_END()
}

// nuv_tcp_init(handle, this)
NAPI_METHOD(nuv_tcp_init) {
  NAPI_ARGV(2)
  NAPI_BUFFER_CAST(uv_tcp_t*, handle, argv[0])
  int NAPI_UV_THROWS(err, uv_tcp_init(uv_default_loop(), handle));
  nuv_context_t* context = malloc(sizeof(*context));
  handle->data = context;
  context->env = env;
  napi_create_reference(env, argv[1], 1, &(context->this));
  napi_set_named_property(env, argv[1], "handle", argv[0]);
  return NULL;
}

// nuv_tcp_nodelay(handle, enable)
NAPI_METHOD(nuv_tcp_nodelay) {
  NAPI_ARGV(2)
  NAPI_BUFFER_CAST(uv_tcp_t*, handle, argv[0])
  NAPI_BOOL(enable, argv[1])
  int NAPI_UV_THROWS(err, uv_tcp_nodelay(handle, enable));
  return NULL;
}

// nuv_tcp_keepalive(handle, enable, delay)
NAPI_METHOD(nuv_tcp_keepalive) {
  NAPI_ARGV(3)
  NAPI_BUFFER_CAST(uv_tcp_t*, handle, argv[0])
  NAPI_BOOL(enable, argv[1])
  NAPI_UINT32(delay, argv[2])
  int NAPI_UV_THROWS(err, uv_tcp_keepalive(handle, enable, delay));
  return NULL;
}

// nuv_tcp_simultaneous_accepts(handle, enable)
NAPI_METHOD(nuv_tcp_simultaneous_accepts) {
  NAPI_ARGV(2)
  NAPI_BUFFER_CAST(uv_tcp_t*, handle, argv[0])
  NAPI_BOOL(enable, argv[1])
  int NAPI_UV_THROWS(err, uv_tcp_simultaneous_accepts(handle, enable));
  return NULL;
}

// nuv_tcp_bind(handle, ip, port)
NAPI_METHOD(nuv_tcp_bind) {
  NAPI_ARGV(3)
  NAPI_BUFFER_CAST(uv_tcp_t*, handle, argv[0])
  NAPI_UTF8(ip, 64, argv[1])
  NAPI_UINT32(port, argv[2])

  struct sockaddr_in addr;
  int err;
  NAPI_UV_THROWS(err, uv_ip4_addr(ip, port, &addr));
  NAPI_UV_THROWS(err, uv_tcp_bind(handle, (const struct sockaddr*)&addr, 0));
  return NULL;
}

// nuv_tcp_getsockname(handle) -> { ip, port }
NAPI_METHOD(nuv_tcp_getsockname) {
  NAPI_ARGV(1)
  NAPI_BUFFER_CAST(uv_tcp_t*, handle, argv[0])
  struct sockaddr_storage name;
  int namelen = sizeof(name);
  int NAPI_UV_THROWS(err, uv_tcp_getsockname(handle, (struct sockaddr*)&name, &namelen));
  return parse_sockaddr(env, &name);
}

// nuv_tcp_getpeername(handle) -> { ip, port }
NAPI_METHOD(nuv_tcp_getpeername) {
  NAPI_ARGV(1)
  NAPI_BUFFER_CAST(uv_tcp_t*, handle, argv[0])
  struct sockaddr_storage name;
  int namelen = sizeof(name);
  int NAPI_UV_THROWS(err, uv_tcp_getpeername(handle, (struct sockaddr*)&name, &namelen));
  // int err = uv_tcp_getpeername(handle, (struct sockaddr*)&name, &namelen);
  // printf("err=%d\n", err);
  return parse_sockaddr(env, &name);
}

NAPI_METHOD(nuv_tcp_connect) {
  NAPI_ARGV(4)
  NAPI_BUFFER_CAST(uv_connect_t*, req, argv[0])
  NAPI_BUFFER_CAST(uv_tcp_t*, handle, argv[1])
  NAPI_UTF8(ip, 64, argv[2])
  NAPI_UINT32(port, argv[3])
  struct sockaddr_in addr;
  int err;
  NAPI_UV_THROWS(err, uv_ip4_addr(ip, port, &addr));
  NAPI_UV_THROWS(err, uv_tcp_connect(req, handle, (const struct sockaddr*)&addr, on_uv_connect));
  napi_create_reference(env, argv[0], 1, req->data);
  return NULL;
}

NAPI_METHOD(nuv_shutdown) {
  NAPI_ARGV(2)
  NAPI_BUFFER_CAST(uv_shutdown_t*, req, argv[0])
  NAPI_BUFFER_CAST(uv_stream_t*, handle, argv[1])
  int NAPI_UV_THROWS(err, uv_shutdown(req, handle, on_uv_shutdown));
  napi_create_reference(env, argv[0], 1, req->data);
  return NULL;
}

NAPI_METHOD(nuv_listen) {
  NAPI_ARGV(2)
  NAPI_BUFFER_CAST(uv_stream_t*, handle, argv[0])
  NAPI_UINT32(backlog, argv[1])
  int NAPI_UV_THROWS(err, uv_listen(handle, backlog, on_uv_connection));
  return NULL;
}

NAPI_METHOD(nuv_accept) {
  NAPI_ARGV(2)
  NAPI_BUFFER_CAST(uv_stream_t*, handle, argv[0])
  NAPI_BUFFER_CAST(uv_stream_t*, client, argv[1])
  int NAPI_UV_THROWS(err, uv_accept(handle, client));
  return NULL;
}

NAPI_METHOD(nuv_read_start) {
  NAPI_ARGV(1)
  NAPI_BUFFER_CAST(uv_stream_t*, handle, argv[0])
  int NAPI_UV_THROWS(err, uv_read_start(handle, on_uv_alloc, on_uv_read));
  return NULL;
}

NAPI_METHOD(nuv_read_stop) {
  NAPI_ARGV(1)
  NAPI_BUFFER_CAST(uv_stream_t*, handle, argv[0])
  int NAPI_UV_THROWS(err, uv_read_stop(handle));
  return NULL;
}

NAPI_METHOD(nuv_write) {
  NAPI_ARGV(3)
  NAPI_BUFFER_CAST(uv_write_t*, req, argv[0])
  NAPI_BUFFER_CAST(uv_stream_t*, handle, argv[1])
  NAPI_BUFFER_CAST(char*, data, argv[2])
  uv_buf_t buf;
  buf.base = data;
  buf.len = data_len;
  int NAPI_UV_THROWS(err, uv_write(req, handle, &buf, 1, on_uv_write));
  napi_create_reference(env, argv[0], 1, req->data);
  return NULL;
}

NAPI_METHOD(nuv_udp_init) {
  NAPI_ARGV(2)
  NAPI_BUFFER_CAST(uv_udp_t*, handle, argv[0])
  int NAPI_UV_THROWS(err, uv_udp_init(uv_default_loop(), handle));
  nuv_context_t* context = malloc(sizeof(*context));
  handle->data = context;
  context->env = env;
  napi_create_reference(env, argv[1], 1, &(context->this));
  napi_set_named_property(env, argv[1], "handle", argv[0]);
  return NULL;
}

NAPI_METHOD(nuv_udp_bind) {
  NAPI_ARGV(3)
  NAPI_BUFFER_CAST(uv_udp_t*, handle, argv[0])
  NAPI_UTF8(ip, 64, argv[1])
  NAPI_UINT32(port, argv[2])
  struct sockaddr_in addr;
  int err;
  NAPI_UV_THROWS(err, uv_ip4_addr(ip, port, &addr));
  NAPI_UV_THROWS(err, uv_udp_bind(handle, (const struct sockaddr*)&addr, 0));
  return NULL;
}

NAPI_METHOD(nuv_udp_getsockname) {
  NAPI_ARGV(1)
  NAPI_BUFFER_CAST(uv_udp_t*, handle, argv[0])
  struct sockaddr_storage name;
  int namelen = sizeof(name);
  int NAPI_UV_THROWS(err, uv_udp_getsockname(handle, (struct sockaddr*)&name, &namelen));
  return parse_sockaddr(env, &name);
}

NAPI_METHOD(nuv_udp_send) {
  NAPI_ARGV(5)
  NAPI_BUFFER_CAST(uv_udp_send_t*, req, argv[0])
  NAPI_BUFFER_CAST(uv_udp_t*, handle, argv[1])
  NAPI_BUFFER_CAST(char*, data, argv[2])
  NAPI_UTF8(ip, 64, argv[3])
  NAPI_UINT32(port, argv[4])
  struct sockaddr_in addr;
  int err;
  NAPI_UV_THROWS(err, uv_ip4_addr(ip, port, &addr));
  uv_buf_t buf;
  buf.base = data;
  buf.len = data_len;
  NAPI_UV_THROWS(err, uv_udp_send(req, handle, &buf, 1, (const struct sockaddr*)&addr, on_uv_send));
  napi_create_reference(env, argv[0], 1, req->data);
  return NULL;
}

NAPI_METHOD(nuv_udp_recv_start) {
  NAPI_ARGV(1)
  NAPI_BUFFER_CAST(uv_udp_t*, handle, argv[0])
  int NAPI_UV_THROWS(err, uv_udp_recv_start(handle, on_uv_alloc, on_uv_recv));
  return NULL;
}

NAPI_METHOD(nuv_udp_recv_stop) {
  NAPI_ARGV(1)
  NAPI_BUFFER_CAST(uv_udp_t*, handle, argv[0])
  int NAPI_UV_THROWS(err, uv_udp_recv_stop(handle));
  return NULL;
}



NAPI_METHOD(nuv_close) {
  NAPI_ARGV(1)
  NAPI_ARGV_BUFFER_CAST(uv_tcp_t*, handle, 0)
  uv_close((uv_handle_t*)handle, on_uv_close);
  return NULL;
}

NAPI_INIT() {
  NAPI_EXPORT_SIZEOF(uv_tcp_t)
  NAPI_EXPORT_SIZEOF(uv_connect_t)
  NAPI_EXPORT_SIZEOF(uv_shutdown_t)
  NAPI_EXPORT_SIZEOF(uv_write_t)
  NAPI_EXPORT_SIZEOF(uv_udp_t)
  NAPI_EXPORT_SIZEOF(uv_udp_send_t)
  // TCP Functions
  NAPI_EXPORT_FUNCTION(nuv_tcp_init)
  NAPI_EXPORT_FUNCTION(nuv_tcp_nodelay)
  NAPI_EXPORT_FUNCTION(nuv_tcp_keepalive)
  NAPI_EXPORT_FUNCTION(nuv_tcp_simultaneous_accepts)
  NAPI_EXPORT_FUNCTION(nuv_tcp_bind)
  NAPI_EXPORT_FUNCTION(nuv_tcp_getsockname)
  NAPI_EXPORT_FUNCTION(nuv_tcp_getpeername)
  NAPI_EXPORT_FUNCTION(nuv_tcp_connect)
  // Stream Functions
  NAPI_EXPORT_FUNCTION(nuv_shutdown)
  NAPI_EXPORT_FUNCTION(nuv_listen)
  NAPI_EXPORT_FUNCTION(nuv_accept)
  NAPI_EXPORT_FUNCTION(nuv_read_start)
  NAPI_EXPORT_FUNCTION(nuv_read_stop)
  NAPI_EXPORT_FUNCTION(nuv_write)
  // UDP Functions
  NAPI_EXPORT_FUNCTION(nuv_udp_init)
  NAPI_EXPORT_FUNCTION(nuv_udp_bind)
  NAPI_EXPORT_FUNCTION(nuv_udp_getsockname)
  NAPI_EXPORT_FUNCTION(nuv_udp_send)
  NAPI_EXPORT_FUNCTION(nuv_udp_recv_start)
  NAPI_EXPORT_FUNCTION(nuv_udp_recv_stop)
  // Handle Functions
  NAPI_EXPORT_FUNCTION(nuv_close)
}
