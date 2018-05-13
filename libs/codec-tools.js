// read / write - stream prmitives
// await read() -> value
// await read() -> undefined
// await write(value)
// await write()

// encode / decode - low-level codecs
// decode(input, offset) -> [value, offset]
// decode(input, offset) -> undefined
// encode(value) -> encoded
// encode() -> encoded?

// encoder / decode - higher-level codecs
// encoder(write) -> newWrite (with updateEncode and updateWrite)
// decoder(read) -> newRead (with updateDecode and updateRead)

// makeEncoder(encode) -> encoder (with updateEncode)
exports.makeEncoder = encode => {
  const encoder = inner => {
    const write = async value => write.inner(write.encode(value))
    write.encode = encoder.encode
    write.inner = inner
    return write
  }
  encoder.encode = encode
  return encoder
}

// makeDecoder(decode) -> decoder (with updateDecode)
exports.makeDecoder = decode => {
  const decoder = inner => {
    let buffer = null
    let offset = null
    let done = false
    const read = async () => {
      while (true) {
        let result = read.decode(buffer, offset)
        if (result) {
          offset = result[1]
          if (buffer && offset >= buffer.length) {
            buffer = null
            offset = null
          }
          return result[0]
        }
        if (done) return
        const next = await read.inner()
        if (!next) {
          done = true
          continue
        }
        if (!buffer) {
          buffer = next
          offset = 0
          continue
        }
        const bytesLeft = buffer.length - offset
        const last = buffer
        buffer = Buffer.alloc(bytesLeft + next.length)
        last.copy(buffer, 0, offset)
        next.copy(buffer, bytesLeft)
        offset = 0
      }
    }
    read.decode = decoder.decode
    read.inner = inner
    return read
  }
  decoder.decode = decode
  return decoder
}
