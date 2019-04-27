package main

import (
    "log"
    "time"
    //"math/rand"
)

func main() {
    i := 20

    start := time.Now()

    for n := 0; n < 500000; n++ {
        //i := rand.Intn(99) + 1

        bufferedChannel := make(chan int, i)

        for m := 0; m < i; m++ {
            bufferedChannel <- 0
        }

        close(bufferedChannel)

        for ele := range bufferedChannel {
          _ = ele
        }
    }

    elapsed := time.Since(start)
    log.Printf("Program took %s", elapsed)
}
