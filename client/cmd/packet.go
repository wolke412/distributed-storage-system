package cmd

import (
	"encoding/binary"
	"math"
)


type Serializable interface {
	Serialize() []byte
}


type MessageType = uint8

const (
	PresentItself 		MessageType = 1
	CreateFilePacket 	MessageType = 10
)

type CommunicationPacket struct {
	SenderID uint64
	Type MessageType 

	Content Serializable 
}

func Packet( typ MessageType, content Serializable ) *CommunicationPacket {
	// identifies client
	var sid  uint64 = math.MaxUint64
	// var sid  uint64 = 14

	return &CommunicationPacket{
		SenderID: sid,
		Type: typ ,
		Content: content,
	}
}


func(c *CommunicationPacket) Serialize() []byte {
	
	b := make([]byte, 0, 4096)

	b = binary.LittleEndian.AppendUint64(b, c.SenderID)
	
	b = append(b, c.Type)
	
	if c.Content != nil {
		b = append(b, c.Content.Serialize()...)
	}

	return b
}


// ------------------------------------------------------------
type FileDeclarationPacket struct {
	FileName [256]byte		
	FileSize uint64
}
func(c *FileDeclarationPacket) Serialize() []byte {
	
	b := make([]byte, 0, 256 + 8)
	
	b = append(b, c.FileName[:]...)

	b = binary.LittleEndian.AppendUint64( b, c.FileSize )

	return b
}
// ------------------------------------------------------------

