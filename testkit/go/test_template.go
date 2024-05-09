package main

import (
	"log"
	"os"
)

func main() {
	inputFile := os.Args[1]
	outputFile := os.Args[2]

	input, err := os.ReadFile(inputFile)
	if err != nil {
		log.Println(err)
		os.Exit(2)
	}

	var t TEST_CLASS

	n, err := t.Decode(input)

	if err != nil {
		log.Println(err)
		os.Exit(1)
	}

	if n != len(input) {
		log.Printf("expect length %d but actual %d", len(input), n)
		os.Exit(1)
	}

	enc, err := t.Encode()
	if err != nil {
		log.Println(err)
		os.Exit(2)
	}

	err = os.WriteFile(outputFile, enc, 0o777)
	if err != nil {
		log.Println(err)
		os.Exit(2)
	}
}
