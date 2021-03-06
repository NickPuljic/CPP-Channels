package main

import (
    "log"
    "time"
    "math/rand"
)

func main() {
    start := time.Now()


    for n := 0; n < 500000; n++ {
        i := rand.Intn(99) + 1

        bufferedChannel := make(chan string, i)

        for m := 0; m < i; m++ {
            go func() { bufferedChannel <- "measurement" }()
        }

        for m := 0; m < i; m++ {
            <- bufferedChannel
        }

        close(bufferedChannel)
    }

    elapsed := time.Since(start)
    log.Printf("Program took %s", elapsed)
}
