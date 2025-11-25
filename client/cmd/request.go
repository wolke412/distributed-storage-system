package cmd

import (
	"errors"
	"fmt"
	"io"
	"net"
	"os"
	"path/filepath"
	"time"
)

func readLargeBuffer(state *ClientState, bufferSize int) ([]byte, error) {

	conn := state.Conn

	buf := make([]byte, bufferSize)
	populated := 0

	for populated < bufferSize {
		n, err := conn.Read(buf[populated:])
		if err != nil {
			return nil, err
		}

		populated += n

		state.Console.ModifyLog(-1, fmt.Sprintf("%.2f %%", float64(populated)/float64(bufferSize)))
		state.Console.Draw()
	}

	return buf, nil
}

func readPacket(conn net.Conn) ([]byte, error) {
	header := make([]byte, 2)
	io.ReadFull(conn, header)

	size := int(header[0])<<8 | int(header[1])
	payload := make([]byte, size-2)

	io.ReadFull(conn, payload)
	return append(header, payload...), nil
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
		return Warning("Usage: req file <name> <?path>")
	}

	fileName := args[0]

	var storeAt string

	if len(args) == 1 {
		storeAt = "./files"
	} else {
		storeAt = args[1]
	}

	pkt := FileRequestPacket{
		FileName: ToFixed200([]byte(fileName)),
	}

	p := Packet(RequestFilePacket, &pkt)

	fmt.Printf("SERIAL: \n %+v \n", p.Serialize())

	ok := sendBytes(state, p.Serialize(), "request")

	if ok.Status == StatusError {
		return ok
	}

	time.Sleep(time.Millisecond * 50)

	fmt.Printf("WAITING FILE DESCRIPTOR PACKET .\n")

	frp, err := ReadPacket(state, &FileResponsePacket{})
	if err != nil {
		return Failure("Error reading FILE DESCRIPTOR", err)
	}

	state.Console.AddLog(fmt.Sprintf("<< SIZE %d SENDER %d TYPE %d", frp.Size, frp.SenderID, frp.Type))
	state.Console.AddLog(fmt.Sprintf("<< FILE  %d SIZE = %d FRAGS = %d", frp.Content.FileId, frp.Content.FileSize, frp.Content.FragmentCountTotal))
	state.Console.Draw()

	if frp.Type != RequestFilePacketRes {
		return Failure("Server responded with NOT OK", errors.New("err code=2"))
	}

	fmt.Printf("Sending ok...\n")

	ok = sendBytes(state, PacketOk().Serialize(), "request")
	if ok.Status == StatusError {
		return ok
	}

	time.Sleep(time.Millisecond * 50)

	fmt.Printf("WAITING RAW...\n")

	state.Console.AddLog("RAW: 0%")
	state.Console.Draw()
	buf, err := readLargeBuffer(state, int(frp.Content.FileSize))
	if err != nil {
		return Failure("Error reading file.", errors.New("err code=3"))
	}

	err = SaveBufferToFile(buf, storeAt, fileName)
	if err != nil {
		return Failure("Error saving buffer.", err)
	}

	return Success("File successfully saved to " + storeAt + " as " + fileName)
}

func SaveBufferToFile(buf []byte, storeTo, fileName string) error {

	if err := os.MkdirAll(storeTo, os.ModePerm); err != nil {
		return fmt.Errorf("failed to create directory: %w", err)
	}

	fullPath := filepath.Join(storeTo, fileName)

	if err := os.WriteFile(fullPath, buf, 0644); err != nil {
		return fmt.Errorf("failed to write file: %w", err)
	}

	return nil
}
