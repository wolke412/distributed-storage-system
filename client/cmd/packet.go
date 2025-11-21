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

	RequestFilePacket MessageType = 15
)

type CommunicationPacket[T Serializable] struct {
	SenderID uint64
	Type     MessageType

	Content *T
}

func Packet[T Serializable](typ MessageType, content *T) *CommunicationPacket[T] {
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

	b = binary.LittleEndian.AppendUint64(b, c.SenderID)

	b = append(b, c.Type)

	if c.Content != nil {
		a := *c.Content
		b = append(b, a.Serialize()...)
	}

	return b
}

func (c *CommunicationPacket[T]) Unserialize(b []byte) error {
	if len(b) < 9 {
		return errors.New("too short")
	}

	c.SenderID = binary.BigEndian.Uint64(b[:8])
	c.Type = uint8(b[8])

	if c.Content != nil {
		a := *c.Content
		return a.Unserialize(b[9:])
	}

	return nil
}

// ------------------------------------------------------------
type FileDeclarationPacket struct {
	FileName [256]byte
	FileSize uint64
}

func (c FileDeclarationPacket) Serialize() []byte {

	b := make([]byte, 0, 256+8)

	b = append(b, c.FileName[:]...)

	b = binary.LittleEndian.AppendUint64(b, c.FileSize)

	return b
}

func (c FileDeclarationPacket) Unserialize(b []byte) error {
	return nil
}

// ------------------------------------------------------------

type FileRequestPacket struct {
	FileName [256]byte
}

func (c FileRequestPacket) Serialize() []byte {

	b := make([]byte, 0, 256+8)
	b = append(b, c.FileName[:]...)

	return b
}

func (c FileRequestPacket) Unserialize(b []byte) error {
	return nil
}

// ------------------------------------------------------------

type FileResponsePacket struct {
	FileSize           uint64
	FileId             uint64
	FragmentCountTotal uint8
}

func (c FileResponsePacket) Serialize() []byte {

	b := make([]byte, 0, 256+8)

	return b
}

func (c FileResponsePacket) Unserialize(b []byte) error {

	if len(b) < 16 {
		return errors.New("Invalid len")
	}
	c.FileSize = binary.BigEndian.Uint64(b[:8])
	c.FileId = binary.BigEndian.Uint64(b[8:16])
	c.FragmentCountTotal = b[16]
	return nil
}

type Empty struct {
}

func (c Empty) Serialize() []byte {
	b := make([]byte, 0, 256+8)
	return b
}

func (c Empty) Unserialize(b []byte) error {
	return nil
}

// ------------------------------------------------------------

func ReadPacket[T Serializable](state *ClientState) (*CommunicationPacket[T], error) {

	var bytes [256]byte

	r, err := state.Conn.Read(bytes[:])

	fmt.Printf("READ %d bytes \n", r)

	if err != nil {
		return nil, err
	}

	c := CommunicationPacket[T]{}

	c.Unserialize(bytes[:r])

	return &c, nil
}
