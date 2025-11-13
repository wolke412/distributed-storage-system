package cmd

import (
	"fmt"
	"io"
)

func ReadPackets(state *ClientState) *CommandResult {
	if state.State != StateConnected || state.Conn == nil {
		return Failure("not connected", nil)
	}

	buf := make([]byte, 1024)
	n, err := state.Conn.Read(buf)
	if err != nil {
		if err == io.EOF {
			state.SetState(StateDisconnected)
			return Warning("connection closed by peer")
		}
		return Failure("read error", err)
	}

	return Success(fmt.Sprintf("Received %d bytes: %v", n, buf[:n]))
}

