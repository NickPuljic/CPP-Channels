package main

import (
    "log"
    "time"
)

func main() {
    unbufferedChannel := make(chan string)

    start := time.Now()


    for n := 0; n < 5000000; n++ {
        go func() { unbufferedChannel <- "measurement" }()

        <-unbufferedChannel
    }

    elapsed := time.Since(start)
    log.Printf("Program took %s", elapsed)
}
