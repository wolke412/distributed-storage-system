package cmd

import (
	"encoding/binary"
	"errors"
	"fmt"
	"math"
)

type Serializable interface {
	Serialize() []byte
	Unserialize([]byte) error
}

type MessageType = uint8

const (
	PresentItself    MessageType = 1
	CreateFilePacket MessageType = 10

	RequestFilePacket    MessageType = 15
	RequestFilePacketRes MessageType = 16

	StatusOK    MessageType = 200
	StatusNotOK MessageType = 220
)

type CommunicationPacket[T Serializable] struct {
	Size     uint16
	SenderID uint64
	Type     MessageType

	Content T
}

func Packet[T Serializable](typ MessageType, content T) *CommunicationPacket[T] {
	// identifies client
	var sid uint64 = math.MaxUint64
	// var sid  uint64 = 14

	return &CommunicationPacket[T]{
		SenderID: sid,
		Type:     typ,
		Content:  content,
	}
}

func (c *CommunicationPacket[T]) Serialize() []byte {

	b := make([]byte, 0, 4096)

	b = binary.LittleEndian.AppendUint16(b, 0)
	b = binary.LittleEndian.AppendUint64(b, c.SenderID)

	b = append(b, c.Type)

	b = append(b, c.Content.Serialize()...)

	binary.LittleEndian.PutUint16(b, uint16(len(b)))

	return b
}

func (c *CommunicationPacket[T]) Unserialize(b []byte) error {
	if len(b) < 9 {
		return errors.New("too short")
	}

	c.Size = binary.LittleEndian.Uint16(b[:2])
	c.SenderID = binary.LittleEndian.Uint64(b[2:10])
	c.Type = uint8(b[10])

	return c.Content.Unserialize(b[11:])

}

func PacketOk() *CommunicationPacket[*Empty] {
	return Packet(StatusOK, &Empty{})
}

// ------------------------------------------------------------
type FileDeclarationPacket struct {
	FileName [200]byte
	FileSize uint64
}

func (c *FileDeclarationPacket) Serialize() []byte {

	b := make([]byte, 0, 256+8)

	b = append(b, c.FileName[:]...)

	b = binary.LittleEndian.AppendUint64(b, c.FileSize)

	return b
}

func (c *FileDeclarationPacket) Unserialize(b []byte) error {
	return nil
}

// ------------------------------------------------------------

type FileRequestPacket struct {
	FileName [200]byte
}

func (c *FileRequestPacket) Serialize() []byte {

	b := make([]byte, 0, 256)
	b = append(b, c.FileName[:]...)

	return b
}

func (c *FileRequestPacket) Unserialize(b []byte) error {
	return nil
}

// ------------------------------------------------------------

type FileResponsePacket struct {
	FileSize           uint64
	FileId             uint64
	FragmentCountTotal uint8
}

func (c *FileResponsePacket) Serialize() []byte {

	b := make([]byte, 0, 256)

	return b
}

func (c *FileResponsePacket) Unserialize(b []byte) error {

	if len(b) < 16 {
		return errors.New("invalid len")
	}

	c.FileSize = binary.LittleEndian.Uint64(b[:8])
	c.FileId = binary.LittleEndian.Uint64(b[8:16])
	c.FragmentCountTotal = b[16]

	fmt.Printf("UNSERIALIZING FRP\n")

	return nil
}

type Empty struct {
}

func (c *Empty) Serialize() []byte {
	b := make([]byte, 0, 256)
	return b
}

func (c *Empty) Unserialize(b []byte) error {
	return nil
}

// ------------------------------------------------------------

func ReadPacket[T Serializable](state *ClientState, content T) (*CommunicationPacket[T], error) {

	var sizeBuf [2]byte
	if _, err := state.Conn.Read(sizeBuf[:]); err != nil {
		return nil, fmt.Errorf("failed to read packet size: %w", err)
	}

	packetSize := uint16(sizeBuf[1])<<8 | uint16(sizeBuf[0])
	if packetSize < 2 {
		return nil, fmt.Errorf("invalid packet size: %d", packetSize)
	}

	state.Console.AddLog(fmt.Sprintf("<< WAITING FOR %d bytes (total packet)", packetSize))
	state.Console.Draw()

	fullBuf := make([]byte, packetSize)
	copy(fullBuf[:2], sizeBuf[:])

	// Read the remaining (packetSize - 2) bytes
	n := 0
	for n < int(packetSize-2) {
		readBytes, err := state.Conn.Read(fullBuf[2+n:])
		if err != nil {
			return nil, fmt.Errorf("failed to read full packet: %w", err)
		}
		n += readBytes
	}

	state.Console.AddLog(fmt.Sprintf("<< READ %d bytes (total packet)", packetSize))

	c := &CommunicationPacket[T]{
		Content: content,
	}

	if err := c.Unserialize(fullBuf); err != nil {
		return nil, fmt.Errorf("failed to unserialize packet: %w", err)
	}

	state.Console.AddLog(fmt.Sprintf("<< READ STRUCT %+v", c))
	state.Console.AddLog(fmt.Sprintf("<< CONT STRUCT %+v", c.Content))
	state.Console.Draw()

	return c, nil
}
