package main

import "fmt"
import "time"

func main() {
    c := make(chan string) // unbuffered string channel

    go func() { // routine 1
        data_1 := <- c
        c <- "routine 1 data"

        fmt.Printf("Data from routine 1: %s\n", data_1)
    }()
    go func() { // routine 2
        c <- "routine 2 data"
        data_2 := <- c

        fmt.Printf("Data from routine 2: %s\n", data_2)
    }()
    time.Sleep(time.Second)
}
