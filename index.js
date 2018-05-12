const bindings = require('bindings')('nuv.node')

const {
  /* eslint camelcase: 0 */
  sizeof_uv_tcp_t,
  sizeof_uv_udp_t,
  sizeof_uv_connect_t,
  sizeof_uv_shutdown_t,
  sizeof_uv_write_t,
  sizeof_uv_udp_send_t,
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
  // UDP Functions
  nuv_udp_init,
  nuv_udp_bind,
  nuv_udp_getsockname,
  nuv_udp_send,
  nuv_udp_recv_start,
  nuv_udp_recv_stop,
  // Handle Functions
  nuv_close
} = bindings

class Handle {
  wait (name, self) {
    return new Promise((resolve, reject) => {
      this[name] = (err, result) => {
        delete this[name]
        return err ? reject(err) : resolve(self || result)
      }
    })
  }

  close () {
    nuv_close(this.handle)
    return this.wait('onClose')
  }
}

class Tcp extends Handle {
  constructor (handlers) {
    super()
    for (const key in handlers) {
      this[key] = handlers[key]
    }
    nuv_tcp_init(Buffer.alloc(sizeof_uv_tcp_t), this)
  }

  get sockname () {
    return nuv_tcp_getsockname(this.handle)
  }

  get peername () {
    return nuv_tcp_getpeername(this.handle)
  }
}

class Server extends Tcp {
  bind (ip, port) {
    nuv_tcp_bind(this.handle, ip, port)
    return this
  }

  listen (backlog) {
    nuv_listen(this.handle, backlog)
    return this
  }

  accept (options) {
    const socket = new Socket(options)
    nuv_accept(this.handle, socket.handle)
    return socket
  }

  set simultaneousAccepts (enable) {
    nuv_tcp_simultaneous_accepts(this.handle, enable)
  }
}

class Socket extends Tcp {
  constructor () {
    super()
    if (!this.readBuffer) this.readBuffer = Buffer.alloc(0x10000)
    this.queue = []
    this.reader = 0
    this.writer = 0
    this.paused = true
  }

  write (data, ...args) {
    if (!data) {
      nuv_shutdown(Buffer.alloc(sizeof_uv_shutdown_t), this.handle)
      return this.wait('onShutdown')
    }
    if (!Buffer.isBuffer(data)) data = Buffer.from(data, ...args)
    nuv_write(Buffer.alloc(sizeof_uv_write_t), this.handle, data)
    return this.wait('onWrite')
  }

  onRead (err, nread) {
    if (this.reader > this.writer) {
      const { resolve, reject } = this.queue[this.writer]
      delete this.queue[this.writer++]
      return err ? reject(err) : resolve(this.readBuffer.slice(0, nread))
    }
    if (!this.paused) {
      this.paused = true
      nuv_read_stop(this.handle)
    }
    this.queue[this.writer++] = { err, nread }
  }

  read () {
    return new Promise((resolve, reject) => {
      if (this.writer > this.reader) {
        const { err, nread } = this.queue[this.reader]
        delete this.queue[this.reader++]
        return err ? reject(err) : resolve(this.readBuffer.slice(0, nread))
      }
      if (this.paused) {
        this.paused = false
        nuv_read_start(this.handle)
      }
      this.queue[this.reader++] = { resolve, reject }
    })
  }

  set keepalive (delay) {
    nuv_tcp_keepalive(this.handle, !!delay, delay)
  }

  set nodelay (enable) {
    nuv_tcp_nodelay(this.handle, enable)
  }

  [Symbol.asyncIterator] () {
    return {
      next: () => new Promise((resolve, reject) => {
        this.read().then(
          value => resolve({value}),
          err => err.code === 'EOF' ? resolve({done: true}) : reject(err)
        )
      })
    }
  }
}

class Client extends Socket {
  connect (ip, port) {
    nuv_tcp_connect(Buffer.alloc(sizeof_uv_connect_t), this.handle, ip, port)
    return this.wait('onConnect', this)
  }
}

class Udp extends Handle {
  constructor (handlers) {
    super()
    for (const key in handlers) {
      this[key] = handlers[key]
    }
    nuv_udp_init(Buffer.alloc(sizeof_uv_udp_t), this)
    if (!this.readBuffer) this.readBuffer = Buffer.alloc(0x10000)
    this.queue = []
    this.reader = 0
    this.writer = 0
    this.paused = true
  }

  get sockname () {
    return nuv_udp_getsockname(this.handle)
  }

  bind (ip, port) {
    nuv_udp_bind(this.handle, ip, port)
    return this
  }

  send (ip, port, data, ...args) {
    if (!Buffer.isBuffer(data)) data = Buffer.from(data, ...args)
    nuv_udp_send(Buffer.alloc(sizeof_uv_udp_send_t), this.handle, data, ip, port)
    return this.wait('onSend')
  }

  onRecv (err, nread, addr) {
    if (this.reader > this.writer) {
      const { resolve, reject } = this.queue[this.writer]
      delete this.queue[this.writer++]
      return err ? reject(err) : resolve({
        data: this.readBuffer.slice(0, nread),
        ip: addr.ip,
        port: addr.port
      })
    }
    if (!this.paused) {
      this.paused = true
      nuv_udp_recv_stop(this.handle)
    }
    this.queue[this.writer++] = { err, nread, addr }
  }

  recv () {
    return new Promise((resolve, reject) => {
      if (this.writer > this.reader) {
        const { err, nread, addr } = this.queue[this.reader]
        delete this.queue[this.reader++]
        return err ? reject(err) : resolve({
          data: this.readBuffer.slice(0, nread),
          ip: addr.ip,
          port: addr.port
        })
      }
      if (this.paused) {
        this.paused = false
        nuv_udp_recv_start(this.handle)
      }
      this.queue[this.reader++] = { resolve, reject }
    })
  }

  [Symbol.asyncIterator] () {
    return {
      next: () => new Promise((resolve, reject) => {
        this.recv().then(
          value => resolve({value}),
          err => reject(err)
        )
      })
    }
  }
}

exports.Server = Server
exports.Client = Client
exports.Udp = Udp
