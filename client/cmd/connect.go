package cmd

import (
	"fmt"
	"net"
	"strings"
)

func HandleConnect(state *ClientState, args []string) *CommandResult {

	var addr string
	if len(args) < 1 {
		addr = ":52000"
		//return Failure("usage: connect <host:port>", nil)
	} else {
		addr = strings.TrimSpace(args[0])
	}

	fmt.Printf("Connecting to %s...\n", addr)

	conn, err := net.Dial("tcp", addr)
	if err != nil {
		state.SetState(StateDisconnected)
		return Failure("connection failed", err)
	}

	state.Conn = conn
	state.ConnectedTo = addr
	state.SetState(StateConnected)

	return Success(fmt.Sprintf("Connected to %s", addr))
}
