package main

import (
    "log"
    "time"
)

func main() {
    i := 50

    start := time.Now()

    for n := 0; n < 500000; n++ {
        
        bufferedChannel := make(chan int, i)

        for m := 0; m < i; m++ {
            bufferedChannel <- 0
        }

        for m := 0; m < i; m++ {
            <- bufferedChannel
        }
    }

    elapsed := time.Since(start)
    log.Printf("Program took %s", elapsed)
}
