package main

import "fmt"
import "time"

func main() {
	messages := make(chan string)

	go func() {
		time.Sleep(time.Second)
		//messages <- "ping"
	}()

	msg := <-messages
	fmt.Println(msg)
}
