// In a [previous](range) example we saw how `for` and
// `range` provide iteration over basic data structures.
// We can also use this syntax to iterate over
// values received from a channel.

package main

import "fmt"
import "time"

func main() {

    // We'll iterate over 2 values in the `queue` channel.
    queue := make(chan string)

	go func() {
		queue <- "one"
	}()
	time.Sleep(time.Second)
    //queue <- "one"
    //queue <- "two"
    //close(queue)

    // This `range` iterates over each element as it's
    // received from `queue`. Because we `close`d the
    // channel above, the iteration terminates after
    // receiving the 2 elements.
    for elem := range queue {
        fmt.Println(elem)
    }
	fmt.Println("test")
	//j := <-queue
	//fmt.Println(j)
}
