package main

import (
	"net/http"
	"os"

	"golang.org/x/net/websocket"
)

func logging(fp *os.File, log chan string) {
	for {
		msg := <-log
		fp.WriteString(msg)
	}
}

func main() {
	file := os.Args[1]
	fp, err := os.Create(file)
	if err != nil {
		panic(err)
	}
	defer fp.Close()
	log := make(chan string)
	go logging(fp, log)
	serv := &websocket.Server{}
	serv.Handler = func(ws *websocket.Conn) {
		for {
			var msg string
			err := websocket.Message.Receive(ws, &msg)
			if err != nil {
				break
			}
			log <- msg
		}
	}
	http.Handle("/log", serv)
	http.ListenAndServe(":8080", nil)
}
