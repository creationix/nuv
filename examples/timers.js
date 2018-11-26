const { setTimeout, setInterval, clearTimeout, clearInterval } = require('..')

console.log('Starting timeout...')
setTimeout((...args) => {
  console.log('Timeout!', args)
}, 500, 1, 2, 3)

console.log('Starting interval')
let count = 10
let interval
interval = setInterval((...args) => {
  console.log('tick...', count, args)
  if (!(--count)) {
    clearInterval(interval)
    console.log('done')
  }
}, 100, 1, 2, 3)
