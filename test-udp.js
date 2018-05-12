const {
  /* eslint camelcase: 0 */
  sizeof_uv_udp_t,
  sizeof_uv_udp_send_t,
  nuv_udp_init,
  nuv_udp_bind,
  nuv_udp_getsockname,
  nuv_udp_send,
  nuv_udp_recv_start,
  nuv_udp_recv_stop,
  nuv_close
} = require('bindings')('nuv.node')

let server = {}
nuv_udp_init(Buffer.alloc(sizeof_uv_udp_t), server)
nuv_udp_bind(server.handle, '0.0.0.0', 0)
console.log(server)
let addr = nuv_udp_getsockname(server.handle)
console.log(addr)

server.readBuffer = Buffer.alloc(1024)
server.onRecv = (...args) => {
  console.log('onRecv', args)
  nuv_udp_recv_stop(server.handle)
  nuv_close(server.handle)
}
nuv_udp_recv_start(server.handle)
server.onClose = () => {
  console.log('Closed')
}

let client = {}
nuv_udp_init(Buffer.alloc(sizeof_uv_udp_t), client)
client.onSend = (...args) => {
  console.log('onSend', args)
}
nuv_udp_send(Buffer.alloc(sizeof_uv_udp_send_t), client.handle, Buffer.from('Hello'), '127.0.0.1', addr.port)
nuv_close(client.handle)
