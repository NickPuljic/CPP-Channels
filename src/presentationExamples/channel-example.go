package main

import "fmt"
import "time"

func main() {
    c := make(chan string) // unbuffered string channel

    go func() { // Goroutine 1
        data_1 := <- c // receive
        fmt.Printf("Data from Goroutine 1: %s\n", data_1)
        c <- "Goroutine 1 data" // send
    }()
    go func() { // Goroutine 2
        c <- "Goroutine 2 data" // send
        data_2 := <- c // receive
        fmt.Printf("Data from Goroutine 2: %s\n", data_2)
    }()
    time.Sleep(time.Second)
}
