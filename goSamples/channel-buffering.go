package main

import "fmt"
import "time"

func main() {
    messages := make(chan string, 2)



    go func() {
        fmt.Println(<-messages)
        fmt.Println("testa")
    }()
    go func() {
        fmt.Println(<-messages)
        fmt.Println("testb")
    }()
    time.Sleep(time.Second)


    messages <- "buffered"
    fmt.Println("test1")
    messages <- "message"
    fmt.Println("test2")
    <- messages

    /*go func() {
        fmt.Println(<- messages)
    }()*/

    //time.Sleep(time.Second)

	//messages <- "buffered"
}
