package cmd

import (
	"fmt"
	"strings"

	"github.com/wolke412/paint"
)

func StartREPL(state *ClientState) {

	// scanner := bufio.NewScanner(os.Stdin)

	fmt.Println("Welcome to the Node Client. Type 'help' for commands.")

	for {
		// fmt.Print("> ")
		// if !scanner.Scan() {
		// 	break
		// }

		input := state.Console.Prompt()


		line := strings.TrimSpace(input)
		if line == "" {
			continue
		}

		parts := strings.Fields(line)
		cmd := parts[0]
		args := parts[1:]

		state.Console.AddLog( " > " + paint.White( line ) )

		var res *CommandResult

		switch cmd {
		case "connect", "con":
			res = HandleConnect(state, args)

		case "send":
			res = HandleSend(state, args)

		case "read":
			res = ReadPackets(state)

		case "state":
			msg := fmt.Sprintf("Current state: %v, Connected to: %s", state.State, state.ConnectedTo)
			res = Success(msg)

		case "close":
			err :=state.Conn.Close()
			if err != nil {
				res = Failure("Error closing connection.", err)
			} else {
				res = Success("Connection closed successfully")
			}


		case "exit", "quit":
			fmt.Println("Exiting...")
			return

		default:
			res = Warning("unknown command: " + cmd)
		}

		if res != nil {
			state.Console.AddLog( res.PrettyString() )
		}
	}
}

