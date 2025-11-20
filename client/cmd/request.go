package cmd

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

	return ok

	// if ok.Status == StatusError {
	// 	return ok
	// }

	//state.Console.AddLog(ok.PrettyString())

}
