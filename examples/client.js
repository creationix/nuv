const { Client } = require('..')

let messages = [ 'Hello', 'How are you', 'Goodbye' ]

async function main () {
  // Create a new client and connect to the echo server.
  let client = await new Client()
    .connect('127.0.0.1', 8080)

  // Show the local and remote ports
  console.log('Connected', client.sockname, client.peername)

  // Send several messages to the echo server
  for (let message of messages) {
    await client.write(message)
    console.log('written', {message})
    let reply = await client.read()
    console.log('read', {reply: reply.toString()})
  }

  // Shutdown the write stream
  await client.write()

  // Read any extra messages from the server.
  for await (let message of client) {
    console.log('extra', { message: message.toString() })
  }

  await client.close()
  console.log('Socket closed')
}

main().catch(console.error)
