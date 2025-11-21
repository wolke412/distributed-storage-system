package cmd

import (
	"fmt"
	"net"
)

func readLargeBuffer(conn net.Conn, bufferSize int) ([]byte, error) {
	buf := make([]byte, bufferSize)
	populated := 0

	for populated < bufferSize {
		n, err := conn.Read(buf[populated:])
		if err != nil {
			return nil, err
		}
		populated += n
	}

	return buf, nil
}

func HandleRequest(state *ClientState, args []string) *CommandResult {

	if state.State != StateConnected || state.Conn == nil {
		return Failure("not connected", nil)
	}

	if len(args) < 2 {
		return Failure("usage: request <command> [,params]", nil)
	}

	subcommand := args[0]

	switch subcommand {
	case "file":
		return HandleFileRequest(state, args[1:])
	default:
		return Warning("invalid action :" + subcommand + ". not a valid subcommand.")
	}

}

func HandleFileRequest(state *ClientState, args []string) *CommandResult {

	if len(args) < 1 {
		// if len(args) < 2 {
		return Warning("Usage: req file <name> <path>")
	}

	fileName := args[0]
	// storeAt := args[1]

	pkt := FileRequestPacket{
		FileName: ToFixed256([]byte(fileName)),
	}

	// buffer := make([]byte, pkt.FileSize)
	// _, err = io.ReadFull(file, buffer)

	// if err != nil {
	// return Failure("Failed to read file", err)
	// }

	p := Packet(RequestFilePacket, &pkt)

	ok := sendBytes(state, p.Serialize(), "file")

	c, err := ReadPacket[FileResponsePacket](state)

	if err != nil {
		return Failure("Error reading package.", err)
	}

	state.Console.AddLog(fmt.Sprintf("TYPE = %d \n", c.Type))
	state.Console.Draw()

	return ok

	// if ok.Status == StatusError {
	// 	return ok
	// }

	//state.Console.AddLog(ok.PrettyString())

}
