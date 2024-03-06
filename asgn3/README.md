A simple FIFO queue built for multi threaded applications. A semaphore is used to track the count of objects in the queue, blocking requests to push/pop until the quantity changes. There's a single mutex lock working for both the push and pull procedures. 

The queue object comes with four functions.
- queue_new()
- queue_delete()
- queue_push()
- queue_pop()