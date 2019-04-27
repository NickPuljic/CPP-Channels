package main

import (
    "log"
    "time"
)

func main() {
    unbufferedChannel1 := make(chan string)
    unbufferedChannel2 := make(chan string)

    start := time.Now()

    for n := 0; n < 5000000; n++ {
        go func() { unbufferedChannel1 <- "measurement" }()
        go func() { unbufferedChannel2 <- "measurement" }()

        select {
        case <- unbufferedChannel1:
            continue
        case <- unbufferedChannel2:
            continue
        default:
            continue
        }
    }

    elapsed := time.Since(start)
    log.Printf("Program took %s", elapsed)
}
