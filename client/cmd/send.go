package cmd

import (
	"fmt"
	"io"
	"math"
	"os"
	"strings"
	"time"

	"github.com/wolke412/paint"
)

func sendBytes(state *ClientState, data []byte, context string) *CommandResult {
	if state.State != StateConnected || state.Conn == nil {
		return Failure("not connected", nil)
	}

	n, err := state.Conn.Write(data)

	if err != nil {
		return Failure(fmt.Sprintf("failed to send %s", context), err)
	}

	return Success(fmt.Sprintf("sent %d bytes (%s)", n, context))
}

func HandleSendRaw(state *ClientState, args []string) *CommandResult {
	if len(args) < 1 {
		return Failure("usage: sendraw <message>", nil)
	}

	message := strings.Join(args, " ")
	return sendBytes(state, []byte(message), "raw")
}

// HandleSend sends a structured command packet of the form:
//
//	send <command> [,params]
func HandleSend(state *ClientState, args []string) *CommandResult {
	if len(args) < 1 {
		return Failure("usage: send <command> [,params]", nil)
	}

	subcommand := args[0]

	switch subcommand {
	case "file":
		return HandleFileTransmission(state, args[1:])
	case "raw":
		return HandleSendRaw(state, args[1:])
	case "auth":
		return HandleAuth(state)
	default:
		return Warning("invalid action :" + subcommand + ". not a valid subcommand.")
	}

}

func HandleAuth(state *ClientState) *CommandResult {

	p := Packet(PresentItself, nil)
	dbg := fmt.Sprintf(" { sender_id: %u, type: %d } ", p.SenderID, p.Type)

	state.Console.AddLog(dbg)

	// return Success("Authorized! " + dbg)
	return sendBytes(state, p.Serialize(), "auth")
}

func HandleFileTransmission(state *ClientState, args []string) *CommandResult {

	if len(args) < 1 {
		return Warning("Usage: send file <path>")
	}

	filePath := args[0]

	file, err := os.Open(filePath)
	if err != nil {
		return Failure("Failed to open file", err)
	}
	defer file.Close()

	// Get file info
	info, err := file.Stat()
	if err != nil {
		return Failure("Failed to stat file", err)
	}

	if len(info.Name()) > 255 {
		return Failure("File name larger than 255 chracters.", nil)
	}

	pkt := FileDeclarationPacket{
		FileName: ToFixed256([]byte(info.Name())),
		FileSize: uint64(info.Size()),
	}

	buffer := make([]byte, pkt.FileSize)
	_, err = io.ReadFull(file, buffer)

	if err != nil {
		return Failure("Failed to read file", err)
	}

	p := Packet(CreateFilePacket, &pkt)

	ok := sendBytes(state, p.Serialize(), "file")

	if ok.Status == StatusError {
		return ok
	}

	state.Console.AddLog(ok.PrettyString())
	// state.Console.Draw()

	// var b [4096]byte
	// n, err := state.Conn.Read(b[:])

	// if err != nil {
	// 	return Failure("Failed to read confirmation", err)
	// }

	// state.Console.AddLog(paint.BrightCyan(fmt.Sprintf("Confirmation successfull, read %d bytes", n)))

	time.Sleep(time.Millisecond * 10)

	// max buffer size on node's side
	state.Console.AddLog(paint.BrightCyan("Sending file..."))
	bufsz := 4096
	iters := int(math.Ceil(float64(pkt.FileSize) / float64(bufsz)))
	for i := 0; i < iters; i++ {
		start := i * bufsz
		end := start + bufsz
		if end > len(buffer) {
			end = len(buffer)
		}

		chunk := buffer[start:end]

		state.Console.AddLog(paint.BrightCyan(fmt.Sprintf("Part %d/%d", i+1, iters)))

		s := sendBytes(state, chunk, "file_contents")

		state.Console.AddLog(s.PrettyString())
		state.Console.Draw()
	}

	return Warning("Sending done.")
}

func ToFixed256(b []byte) [256]byte {
	var arr [256]byte
	copy(arr[:], b)
	return arr
}
