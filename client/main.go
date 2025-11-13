package main

import "client/cmd"


func main() {
	state := cmd.NewClientState()
	cmd.StartREPL(state)
}



