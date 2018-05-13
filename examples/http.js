const { Server } = require('..')
const { encoder, decoder } = require('../libs/codec-http')

// Easy server creation with chainable methods.
console.log('Listening:', new Server({ onConnection })
  .bind('0.0.0.0', 8080)
  .listen(128)
  .sockname)

async function onConnection () {
  // Accept the incoming connection and create a socket
  let socket = this.accept()
  console.log('Accepted:', socket.sockname, socket.peername)
  const socketRead = async () => socket.read()
  const socketWrite = async value => socket.write(value)
  const read = decoder(socketRead)
  const write = encoder(socketWrite)

  // Use async iterators to read TCP packets off the socket.
  console.log(read)
  for await (const head of read) {
    console.log(read)
    const chunks = []
    console.log(head)
    chunks.push(Buffer.from(`${head.method} ${head.path} HTTP/${head.version}\n`))
    let i = 0
    for (const val of head.headers) {
      chunks.push(Buffer.from(val + (i++ % 2 ? '\n' : ': ')))
    }
    chunks.push(Buffer.from('\n'))
    for await (const chunk of read) {
      console.log(read)
      console.log(chunk)
      chunks.push(chunk)
      chunks.push(Buffer.from('\n'))
    }
    const body = Buffer.concat(chunks)
    console.log(read)
    console.log(body)
    console.log(write)
    await write({
      code: 200,
      headers: [
        'Content-Length', body.length
      ]
    })
    console.log(write)
    await write(body)
    console.log(write)
  }
  await socket.write()
}
