const {
  /* eslint camelcase: 0 */
  sizeof_uv_tcp_t,
  sizeof_uv_connect_t,
  sizeof_uv_shutdown_t,
  sizeof_uv_write_t,
  // TCP Functions
  nuv_tcp_init,
  nuv_tcp_nodelay,
  nuv_tcp_keepalive,
  nuv_tcp_simultaneous_accepts,
  nuv_tcp_bind,
  nuv_tcp_getsockname,
  nuv_tcp_getpeername,
  nuv_tcp_connect,
  // Stream Functions
  nuv_shutdown,
  nuv_listen,
  nuv_accept,
  nuv_read_start,
  nuv_read_stop,
  nuv_write,
  // Handle Functions
  nuv_close
} = require('bindings')('nuv.node')

let server = {}
nuv_tcp_init(Buffer.alloc(sizeof_uv_tcp_t), server)

nuv_tcp_simultaneous_accepts(server.handle, true)
nuv_tcp_bind(server.handle, '0.0.0.0', 0)
let addr = nuv_tcp_getsockname(server.handle)
console.log('server-sock', addr)

server.onConnection = err => {
  console.log('on_connection', {err})
  let socket = {}
  nuv_tcp_init(Buffer.alloc(sizeof_uv_tcp_t), socket)
  nuv_tcp_nodelay(socket.handle, true)
  nuv_tcp_keepalive(socket.handle, true, 300)
  nuv_accept(server.handle, socket.handle)
  console.log('client-sock', nuv_tcp_getsockname(socket.handle))
  console.log('client-peer', nuv_tcp_getpeername(socket.handle))
  socket.readBuffer = Buffer.alloc(1024)
  socket.onRead = (nread) => {
    console.log('socket.onRead', {nread})
    if (!nread) {
      nuv_read_stop(socket.handle)
      socket.onClose = () => {
        console.log('socket.onClose')
      }
      nuv_close(socket.handle)
    }
  }
  nuv_read_start(socket.handle)
}
nuv_listen(server.handle, 128)

let client = {}
nuv_tcp_init(Buffer.alloc(sizeof_uv_tcp_t), client)
client.onConnect = err => {
  delete client.onConnect
  console.log('client.onConnect', {err})
  nuv_write(Buffer.alloc(sizeof_uv_write_t), client.handle, Buffer.from('Hello World\n'))
}
client.onWrite = err => {
  console.log('client.onWrite', {err})
  nuv_shutdown(Buffer.alloc(sizeof_uv_shutdown_t), client.handle)
}
client.onShutdown = err => {
  delete client.onShutdown
  delete client.onWrite
  console.log('client.onShutdown', {err})
  nuv_close(client.handle)
}
client.onClose = () => {
  delete client.onClose
  delete client.handle
  console.log('client.onClose')
  nuv_close(server.handle)
}

nuv_tcp_connect(Buffer.alloc(sizeof_uv_connect_t), client.handle, '127.0.0.1', addr.port)
