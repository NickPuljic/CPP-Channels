package main

import (
    "log"
    "time"
)

func main() {
    unbufferedChannel := make(chan int)

    start := time.Now()

    for n := 0; n < 50000; n++ {
        go func() { unbufferedChannel <- 0 }()

        <- unbufferedChannel
    }

    elapsed := time.Since(start)

    log.Printf("Program took %s", elapsed)
}
