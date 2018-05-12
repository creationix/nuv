const { Udp } = require('.')

async function serverMain () {
  let server = new Udp()
  server.bind('0.0.0.0', 1337)
  console.log(server.sockname)
  for await (let frame of server) {
    console.log(frame)
    server.send(frame.ip, frame.port, frame.data)
    await server.close()
  }
}

async function clientMain () {
  let client = new Udp()
  console.log('Sending')
  await client.send('127.0.0.1', 1337, 'Hello World')
  console.log('Sent!')
  console.log('Echo back', await client.recv())
  await client.close()
}

serverMain().catch(console.error)
clientMain().catch(console.error)
