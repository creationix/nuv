exports.assert = assert

// lua-style assert helper
function assert (val, message) {
  if (!val) throw new Error(message || 'Assertion Failed')
  return val
}
