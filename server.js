const { Server } = require('.')

// Easy server creation with chainable methods.
console.log('Listening:', new Server({ onConnection })
  .bind('0.0.0.0', 8080)
  .listen(128)
  .sockname)

async function onConnection () {
  // Accept the incoming connection and create a socket
  let socket = this.accept()
  console.log('Accepted:', socket.sockname, socket.peername)

  // Use async iterators to read TCP packets off the socket.
  for await (let chunk of socket) {
    console.log('Echo:', { chunk: chunk.toString() })

    // Since we're waiting on writes to flush, there is automatic backpressure
    // all the way back.  The iterator automatically calls read_start/read-stop.
    await socket.write(chunk)
  }

  // When the iterator exits, it means the remote side sent an EOF
  console.log('Client sent EOF, shutting down...')

  // Send an extra message to client
  await socket.write('Server shutting down...')

  // Writing nothing flushes any pending data and shuts down the write side.
  await socket.write()

  // We can now close our socket and the server in general
  await socket.close()
  await this.close()
}
