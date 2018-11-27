const { Timer } = require('..')

function setTimeout (fn, ms = 0, ...args) {
  let timer = new Timer({
    onTimeout () {
      clearTimeout(timer)
      fn(...args)
    }
  })
  timer.start(ms)
  return timer
}

function setInterval (fn, ms = 0, ...args) {
  let timer = new Timer({
    onTimeout () { fn(...args) }
  })
  timer.start(ms, ms)
  return timer
}

function clearTimeout (timer) {
  timer.stop()
  timer.close()
}

let clearInterval = clearTimeout

let idle = new Timer({
  onTimeout () {
    console.log('Finally idle')
    idle.close()
  }
})
idle.start(0, 100)
console.log('Starting idle timer...', idle)

console.log('This timeout should cancel')
let timeout = setTimeout(() => {
  throw new Error("OOPS! This shouldn't happen")
}, 1000)
console.log('First timeout', timeout)
console.log('first repeat', timeout.repeat)

console.log('Starting another timeout...')
setTimeout((...args) => {
  idle.again()
  console.log('Canceling first timeout')
  clearTimeout(timeout)
  console.log('Second timeout fired', args)
}, 500, 1, 2, 3)

console.log('Starting interval')
let count = 10
let interval
interval = setInterval((...args) => {
  idle.again()
  console.log('tick...', count, args)
  if (!(--count)) {
    clearInterval(interval)
    console.log('done')
  }
}, 100, 1, 2, 3)
console.log('interval', interval)

console.log('Shrinking interval')
let shrink = new Timer()
shrink.start(0, 400)
shrink.onTimeout = () => {
  idle.again()
  console.log('Shrink', shrink.repeat)
  shrink.repeat >>>= 1
  if (!shrink.repeat) shrink.close()
}
idle.again()
